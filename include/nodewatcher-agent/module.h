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

/* Location of nodewatcher library modules */
#define NW_MODULE_DIRECTORY "/usr/lib/nodewatcher-agent"

/**
 * Hooks that must be implemented by nodewatcher agent modules.
 */
struct nodewatcher_module_hooks {
  /* Hook that initializes the module */
  int (*init)(struct ubus_context *ctx);
  /* Hook that requests the module to start acquiring data */
  int (*start_acquire_data)(struct ubus_context *ctx);
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
  /* Module registry AVL node */
  struct avl_node avl;
};

/**
 * Performs module discovery and initialization.
 *
 * @param ctx UBUS context
 * @return On success 0 is returned, -1 otherwise
 */
int nw_module_init(struct ubus_context *ctx);

/**
 * Signals that a module has finished acquiring data and has a resulting
 * JSON object ready.
 *
 * @param module Module that has finished acquiring data
 * @param object JSON result object
 * @return On success 0 is returned, -1 otherwise
 */
int nw_finish_acquire_data(struct nodewatcher_module *module, json_object *object);

#endif
