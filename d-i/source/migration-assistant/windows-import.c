#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "windows-import.h"
#include "utils.h"


// FIXME: gnome-about-me also adds this to EDS.
void windowsxp_import_userpicture (void) {
    char *from, *to;

    asprintf(&from, "%s/Documents and Settings/All Users/Application Data"
	    "/Microsoft/User Account Pictures/%s.bmp", from_location,
	    from_user);

    asprintf(&to, "%s/home/%s/.face", to_location, to_user);

    copyfile(from, to);
    free(from);
    free(to);
}

// It appears that we need to set this in both gconf and Firefox's prefs.js.
void windowsxp_import_proxy (void) { return; }

void windowsxp_import_mymusic (void) {
    char* to, *from;

    asprintf(&to, "%s/home/%s/Music", to_location, to_user);
    asprintf(&from, "%s/Documents and Settings/%s/My Documents/My Music",
	    from_location, from_user);

    rcopy(from, to);
    free(from);
    free(to);
    
    asprintf(&to, "file:///home/%s/Music", to_user);
    set_gconf_key("/apps/rhythmbox", "library_locations", GCONF_LIST_STRING, to);
    set_gconf_key("/apps/rhythmbox", "first_time_flag", GCONF_BOOLEAN, "true");
    set_gconf_key("/apps/rhythmbox", "monitor_library", GCONF_BOOLEAN, "true");
}

void windowsxp_import_mypictures (void) {
    char *to, *from;

    asprintf(&to, "%s/home/%s/Pictures", to_location, to_user);
    asprintf(&from, "%s/Documents and Settings/%s/My Documents/My Pictures",
	    from_location, from_user);

    rcopy(from, to);
    free(to);
    free(from);
}

void windowsxp_import_mydocuments (void) {
    char *to, *from, *f, *t;
    DIR *d;
    struct dirent *rep;

    asprintf(&to, "%s/home/%s/Documents", to_location, to_user);
    asprintf(&from, "%s/Documents and Settings/%s/My Documents", from_location,
	    from_user);

    makedirs(to);

    d = opendir(from);
    while((rep = readdir(d)) != NULL) {
        if(strcmp(rep->d_name,".") == 0 || strcmp(rep->d_name,"..") == 0)
            continue;
        else if(strcmp(rep->d_name,"My Music") == 0 ||
		strcmp(rep->d_name,"My Pictures") == 0) {
            continue;
	} else {
	    asprintf(&f, "%s/%s", from, rep->d_name);
	    //mkdir(to, 0755);
	    mkdir(t, 0755); // Test to see if this is needed.
	    asprintf(&t, "%s/%s", to, rep->d_name);
	    if(rep->d_type == DT_REG) {
		copyfile(f,t);
	    } else if(rep->d_type == DT_DIR) {
		rcopy(f,t);
	    }
	    free(t);
	    free(f);
	}
    }

    closedir(d);

    free(to);
    free(from);
}

void windowsxp_import_wallpaper (void) {

    // FIXME: copy the .jpg instead.  The plus is we get a somewhat unique
    // filename.
    char *to, *from;
    
    asprintf(&to, "%s/home/%s/wallpaper.bmp", to_location, to_user);

    asprintf(&from, "%s/Documents and Settings/%s/Local Settings"
	    "/Application Data/Microsoft/Wallpaper1.bmp", from_location,
	    from_user);

    copyfile(from, to);
    free(to);
    free(from);
    
    asprintf(&to, "/home/%s/wallpaper.bmp", to_user);
    set_gconf_key("/desktop/gnome/background", "picture_filename", GCONF_STRING, to);
    add_wallpaper(to);
    free(to);
}

