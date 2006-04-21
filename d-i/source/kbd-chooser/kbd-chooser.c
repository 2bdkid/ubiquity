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


typedef enum { 
	SERIAL_ABSENT = 0,
	SERIAL_PRESENT = 1,
	SERIAL_UNKNOWN = 2
} sercon_state;

typedef enum {
	GOBACK,
	CHOOSE_METHOD,
	CHOOSE_TRY,
	CHOOSE_ARCH,
	CHOOSE_KEYMAP,
	CHOOSE_TEST,
	QUIT,
	ERROR
} state_t;

struct debconfclient *
mydebconf_client (void) {
	static struct debconfclient *client = NULL;
	if (client == NULL)
		client = debconfclient_new ();
	return client;
}

int
mydebconf_ask (char *priority, char *template, char **result)
{
	int res;
	struct debconfclient *client = mydebconf_client ();

	debconf_input (client, priority, template);
	res = debconf_go (client);
	if (res != CMD_SUCCESS)
		return res;
	res = debconf_get (client, template);
	*result = client->value;
	return res;
}

/**
 * @brief    Set a default value for a question
 * @param The question
 * @param    The default to set
 * @param    if true: overwrite previous choice
 */
int
mydebconf_default_set (char *template, char *value, bool force)
{
	int res = 0;
	struct debconfclient *client = mydebconf_client ();

	if ((res = debconf_get (client, template)))
		return res;

	if (force || client->value == NULL || (strlen (client->value) == 0))
		res = debconf_set (client, template, strdup (value));
	return res;
}


/**
 * @brief  Ensure a directory is present (and readable)
 */
int check_dir (const char *dirname)
{
	struct stat buf;
	if (stat (dirname, &buf))
		return 0;
	return S_ISDIR(buf.st_mode);
}

/**
 * @brief  Do a grep for a string
 * @return 0 if not found, -errno if error, 
 *   and LINESIZE * (line - 1) + pos, when found at line, pos.
 */
int
grep (const char *file, const char *string)
{
	FILE *fp = fopen (file, "r");
	char buf[LINESIZE];
	char * ret;
	int lines = 0;
	if (!fp)
		return -errno;
	while (!feof (fp))	{
		fgets (buf, LINESIZE, fp);
		if ((ret = strstr (buf, string)) != NULL)	{
			fclose (fp);
			return ((int) (ret - buf) + 1 + (lines * LINESIZE));
		}
		lines ++;
	}
	fclose (fp);
	return 0;
}

/*
 * Helper routines for maplist_select
 */

/**
 * @brief return a default locale name, eg. en_US.UTF-8 (Change C, POSIX to en_US)
 * @return - char * locale name (freed by caller)
 */
char *
locale_get (void)
{
	int ret = 0;

	struct debconfclient *client = mydebconf_client ();
	// languagechooser sets locale of the form xx_YY
	// NO encoding used.

	ret = debconf_get (client, "debian-installer/locale");
	if ((ret == 0) && client->value && (strlen (client->value) > 0))
		return strdup(client->value);
	else
		return strdup("en_US");
}

/**
 * @brief parse a locale into pieces. Assume a well-formed locale name
 *
 */
void
locale_parse (char *locale, char **lang, char **territory, char **modifier, char **charset)
{
	char *und, *at, *dot, *loc = strdup (locale);

	// Each separator occurs only once in a well-formed locale
	und = strchr (loc, '_');
	at = strchr (loc, '@');
	dot = strchr (loc, '.');

	if (und)
	    *und = '\0';
	if (at)
	    *at = '\0';
	if (dot)
	    *dot = '\0';

	*lang = loc;
	*territory = und ? (und + 1) : NULL;
	*modifier  = at  ? (at + 1 ) : NULL;
	*charset   = dot ? (dot + 1) : NULL;
}

/**
 * @brief compare langs list with the preferred locale
 * @param langs: colon-seperated list of locales
 * @return score 0-8
 */
int
locale_list_compare (char *langs)
{
	static char *locale = NULL, *lang1 = NULL, *territory1 = NULL, 
	  *modifier1 = NULL, *charset1 = NULL;
	char *lang2 = NULL, *territory2 = NULL, *charset2 =	 
		NULL, *modifier2 = NULL, buf[LINESIZE], *s, *colon;
	int score = 0, best = -1;

	if (!locale)	{
		locale = locale_get ();
		locale_parse (locale, &lang1, &territory1, &modifier1, &charset1);
	}
	strcpy (buf, langs);
	s = buf;
	while (s)	{
		colon = strchr (s, ':');
		if (colon)
			*colon = '\0';
		locale_parse (s, &lang2, &territory2, &modifier2, &charset2);
		if (!strcmp (lang1, lang2))    {
			score = 3;
			if (territory1  && territory2 && !strcmp (territory1, territory2))  	{
				score+=2;
			}
			// Favour 'generic' locales; ie 'fr' matches 'fr' better
			// than 'fr_BE' does
			if (!territory1 && territory2)
				score--;
			if (territory1 && !territory2)
				score++;
			if (charset1 && charset2 && !strcmp (charset1, charset2))	       	{
				score += 3;	// charset more important than territory
			}
		}
		best = (score > best) ? score : best;
		s = colon ? (colon + 1) : NULL;
	}
	return best;
}


/**
 * @brief  Insert description into buffer
 * @description ; may be NULL.
 * @return      ptr to char after description.
 */
char *
insert_description (char *buf, char *description, int *first_entry)
{
	char *s = buf;

	if (*first_entry) {
		*first_entry = 0;
	} else {
		strcpy (s, ", ");
		s += 2;
	}
	strcpy (s, description);
	s += strlen (description);
	*s = '\0';
	return s;
}


/**
 * @brief Enter a maplist into debconf, picking a default via locale.
 * @param maplist - a maplist (for a given arch, for example)
 */
void
maplist_select (maplist_t * maplist)
{
	char template[LINESIZE];
	keymap_t *mp, *preferred = NULL;
	int score = 0, best = -1;

	// Pick the default
	for (mp = maplist->maps ; mp != NULL ; mp  = mp->next)	{
		score = locale_list_compare (mp->langs);
		if (score > best) {
			best = score;
			preferred = mp;
		}
	}
	if (best > 0)	{
		sprintf (template, "console-keymaps-%s/keymap", maplist->name);
		mydebconf_default_set (template, preferred->name, false);
	}
}


/**
 * @brief	Get a maplist "name", creating if necessary
 * @name	name of arch, eg. "at", "mac"
 */
maplist_t *maplist_get (const char *name)
{
	static maplist_t *maplists = NULL;

	maplist_t *p;

	for (p = maplists; p != NULL; p = p->next)    {
		if (strcmp (p->name, name) == 0)
			break;
	}
	if (p)
		return p;
	p = di_new (maplist_t,1);
	if (p == NULL)    {
		di_error (": Failed to create maplist (out of memory)\n");
		exit (1);
	}
	p->next = maplists;
	p->maps = NULL;
	p->name = strdup (name);
	maplists = p;
	return p;
}

/**
 * @brief	Get a keymap in a maplist; create if necessary
 * @list	maplist to search
 * @name	name of list
 * @langs	if non-NULL, match these languages
 */
keymap_t *keymap_get (maplist_t * list, const char *name, const char *langs)
{
	keymap_t *mp;

	for (mp = list->maps ; mp != NULL; mp = mp->next)    {
		if (strcmp (mp->name, name) == 0 &&
		    (!langs || strcmp (mp->langs, langs) == 0))
			break;
	}
	if (mp)
		return mp;
	mp = di_new (keymap_t,1);
	if (mp == NULL)    {
		di_error (": Failed to malloc keymap_t");
		exit (2);
	}
	mp->langs = NULL;
	mp->name = strdup (name);
	mp->description = NULL;
	mp->next = list->maps;
	list->maps = mp;
	return mp;
}


/**
 * @brief    Load the keymap files into memory
 @ @name     keymap filename.
 * @warning  No error checking on file contents. Assumed correct by installation checks.
 */
maplist_t *
maplist_parse_file (const char *name)
{
	FILE *fp;
	maplist_t *maplist;
	keymap_t *map;
	char buf[LINESIZE], *tab1, *tab2, *nl;
	fp = fopen (name, "r");

	if (fp == NULL) {
		di_error (": Failed to open %s: %s \n", name, strerror (errno));
		exit (3);
	}
	maplist = maplist_get ((char *) (name + strlen (KEYMAPLISTDIR) +
				       strlen ("console-keymaps-") + 1));

	while (!feof (fp))   {
		fgets (buf, LINESIZE, fp);
		if (*buf == '#')		//comment ; skip line
			continue;
		tab1 = strchr (buf, '\t');
		if (!tab1)
			continue;		// malformed line
		tab2 = strchr (tab1 + 1, '\t');
		if (!tab2)
			continue;		// malformed line
		nl = strchr (tab2, '\n');
		if (!nl)
			continue;		// malformed line
		*tab1 = '\0';
		*tab2 = '\0';
		*nl = '\0';

		map = keymap_get (maplist, tab1 + 1, buf);
		if (!map->langs) {	// new keymap
			map->langs = strdup (buf);
			map->description = strdup (tab2 + 1);
		}
	}
	fclose (fp);
	return maplist;
}


/**
 * @brief   Read keymap files from /usr/share/console/lists and parse them
 * @listdir Directory to look in
 * @warning Assumes files present, readable: this should be guaranteed by the installer dependencies
 */
void
read_keymap_files (char *listdir)
{
	DIR *d;
	char *p, fullname[LINESIZE];
	struct dirent *ent;
	struct stat sbuf;

	strncpy (fullname, listdir, LINESIZE);
	p = fullname + strlen (listdir);
	*p++ = '/';

	d = opendir (listdir);
	if (d == NULL)	{
		di_error (": Failed to open %s: %s (keymap files probably not installed)\n",
			 listdir, strerror (errno));
		exit (4);
	}
	ent = readdir (d);
	for (; ent; ent = readdir (d))	{

		if ((strcmp (ent->d_name, ".") == 0) ||
		    (strcmp (ent->d_name, "..") == 0))
			continue;
		strcpy (p, ent->d_name);
		if (stat (fullname, &sbuf) == -1)		{
			di_error (": Failed to stat %s: %s\n", fullname,
				 strerror (errno));
			exit (5);
		}
		if (S_ISDIR (sbuf.st_mode))	{
			read_keymap_files (fullname);
		}	else	{				// Assume a file
			/* two types of name allowed (for the moment; )
			 * legacy 'console-keymaps-* names and *.keymaps names
			 */
			if (strncmp (ent->d_name, "console-keymaps-", 16) == 0) {
				strcpy (p, ent->d_name);
				maplist_select (maplist_parse_file (fullname));
			}
		}
	}

	closedir (d);
}
 
/**
 * @brief Sort keyboards
 */
void
keyboards_sort (kbd_t ** keyboards)
{
	kbd_t *p = *keyboards, **prev;
	int in_order = 1;

// Yes, it's bubblesort. But for this size of list, it's efficient
	while (!in_order) {
		in_order = 1;
		p = *keyboards;
		prev = keyboards;
		while (p) {
			if (p->next && 
			    (strcmp (p->next->description, p->description) < 0)) {
				in_order = 0;
				*prev = p->next;
				p->next = p->next->next;
				(*prev)->next = p;
			}
			prev = &(p->next);
			p = p->next;
		}
	}
}

/**
 * @brief Get template contents (in the current language).
 * @param template Template name
 * @param type Kind of entry
 * @return the translation, to be freed by caller
 */
char *
template_get(const char *template, char *type)
{
	struct debconfclient *client = mydebconf_client();

	/* cdebconf auto-translates */
	debconf_metaget(client, template, type);
	return strdup(client->value);
}

/**
 * @brief Get translated description for a given template.
 * @param template Template name
 * @return the translation, to be freed by caller
 */
char *
description_get(const char *template)
{
	return template_get(template, "Description");
}

char *
skipspc(char *x)
{
	while(*x && isspace(*x)) x++;
	return x;
}

/**
 * @brief Find translated substring for arch+keyboard name.
 * @arch  Keyboard architecture (at, usb, ...)
 * @name  keyboard name
 * @to_c  Flag whether to translate local=>C, or vice versa
 * @return the translation, to be freed by caller
 *
 * The current templates hold a per-arch list of keymap names and
 * their translations, in order to avoud having n*m templates. 
 * Unfortunately, this means that extracting the translated name of a
 * single keymap name is a bit of work.
 */
char *
translate_keyboard_name(const char *arch, const char *name, bool to_c)
{
	struct debconfclient *client = mydebconf_client();
	char template[LINESIZE], *lang;
	char *buf1, *buf2;
	char *bufend1, *bufend2;
	char *p1,*p2;
	char *lim1,*lim2;

	if (strcmp(arch, "no-keyboard") == 0 ||
            strcmp(arch, "skip-config") == 0)
		return strdup (name);

	/*
	 * cdebconf auto-translates its template data, depending on the
	 * language that's been set. Thus, in order to get the untranslated
	 * string, temporarily set the language to something nonexistent.
	 */
        
	if (debconf_get(client, "debconf/language")) {
          lang = "";
        } else {
          lang = strdup(client->value);
        }

	debconf_set (client, "debconf/language", strdup ("xx_XX"));

	sprintf (template, "console-keymaps-%s/keymap", arch);
	buf1 = template_get(template, "Choices");

	debconf_set (client, "debconf/language", lang);
        free(lang);
	buf2 = template_get(template, "Choices");

	if (!strcmp(buf1,buf2)) {
		free (buf1);
		free (buf2);
		return strdup(name);
	}

	if (to_c) { /* swap directions */
		p1 = buf1; buf1 = buf2; buf2 = p1;
	}
	
	p1 = buf1; p2 = buf2;
        bufend1 = buf1+strlen(buf1)+1;
        bufend2 = buf2+strlen(buf2)+1;
	while (p1 && p2) {
                for (lim1 = p1; *lim1 && *lim1 != ','; lim1++) {
                        if (*lim1 == '\\')
                                lim1++;
                }

                for (lim2 = p2; *lim2 && *lim2 != ','; lim2++) {
                        if (*lim2 == '\\')
                                lim2++;
                }

                *lim1 = '\0';
		*lim2 = '\0';
		if (! strcmp (name, p1)) {
			p2 = strdup (p2);
			free (buf1);
			free (buf2);
			return p2;
		}

		if (!(lim1 < bufend1 && lim2 < bufend2))
                        break;
 
                p1 = skipspc(lim1+1);
                p2 = skipspc(lim2+1);
	}
	/* not found */
	free (buf1);
	free (buf2);
	return strdup (name);
}

/**
 * @brief Build a list of the keyboards present on this computer
 * @returns kbd_t list
 */
kbd_t *
keyboards_get (void)
{
	static kbd_t *keyboards = NULL, *p = NULL;
	char buf[25];
	const char *subarch = di_system_subarch_analyze();

	if (keyboards != NULL)
		return keyboards;

#if defined (USB_KBD)
	keyboards = usb_kbd_get (keyboards, subarch);
#endif
#if defined (AT_KBD)
	keyboards = at_kbd_get (keyboards, subarch);
#endif
#if defined (MAC_KBD)
	keyboards = mac_kbd_get (keyboards, subarch);
#endif
#if defined (SPARC_KBD)
	keyboards = sparc_kbd_get (keyboards, subarch);
#endif
#if defined (ATARI_KBD)
	keyboards = atari_kbd_get (keyboards, subarch);
#endif
#if defined (AMIGA_KBD)
	keyboards = amiga_kbd_get (keyboards, subarch);
#endif
#if defined (SERIAL_KBD)
	keyboards = serial_kbd_get (keyboards, subarch);
#endif
#if defined (DEC_KBD)
	keyboards = dec_kbd_get (keyboards, subarch);
#endif
#if defined (HIL_KBD)
	keyboards = hil_kbd_get (keyboards, subarch);
#endif

	// Did we forget to compile in a keyboard ???
	if (DEBUG && keyboards == NULL) {
		di_error (": No keyboards found\n");
		exit (6);
	}
	// Get the (translated) keyboard names
	for (p = keyboards; p != NULL; p = p->next) {
		sprintf(buf, "kbd-chooser/kbd/%s", p->name);
		p->description = description_get(buf);
	}
	return keyboards;
}

/**
 * @brief translate localised keyboard name back to kbd. arch name
 */
char *keyboard_parse (char *reply)
{
	kbd_t *kb;
	for (kb = keyboards_get(); kb != NULL; kb = kb->next) {
		if (!strcmp (reply, kb->description))
			break;
	}
	return (kb) ? kb->name : "no-keyboard";
}

/**
 * @brief set debian-installer/uml-console as to whether we are using a user mode linux console
 * This is then passed via prebaseconfig to base-config
 * @return 1 if present, 0 if absent, 2 if unknown.
 */
sercon_state
check_if_uml_console (void)
{
	sercon_state present = SERIAL_UNKNOWN;
	struct debconfclient *client = mydebconf_client ();

	if (grep("/proc/cpuinfo", "User Mode Linux") > 0)
		present = SERIAL_PRESENT;
	else
		present = SERIAL_ABSENT;

	debconf_set (client, "debian-installer/uml-console", present ? "true" : "false");
	di_info ("Setting debian-installer/uml-console to %s", present ? "true" : "false");
	return present;
}

/**
 * @brief set debian-installer/serial console as to whether we are using a serial console
 * This is then passed via prebaseconfig to base-config
 * @return 1 if present, 0 if absent, 2 if unknown.
 */
sercon_state
check_if_serial_console (void)
{
	sercon_state present = SERIAL_UNKNOWN;
	struct debconfclient *client = mydebconf_client ();
	int fd;
	struct serial_struct sr;
	int rets, ret1, ret2, ret;

	// Some UARTs don't support the TIOCGSERIAL ioctl(), so also
        // try to detect serial console via cmdline
	ret1 = grep("/proc/cmdline","console=ttyS");
	ret2 = grep("/proc/cmdline","console=ttys");
	rets = ret1 > ret2 ? ret1 : ret2;
	ret = grep("/proc/cmdline","console=tty0");

	if ((rets > 0) && (rets > ret)) {
		present = SERIAL_PRESENT;
	} else {
                fd = open ("/dev/console", O_NONBLOCK);
                if (fd == -1)
	                return SERIAL_UNKNOWN;
                present = (ioctl (fd, TIOCGSERIAL, &sr) == 0) ? SERIAL_PRESENT : SERIAL_ABSENT;
		close (fd);
	}
	
	debconf_set (client, "debian-installer/serial-console", present ? "true" : "false");
	di_info ("Setting debian-installer/serial-console to %s", present ? "true" : "false");
	return present;
}

/**
 * @brief  Make sure there are no double values in the arch selection dialog.
 * Because usb-kbd.c adds a keyboard for each keyboard detected and
 * because usb-kbd.c may add keyboard type 'at' in some situations when an
 * usb keyboard is detected, it is possible the types 'usb' and 'at' could
 * be present more than once.
 * The functions add_kbdtype and kbdtype_present test for this situation.
 * FIXME: This could maybe be removed when the problems with keymap types for
 *        usb keyboards have been resolved.
 */

typedef struct kbdtype_s {
	char *name;		// short name of kbd arch
	struct kbdtype_s *next;
} kbdtype_t;

static inline kbdtype_t *add_kbdtype (kbdtype_t *archlist, char *name) {
	kbdtype_t *ka = xmalloc (sizeof(kbdtype_t));
	ka->name = name;
	ka->next = archlist;
	archlist = ka;
	return archlist;
}

int kbdtype_present (kbdtype_t *archlist, char *name) {
	kbdtype_t *ka = NULL;

	for (ka = archlist; ka != NULL; ka = ka->next) {
		if (strcmp (ka->name, name) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * @brief  Pick a keyboard, adding it to debconf.
 * @return const char *  - priority of question
 */
char *
keyboard_select (char *curr_arch)
{
	kbd_t *kp = NULL, *preferred = NULL;
	kbdtype_t *archlist = NULL;
	char buf_s[LINESIZE], *s = NULL, buf_t[LINESIZE], *t = NULL, *arch_descr = NULL;
	int choices = 0, first_entry_s = 1, first_entry_t = 1;
	sercon_state sercon;
	sercon_state umlcon;
	struct debconfclient *client = mydebconf_client ();

	/* k is returned by a method if it is preferred keyboard.
	 * For 2.4 kernels, we just select one keyboard. 
	 * In 2.6+ we may have per-keyboard keymaps, and better autodetection
	 * of keyboards present.
	 */

	s = buf_s; t = buf_t;
	// Add the keyboards to debconf
	for (kp = keyboards_get (); kp != NULL; kp = kp->next) {
		di_info ("keyboard type %s: present: %s \n", kp->name,
			kp->present == UNKNOWN ? "unknown ": 
			(kp->present == TRUE ? "true: " : "false" ));
		if ((kp->present != FALSE) &&
		    (kbdtype_present (archlist, kp->name) == FALSE)) {
			choices++;
			s = insert_description (s, kp->name, &first_entry_s);
			t = insert_description (t, kp->description, &first_entry_t);
			archlist = add_kbdtype (archlist, kp->name);
			if (strcmp (PREFERRED_KBD, kp->name) == 0) {
				if ((preferred == NULL) || (preferred->present == UNKNOWN)
				    || (kp->present == TRUE))
					preferred = kp;
			} else {
				if ((preferred == NULL) || 
				    (preferred->present != TRUE && kp->present == TRUE))
					preferred = kp;
			}
		}
	}
	sercon = check_if_serial_console();
	umlcon = check_if_uml_console();
	if (sercon == SERIAL_PRESENT || umlcon == SERIAL_PRESENT) {
		debconf_metaget(client, "kbd-chooser/no-keyboard", "Description");
		arch_descr = strdup(client->value);
		choices++;
		s = insert_description (s, "no-keyboard", &first_entry_s);
		t = insert_description (t, arch_descr, &first_entry_t);
		mydebconf_default_set ("console-tools/archs", "no-keyboard", false);
	} else {
		// Add option to skip keyboard configuration (use kernel keymap)
		debconf_metaget(client, "kbd-chooser/skip-config", "Description");
		arch_descr = strdup(client->value);
		choices++;
		s = insert_description (s, "skip-config", &first_entry_s);
		t = insert_description (t, arch_descr, &first_entry_t);
		mydebconf_default_set ("console-tools/archs",  
				      preferred ? preferred->name : "skip-config", false);
	}
	debconf_subst (client, "console-tools/archs", "KBD-ARCHS", buf_s);
	debconf_subst (client, "console-tools/archs", "KBD-ARCHS-L10N", buf_t);
	free(arch_descr);
	// Set medium priority if current selection is no-keyboard or skip-config
	return ((sercon == SERIAL_PRESENT) || (umlcon == SERIAL_PRESENT) ||
		((preferred && preferred->present == TRUE) &&
		 (strcmp (curr_arch, "skip-config") != 0) &&
		 (strcmp (curr_arch, "no-keyboard") != 0))) ? "low" : "medium";
}

/**
 * @brief   return the default keymap for a given arch
 * @arch    keyboard architecture
 * @return  the name of the default keymap (allocated), or NULL if not found
 */

char *
default_keymap (const char *arch)
{
	kbd_t *kb;

	di_info ("default_keymap: have arch: %s", arch ? *arch ? arch : "<empty>" : "<none>");
	for (kb = keyboards_get (); kb != NULL; kb = kb->next) {
		di_info ("default_keymap: check: %s", kb->name);
		if (!strcmp (kb->name, arch))
			return (kb->deflt ? strdup(kb->deflt) : NULL);
	}
	if (DEBUG && !kb) {
		di_error ("Keyboard not found\n");
		exit (7);
	}
	di_info ("default_keymap: not found");
	return NULL;
}

/**
 * @brief   choose a given keyboard
 * @arch    keyboard architecture
 * @keymap  ptr to buffer in which to store chosen keymap name
 * @returns CMD_SUCCESS or CMD_GOBACK, keymap set if SUCCESS
 */

state_t
keymap_ask (char *arch, char **keymap)
{
	char *ptr;
	int res;
	char template[LINESIZE], buf[LINESIZE], *s = NULL;
	char *txt_default, *txt_try, *txt_select, *txt_test, *txt_none;
	keymap_t *def;
	int first_entry = 1;
	struct debconfclient *client = mydebconf_client ();

	txt_try = description_get ("kbd-chooser/do_try");
	txt_select = description_get ("kbd-chooser/do_select");
	txt_test = description_get ("kbd-chooser/do_test");
	txt_none = description_get ("kbd-chooser/no-keyboard");

	ptr = default_keymap (arch);
	if (ptr) {
		*keymap = ptr;
		def = keymap_get (maplist_get (arch), ptr, NULL);
		mydebconf_default_set (template, ptr, false);
		txt_default = translate_keyboard_name (arch, ptr, false);
		mydebconf_default_set ("kbd-chooser/method", txt_default, true);
	} else {
		sprintf (template, "console-keymaps-%s/keymap", arch);
		if (!debconf_get (client, template) && *client->value) {
			di_info ("keymap_ask: default map: %s", *keymap);
                        *keymap = strdup(client->value);
			txt_default = translate_keyboard_name (arch, *keymap, false);
			di_info ("keymap_ask: trans: %s", *keymap);
			mydebconf_default_set ("kbd-chooser/method", txt_default, true);
		} else {
			di_info ("keymap_ask: no default map!");
			txt_default = description_get ("kbd-chooser/no-keyboard");
			if (access("/usr/share/keymaps/decision-tree", R_OK) == 0)
				mydebconf_default_set ("kbd-chooser/method", txt_try, false);
			else
				mydebconf_default_set ("kbd-chooser/method", txt_select, true);
		}
	}

	s = buf;
	s = insert_description (s, txt_default, &first_entry);
	s = insert_description (s, "", &first_entry);
	if (access("/usr/share/keymaps/decision-tree", R_OK) == 0)
		s = insert_description (s, txt_try, &first_entry);
	s = insert_description (s, txt_select, &first_entry);
	s = insert_description (s, txt_test, &first_entry);
	debconf_subst (client, "kbd-chooser/method", "choices", buf);

	res = mydebconf_ask ( "high", "kbd-chooser/method", &ptr);
	if (res == CMD_GOBACK) {
		res = GOBACK;
		goto out;
	}
	if (res == CMD_INTERNALERROR) {
		res = CHOOSE_ARCH;
		goto out;
	}
	if (res) {
		res = GOBACK;
		goto out;
	}
	if (!*ptr) { /* empty selection => re-do */
		res = CHOOSE_METHOD;
		goto out;
	}

	if(!strcmp (ptr, txt_try)) {
		res = CHOOSE_TRY;
		goto out;
	}
	if(!strcmp (ptr, txt_select)) {
		res = CHOOSE_ARCH;
		goto out;
	}
	if(!strcmp (ptr, txt_test)) {
		res = CHOOSE_TEST;
		goto out;
	}
	if(!strcmp (ptr, txt_none)) {
		res = QUIT;
		goto out;
	}
	/* Assume that anything else is (the local name of) a keyboard. */
	res = CMD_SUCCESS;

	*keymap = strdup(ptr); /* still points to debconf-internal data */
	ptr = translate_keyboard_name (arch, *keymap, true);
        free(*keymap);
	*keymap = strdup(ptr);
	free(ptr);
	res = QUIT;

out:
	free(txt_default);
	free(txt_try);
	free(txt_select);
	free(txt_test);
	free(txt_none);
	return res;
}

/**
 * @brief   Test a keymap
 * @arch    keyboard architecture
 * @keymap  keymap name
 * @returns CMD_SUCCESS or CMD_GOBACK, keymap set if SUCCESS
 */

state_t
keymap_test (char *arch, const char *keymap)
{
	char *ptr;
	int res;
	struct debconfclient *client = mydebconf_client ();
	char *kbd_name;

	debconf_fset (client, "kbd-chooser/test_kbd", "seen", "no");
	kbd_name = translate_keyboard_name (arch, keymap, false);
	debconf_subst(client, "kbd-chooser/test_kbd", "kbd", kbd_name);
	debconf_set(client, "kbd-chooser/test_kbd", "");

	res = mydebconf_ask ("high", "kbd-chooser/test_kbd", &ptr);

	free (kbd_name);
	return res;
}

/**
 * @brief   choose a keyboard by selecting from a list
 * @arch    keyboard architecture
 * @keymap  ptr to buffer in which to store chosen keymap name, freed by caller
 * @returns CMD_SUCCESS or CMD_GOBACK, keymap set if SUCCESS
 */

int
keymap_select (char *arch, char **keymap)
{
	char template[50], *ptr;
	int res;

	sprintf (template, "console-keymaps-%s/keymap", arch);

	// If there is a default keymap for this keyboard, select it
	// This is set if we can actually read preferred type from keyboard,
	// and shouldn't have to ask the question.
	ptr = default_keymap (arch);
	if (ptr) {
		keymap_get (maplist_get (arch), ptr, NULL);
		mydebconf_default_set (template, ptr, false);
	}
	res = mydebconf_ask ( "high", template, &ptr);
	if (res != CMD_SUCCESS)
		return res;
        *keymap = strdup((strlen (ptr) == 0) ? "skip-config" : ptr);

	return CMD_SUCCESS;
}

/**
 * @brief   choose a keyboard by pressing a few keys
 * @keymap  ptr to buffer in which to store chosen keymap name, caller has to free this.
 * @returns CMD_SUCCESS or CMD_GOBACK, keymap set if SUCCESS
 */

int
keymap_try (char **keymap)
{
	char *ptr;
	int res;

	res = mydebconf_ask ( "high", "kbd-chooser/try", &ptr);
	if (res != CMD_SUCCESS)
		return res;
        *keymap = ((strlen (ptr) == 0) ? strdup("no-keyboard") : strdup(ptr));

	return CMD_SUCCESS;
}

/**
 *  @brief set the keymap, and debconf database
 */
void
keymap_set (struct debconfclient *client, const char *keymap)
{
	char template[LINESIZE], *arch;

	di_info ("kbd_chooser: setting keymap %s", keymap);
	debconf_set (client, "debian-installer/keymap", keymap);
	// "seen" Used by scripts to decide not to call us again
	// NOTE: not a typo, using 'true' makes things fail. amck!!!
	debconf_fset (client, "debian-installer/keymap", "seen", "yes");

	/* Settings for revisiting this question */
	if (!debconf_get(client, "console-tools/archs") && client->value &&
	    strcmp(client->value, "no-keyboard") != 0) {
                arch = strdup(client->value);
		sprintf (template, "console-keymaps-%s/keymap", arch);
		debconf_set (client, template, keymap);
                free(arch);
	}

        system("kbd-chooser-apply");
}


int
main (int argc, char **argv)
{
	char *kbd_priority, buf[LINESIZE], *arch;
	char *keymap = strdup("no keymap");
        char *tmp;
	state_t state = CHOOSE_METHOD;
	struct debconfclient *client;
	int res;

	setlocale (LC_ALL, "");
	client = mydebconf_client ();
	di_system_init("kbd-chooser"); // enable syslog

	if (argc == 2) { // keymap may be specified on command-line
		keymap_set (client, argv[1]);
		exit (0);
	}

	debconf_capb (client, "backup");
	debconf_version (client,  2);

	read_keymap_files (KEYMAPLISTDIR);
	arch = buf;
	kbd_priority = keyboard_select ("no-keyboard");
	if (debconf_get (client, "console-tools/archs") == CMD_SUCCESS)
		arch = strdup(client->value);

	while (1) {
		switch (state) {
		case GOBACK:
			di_info ("kbdchooser: GOBACK received; leaving");
			exit (10);
		case CHOOSE_METHOD: // Ask how to ask -- or maybe the default is OK.
			res = debconf_get (client, "console-tools/archs");
			if (res) {
				state = GOBACK;
				break;
			}
                        tmp = strdup(client->value);
			state = keymap_ask(tmp, &keymap);
                        free(tmp);
			break;

		case CHOOSE_TRY: // The Press-a-few-keys method
                        if (keymap)
                                free(keymap);
			res = keymap_try (&keymap);
			if (res == CMD_GOBACK) {
				state = CHOOSE_METHOD;
			} else if (res == CMD_INTERNALERROR) {
				state = CHOOSE_ARCH;
			} else if (res) {
				state = GOBACK;
			} else {
				keymap_set (client, keymap);
				state = CHOOSE_METHOD;
			}
			break;

		case CHOOSE_ARCH: // First select a keyboard arch.
			res = mydebconf_ask (kbd_priority, "console-tools/archs", &tmp);
			if (res == CMD_GOBACK) {
				state = CHOOSE_METHOD;
			} else if (res) {
				state = GOBACK;
			} else {
				if (tmp == NULL || (strlen (tmp) == 0)) {
					di_info ("kbd-chooser: not setting keymap (console-tools/archs not set)");
					state = QUIT;
					break;
				}
				di_info ("kbd-chooser: arch %s selected", arch);
				if ((strcmp (arch, "no-keyboard") == 0) ||
				    (strcmp (arch, "skip-config") == 0))	 {
					di_info ("kbd-chooser: not setting keymap");
					state = QUIT;
					break;
				}
				state  = CHOOSE_KEYMAP;
			}
			break;
			
		case CHOOSE_KEYMAP: // Then a keymap within that arch.
                        if (keymap)
                                free(keymap);
			res = keymap_select (arch, &keymap);
			if (res == CMD_GOBACK) {
				state = CHOOSE_ARCH;
			} else if (res) {
				state = CMD_GOBACK;
			} else {
				di_info ("choose_keymap: keymap = %s", keymap);
				keymap_set (client, keymap);
				// state = QUIT;
				state = CHOOSE_METHOD;
			}
			break;

		case CHOOSE_TEST:
			res = debconf_get (client, "console-tools/archs");
			if(res) {
				state = GOBACK;
				break;
			}
                        tmp = strdup(client->value);
			keymap_set (client, keymap);
			res = keymap_test (tmp, keymap);
                        free(tmp);
			if (res == CMD_GOBACK)
				state = CHOOSE_METHOD;
			else if (res)
				state = CMD_GOBACK;
			else
				/* Just returning may be misunderstood. */
				// state = QUIT;
				state = CHOOSE_METHOD;
			break;

		case QUIT:
			keymap_set (client, keymap);
			exit (0);
		case ERROR:
		default:
			exit(1);
		}
	}
	exit (0);
}
