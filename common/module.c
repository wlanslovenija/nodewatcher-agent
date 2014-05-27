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
#include <nodewatcher-agent/scheduler.h>

#include <libubox/avl-cmp.h>
#include <libubox/blobmsg_json.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

/* AVL tree containing all registered modules with module name as their key */
static struct avl_tree module_registry;
/* Module ubus connection context */
static struct ubus_context *module_ubus;
/* Module UCI context */
static struct uci_context *module_uci;
/* Ubus reply buffer */
static struct blob_buf reply_buf;

enum {
  AGENT_D_MODULE,
  __AGENT_D_MAX,
};

static const struct blobmsg_policy nw_module_policy[__AGENT_D_MAX] = {
  [AGENT_D_MODULE] = { .name = "module", .type = BLOBMSG_TYPE_STRING },
};

static int nw_module_register_library(struct ubus_context *ubus, const char *path)
{
  struct nodewatcher_module *module;
  void *handle;
  int ret = 0;

  handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
  if (!handle) {
    syslog(LOG_WARNING, "Unable to open module '%s'!", path);
    return -1;
  }

  module = dlsym(handle, "nw_module");
  if (!module) {
    syslog(LOG_WARNING, "Module '%s' is not a valid nodewatcher agent module!", path);
    return -1;
  }

  /* Register module in our list of modules */
  module->avl.key = module->name;
  if (avl_insert(&module_registry, &module->avl) != 0) {
    syslog(LOG_WARNING, "Not loading module '%s' (%s) because of name conflict!", module->name, path);
    return -1;
  }

  /* Initialize module data object */
  module->data = json_object_new_object();
  json_object *meta = json_object_new_object();
  json_object_object_add(meta, "version", json_object_new_int(module->version));
  json_object_object_add(module->data, "_meta", meta);

  /* Perform module initialization */
  module->sched_status = NW_MODULE_INIT;
  ret = module->hooks.init(module, ubus);
  if (ret != 0) {
    avl_delete(&module_registry, &module->avl);
    syslog(LOG_WARNING, "Loading of module '%s' (%s) has failed!", module->name, path);
  } else {
    syslog(LOG_INFO, "Loaded module '%s' (%s).", module->name, path);

    /* Schedule module for immediate execution */
    nw_scheduler_schedule_module(module);
  }

  return ret;
}

static int nw_handle_module_get_data(struct ubus_context *ctx, struct ubus_object *obj,
                                     struct ubus_request_data *req, const char *method,
                                     struct blob_attr *msg)
{
  struct blob_attr *tb[__AGENT_D_MAX];
  void *c;

  blobmsg_parse(nw_module_policy, __AGENT_D_MAX, tb, blob_data(msg), blob_len(msg));

  if (tb[AGENT_D_MODULE]) {
    /* Handle agent parameter to filter to a specific module */
    struct nodewatcher_module *module;
    module = avl_find_element(&module_registry, blobmsg_data(tb[AGENT_D_MODULE]), module, avl);
    if (!module)
      return UBUS_STATUS_NOT_FOUND;

    blob_buf_init(&reply_buf, 0);
    c = blobmsg_open_table(&reply_buf, module->name);
    blobmsg_add_object(&reply_buf, module->data);
    blobmsg_close_table(&reply_buf, c);
  } else {
    /* Iterate through all modules and add them to our reply */
    struct nodewatcher_module *module;
    blob_buf_init(&reply_buf, 0);

    avl_for_each_element(&module_registry, module, avl) {
      c = blobmsg_open_table(&reply_buf, module->name);
      blobmsg_add_object(&reply_buf,  module->data);
      blobmsg_close_table(&reply_buf, c);
    }
  }

  ubus_send_reply(ctx, req, reply_buf.head);

  return UBUS_STATUS_OK;
}

int nw_module_init(struct ubus_context *ubus, struct uci_context *uci)
{
  /* Initialize ubus and UCI contexts */
  module_ubus = ubus;
  module_uci = uci;

  /* Initialize the module registry */
  avl_init(&module_registry, avl_strcmp, false, NULL);

  /* Discover and initialize all the modules */
  DIR *d;
  struct stat s;
  struct dirent *e;
  char path[PATH_MAX];
  int ret = 0;

  syslog(LOG_INFO, "Loading modules from '%s'.", NW_MODULE_DIRECTORY);
  if ((d = opendir(NW_MODULE_DIRECTORY)) != NULL) {
    while ((e = readdir(d)) != NULL) {
      snprintf(path, sizeof(path) - 1, NW_MODULE_DIRECTORY "/%s", e->d_name);

      if (stat(path, &s) || !S_ISREG(s.st_mode))
        continue;

      ret |= nw_module_register_library(ubus, path);
    }

    closedir(d);
  }

  if (ret != 0)
    return ret;

  /* Initialize ubus methods */
  static const struct ubus_method agent_methods[] = {
    UBUS_METHOD("get_data", nw_handle_module_get_data, nw_module_policy),
  };

  static struct ubus_object_type agent_type =
    UBUS_OBJECT_TYPE("nodewatcher-agent", agent_methods);

  static struct ubus_object obj = {
    .name = "nodewatcher.agent",
    .type = &agent_type,
    .methods = agent_methods,
    .n_methods = ARRAY_SIZE(agent_methods),
  };

  return ubus_add_object(ubus, &obj);
}

int nw_module_start_acquire_data(struct nodewatcher_module *module)
{
  return module->hooks.start_acquire_data(module, module_ubus, module_uci);
}

int nw_module_finish_acquire_data(struct nodewatcher_module *module, json_object *object)
{
  /* Update module data */
  json_object *meta;
  json_object_object_get_ex(module->data, "_meta", &meta);
  json_object_object_add(object, "_meta", json_object_get(meta));

  /* Free the previous data object and exchange with new object */
  json_object_put(module->data);
  module->data = object;

  /* Reschedule module */
  module->sched_status = NW_MODULE_NONE;
  nw_scheduler_schedule_module(module);

  return 0;
}
