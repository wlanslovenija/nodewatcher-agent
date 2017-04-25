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
#ifndef NODEWATCHER_AGENT_OUTPUT_H
#define NODEWATCHER_AGENT_OUTPUT_H

#include <json.h>
#include <uci.h>

/**
 * Perform output initialization.
 *
 * @param uci UCI context
 * @return On success 0 is returned, -1 otherwise
 */
int nw_output_init(struct uci_context *uci);

/**
 * Returns true if output export is configured.
 */
bool nw_output_is_exporting();

/**
 * Exports the specified JSON object to the designated output
 * file.
 *
 * @param object JSON object to export
 */
void nw_output_export(json_object *object);

#endif
