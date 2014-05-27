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
#include <nodewatcher-agent/scheduler.h>

#include <time.h>

static void nw_scheduler_run_module(struct uloop_timeout *timeout)
{
  struct nodewatcher_module *module;

  /* Extract the module where the timeout ocurred */
  module = container_of(timeout, struct nodewatcher_module, sched_timeout);
  /* Signal the module to start acquiring data */
  nw_module_start_acquire_data(module);
}

int nw_scheduler_schedule_module(struct nodewatcher_module *module)
{
  /* Skip modules that are still acquiring data or have already been scheduled for execution */
  if (module->sched_status == NW_MODULE_PENDING_DATA || module->sched_status == NW_MODULE_SCHEDULED)
    return -1;

  /* If the module has just been initialized, we schedule it for immediate execution */
  time_t timeout;
  if (module->sched_status == NW_MODULE_INIT) {
    timeout = 0;
  } else {
    timeout = module->schedule.refresh_interval;
  }

  /* Schedule the module */
  uloop_timeout_cancel(&module->sched_timeout);
  module->sched_timeout.cb = nw_scheduler_run_module;
  uloop_timeout_set(&module->sched_timeout, timeout * 1000);
  module->sched_status = NW_MODULE_SCHEDULED;

  return 0;
}

int nw_scheduler_init()
{
  return 0;
}
