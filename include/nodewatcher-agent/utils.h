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

#include <stddef.h>

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

/**
 * Returns the number of lines in a file.
 *
 * @param filename Filename
 * @return Number of lines on success, -1 on failure
 */
int nw_file_line_count(const char *filename);

/**
 * Encodes data as Base64.
 *
 * @param data Input data to encode
 * @param data_length Length of input data
 * @param result Output buffer
 * @param result_length Length of output buffer
 * @return 0 on success, -1 on failure
 */
int nw_base64_encode(const void *data, size_t data_length, char *result, size_t result_length);

/**
 * Reads some random bytes and stores them in buf.
 *
 * @param buf Destination buffer
 * @param len Number of bytes to read
 * @return Number of bytes actually read or -1 on failure
 */
int nw_read_random_bytes(void *buf, size_t len);

/**
 * Returns a randomly modified value around the given value.
 *
 * @param value Input value
 * @return Roughly the same value
 */
int nw_roughly(int value);

#endif
