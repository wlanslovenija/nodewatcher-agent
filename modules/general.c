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

/* Module forward declaration */
struct nodewatcher_module nw_module;

static int nw_general_start_acquire_data(struct ubus_context *ubus,
                                         struct uci_context *uci)
{
  json_object *object = json_object_new_object();
  /* UUID */
  nw_json_from_uci(uci, "system.@system[0].uuid", object, "uuid");
  /* Hostname */
  nw_json_from_uci(uci, "system.@system[0].hostname", object, "hostname");
  /* Firmware version */
  nw_json_from_file("/etc/version", object, "version");
  /* Local UNIX time */
  json_object_object_add(object, "local_time", json_object_new_int(time(NULL)));
  /* Uptime in seconds */
  FILE *uptime_file = fopen("/proc/uptime", "r");
  int64_t uptime;
  if (fscanf(uptime_file, "%lld", &uptime) == 1)
    json_object_object_add(object, "uptime", json_object_new_int(uptime));
  fclose(uptime_file);

  /* Machine identifier from /proc/cpuinfo */
  FILE *cpuinfo_file = fopen("/proc/cpuinfo", "r");
  while (!feof(cpuinfo_file)) {
    char key[128];
    char value[1024];

    if (fscanf(uptime_file, "%127[^:]%*c%1023[^\n]", key, value) == 2) {
      if (strcmp(nw_string_trim(key), "machine") == 0) {
        json_object_object_add(object, "machine", json_object_new_string(nw_string_trim(value)));
        break;
      }
    }
  }
  fclose(cpuinfo_file);

  /* Store resulting JSON object */
  return nw_module_finish_acquire_data(&nw_module, object);
}

static int nw_general_init(struct ubus_context *ubus)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.general",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 4,
  .hooks = {
    .init               = nw_general_init,
    .start_acquire_data = nw_general_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};
