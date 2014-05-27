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
#ifndef NODEWATCHER_AGENT_MODULE_H
#define NODEWATCHER_AGENT_MODULE_H

#include <json/json.h>
#include <libubox/avl.h>
#include <libubus.h>
#include <uci.h>

/* Location of nodewatcher library modules */
#define NW_MODULE_DIRECTORY "/usr/lib/nodewatcher-agent"

struct nodewatcher_module;

/**
 * Hooks that must be implemented by nodewatcher agent modules.
 */
struct nodewatcher_module_hooks {
  /* Hook that initializes the module */
  int (*init)(struct nodewatcher_module *module,
              struct ubus_context *ubus);
  /* Hook that requests the module to start acquiring data */
  int (*start_acquire_data)(struct nodewatcher_module *module,
                            struct ubus_context *ubus,
                            struct uci_context *uci);
};

struct nodewatcher_module_schedule {
  /* Data refresh interval */
  time_t refresh_interval;
};

enum {
  NW_MODULE_NONE = 0,
  NW_MODULE_SCHEDULED = 1,
  NW_MODULE_PENDING_DATA = 2,
  NW_MODULE_INIT = 3,
};

/**
 * Nodewatcher agent module descriptor. One should be provided by each
 * module and named nw_module.
 */
struct nodewatcher_module {
  /* Module name (must be unique) */
  const char *name;
  /* Module author and e-mail */
  const char *author;
  /* Module version */
  unsigned int version;
  /* Operation implementing hooks */
  struct nodewatcher_module_hooks hooks;
  /* Schedule interval */
  struct nodewatcher_module_schedule schedule;

  /* --- Internal attributes below --- */

  /* Module registry AVL node */
  struct avl_node avl;
  /* Module scheduling status */
  int sched_status;
  /* Module next scheduled run */
  struct uloop_timeout sched_timeout;
  /* Last data object */
  json_object *data;
};

/**
 * Performs module discovery and initialization.
 *
 * @param ubus UBUS context
 * @param uci UCI context
 * @return On success 0 is returned, -1 otherwise
 */
int nw_module_init(struct ubus_context *ubus, struct uci_context *uci);

/**
 * Invokes the module's hook for start of data acquiry.
 *
 * @param module Module that should start with data acquiry
 * @return On success 0 is returned, -1 otherwise
 */
int nw_module_start_acquire_data(struct nodewatcher_module *module);

/**
 * Signals that a module has finished acquiring data and has a resulting
 * JSON object ready.
 *
 * @param module Module that has finished acquiring data
 * @param object JSON result object
 * @return On success 0 is returned, -1 otherwise
 */
int nw_module_finish_acquire_data(struct nodewatcher_module *module, json_object *object);

#endif
