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

#include <json/json.h>
#include <uci.h>

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
 * @return 0 on success, -1 on failure
 */
int nw_json_from_file(const char *filename,
                      json_object *object,
                      const char *key);

#endif
