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
