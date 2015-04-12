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
#include <libubox/md5.h>

#define NW_CLIENT_ID_SALT "nw-client-c3U0XX"
#define NW_CLIENT_ID_SALT_LENGTH 16

/* Results of last scan survey */
static json_object *last_scan_survey = NULL;
/* Number of monitoring intervals */
static int counter_monitor_intervals = 0;

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

static void nw_wireless_add_encryption(json_object *object,
                                       const char *key,
                                       struct iwinfo_crypto_entry *entry)
{
  json_object *encryption = json_object_new_object();

  json_object_object_add(encryption, "enabled", json_object_new_boolean(entry->enabled));

  if (entry->enabled) {
    if (!entry->wpa_version) {
      /* WEP */
      json_object *wep = json_object_new_array();

      if (entry->auth_algs & IWINFO_AUTH_OPEN)
        json_object_array_add(wep, json_object_new_string("open"));

      if (entry->auth_algs & IWINFO_AUTH_SHARED)
        json_object_array_add(wep, json_object_new_string("shared"));

      json_object_object_add(encryption, "wep", wep);
    } else {
      /* WPA */
      json_object *wpa = json_object_new_array();

      if (entry->wpa_version > 2) {
        json_object_array_add(wpa, json_object_new_int(1));
        json_object_array_add(wpa, json_object_new_int(2));
      } else {
        json_object_array_add(wpa, json_object_new_int(entry->wpa_version));
      }

      json_object_object_add(encryption, "wpa", wpa);

      /* Authentication details */
      json_object *authentication = json_object_new_array();

      if (entry->auth_suites & IWINFO_KMGMT_PSK)
        json_object_array_add(authentication, json_object_new_string("psk"));

      if (entry->auth_suites & IWINFO_KMGMT_8021x)
        json_object_array_add(authentication, json_object_new_string("802.1x"));

      if (!entry->auth_suites  || entry->auth_suites & IWINFO_KMGMT_NONE)
        json_object_array_add(authentication, json_object_new_string("none"));

      json_object_object_add(encryption, "authentication", authentication);
    }

    /* Ciphers */
    json_object *ciphers = json_object_new_array();
    int ciph = entry->pair_ciphers | entry->group_ciphers;

    if (ciph & IWINFO_CIPHER_WEP40)
      json_object_array_add(ciphers, json_object_new_string("wep-40"));

    if (ciph & IWINFO_CIPHER_WEP104)
      json_object_array_add(ciphers, json_object_new_string("wep-104"));

    if (ciph & IWINFO_CIPHER_TKIP)
      json_object_array_add(ciphers, json_object_new_string("tkip"));

    if (ciph & IWINFO_CIPHER_CCMP)
      json_object_array_add(ciphers, json_object_new_string("ccmp"));

    if (ciph & IWINFO_CIPHER_WRAP)
      json_object_array_add(ciphers, json_object_new_string("wrap"));

    if (ciph & IWINFO_CIPHER_AESOCB)
      json_object_array_add(ciphers, json_object_new_string("aes-ocb"));

    if (ciph & IWINFO_CIPHER_CKIP)
      json_object_array_add(ciphers, json_object_new_string("ckip"));

    if (!ciph || ciph & IWINFO_CIPHER_NONE)
      json_object_array_add(ciphers, json_object_new_string("none"));

    json_object_object_add(encryption, "ciphers", ciphers);
  }

  json_object_object_add(object, key, encryption);
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

  /* Encryption */
  struct iwinfo_crypto_entry crypto = { 0, };
  if (!iwinfo->encryption(ifname, (char*) &crypto)) {
    nw_wireless_add_encryption(interface, "encryption", &crypto);
  }

  /* Stations */
  char result[IWINFO_BUFSIZE];
  int length, i;
  if (!iwinfo->assoclist(ifname, result, &length)) {
    json_object *stations = json_object_new_array();
    for (i = 0; i < length; i += sizeof(struct iwinfo_assoclist_entry)) {
      struct iwinfo_assoclist_entry *entry = (struct iwinfo_assoclist_entry*) &result[i];
      json_object *station = json_object_new_object();

      char mac[18];
      snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
        entry->mac[0], entry->mac[1], entry->mac[2],
        entry->mac[3], entry->mac[4], entry->mac[5]);

      /* Compute salted hash of MAC address */
      md5_ctx_t ctx;
      uint8_t raw_mac_id[16];
      char mac_id[33];

      md5_begin(&ctx);
      md5_hash(NW_CLIENT_ID_SALT, NW_CLIENT_ID_SALT_LENGTH, &ctx);
      md5_hash(mac, 17, &ctx);
      md5_end(raw_mac_id, &ctx);

      /* Base64 encode the hash so it is more compact */
      if (nw_base64_encode(raw_mac_id, sizeof(raw_mac_id), mac_id, sizeof(mac_id)) == 0) {
        json_object_object_add(station, "client_id", json_object_new_string(mac_id));
      } else {
        json_object_put(station);
        continue;
      }

      json_object_object_add(station, "signal", json_object_new_int(entry->signal));
      json_object_object_add(station, "noise", json_object_new_int(entry->noise));
      json_object_object_add(station, "inactive", json_object_new_int(entry->inactive));

      json_object *rx = json_object_new_object();
      json_object_object_add(rx, "rate", json_object_new_int(entry->rx_rate.rate));
      json_object_object_add(rx, "mcs", json_object_new_int(entry->rx_rate.mcs));
      json_object_object_add(rx, "40mhz", json_object_new_boolean(entry->rx_rate.is_40mhz));
      json_object_object_add(rx, "short_gi", json_object_new_boolean(entry->rx_rate.is_short_gi));
      json_object_object_add(station, "rx", rx);

      json_object *tx = json_object_new_object();
      json_object_object_add(tx, "rate", json_object_new_int(entry->tx_rate.rate));
      json_object_object_add(tx, "mcs", json_object_new_int(entry->tx_rate.mcs));
      json_object_object_add(tx, "40mhz", json_object_new_boolean(entry->tx_rate.is_40mhz));
      json_object_object_add(tx, "short_gi", json_object_new_boolean(entry->tx_rate.is_short_gi));
      json_object_object_add(station, "tx", tx);

      json_object_array_add(stations, station);
    }

    json_object_object_add(interface, "stations", stations);
  }

  json_object_object_add(object, ifname, interface);

  iwinfo = NULL;
  iwinfo_finish();
  return true;
}

static bool nw_wireless_process_radio(const char *ifname,
                                      json_object *object)
{
  /* Initialize iwinfo backend for this device */
  static const struct iwinfo_ops *iwinfo;
  iwinfo = iwinfo_backend(ifname);
  if (!iwinfo)
    return false;

  json_object *radio = json_object_new_object();

  /* Wireless network survey */
  char result[IWINFO_BUFSIZE];
  int len, i;
  if (!iwinfo->scanlist(ifname, result, &len) && len > 0) {
    json_object *survey = json_object_new_array();
    for (i = 0; i < len; i += sizeof(struct iwinfo_scanlist_entry)) {
      json_object *network = json_object_new_object();
      struct iwinfo_scanlist_entry *entry = (struct iwinfo_scanlist_entry*) &result[i];

      /* SSID */
      if (entry->ssid[0])
        json_object_object_add(network, "ssid", json_object_new_string((const char*) entry->ssid));

      /* BSSID */
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
        entry->mac[0], entry->mac[1], entry->mac[2],
        entry->mac[3], entry->mac[4], entry->mac[5]);

      json_object_object_add(network, "bssid", json_object_new_string(mac));

      /* Mode */
      json_object_object_add(network, "mode", json_object_new_string(IWINFO_OPMODE_NAMES[entry->mode]));
      /* Channel */
      json_object_object_add(network, "channel", json_object_new_int(entry->channel));
      /* Signal */
      json_object_object_add(network, "signal", json_object_new_int((uint32_t) (entry->signal - 0x100)));
      /* Encryption */
      nw_wireless_add_encryption(network, "encryption", &entry->crypto);

      json_object_array_add(survey, network);
    }

    json_object_object_add(radio, "survey", survey);
  }

  /* Determine the PHY name and add appropriate section */
  char phyname[IWINFO_BUFSIZE] = { 0, };
  if (!iwinfo->phyname(ifname, phyname)) {
    json_object_object_add(object, phyname, radio);
  } else {
    json_object_put(radio);
  }


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

  json_object *interfaces = json_object_new_object();
  json_object *radios = json_object_new_object();

  /* Limit radio scans to once every ~240 monitoring intervals */
  bool new_radio_survey = false;
  if (!last_scan_survey || ++counter_monitor_intervals >= nw_roughly(240)) {
    new_radio_survey = true;
    counter_monitor_intervals = 0;
  }

  /* Iterate over the list of radios */
  json_object_object_foreach(data, key, val) {
    /* Supress unused variable warning */
    (void) key;

    json_object *ninterfaces = NULL;
    json_object_object_get_ex(val, "interfaces", &ninterfaces);
    if (!ninterfaces)
      continue;

    /* Process interfaces */
    const char *first_radio_iface = NULL;
    int i;
    for (i = 0; i < json_object_array_length(ninterfaces); i++) {
      json_object *interface = json_object_array_get_idx(ninterfaces, i);
      json_object *ifname = NULL;
      json_object_object_get_ex(interface, "ifname", &ifname);
      if (!ifname)
        continue;

      nw_wireless_process_interface(json_object_get_string(ifname), interfaces);
      if (!first_radio_iface)
        first_radio_iface = json_object_get_string(ifname);
    }

    /* Process radio */
    if (new_radio_survey)
      nw_wireless_process_radio(first_radio_iface, radios);
  }

  json_object_object_add(object, "interfaces", interfaces);
  if (new_radio_survey) {
    json_object_object_add(object, "radios", radios);
    if (last_scan_survey != NULL)
      json_object_put(last_scan_survey);

    last_scan_survey = json_object_get(radios);
  } else {
    /* Free the radios object as it has not been used this time */
    json_object_put(radios);
    /* Just reuse previous survey */
    json_object_object_add(object, "radios", json_object_get(last_scan_survey));
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
