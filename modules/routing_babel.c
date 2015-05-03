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

#define INFO_SELF 1
#define INFO_SELF_NAME "self"
#define INFO_NEIGHBOUR 2
#define INFO_NEIGHBOUR_NAME "neighbour"
#define INFO_XROUTE 3
#define INFO_XROUTE_NAME "xroute"
#define INFO_ROUTE 4
#define INFO_ROUTE_NAME "route"

struct babel_client {
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
  /* Result object. */
  json_object *object;
};

/* Global Babel client instance. */
static struct babel_client bc;

static void nw_routing_babel_client_close(struct ustream *s)
{
  ustream_free(s);
  close(bc.client.fd);

  /* Since we are done, we may cancel the timer. */
  uloop_timeout_cancel(&bc.timer);

  /* We have finished acquiring data. */
  bc.ready = true;
  nw_module_finish_acquire_data(bc.module, bc.object);
  /* We have passed object ownership, so remove the reference. */
  bc.object = NULL;
}

static void nw_routing_babel_client_timeout(struct uloop_timeout *timeout)
{
  syslog(LOG_WARNING, "routing-babel: Connection with local Babel instance timed out.");
  nw_routing_babel_client_close(&bc.stream.stream);
}

static void nw_routing_babel_client_read(struct ustream *s, int bytes)
{
  struct ustream_buf *buf = s->r.head;

  /* Read line by line and parse the data. */
  for (;;) {
    char *str = ustream_get_read_buf(s, NULL);
    if (!str)
      break;

    /* Check if we have read a whole line. */
    char *newline = strchr(buf->data, '\n');
    if (!newline)
      break;

    /* Replace newline with NULL, to get a NULL-terminated line in "str". */
    *newline = 0;
    /* Tokenize by spaces. */
    char *type = strtok(str, " ");
    if (!strcmp(type, "BABEL")) {
      /* Header. */
    } else if (!strcmp(type, "add")) {
      /* Information. */
      char *info_type = strtok(NULL, " ");
      char *info_id = strtok(NULL, " ");
      int info = -1;
      json_object *item = NULL;

      /* Currently ignored. */
      (void) info_id;

      if (!strcmp(info_type, INFO_SELF_NAME)) {
        /* Router ID. */
        info = INFO_SELF;
      } else if (!strcmp(info_type, INFO_NEIGHBOUR_NAME)) {
        /* Neighbours. */
        info = INFO_NEIGHBOUR;

        /* Get the list of existing neighbours or create a new one. */
        json_object *neighbours;
        json_object_object_get_ex(bc.object, "neighbours", &neighbours);
        if (!neighbours) {
          neighbours = json_object_new_array();
          json_object_object_add(bc.object, "neighbours", neighbours);
        }

        /* Create a new neighbour entry. */
        item = json_object_new_object();
        json_object_array_add(neighbours, item);
      } else if (!strcmp(info_type, INFO_XROUTE_NAME)) {
        /* Exported routes. */
        info = INFO_XROUTE;
      } else if (!strcmp(info_type, INFO_ROUTE_NAME)) {
        /* Imported routes. */
        info = INFO_ROUTE;
      }

      for (;;) {
        char *key = strtok(NULL, " ");
        char *value = strtok(NULL, " ");
        if (!key || !value)
          break;

        switch (info) {
          case INFO_SELF: {
            if (!strcmp(key, "id")) {
              /* Router identifier. */
              json_object_object_add(bc.object, "router_id", json_object_new_string(value));
            }
            break;
          }
          case INFO_NEIGHBOUR: {
            if (!strcmp(key, "address")) {
              /* Link-local address of the neighbour. */
              json_object_object_add(item, "address", json_object_new_string(value));
            } else if (!strcmp(key, "if")) {
              /* Neighbour interface. */
              json_object_object_add(item, "interface", json_object_new_string(value));
            } else if (!strcmp(key, "reach")) {
              /* Neighbour reachability. */
              json_object_object_add(item, "reachability", json_object_new_int(strtol(value, NULL, 16)));
            } else if (!strcmp(key, "rxcost")) {
              /* Neighbour RX cost. */
              json_object_object_add(item, "rxcost", json_object_new_int(atoi(value)));
            } else if (!strcmp(key, "txcost")) {
              /* Neighbour TX cost. */
              json_object_object_add(item, "txcost", json_object_new_int(atoi(value)));
            } else if (!strcmp(key, "cost")) {
              /* Neighbour cost. */
              json_object_object_add(item, "cost", json_object_new_int(atoi(value)));
            }
            break;
          }
          case INFO_XROUTE: {
            /* Currently we do not report exported routes. */
            break;
          }
          case INFO_ROUTE: {
            /* Currently we do not report imported routes. */
            break;
          }
        }
      }
    } else if (!strcmp(type, "done")) {
      /* Finished. */
      return nw_routing_babel_client_close(s);
    }

    /* Mark data as consumed. */
    ustream_consume(s, newline + 1 - str);
  }
}

static void nw_routing_babel_client_notify_state(struct ustream *s)
{
  if (!s->eof)
    return;

  nw_routing_babel_client_close(s);
}

static void nw_routing_babel_client_callback(struct uloop_fd *fd, unsigned int events)
{
  if (fd->eof || fd->error) {
    syslog(LOG_WARNING, "routing-babel: Failed to connect to local Babel instance.");
    nw_routing_babel_client_close(&bc.stream.stream);
    return;
  }

  /* Connection has been established, prepare the stream. */
  uloop_fd_delete(&bc.client);
  bc.stream.stream.string_data = true;
  bc.stream.stream.notify_read = nw_routing_babel_client_read;
  bc.stream.stream.notify_state = nw_routing_babel_client_notify_state;
  ustream_fd_init(&bc.stream, bc.client.fd);
}

static int nw_routing_babel_start_acquire_data(struct nodewatcher_module *module,
                                               struct ubus_context *ubus,
                                               struct uci_context *uci)
{
  /* Ignore new requests if previous ones did not complete yet. */
  if (!bc.ready)
    return -1;

  bc.object = json_object_new_object();

  /* Get the link-local addresses of the local interfaces. */
  struct ifaddrs *ifaddr, *ifa;
  json_object *link_local = json_object_new_array();
  json_object_object_add(bc.object, "link_local", link_local);

  if (!getifaddrs(&ifaddr)) {
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == NULL)
        continue;

      /* Skip interfaces which are down. */
      if (!(ifa->ifa_flags & IFF_UP))
        continue;

      /* Skip non-ipv6 addresses. */
      if (ifa->ifa_addr->sa_family != AF_INET6)
        continue;

      /* Skip non-link-local addresses. */
      if (!IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6*) ifa->ifa_addr)->sin6_addr))
        continue;

      char host[NI_MAXHOST];
      if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) != 0)
        continue;

      json_object_array_add(link_local, json_object_new_string(host));
    }

    freeifaddrs(ifaddr);
  } else {
    syslog(LOG_WARNING, "routing-babel: Failed to obtain link-local addresses.");
  }

  /* Open a connection to the Babel local socket. */
  /* TODO: We currently assume the local server to be running on [::1]:33123. */
  bc.client.cb = nw_routing_babel_client_callback;
  bc.client.fd = usock(USOCK_TCP | USOCK_NUMERIC | USOCK_NONBLOCK, "::1", "33123");
  if (bc.client.fd < 0) {
    syslog(LOG_WARNING, "routing-babel: Failed to connect to local Babel instance.");
    return nw_module_finish_acquire_data(module, bc.object);
  }

  bc.ready = false;
  uloop_fd_add(&bc.client, ULOOP_READ);

  /* Start a timer that will abort data retrieval. */
  bc.timer.cb = nw_routing_babel_client_timeout;
  uloop_timeout_set(&bc.timer, 5000);

  return 0;
}

static int nw_routing_babel_init(struct nodewatcher_module *module,
                                 struct ubus_context *ubus,
                                 struct uci_context *uci)
{
  /* Initialize the client structure. */
  bc.ready = true;
  bc.module = module;

  return 0;
}

/* Module descriptor. */
struct nodewatcher_module nw_module = {
  .name = "core.routing.babel",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 1,
  .hooks = {
    .init               = nw_routing_babel_init,
    .start_acquire_data = nw_routing_babel_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};
