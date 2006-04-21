/**
 * Copyright (C) 2002,2003, 2005-2006 Alastair McKinstry, <mckinstry@debian.org>
 * Released under the GPL
 *
 * $Id: kbd-chooser.c 33904 2006-01-09 18:46:38Z smarenka $
 */

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <debian-installer.h>
#include <cdebconf/debconfclient.h>
#include <linux/serial.h>
#include <locale.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include "nls.h"
#include "xmalloc.h"
#include "kbd-chooser.h"


extern void loadkeys_wrapper (const char *map);	// in loadkeys.y

int main (int argc, char **argv) {
  struct debconfclient *client = debconfclient_new();

  setlocale (LC_ALL, "");
  di_system_init("kbd-chooser-apply"); // enable syslog
  
  if (debconf_get(client, "console-tools/archs") == CMD_SUCCESS 
      && client->value && strcmp(client->value, "no-keyboard") == 0)
    return 0; /* La, la, la, we just exit silently if we don't
               * want a keyboard */
  
  if (debconf_get(client, "debian-installer/keymap") == CMD_SUCCESS &&
      client->value) {
    loadkeys_wrapper(strdup(client->value));
    return 0;
  }
  return 1;
}
