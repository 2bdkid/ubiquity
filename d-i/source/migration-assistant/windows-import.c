#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "windows-import.h"
#include "utils.h"
#include "registry.h"

const char* mydocumentskey = "\\Software\\Microsoft\\Windows\\CurrentVersion"
    "\\Explorer\\Shell Folders\\Personal";
const char* mymusickey = "\\Software\\Microsoft\\Windows\\CurrentVersion"
    "\\Explorer\\Shell Folders\\My Music";
const char* mypictureskey = "\\Software\\Microsoft\\Windows\\CurrentVersion"
    "\\Explorer\\Shell Folders\\My Pictures";


// FIXME: gnome-about-me also adds this to EDS.
void windowsxp_import_userpicture (void) {
    char *from, *to;
    char* filename = NULL;
    char* appdata = NULL;
    char* path = NULL;

    // FIXME: what about WINNT?
    asprintf(&filename, "%s/WINDOWS/system32/config/software", from_location);
    appdata = findkey(filename, "\\Microsoft\\Windows\\CurrentVersion\\"
        "Explorer\\Shell Folders\\Common AppData");
    if(!appdata) {
        puts("Couldn't find Common AppData");
        return;
    }
    path = reformat_path(appdata);
    free(appdata);
    asprintf(&from, "%s/%s/Microsoft/User Account Pictures/%s.bmp",
        from_location, path, from_user);
    free(path);

    asprintf(&to, "%s/home/%s/.face", to_location, to_user);

    copyfile(from, to);
    free(from);
    free(to);
}

// It appears that we need to set this in both gconf and Firefox's prefs.js.
void windowsxp_import_proxy (void) { return; }

void windowsxp_import_mymusic (void) {
    char* to, *from;
    char* mymusic = NULL;
    char* filename = NULL;
    char* path = NULL;
    
    asprintf(&filename, "%s/%s/%s/%s", from_location,
	    "Documents and Settings", from_user, "NTUSER.DAT");
    mymusic = findkey(filename, mymusickey);
    if(!mymusic) {
        printf("Couldn't find %s\n", mymusickey);
        return;
    }
    path = reformat_path(mymusic);
    free(mymusic);
    asprintf(&to, "%s/home/%s/Music", to_location, to_user);
    asprintf(&from, "%s/%s", from_location, path);
    free(path);

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
    char* filename = NULL;
    char* mypictures = NULL;
    char* path = NULL;

    asprintf(&filename, "%s/%s/%s/%s", from_location,
	    "Documents and Settings", from_user, "NTUSER.DAT");
    mypictures = findkey(filename, mypictureskey);
    if(!mypictures) {
        printf("Couldn't find %s\n", mypictureskey);
        return;
    }
    path = reformat_path(mypictures);
    free(mypictures);

    asprintf(&to, "%s/home/%s/Pictures", to_location, to_user);
    asprintf(&from, "%s/%s", from_location, path);
    free(path);

    rcopy(from, to);
    free(to);
    free(from);
}

void windowsxp_import_mydocuments (void) {
    char *to, *from, *f, *t;
    DIR *d;
    struct dirent *rep;
    char* mydocuments = NULL;
    char* path = NULL;
    char* filename = NULL;
    char* extension = NULL;


    asprintf(&filename, "%s/%s/%s/%s", from_location,
	    "Documents and Settings", from_user, "NTUSER.DAT");
    mydocuments = findkey(filename, mydocumentskey);
    if(!mydocuments) {
        printf("Couldn't find %s\n", mydocumentskey);
        return;
    }
    path = reformat_path(mydocuments);
    free(mydocuments);

    char* mypictures = NULL;
    char* mymusic = NULL;
    mypictures = findkey(filename, mypictureskey);
    if(!mypictures) {
        printf("Couldn't find %s\n", mypictureskey);
        return;
    }
    mymusic = findkey(filename, mymusickey);
    if(!mymusic) {
        printf("Couldn't find %s\n", mymusickey);
        return;
    }
    const char* pic = mypictures;
    const char* mus = mymusic;
    while(*pic != '\0') pic++;
    while(*pic != '\\') pic--;
    pic++;
    while(*mus != '\0') mus++;
    while(*mus != '\\') mus--;
    mus++;

    asprintf(&to, "%s/home/%s/Documents", to_location, to_user);
    asprintf(&from, "%s/%s", from_location, path);
    free(path);

    FILE* fp;
    if((fp = fopen(from, "r")) == NULL) {
        printf("%s does not exist.\n", from);
        return;
    }
    fclose(fp);

    makedirs(to);

    d = opendir(from);
    while((rep = readdir(d)) != NULL) {
        if(strcmp(rep->d_name,".") == 0 || strcmp(rep->d_name,"..") == 0)
            continue;
        else if(strcmp(rep->d_name, pic) == 0 ||
		strcmp(rep->d_name,mus) == 0) {
            continue;
	} else {
	    asprintf(&f, "%s/%s", from, rep->d_name);
	    //mkdir(to, 0755);
	    mkdir(t, 0755); // Test to see if this is needed.
	    asprintf(&t, "%s/%s", to, rep->d_name);
	    if(rep->d_type == DT_REG) {
            extension = rep->d_name;
            while(*extension != '\0') extension++;
            while(extension != rep->d_name && *extension != '.') extension--;
            if(extension == rep->d_name) extension = NULL;
            else extension++;

            // TODO: Make a array of ignored extensions instead.
            if(!((strcmp(extension, "ini") == 0)
                || (strcmp(extension, "lnk") == 0))) {
		            copyfile(f,t);
            }
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
    free(mypictures);
    free(mymusic);
}

void windowsxp_import_wallpaper (void) {
    char *to, *from, *image, *path;
    char* filename = NULL;
    char* wallpaperloc = NULL;
    
    asprintf(&filename, "%s/%s/%s/%s", from_location,
	    "Documents and Settings", from_user, "NTUSER.DAT");
    wallpaperloc = findkey(filename,
        "\\Control Panel\\Desktop\\ConvertedWallpaper");
    if(!wallpaperloc) {
        wallpaperloc = findkey(filename,
            "\\Control Panel\\Desktop\\Wallpaper");
    }
    if(!wallpaperloc) {
        printf("Couldn't find %s\n", wallpaperloc);
        return;
    }
    path = reformat_path(wallpaperloc);
    free(wallpaperloc);
    image = basename(path);
    asprintf(&from, "%s/%s", from_location, path);
    asprintf(&to, "%s/home/%s/%s", to_location, to_user, image);

    copyfile(from, to);
    free(from);
    free(to);
    
    asprintf(&to, "/home/%s/%s", to_user, image);
    free(path);
    set_gconf_key("/desktop/gnome/background", "picture_filename", GCONF_STRING, to);
    add_wallpaper(to);
    free(to);
}

