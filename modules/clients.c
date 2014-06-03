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

#include <libubox/md5.h>

#define NW_CLIENT_ID_SALT "nw-client-c3U0XX"
#define NW_CLIENT_ID_SALT_LENGTH 16

static int nw_clients_start_acquire_data(struct nodewatcher_module *module,
                                         struct ubus_context *ubus,
                                         struct uci_context *uci)
{
  json_object *object = json_object_new_object();

  /* Iterate over DHCP leases */
  FILE *leases_file = fopen("/tmp/dhcp.leases", "r");
  if (leases_file) {
    while (!feof(leases_file)) {
      unsigned int expiry;
      char mac[18];
      char ip_address[46];

      if (fscanf(leases_file, "%u %17s %45s %*[^\n]\n", &expiry, mac, ip_address) == 3) {
        json_object *client = json_object_new_object();
        json_object *addresses = json_object_new_array();
        json_object *address = json_object_new_object();
        json_object_object_add(address, "family", json_object_new_string("ipv4"));
        json_object_object_add(address, "address", json_object_new_string(ip_address));
        json_object_object_add(address, "expires", json_object_new_int(expiry));
        json_object_array_add(addresses, address);
        json_object_object_add(client, "addresses", addresses);

        /* Compute salted hash of MAC address */
        md5_ctx_t ctx;
        uint8_t raw_mac_id[16];
        char mac_id[33];

        md5_begin(&ctx);
        md5_hash(NW_CLIENT_ID_SALT, NW_CLIENT_ID_SALT_LENGTH, &ctx);
        md5_hash(mac, 17, &ctx);
        md5_end(raw_mac_id, &ctx);

        /* Base64 encode the hash so it is more compact */
        if (nw_base64_encode(raw_mac_id, sizeof(raw_mac_id), mac_id, sizeof(mac_id)) == 0) {
          json_object_object_add(object, mac_id, client);
        } else {
          json_object_put(client);
        }
      }
    }

    fclose(leases_file);
  }

  /* Store resulting JSON object */
  return nw_module_finish_acquire_data(module, object);
}

static int nw_clients_init(struct nodewatcher_module *module, struct ubus_context *ubus)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.clients",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 1,
  .hooks = {
    .init               = nw_clients_init,
    .start_acquire_data = nw_clients_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};
