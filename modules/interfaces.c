/*
 * nodewatcher-agent - remote monitoring daemon
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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

#include <uci.h>
#include <syslog.h>

static bool nw_interfaces_process_device(struct ubus_context *ubus,
                                         const char *ifname,
                                         const char *devname,
                                         const char *parent,
                                         json_object *addresses,
                                         json_object *object)
{
  json_object *device = json_object_new_object();
  json_object_object_add(device, "name", json_object_new_string(devname));
  json_object_object_add(device, "config", json_object_new_string(ifname));
  if (parent)
    json_object_object_add(device, "parent", json_object_new_string(parent));

  /* Include addresses (can be NULL) */
  if (addresses)
    json_object_object_add(device, "addresses", json_object_get(addresses));

  /* Request detailed device statistics */
  uint32_t ubus_id;
  if (ubus_lookup_id(ubus, "network.device", &ubus_id)) {
    syslog(LOG_WARNING, "interfaces: Failed to find netifd object 'network.device'!");
    goto data_error;
  }

  json_object *data = NULL;
  static struct blob_buf req;
  blob_buf_init(&req, 0);
  blobmsg_add_string(&req, "name", devname);

  if (ubus_invoke(ubus, ubus_id, "status", req.head, nw_json_from_ubus, &data, 500) != UBUS_STATUS_OK)
    goto data_error;
  if (!data)
    goto data_error;

  /* XXX: Currently, MAC address of wireless interfaces is not reported because
          of a bug in netifd. See OpenWrt ticket #16633. */
  NW_COPY_JSON_OBJECT(data, "macaddr", device, "mac");
  NW_COPY_JSON_OBJECT(data, "mtu", device, "mtu");
  NW_COPY_JSON_OBJECT(data, "up", device, "up");
  NW_COPY_JSON_OBJECT(data, "carrier", device, "carrier");
  NW_COPY_JSON_OBJECT(data, "speed", device, "speed");
  NW_COPY_JSON_OBJECT(data, "statistics", device, "statistics");

  json_object_object_add(object, devname, device);

  /* If the device is a bridge and has any children, we should add them as well */
  json_object *children = NULL;
  json_object_object_get_ex(data, "bridge-members", &children);
  if (children) {
    int i;
    for (i = 0; i < json_object_array_length(children); i++) {
      const char *childname = json_object_get_string(json_object_array_get_idx(children, i));
      nw_interfaces_process_device(ubus, ifname, childname, devname, NULL, object);
    }
  }

  json_object_put(data);
  return true;

data_error:
  json_object_put(device);
  return false;
}

static bool nw_interfaces_process_interface(struct ubus_context *ubus,
                                            const char *ifname,
                                            json_object *object)
{
  /* Resolve proper ubus object path */
  char ubus_path[64] = { 0, };
  snprintf(ubus_path, sizeof(ubus_path), "network.interface.%s", ifname);

  uint32_t ubus_id;
  if (ubus_lookup_id(ubus, ubus_path, &ubus_id)) {
    syslog(LOG_WARNING, "interfaces: Failed to find netifd object '%s'!", ubus_path);
    return false;
  }

  /* Prepare and send a request */
  json_object *data = NULL;
  static struct blob_buf req;
  blob_buf_init(&req, 0);
  if (ubus_invoke(ubus, ubus_id, "status", req.head, nw_json_from_ubus, &data, 500) != UBUS_STATUS_OK)
    return false;
  if (!data)
    goto data_error;

  /* Extract underlying network device */
  json_object *device = NULL;
  json_object_object_get_ex(data, "device", &device);
  if (!device)
    goto data_error;

  /* Parse interface addresses as they are not available per-device */
  int i;
  json_object *addresses = json_object_new_array();
  /* Add IPv4 addresses */
  json_object *ipv4 = NULL;
  json_object_object_get_ex(data, "ipv4-address", &ipv4);
  for (i = 0; i < json_object_array_length(ipv4); i++) {
    json_object *address = json_object_new_object();
    /* Set address type */
    json_object_object_add(address, "family", json_object_new_string("ipv4"));
    /* Copy interface address */
    json_object *a = json_object_array_get_idx(ipv4, i);
    NW_COPY_JSON_OBJECT(a, "address", address, "address");
    NW_COPY_JSON_OBJECT(a, "mask", address, "mask");

    json_object_array_add(addresses, address);
  }
  /* Add IPv6 addresses */
  json_object *ipv6 = NULL;
  json_object_object_get_ex(data, "ipv6-address", &ipv6);
  for (i = 0; i < json_object_array_length(ipv6); i++) {
    json_object *address = json_object_new_object();
    /* Set address type */
    json_object_object_add(address, "family", json_object_new_string("ipv6"));
    /* Copy interface address */
    json_object *a = json_object_array_get_idx(ipv6, i);
    NW_COPY_JSON_OBJECT(a, "address", address, "address");
    NW_COPY_JSON_OBJECT(a, "mask", address, "mask");

    json_object_array_add(addresses, address);
  }

  /* Process the individual device */
  if (!nw_interfaces_process_device(ubus, ifname, json_object_get_string(device), NULL, addresses, object))
    goto data_error_free_addresses;

  json_object_put(addresses);
  json_object_put(data);
  return true;

data_error_free_addresses:
  json_object_put(addresses);
data_error:
  syslog(LOG_WARNING, "interfaces: Failed to parse netifd interface data!");
  if (data)
    json_object_put(data);
  return false;
}

static int nw_interfaces_start_acquire_data(struct nodewatcher_module *module,
                                            struct ubus_context *ubus,
                                            struct uci_context *uci)
{
  json_object *object = json_object_new_object();

  /* Lookup a list of interfaces in UCI */
  struct uci_package *cfg_network = uci_lookup_package(uci, "network");
  if (cfg_network)
    uci_unload(uci, cfg_network);
  if (uci_load(uci, "network", &cfg_network)) {
    syslog(LOG_WARNING, "interfaces: Failed to load network configuration!");
    return nw_module_finish_acquire_data(module, object);
  }

  struct uci_element *e;
  uci_foreach_element(&cfg_network->sections, e) {
    struct uci_section *cfg_section = uci_to_section(e);
    if (strcmp(cfg_section->type, "interface") != 0)
      continue;

    /* Process interface */
    nw_interfaces_process_interface(ubus, cfg_section->e.name, object);
  }

  /* Store resulting JSON object */
  return nw_module_finish_acquire_data(module, object);
}

static int nw_interfaces_init(struct nodewatcher_module *module, struct ubus_context *ubus)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.interfaces",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 3,
  .hooks = {
    .init               = nw_interfaces_init,
    .start_acquire_data = nw_interfaces_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};
