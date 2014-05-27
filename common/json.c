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
#include <nodewatcher-agent/json.h>

#include <string.h>
#include <ctype.h>

int nw_json_from_uci(struct uci_context *uci,
                     const char *location,
                     json_object *object,
                     const char *key)
{
  struct uci_ptr ptr;
  char *loc = strdup(location);

  /* Perform an UCI extended lookup */
  if (uci_lookup_ptr(uci, &ptr, loc, true) != UCI_OK) {
    free(loc);
    return -1;
  }

  /* Copy value to JSON object */
  switch (ptr.o->type) {
    case UCI_TYPE_STRING: {
      json_object_object_add(object, key, json_object_new_string(ptr.o->v.string));
      break;
    }

    case UCI_TYPE_LIST: {
      struct uci_element *e = NULL;
      json_object *list = json_object_new_array();
      uci_foreach_element(&ptr.o->v.list, e) {
        json_object_array_add(list, json_object_new_string(e->name));
      }
      json_object_object_add(object, key, list);
      break;
    }

    default: {
      /* Unknown/unsupported option type */
      free(loc);
      return -1;
    }
  }

  free(loc);
  return 0;
}

static char *trim_string(char *str)
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

int nw_json_from_file(const char *filename,
                      json_object *object,
                      const char *key)
{
  char tmp[1024];
  FILE *file = fopen(filename, "r");
  if (!file)
    return -1;

  char *buffer = NULL;
  size_t buffer_len = 0;
  while (!feof(file)) {
    size_t n = fread(tmp, 1, sizeof(tmp), file);
    buffer_len += n;
    char *tbuffer = (char*) realloc(buffer, buffer_len + 1);
    if (!tbuffer) {
      free(buffer);
      return -1;
    }
    buffer = tbuffer;

    memcpy(buffer, tmp, n);
  }
  fclose(file);

  buffer[buffer_len] = 0;
  json_object_object_add(object, key, json_object_new_string(trim_string(buffer)));
  free(buffer);
  return 0;
}
