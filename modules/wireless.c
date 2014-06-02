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

#include <uci.h>
#include <syslog.h>
#include <iwinfo.h>
#include <iwinfo/utils.h>

static void nw_wireless_call_str(json_object *object,
                                 const char *ifname,
                                 const char *key,
                                 int (*func)(const char*, char*))
{
  char rv[IWINFO_BUFSIZE] = { 0, };

  if (!func(ifname, rv))
    json_object_object_add(object, key, json_object_new_string(rv));
}

static void nw_wireless_call_int(json_object *object,
                                 const char *ifname,
                                 const char *key,
                                 int (*func)(const char*, int*),
                                 const char **map)
{
  int rv;

  if (!func(ifname, &rv)) {
    if (!map)
      json_object_object_add(object, key, json_object_new_int(rv));
    else
      json_object_object_add(object, key, json_object_new_string(map[rv]));
  }
}

static bool nw_wireless_process_interface(const char *ifname,
                                          json_object *object)
{
  /* Initialize iwinfo backend for this device */
  static const struct iwinfo_ops *iwinfo;
  iwinfo = iwinfo_backend(ifname);
  if (!iwinfo)
    return false;

  json_object *interface = json_object_new_object();
  nw_wireless_call_str(interface, ifname, "phy", iwinfo->phyname);

  nw_wireless_call_str(interface, ifname, "ssid", iwinfo->ssid);
  nw_wireless_call_str(interface, ifname, "bssid", iwinfo->bssid);
  nw_wireless_call_str(interface, ifname, "country", iwinfo->country);

  nw_wireless_call_int(interface, ifname, "mode", iwinfo->mode, IWINFO_OPMODE_NAMES);
  nw_wireless_call_int(interface, ifname, "channel", iwinfo->channel, NULL);

  nw_wireless_call_int(interface, ifname, "frequency", iwinfo->frequency, NULL);
  nw_wireless_call_int(interface, ifname, "txpower", iwinfo->txpower, NULL);

  nw_wireless_call_int(interface, ifname, "signal", iwinfo->signal, NULL);
  nw_wireless_call_int(interface, ifname, "noise", iwinfo->noise, NULL);

  nw_wireless_call_int(interface, ifname, "bitrate", iwinfo->bitrate, NULL);

  /* Protocols */
  int modes;
  if (!iwinfo->hwmodelist(ifname, &modes)) {
    json_object *protocols = json_object_new_array();
    if (modes & IWINFO_80211_A)
      json_object_array_add(protocols, json_object_new_string("a"));
    if (modes & IWINFO_80211_B)
      json_object_array_add(protocols, json_object_new_string("b"));
    if (modes & IWINFO_80211_G)
      json_object_array_add(protocols, json_object_new_string("g"));
    if (modes & IWINFO_80211_N)
      json_object_array_add(protocols, json_object_new_string("n"));
    json_object_object_add(interface, "protocols", protocols);
  }

  /* TODO: channel width (currently not supported in iwinfo!) */
  /* TODO: encryption */
  /* TODO: survey*/

  json_object_object_add(object, ifname, interface);

  iwinfo = NULL;
  iwinfo_finish();
  return true;
}

static int nw_wireless_start_acquire_data(struct nodewatcher_module *module,
                                          struct ubus_context *ubus,
                                          struct uci_context *uci)
{
  json_object *object = json_object_new_object();

  /* Obtain a list of wireless interfaces */
  uint32_t ubus_id;
  if (ubus_lookup_id(ubus, "network.wireless", &ubus_id)) {
    syslog(LOG_WARNING, "wireless: Failed to find netifd object 'network.wireless'!");
    return nw_module_finish_acquire_data(module, object);
  }

  /* Prepare and send a request */
  json_object *data = NULL;
  static struct blob_buf req;
  blob_buf_init(&req, 0);

  if (ubus_invoke(ubus, ubus_id, "status", req.head, nw_json_from_ubus, &data, 500) != UBUS_STATUS_OK)
    return false;

  if (!data) {
    syslog(LOG_WARNING, "wireless: Failed to parse netifd wireless status data!");
    return nw_module_finish_acquire_data(module, object);
  }

  /* Iterate over the list of radios */
  json_object_object_foreach(data, key, val) {
    json_object *interfaces = NULL;
    json_object_object_get_ex(val, "interfaces", &interfaces);
    if (!interfaces)
      continue;

    int i;
    for (i = 0; i < json_object_array_length(interfaces); i++) {
      json_object *interface = json_object_array_get_idx(interfaces, i);
      json_object *ifname = NULL;
      json_object_object_get_ex(interface, "ifname", &ifname);
      if (!ifname)
        continue;

      nw_wireless_process_interface(json_object_get_string(ifname), object);
    }
  }

  /* Free data */
  json_object_put(data);

  /* Store resulting JSON object */
  return nw_module_finish_acquire_data(module, object);
}

static int nw_wireless_init(struct nodewatcher_module *module, struct ubus_context *ubus)
{
  return 0;
}

/* Module descriptor */
struct nodewatcher_module nw_module = {
  .name = "core.wireless",
  .author = "Jernej Kos <jernej@kos.mx>",
  .version = 3,
  .hooks = {
    .init               = nw_wireless_init,
    .start_acquire_data = nw_wireless_start_acquire_data,
  },
  .schedule = {
    .refresh_interval = 30,
  },
};
