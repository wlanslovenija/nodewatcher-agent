/*
 * nodewatcher-agent - remote monitoring daemon
 *
 * Copyright (C) 2015 Jernej Kos <jernej@kos.mx>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <nodewatcher-agent/module.h>
#include <nodewatcher-agent/json.h>
#include <nodewatcher-agent/utils.h>

#include <libubox/uloop.h>
#include <libubox/ustream.h>
#include <libubox/usock.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <net/if.h>

struct olsr_client {
  /* A flag indicating the client to be ready for new connections. */
  bool ready;
  /* Module reference. */
  struct nodewatcher_module *module;
  /* File descriptor. */
  struct uloop_fd client;
  /* Read stream. */
  struct ustream_fd stream;
  /* Timer. */
  struct uloop_timeout timer;
  /* JSON parser. */
  struct json_tokener *jstok;
  /* Result object. */
  json_object *object;
};

/* Global OLSR client instance. */
static struct olsr_client oc;

static void nw_routing_olsr_client_close(struct ustream *s)
{
  ustream_free(s);
  close(oc.client.fd);

  /* Free the JSON parser. */
  json_tokener_free(oc.jstok);
  oc.jstok = NULL;

  /* Since we are done, we may cancel the timer. */
  uloop_timeout_cancel(&oc.timer);

  /* We have finished acquiring data. */
  oc.ready = true;
  nw_module_finish_acquire_data(oc.module, oc.object);
  /* We have passed object ownership, so remove the reference. */
  oc.object = NULL;
}

static void nw_routing_olsr_client_timeout(struct uloop_timeout *timeout)
{
  syslog(LOG_WARNING, "routing-olsr: Connection with local OLSR instance timed out.");
  nw_routing_olsr_client_close(&oc.stream.stream);
}

static void nw_routing_olsr_client_read(struct ustream *s, int bytes)
{
  /* Read and parse the JSON response. */
  for (;;) {
    int len;
    char *data = ustream_get_read_buf(s, &len);
    if (!data)
      break;

    /* Parse data as JSON, incrementally. */
    json_object *object = json_tokener_parse_ex(oc.jstok, data, len);
    if (object) {
      /* Request has been parsed. */
      json_object *config = NULL;
      json_object *interfaces = NULL;
      json_object *links = NULL;
      json_object_object_get_ex(object, "config", &config);
      json_object_object_get_ex(object, "interfaces", &interfaces);
      json_object_object_get_ex(object, "links", &links);

      if (config) {
        /* Router identifier. */
        NW_COPY_JSON_OBJECT(config, "mainIpAddress", oc.object, "router_id");
        /* Exported routes. */
        json_object *exported_routes = json_object_new_array();
        json_object *hna;
        json_object_object_get_ex(config, "hna", &hna);
        if (hna) {
          int i;
          for (i = 0; i < json_object_array_length(hna); i++) {
            json_object *entry = json_object_array_get_idx(hna, i);
            json_object *json_destination;
            json_object *json_genmask;
            json_object_object_get_ex(entry, "destination", &json_destination);
            json_object_object_get_ex(entry, "genmask", &json_genmask);
            if (!json_destination || !json_genmask)
              continue;

            const char *destination = json_object_get_string(json_destination);
            int genmask = json_object_get_int(json_genmask);
            /* Maximum address length + prefix size. */
            char dst_prefix[INET6_ADDRSTRLEN + 4];
            snprintf(dst_prefix, sizeof(dst_prefix), "%s/%d", destination, genmask);

            json_object *exported_route = json_object_new_object();
            json_object_object_add(exported_route, "dst_prefix", json_object_new_string(dst_prefix));
            json_object_array_add(exported_routes, exported_route);
          }
        }

        json_object_object_add(oc.object, "exported_routes", exported_routes);
      }

      /* MID. */
      json_object *aliases = json_object_new_array();
      json_object_object_add(oc.object, "link_local", aliases);
      if (interfaces) {
        int i;
        for (i = 0; i < json_object_array_length(interfaces); i++) {
          json_object *iface = json_object_array_get_idx(interfaces, i);
          json_object *json_address;
          json_object *json_name;
          json_object_object_get_ex(iface, "nameFromKernel", &json_name);
          json_object_object_get_ex(iface, "ipv4Address", &json_address);
          /* Try IPv6 if no address set. */
          if (!json_address) {
            json_object_object_get_ex(iface, "ipv6Address", &json_address);
          }

          if (!json_address || !json_name)
            continue;

          const char *address = json_object_get_string(json_address);
          const char *name = json_object_get_string(json_name);
          char address_interface[INET6_ADDRSTRLEN + 1 + IFNAMSIZ];
          snprintf(address_interface, sizeof(address_interface), "%s%%%s", address, name);
          json_object_array_add(aliases, json_object_new_string(address_interface));
        }
      }

      /* Neighbours. */
      json_object *neighbours = json_object_new_array();
      json_object_object_add(oc.object, "neighbours", neighbours);
      if (links) {
        int i;
        for (i = 0; i < json_object_array_length(links); i++) {
          json_object *link = json_object_array_get_idx(links, i);
          json_object *neighbour = json_object_new_object();
          json_object_array_add(neighbours, neighbour);

          NW_COPY_JSON_OBJECT(link, "remoteIP", neighbour, "address");
          NW_COPY_JSON_OBJECT(link, "linkQuality", neighbour, "lq");
          NW_COPY_JSON_OBJECT(link, "neighborLinkQuality", neighbour, "ilq");
          NW_COPY_JSON_OBJECT(link, "linkCost", neighbour, "cost");

          /* Determine which interface has this address. */
          if (interfaces) {
            json_object *json_local_ip;
            json_object_object_get_ex(link, "localIP", &json_local_ip);

            if (json_local_ip) {
              int j;
              const char *local_ip = json_object_get_string(json_local_ip);

              for (j = 0; j < json_object_array_length(interfaces); j++) {
                json_object *iface = json_object_array_get_idx(interfaces, i);
                json_object *json_address;
                json_object *json_name;
                json_object_object_get_ex(iface, "nameFromKernel", &json_name);
                json_object_object_get_ex(iface, "ipv4Address", &json_address);
                /* Try IPv6 if no address set. */
                if (!json_address) {
                  json_object_object_get_ex(iface, "ipv6Address", &json_address);
                }

                if (!json_address || !json_name)
                  continue;

                const char *address = json_object_get_string(json_address);
                const char *name = json_object_get_string(json_name);
                if (!strcmp(address, local_ip)) {
                  json_object_object_add(link, "interface", json_object_new_string(name));
                  break;
                }
              }
            }
          }
        }
      }

      json_object_put(object);

      return nw_routing_olsr_client_close(s);
    } else if (json_tokener_get_error(oc.jstok) != json_tokener_continue) {
      /* Parse error has occurred. */
      syslog(LOG_WARNING, "routing-olsr: Parse error while processing jsoninfo output.");
      return nw_routing_olsr_client_close(s);
    }

    /* Mark data as consumed. */
    ustream_consume(s, len);
  }
}

static void nw_routing_olsr_client_notify_state(struct ustream *s)
{
  if (!s->eof)
    return;

  nw_routing_olsr_client_close(s);
}

static void nw_routing_olsr_client_callback(struct uloop_fd *fd, unsigned int events)
{
  if (fd->eof || fd->error) {
    syslog(LOG_WARNING, "routing-olsr: Failed to connect to local OLSR instance.");
    nw_routing_olsr_client_close(&oc.stream.stream);
    return;
  }

  /* Connection has been established, prepare the stream. */
  uloop_fd_delete(&oc.client);
  oc.stream.stream.string_data = true;
  oc.stream.stream.notify_read = nw_routing_olsr_client_read;
  oc.stream.stream.notify_state = nw_routing_olsr_client_notify_state;
  ustream_fd_init(&oc.stream, oc.client.fd);

  /* Send the request. */
  ustream_printf(&oc.stream.stream, "/config/interfaces/links\n");
}

static int nw_routing_olsr_start_acquire_data(struct nodewatcher_module *module,
                                               struct ubus_context *ubus,
                                               struct uci_context *uci)
{
  /* Ignore new requests if previous ones did not complete yet. */
  if (!oc.ready)
    return -1;

  oc.object = json_object_new_object();
  oc.jstok = json_tokener_new();

  /* Open a connection to the olsrd jsoninfo socket. */
  /* TODO: We currently assume the local server to be running on 127.0.0.1:9090. */
  oc.client.cb = nw_routing_olsr_client_callback;
  oc.client.fd = usock(USOCK_TCP | USOCK_NUMERIC | USOCK_NONBLOCK, "127.0.0.1", "9090");
  if (oc.client.fd < 0) {
    syslog(LOG_WARNING, "routing-olsr: Failed to connect to local OLSR instance.");
    return nw_module_finish_acquire_data(module, oc.object);
  }

  oc.ready = false;
  uloop_fd_add(&oc.client, ULOOP_WRITE);

  /* Start a timer that will abort data retrieval. */
  oc.timer.cb = nw_routing_olsr_client_timeout;
  uloop_timeout_set(&oc.timer, 5000);

  return 0;
}

static int nw_routing_olsr_init(struct nodewatcher_module *module,
                                 struct ubus_context *ubus,
                                 struct uci_context *uci)
{
  /* Initialize the client structure. */
  oc.ready = true;
  oc.module = module;

  return 0;
}

/* Module descriptor. */
struct nodewatcher_module nw_module = {
  .name = "core.routing.olsr",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 1,
  .hooks = {
    .init               = nw_routing_olsr_init,
    .start_acquire_data = nw_routing_olsr_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};
