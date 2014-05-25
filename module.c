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

#include <libubox/avl-cmp.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

/* AVL tree containing all registered modules with module name as their key */
static struct avl_tree module_registry;

static int nw_module_register_library(struct ubus_context *ctx, const char *path)
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

  /* Perform module initialization */
  ret = module->hooks.init(ctx);
  if (ret != 0) {
    avl_delete(&module_registry, &module->avl);
    syslog(LOG_WARNING, "Loading of module '%s' (%s) has failed!", module->name, path);
  } else {
    syslog(LOG_INFO, "Loaded module '%s' (%s).", module->name, path);
  }

  return ret;
}

int nw_module_init(struct ubus_context *ctx)
{
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

      ret |= nw_module_register_library(ctx, path);
    }

    closedir(d);
  }

  return ret;
}

int nw_finish_acquire_data(struct nodewatcher_module *module, json_object *object)
{
  /* TODO */
  return -1;
}
