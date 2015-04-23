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

static int nw_keys_ssh_get_key(json_object *object,
                               const char *filename,
                               const char *name)
{
  char tmp[1024];
  snprintf(tmp, sizeof(tmp), "/usr/bin/dropbearkey -y -f %s", filename);

  FILE *key = popen(tmp, "r");
  if (!key)
    return -1;

  fscanf(key, "Public key portion is:\n");

  if (fscanf(key, "%*s %1023s %*[^\n]\n", tmp) == 1) {
    json_object *key_object = json_object_new_object();
    json_object_object_add(key_object, "base64", json_object_new_string(tmp));

    char algo[16];
    if (fscanf(key, "Fingerprint: %15s %1023s", algo, tmp) == 2) {
      json_object *fingerprint = json_object_new_object();
      json_object_object_add(fingerprint, "algo", json_object_new_string(algo));
      json_object_object_add(fingerprint, "hex", json_object_new_string(tmp));
      json_object_object_add(key_object, "fingerprint", fingerprint);
    }

    json_object_object_add(object, name, key_object);
  }

  pclose(key);
  return 0;
}

static int nw_keys_ssh_start_acquire_data(struct nodewatcher_module *module,
                                          struct ubus_context *ubus,
                                          struct uci_context *uci)
{
  json_object *object = json_object_new_object();
  /* RSA key */
  nw_keys_ssh_get_key(object, "/etc/dropbear/dropbear_rsa_host_key", "rsa");
  /* DSS key */
  nw_keys_ssh_get_key(object, "/etc/dropbear/dropbear_dss_host_key", "dss");

  /* Store resulting JSON object */
  return nw_module_finish_acquire_data(module, object);
}

static int nw_keys_ssh_init(struct nodewatcher_module *module,
                            struct ubus_context *ubus,
                            struct uci_context *uci)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.keys.ssh",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 1,
  .hooks = {
    .init               = nw_keys_ssh_init,
    .start_acquire_data = nw_keys_ssh_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 600,
  },
};
