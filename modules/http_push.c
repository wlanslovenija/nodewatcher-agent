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

#include <syslog.h>
#include <curl/curl.h>

/* Timestamp when last successful push occurred. */
static time_t last_push_at = 0;

static size_t nw_http_push_ignore_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
  /* Helper function that ignores any received data. */
  return size * nmemb;
}

static int nw_http_push_start_acquire_data(struct nodewatcher_module *module,
                                           struct ubus_context *ubus,
                                           struct uci_context *uci)
{
  json_object *object = json_object_new_object();
  char *push_result = "not_configured";

  /* Reload UCI configuration section to enable live changes. */
  struct uci_package *cfg_agent = uci_lookup_package(uci, "nodewatcher");
  if (cfg_agent)
    uci_unload(uci, cfg_agent);
  if (uci_load(uci, "nodewatcher", &cfg_agent)) {
    syslog(LOG_WARNING, "http-push: Failed to load nodewatcher agent configuration!");
  }

  /* Get the configured URL from UCI. */
  char *url = nw_uci_get_string(uci, "nodewatcher.@agent[0].push_url");
  if (url) {
    /* Collect all the module data and perform the push. */
    json_object *data = nw_module_get_output();
    if (data) {
      CURL *curl = curl_easy_init();
      if (curl) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nw_http_push_ignore_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_object_to_json_string(data));
        // TODO: Use CURLOPT_PINNEDPUBLICKEY to define server-side key.

        /* Perform the push request. */
        CURLcode result = curl_easy_perform(curl);
        /* Map result codesto string enumerations. */
        switch (result) {
          case CURLE_OK: push_result = "ok"; break;
          case CURLE_URL_MALFORMAT: push_result = "bad_url"; break;
          case CURLE_COULDNT_RESOLVE_HOST: push_result = "host_not_resolved"; break;
          case CURLE_COULDNT_CONNECT: push_result = "connect_error"; break;
          case CURLE_REMOTE_ACCESS_DENIED: push_result = "access_denied"; break;
          case CURLE_HTTP_RETURNED_ERROR: push_result = "http_error"; break;
          case CURLE_OPERATION_TIMEDOUT: push_result = "timeout"; break;
          case CURLE_PEER_FAILED_VERIFICATION: push_result = "peer_verify_error"; break;
          default: push_result = "unknown_error"; break;
        }

        /* Update the last push timestamp. */
        if (result == CURLE_OK)
          last_push_at = time(NULL);

        curl_easy_cleanup(curl);
      } else {
        push_result = "init_error";
      }

      json_object_put(data);
    }

    free(url);
  }

  /* Dynamically configure the refresh interval from UCI. */
  int interval = nw_uci_get_int(uci, "nodewatcher.@agent[0].push_interval");
  if (interval <= 30) {
    syslog(LOG_WARNING, "http-push: Push interval set too low, setting to 300.");
    interval = 300;
  }
  /* Randomly adjust the interval to spread data pushes. */
  interval = nw_roughly(interval);

  module->schedule.refresh_interval = (time_t) interval;

  /* Publish last push status. */
  if (last_push_at > 0)
    json_object_object_add(object, "pushed_at", json_object_new_int(last_push_at));
  json_object_object_add(object, "status", json_object_new_string(push_result));

  /* Store resulting JSON object */
  nw_module_finish_acquire_data(module, object);
  return 0;
}

static int nw_http_push_init(struct nodewatcher_module *module,
                             struct ubus_context *ubus,
                             struct uci_context *uci)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.push.http",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 1,
  .hooks = {
    .init               = nw_http_push_init,
    .start_acquire_data = nw_http_push_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 300,
  },
};
