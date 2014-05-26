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

/* Module forward declaration */
struct nodewatcher_module nw_module;

static int nw_resources_start_acquire_data(struct ubus_context *ubus,
                                           struct uci_context *uci)
{
  json_object *object = json_object_new_object();
  /* Load average */
  /* Free memory */
  /* Memory used for buffers */
  /* Memory used for cache */
  /* Number of IPv4 routes */
  /* Number of IPv6 routes */
  /* Number of TCP connections */
  /* Number of UDP connections */
  /* Number of processes by status */
  /* CPU usage by category */

  /* Store resulting JSON object */
  nw_module_finish_acquire_data(&nw_module, object);

  /*
    uptime (file:/proc/uptime)
    load_average (file:/proc/loadavg)
    memory.free (file:/proc/meminfo)
    memory.buffers (file:/proc/meminfo)
    memory.cache (file:/proc/meminfo)
    routes.ipv4 (?)
    routes.ipv6 (?)
    connections.tcp (file:/proc/net/{tcp,tcp6})
    connections.udp (file:/proc/net/{udp,udp6})
    processes.running (file:/proc/.../stat)
    processes.sleeping (file:/proc/.../stat)
    processes.blocked (file:/proc/.../stat)
    processes.zombie (file:/proc/.../stat)
    processes.stopped (file:/proc/.../stat)
    processes.paging (file:/proc/.../stat)
    cpu.user (file:/proc/stat)
    cpu.system (file:/proc/stat)
    cpu.nice (file:/proc/stat)
    cpu.idle (file:/proc/stat)
    cpu.iowait (file:/proc/stat)
    cpu.irq (file:/proc/stat)
    cpu.softirq (file:/proc/stat)
  */

  return 0;
}

static int nw_resources_init(struct ubus_context *ubus)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.resources",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 1,
  .hooks = {
    .init               = nw_resources_init,
    .start_acquire_data = nw_resources_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};
