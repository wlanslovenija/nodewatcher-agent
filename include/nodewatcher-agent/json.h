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
#ifndef NODEWATCHER_AGENT_JSON_H
#define NODEWATCHER_AGENT_JSON_H

#include <json-c/json.h>
#include <uci.h>
#include <libubox/blobmsg.h>
#include <libubus.h>

/**
 * Looks up an UCI location and stores the retrieved value into
 * the specified JSON object under the specified key.
 *
 * @param uci UCI context
 * @param location UCI location expression (extended syntax)
 * @param object Destination JSON object
 * @param key Key in JSON object
 * @return 0 on success, -1 on failure
 */
int nw_json_from_uci(struct uci_context *uci,
                     const char *location,
                     json_object *object,
                     const char *key);

/**
 * Opens a file and stores the contents into the specified JSON
 * object under the specified key.
 *
 * @param filename Filename
 * @param object Destination JSON object
 * @param key Key in JSON object
 * @param integer True if result should be converted to an integer
 * @return 0 on success, -1 on failure
 */
int nw_json_from_file(const char *filename,
                      json_object *object,
                      const char *key,
                      bool integer);

/**
 * Converts a blob to JSON objects.
 *
 * @param attr Blob
 * @param table True if blob is a table
 * @param object Destination JSON object
 */
void nw_json_from_blob(struct blob_attr *attr,
                       bool table,
                       json_object **object);

/**
 * This function can be used as an ubus response callback to convert
 * the response into JSON structure. The request's priv attribute
 * should be of type json_object** (destination JSON object address).
 */
void nw_json_from_ubus(struct ubus_request *req,
                       int type,
                       struct blob_attr *msg);

/* Macro to simplify copying of JSON attributes */
#define NW_COPY_JSON_OBJECT(src, src_key, dst, dst_key) \
  { \
    json_object *tmp; \
    json_object_object_get_ex((src), src_key, &tmp); \
    if (tmp) \
      json_object_object_add((dst), dst_key, json_object_get(tmp)); \
  }

#endif
