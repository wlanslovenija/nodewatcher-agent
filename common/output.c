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
#include <nodewatcher-agent/output.h>

#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* Output file path */
static char *output_filename = NULL;

int nw_output_init(struct uci_context *uci)
{
  /* Obtain the output filename via UCI */
  struct uci_ptr ptr;
  char *loc = strdup("nodewatcher.@agent[0].output_json");

  /* Perform an UCI extended lookup */
  if (uci_lookup_ptr(uci, &ptr, loc, true) != UCI_OK) {
    free(loc);
    return 0;
  }

  if (ptr.o && ptr.o->type == UCI_TYPE_STRING) {
    output_filename = strdup(ptr.o->v.string);
    syslog(LOG_INFO, "Configured for direct output to '%s'.", output_filename);
  }

  free(loc);

  /* Ensure that the output filename is a symlink to /tmp to avoid flash wear. */
  unlink(output_filename);
  /* Return code of 'unlink' is ignored as the file may not even exist. */
  if (symlink("/tmp/nodewatcher_agent_feed", output_filename) != 0) {
    syslog(LOG_WARNING, "Unable to create symlink to '/tmp'! This may cause increased flash wear.");
  }

  return 0;
}

bool nw_output_is_exporting()
{
  return output_filename != NULL;
}

void nw_output_export(json_object *object)
{
  if (!output_filename)
    return;

  /* Export JSON to configured output file */
  mode_t pmask = umask(0022);
  FILE *output_file = fopen(output_filename, "w");
  fprintf(output_file, "%s\n", json_object_to_json_string(object));
  fclose(output_file);

  /* Restore umask */
  umask(pmask);
}
