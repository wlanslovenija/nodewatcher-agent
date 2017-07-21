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

#include <sys/types.h>
#include <dirent.h>
#include <math.h>
#include <limits.h>

/* Previous CPU usage values */
static unsigned int last_cpu_times[7] = {0, };

static int nw_resources_start_acquire_data(struct nodewatcher_module *module,
                                           struct ubus_context *ubus,
                                           struct uci_context *uci)
{
  json_object *object = json_object_new_object();
  /* Load average */
  FILE *loadavg_file = fopen("/proc/loadavg", "r");
  if (loadavg_file) {
    char load1min[16], load5min[16], load15min[16];
    if (fscanf(loadavg_file, "%15s %15s %15s", load1min, load5min, load15min) == 3) {
      json_object *load_average = json_object_new_array();
      json_object_array_add(load_average, json_object_new_string(load1min));
      json_object_array_add(load_average, json_object_new_string(load5min));
      json_object_array_add(load_average, json_object_new_string(load15min));
      json_object_object_add(object, "load_average", load_average);
    }
    fclose(loadavg_file);
  }

  /* Memory usage counters */
  FILE *memory_file = fopen("/proc/meminfo", "r");
  if (memory_file) {
    json_object *memory = json_object_new_object();

    while (!feof(memory_file)) {
      char key[128];
      int value;

      if (fscanf(memory_file, "%127[^:]%*c%d kB", key, &value) == 2) {
        if (strcmp(nw_string_trim(key), "MemTotal") == 0) {
          json_object_object_add(memory, "total", json_object_new_int(value));
        } else if (strcmp(nw_string_trim(key), "MemFree") == 0) {
          json_object_object_add(memory, "free", json_object_new_int(value));
        } else if (strcmp(nw_string_trim(key), "Buffers") == 0) {
          json_object_object_add(memory, "buffers", json_object_new_int(value));
        } else if (strcmp(nw_string_trim(key), "Cached") == 0) {
          json_object_object_add(memory, "cache", json_object_new_int(value));
          /* We can break as we don't need entries after "cache" */
          break;
        }
      }
    }
    fclose(memory_file);
    json_object_object_add(object, "memory", memory);
  }

  /* Number of local TCP/UDP connections */
  json_object *connections = json_object_new_object();
  json_object *connections_ipv4 = json_object_new_object();
  json_object_object_add(connections_ipv4, "tcp", json_object_new_int(nw_file_line_count("/proc/net/tcp") - 1));
  json_object_object_add(connections_ipv4, "udp", json_object_new_int(nw_file_line_count("/proc/net/udp") - 1));
  json_object_object_add(connections, "ipv4", connections_ipv4);
  json_object *connections_ipv6 = json_object_new_object();
  json_object_object_add(connections_ipv6, "tcp", json_object_new_int(nw_file_line_count("/proc/net/tcp6") - 1));
  json_object_object_add(connections_ipv6, "udp", json_object_new_int(nw_file_line_count("/proc/net/udp6") - 1));
  json_object_object_add(connections, "ipv6", connections_ipv6);
  /* Number of entries in connection tracking table */
  json_object *connections_tracking = json_object_new_object();
  nw_json_from_file("/proc/sys/net/netfilter/nf_conntrack_count", connections_tracking, "count", true);
  nw_json_from_file("/proc/sys/net/netfilter/nf_conntrack_max", connections_tracking, "max", true);
  json_object_object_add(connections, "tracking", connections_tracking);
  json_object_object_add(object, "connections", connections);

  /* Number of processes by status */
  DIR *proc_dir;
  struct dirent *proc_entry;
  char path[PATH_MAX];

  if ((proc_dir = opendir("/proc")) != NULL) {
    json_object *processes = json_object_new_object();
    int proc_by_state[6] = {0, };

    while ((proc_entry = readdir(proc_dir)) != NULL) {
      snprintf(path, sizeof(path) - 1, "/proc/%s/stat", proc_entry->d_name);

      FILE *proc_file = fopen(path, "r");
      if (proc_file) {
        char state;
        if (fscanf(proc_file, "%*d (%*[^)]) %c", &state) == 1) {
          switch (state) {
            case 'R': proc_by_state[0]++; break;
            case 'S': proc_by_state[1]++; break;
            case 'D': proc_by_state[2]++; break;
            case 'Z': proc_by_state[3]++; break;
            case 'T': proc_by_state[4]++; break;
            case 'W': proc_by_state[5]++; break;
          }
        }
        fclose(proc_file);
      }
    }

    closedir(proc_dir);
    json_object_object_add(processes, "running", json_object_new_int(proc_by_state[0]));
    json_object_object_add(processes, "sleeping", json_object_new_int(proc_by_state[1]));
    json_object_object_add(processes, "blocked", json_object_new_int(proc_by_state[2]));
    json_object_object_add(processes, "zombie", json_object_new_int(proc_by_state[3]));
    json_object_object_add(processes, "stopped", json_object_new_int(proc_by_state[4]));
    json_object_object_add(processes, "paging", json_object_new_int(proc_by_state[5]));
    json_object_object_add(object, "processes", processes);
  }

  /* CPU usage by category */
  FILE *cpu_file = fopen("/proc/stat", "r");
  if (cpu_file) {
    unsigned int cpu_times[7] = {0, };
    if (fscanf(cpu_file, "cpu %u %u %u %u %u %u %u",
          &cpu_times[0], &cpu_times[1], &cpu_times[2], &cpu_times[3],
          &cpu_times[4], &cpu_times[5], &cpu_times[6]) == 7) {
      unsigned long sum = 0;
      for (int i = 0; i < 7; i++) {
        int tmp = cpu_times[i];
        cpu_times[i] -= last_cpu_times[i];
        last_cpu_times[i] = tmp;
        sum += cpu_times[i];
      }

      /* Compute CPU usage percentages since the last run interval */
      for (int i = 0; i < 7; i++)
        cpu_times[i] = (unsigned int) (round((double) cpu_times[i] * 100 / sum));

      json_object *cpu = json_object_new_object();
      json_object_object_add(cpu, "user", json_object_new_int(cpu_times[0]));
      json_object_object_add(cpu, "system", json_object_new_int(cpu_times[1]));
      json_object_object_add(cpu, "nice", json_object_new_int(cpu_times[2]));
      json_object_object_add(cpu, "idle", json_object_new_int(cpu_times[3]));
      json_object_object_add(cpu, "iowait", json_object_new_int(cpu_times[4]));
      json_object_object_add(cpu, "irq", json_object_new_int(cpu_times[5]));
      json_object_object_add(cpu, "softirq", json_object_new_int(cpu_times[6]));
      json_object_object_add(object, "cpu", cpu);
    }
    fclose(cpu_file);
  }

  /* Number of IPv4 routes */
  /* Number of IPv6 routes */

  /* Number of open file descriptors */
  FILE *filenr_file = fopen("/proc/sys/fs/file-nr", "r");
  if (filenr_file) {
    unsigned int fn_current = 0, fn_available = 0, fn_max = 0;
    if (fscanf(filenr_file, "%u\t%u\t%u", &fn_current, &fn_available, &fn_max) == 3) {
      json_object *files = json_object_new_object();
      json_object_object_add(files, "open", json_object_new_int(fn_current));
      json_object_object_add(files, "available", json_object_new_int(fn_available));
      json_object_object_add(files, "max", json_object_new_int(fn_max));
      json_object_object_add(object, "files", files);
    }
    fclose(filenr_file);
  }

  /* Store resulting JSON object */
  nw_module_finish_acquire_data(module, object);
  return 0;
}

static int nw_resources_init(struct nodewatcher_module *module,
                             struct ubus_context *ubus,
                             struct uci_context *uci)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.resources",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 2,
  .hooks = {
    .init               = nw_resources_init,
    .start_acquire_data = nw_resources_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};
