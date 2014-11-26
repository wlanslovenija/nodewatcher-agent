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
#include <nodewatcher-agent/utils.h>

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

  if (!ptr.o) {
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

int nw_json_from_file(const char *filename,
                      json_object *object,
                      const char *key,
                      bool integer)
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
  if (integer)
    json_object_object_add(object, key, json_object_new_int(atoi(nw_string_trim(buffer))));
  else
    json_object_object_add(object, key, json_object_new_string(nw_string_trim(buffer)));
  free(buffer);
  return 0;
}

/* Forward declaration */
static void nw_json_from_blob_element(struct blob_attr *attr,
                                      json_object **object);

static void nw_json_from_blob_list(struct blob_attr *attr,
                                   int len,
                                   bool array,
                                   json_object **object)
{
  struct blob_attr *pos;
  int rem = len;

  if (array)
    *object = json_object_new_array();
  else
    *object = json_object_new_object();

  __blob_for_each_attr(pos, attr, rem) {
    json_object *e = NULL;
    nw_json_from_blob_element(pos, &e);
    if (array)
      json_object_array_add(*object, e);
    else
      json_object_object_add(*object, blobmsg_name(pos), e);
  }
}

static void nw_json_from_blob_element(struct blob_attr *attr,
                                      json_object **object)
{
  void *data;
  int len;

  if (!blobmsg_check_attr(attr, false))
    return;

  data = blobmsg_data(attr);
  len = blobmsg_data_len(attr);

  switch (blob_id(attr)) {
    case BLOBMSG_TYPE_UNSPEC: *object = NULL; break;
    case BLOBMSG_TYPE_BOOL: *object = json_object_new_boolean(*(uint8_t *) data ? true : false); break;
    case BLOBMSG_TYPE_INT16: *object = json_object_new_int(be16_to_cpu(*(uint16_t *) data)); break;
    case BLOBMSG_TYPE_INT32: *object = json_object_new_int((int32_t) be32_to_cpu(*(uint32_t *) data)); break;
    case BLOBMSG_TYPE_INT64: *object = json_object_new_int64((int64_t) be64_to_cpu(*(uint64_t *) data)); break;
    case BLOBMSG_TYPE_STRING: *object = json_object_new_string(data); break;
    case BLOBMSG_TYPE_ARRAY: nw_json_from_blob_list(data, len, true, object); break;
    case BLOBMSG_TYPE_TABLE: nw_json_from_blob_list(data, len, false, object); break;
  }
}

void nw_json_from_blob(struct blob_attr *attr,
                       bool table,
                       json_object **object)
{
  if (table)
    nw_json_from_blob_list(blobmsg_data(attr), blobmsg_data_len(attr), false, object);
  else
    nw_json_from_blob_element(attr, object);
}

void nw_json_from_ubus(struct ubus_request *req,
                       int type,
                       struct blob_attr *msg)
{
  if (!msg)
    return;

  /* Convert the response to JSON objects */
  nw_json_from_blob(msg, true, (json_object**) req->priv);
}
