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
#include <nodewatcher-agent/utils.h>

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

char *nw_string_trim(char *str)
{
  char *end;

  /* Trim leading spaces */
  while (isspace(*str))
    str++;

  if (*str == 0)
    return str;

  /* Trim trailing spaces */
  end = str + strlen(str) - 1;
  while (end > str && isspace(*end))
    end--;

  /* Write new null terminator */
  *(end + 1) = 0;
  return str;
}

int nw_file_line_count(const char *filename)
{
  FILE *file = fopen(filename, "r");
  if (!file)
    return -1;

  int lines = 0;
  while (!feof(file)) {
    fscanf(file, "%*[^\n]\n");
    lines++;
  }

  fclose(file);
  return lines;
}

int nw_base64_encode(const void *data, size_t data_length, char *result, size_t result_length)
{
  const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const uint8_t *data_buf = (const uint8_t*) data;
  size_t result_index = 0;
  size_t x;
  uint32_t n = 0;
  int padding = data_length % 3;
  uint8_t n0, n1, n2, n3;

  /* Increment over the length of the string, three characters at a time. */
  for (x = 0; x < data_length; x += 3)  {
    /* These three 8-bit (ASCII) characters become one 24-bit number. */
    n = data_buf[x] << 16;

    if ((x + 1) < data_length)
      n += data_buf[x + 1] << 8;

    if ((x + 2) < data_length)
      n += data_buf[x + 2];

    /* This 24-bit number gets separated into four 6-bit numbers. */
    n0 = (uint8_t) (n >> 18) & 63;
    n1 = (uint8_t) (n >> 12) & 63;
    n2 = (uint8_t) (n >> 6) & 63;
    n3 = (uint8_t) n & 63;

    /* If we have one byte available, then its encoding is spread out over two characters. */
    if (result_index >= result_length)
      return -1;
    result[result_index++] = base64chars[n0];
    if (result_index >= result_length)
      return -1;
    result[result_index++] = base64chars[n1];

    /* If we have only two bytes available, then their encoding is spread out over three chars. */
    if ((x + 1) < data_length) {
      if (result_index >= result_length)
        return -1;
      result[result_index++] = base64chars[n2];
    }

    /* If we have all three bytes available, then their encoding is spread out over four characters */
    if ((x + 2) < data_length) {
      if (result_index >= result_length)
        return -1;
      result[result_index++] = base64chars[n3];
    }
  }

  /* Create and add padding that is required if we did not have a multiple of 3
     number of characters available. */
  if (padding > 0) {
    for (; padding < 3; padding++) {
      if (result_index >= result_length)
        return -1;
      result[result_index++] = '=';
    }
  }

  if (result_index >= result_length)
    return -1;

  result[result_index] = 0;
  return 0;
}

int nw_read_random_bytes(void *buf, size_t len)
{
  int fd;
  int rc;

  fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) {
    rc = -1;
  } else {
    rc = read(fd, buf, len);
    if (rc < 0 || (unsigned)rc < len)
      rc = -1;
    close(fd);
  }

  return rc;
}

int nw_roughly(int value)
{
  if(value < 0)
    return -nw_roughly(-value);
  else if(value <= 1)
    return value;
  else
    return value * 3 / 4 + random() % (value / 2);
}

char *nw_uci_get_string(struct uci_context *uci, const char *location)
{
  struct uci_ptr ptr;
  char *loc = strdup(location);
  char *result = NULL;

  /* Perform an UCI extended lookup */
  if (uci_lookup_ptr(uci, &ptr, loc, true) != UCI_OK) {
    free(loc);
    return NULL;
  }

  if (ptr.o && ptr.o->type == UCI_TYPE_STRING) {
    result = strdup(ptr.o->v.string);
  }

  free(loc);
  return result;
}

int nw_uci_get_int(struct uci_context *uci, const char *location)
{
  char *string = nw_uci_get_string(uci, location);
  if (!string)
    return 0;

  int result = atoi(string);
  free(string);

  return result;
}
