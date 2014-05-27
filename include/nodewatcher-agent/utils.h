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
#ifndef NODEWATCHER_AGENT_UTILS_H
#define NODEWATCHER_AGENT_UTILS_H

/**
 * Trims a string of whitespace characters. The original string
 * memory is modified but no copy is made. This means that the
 * resulting string must not be freed -- instead the original string
 * should be freed.
 *
 * @param str The string to trim
 * @return Modified trimmed string
 */
char *nw_string_trim(char *str);

#endif
