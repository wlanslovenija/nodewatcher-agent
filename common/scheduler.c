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

/* Module schedule queue */
static LIST_HEAD(sched_modules);
/* Timeout for running module scheduling */
static struct uloop_timeout run_module_timeout;

static void nw_scheduler_gettime(struct timeval *tv)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;
}

static int tv_diff(struct timeval *t1, struct timeval *t2)
{
  return (t1->tv_sec - t2->tv_sec) * 1000 +
         (t1->tv_usec - t2->tv_usec) / 1000;
}

static void nw_scheduler_run_modules(struct uloop_timeout *timeout)
{
  struct nodewatcher_module *module, *tmp;
  struct timeval now;
  nw_scheduler_gettime(&now);

  /* Iterate through the modules and execute those that are pending execution */
  list_for_each_entry_safe(module, tmp, &sched_modules, sched_list) {
    if (tv_diff(&module->sched_next_run, &now) <= 0) {
      /* Invoke module hooks. Note that this call may cause the module to be
         rescheduled, but since the timer has just expired, it will not be
         reset, so we still need to reschedule the timer below. */
      list_del(&module->sched_list);
      nw_module_start_acquire_data(module);
    } else {
      /* Since the modules are sorted by time, no more modules will match */
      break;
    }
  }

  /* Reschedule timer if there are any pending modules */
  uloop_timeout_cancel(&run_module_timeout);
  if (!list_empty(&sched_modules)) {
    struct nodewatcher_module *next_module =
      container_of(&sched_modules, struct nodewatcher_module, sched_list);

    run_module_timeout.time.tv_sec = next_module->sched_next_run.tv_sec;
    run_module_timeout.time.tv_usec = next_module->sched_next_run.tv_usec;
    uloop_timeout_add(&run_module_timeout);
  }
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
  nw_scheduler_gettime(&module->sched_next_run);
  module->sched_next_run.tv_sec += timeout;

  struct nodewatcher_module *tmp;
  struct list_head *head = &sched_modules;
  list_for_each_entry(tmp, &sched_modules, sched_list) {
    if (tv_diff(&tmp->sched_next_run, &module->sched_next_run) > 0) {
      head = &tmp->sched_list;
      break;
    }
  }

  list_add_tail(&module->sched_list, head);
  module->sched_status = NW_MODULE_SCHEDULED;

  if (tv_diff(&module->sched_next_run, &run_module_timeout.time) < 0) {
    uloop_timeout_cancel(&run_module_timeout);
    run_module_timeout.time.tv_sec = module->sched_next_run.tv_sec;
    run_module_timeout.time.tv_usec = module->sched_next_run.tv_usec;
    uloop_timeout_add(&run_module_timeout);
  }

  return 0;
}

int nw_scheduler_init()
{
  /* Setup the callback and set the scheduler to run immediately */
  run_module_timeout.cb = nw_scheduler_run_modules;
  uloop_timeout_set(&run_module_timeout, 0);

  return 0;
}
