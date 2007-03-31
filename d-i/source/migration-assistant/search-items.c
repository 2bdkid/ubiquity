#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <dirent.h>

#include "registry.h"
#include "utils.h"

const char* mount_location;
const char* user;


/*		Utility Functions			*/

char* windows_get_user_registry(void) {
    char* ret;
    asprintf(&ret, "%s/%s/%s/%s", mount_location,
	    "Documents and Settings", user, "NTUSER.DAT");

    return ret;
}

char* gaim_get_accounts_file_windows(void) {
    char* filename, *accounts_file;
    char* appdata = NULL;
    char* path = NULL;
    filename = windows_get_user_registry();
    appdata = findkey(filename, "\\Software\\Microsoft\\Windows\\"
        "CurrentVersion\\Explorer\\Shell Folders\\Local AppData");
    free(filename);
    if(!appdata) {
        return NULL;
    }
    path = reformat_path(appdata);
    if(!path) return NULL;
    free(appdata);
    asprintf(&accounts_file, "%s/%s/%s", mount_location, path,
        "/.gaim/accounts.xml");
    free(path);

    return accounts_file;
}

char* gaim_get_accounts_file_linux(void) {
    char* ret;
    asprintf(&ret, "%s/%s/%s/%s", mount_location,
	    "home", user,
	    ".gaim/accounts.xml");

    return ret;
}

/*		Windows Applications			*/

/*		Instant Messaging			*/

const char* windowsxp_yahoo (void) {
    // Doesn't account for more than one account.
    // Doesn't matter here, but just don't do this at the import stage.
    char* registry_location;
    char* registry_key;

    registry_location = windows_get_user_registry();
    registry_key = findkey(registry_location,
	    "\\Software\\Yahoo\\pager\\Yahoo! User ID");

    if(registry_location)
	free(registry_location);

    if(registry_key) {
	free(registry_key);
	return "Yahoo";
    } else {
	return NULL;
    }

}

const char* windowsxp_gaim (void) {
    char* accounts_file;
    FILE* fp;
    accounts_file = gaim_get_accounts_file_windows();
    if((fp = fopen(accounts_file, "r")) != NULL) {
	fclose(fp);
	return "Gaim";
    } else {
	return NULL;
    }
}

const char* windowsxp_msn (void) {
    return NULL;
}

const char* windowsxp_aim_triton (void) {
/* Any directory in
 * Documents and Settings\User\Local Settings\Application Data\AOL\UserProfiles
 * aside from "All Users" is an account.
 */

    DIR* dir;
    struct dirent *entry;
    char* dirname, *filename;
    char* appdata = NULL;
    char* path = NULL;

    filename = windows_get_user_registry();
    appdata = findkey(filename, "\\Software\\Microsoft\\Windows\\"
        "CurrentVersion\\Explorer\\Shell Folders\\Local AppData");
    free(filename);
    if(!appdata) {
        return NULL;
    }
    path = reformat_path(appdata);
    if(!path) return NULL;
    free(appdata);
    asprintf(&dirname, "%s/%s/%s", mount_location, path, "AOL/UserProfiles");
    free(path);
    dir = opendir(dirname);
    if(!dir) return NULL;
    free(dirname);

    while((entry = readdir(dir)) != NULL) {
        if(entry->d_type == DT_DIR) {
            if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0)
                continue;
            if(strcmp(entry->d_name,"All Users") != 0)
                return "AIM Triton";
        }
    }
    return NULL;
}

/*		Web Browsers				*/

const char* windowsxp_opera (void) {
    char* filename;
    char* appdata = NULL;
    char* path = NULL;

    filename = windows_get_user_registry();
    appdata = findkey(filename, "\\Software\\Microsoft\\Windows\\"
        "CurrentVersion\\Explorer\\Shell Folders\\Local AppData");
    free(filename);
    if(!appdata) {
        return NULL;
    }
    path = reformat_path(appdata);
    if(!path) return NULL;
    free(appdata);
    asprintf(&filename, "%s/%s/%s", mount_location, path,
        "Opera/Opera/profile/opera6.adr");
    free(path);

    FILE* fp;
    if((fp = fopen(filename, "r")) != NULL) {
	fclose(fp);
        return "Opera";
    } else {
        return NULL;
    }
}

const char* windowsxp_firefox (void) {

    DIR* dir;
    struct dirent *entry;
    char* dirname, *filename;
    char* appdata = NULL;
    char* path = NULL;

    filename = windows_get_user_registry();
    appdata = findkey(filename, "\\Software\\Microsoft\\Windows\\"
        "CurrentVersion\\Explorer\\Shell Folders\\Local AppData");
    free(filename);
    if(!appdata) {
        return NULL;
    }
    path = reformat_path(appdata);
    if(!path) return NULL;
    free(appdata);
    asprintf(&dirname, "%s/%s/%s", mount_location, path, "Mozilla/Firefox/Profiles");
    free(path);
    dir = opendir(dirname);
    if(!dir) return NULL;
    free(dirname);

    while((entry = readdir(dir)) != NULL) {
	if(entry->d_type == DT_DIR) {
            if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0)
                continue;
	    return "Mozilla Firefox";
	}
    }
    return NULL;
}

const char* windowsxp_iexplorer (void) {
    DIR* dir;
    struct dirent *entry;
    char* filename, *path, *iedir;
    char* favorites = NULL;
    filename = windows_get_user_registry();
    favorites = findkey(filename, "\\Software\\Microsoft\\Windows\\"
        "CurrentVersion\\Explorer\\Shell Folders\\Favorites");
    if(!favorites) return NULL;
    path = reformat_path(favorites);
    if(!path) return NULL;
    free(favorites);
    asprintf(&iedir, "%s/%s", mount_location, path);
    free(path);
    dir = opendir(iedir);
    if(!dir) return NULL;
    free(iedir);

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0)
            continue;
        if(strcmp(entry->d_name, "Desktop.ini") == 0)
            continue;
        return "Internet Explorer";
    }
    return NULL;
}

/*		Windows Settings			*/

const char* windowsxp_wallpaper (void) {
    return "Wallpaper";
}

const char* windowsxp_userpicture (void) {
    char* filename = NULL;
    char* appdata = NULL;
    char* path = NULL;
    char* from = NULL;
    FILE* fp;

    // FIXME: what about WINNT?
    asprintf(&filename, "%s/WINDOWS/system32/config/software", mount_location);
    appdata = findkey(filename, "\\Microsoft\\Windows\\CurrentVersion\\"
        "Explorer\\Shell Folders\\Common AppData");
    if(!appdata)
        return NULL;
    path = reformat_path(appdata);
    if(!path) return NULL;
    free(appdata);
    asprintf(&from, "%s/%s/Microsoft/User Account Pictures/%s.bmp",
        mount_location, path, user);
    free(path);

    if((fp = fopen(from, "r")) == NULL)
        return NULL;
    else
        return "User Picture";
}

const char* windowsxp_mydocuments (void) {
    // Only return success if there's more than the default in there.
    return "My Documents";
}

const char* windowsxp_mymusic (void) {
    // Only return success if there's more than the default in there.
    return "My Music";
}

const char* windowsxp_mypictures (void) {
    // Only return success if there's more than the default in there.
    return "My Pictures";
}

const char* windowsxp_proxy (void) {
    // Need to add REG_DWORD support to the registry utils.
    char* registry_location;
    char* registry_key;

    registry_location = windows_get_user_registry();
    registry_key = findkey(registry_location,
	    "\\Software\\Microsoft\\Windows\\CurrentVersion\\"
	    "Internet Settings\\ProxyEnable");

    if(registry_key && (strcmp(registry_key, "0x00000001") == 0))
	return "Proxy";
    else
	return NULL;
}

const char* windowsxp_outlookexpress (void) {
    char* registry_location;
    char* registry_key = NULL;

    registry_location = windows_get_user_registry();
    registry_key = findkey(registry_location,
        "\\Software\\Microsoft\\Internet Account Manager\\Accounts\\00000001"
        "\\Account Name");
    if(registry_key)
        return "Outlook Express";
    else
        return NULL;
}

/*		Linux Applications			*/

const char* linux_gaim (void) {
    char* accounts_file;
    FILE* fp;
    accounts_file = gaim_get_accounts_file_linux();
    if((fp = fopen(accounts_file, "r")) != NULL) {
        fclose(fp);
        return "Gaim";
    } else {
        return NULL;
    }
}

const char* linux_firefox (void) {

    DIR* dir;
    struct dirent *entry;
    char* dirname;

    asprintf(&dirname, "%s/%s/%s/%s", mount_location, "home",
	    user, ".mozilla/firefox");
    dir = opendir(dirname);
    if(!dir) return NULL;
    free(dirname);

    while((entry = readdir(dir)) != NULL) {
	if(entry->d_type == DT_DIR) {
            if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0)
                continue;
	    return "Mozilla Firefox";
	}
    }
    return NULL;
}

const char* linux_evolution (void) {
    FILE* fp = NULL;
    char* file = NULL;
    asprintf(&file, "%s/%s/%s/%s", mount_location, "home", user,
        ".gconf/apps/evolution/mail/\%gconf.xml");
    if((fp = fopen(file, "r")) != NULL) {
	fclose(fp);
        free(file);
	return "Evolution";
    } else {
        free(file);
	return NULL;
    }
}

void usage(void) {
    puts("USAGE: --path=\"PATH\" --user=\"USER\" --ostype=\"OSTYPE\"");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    
    if(argc < 4) usage();

    enum { WINDOWSXP, LINUX };
    int ostype;

    int test = 0;
    int passed = 0;
    const char* ret;
    const char* (**tests)();

    const char* (*windowsxp_tests[])() = {
	windowsxp_yahoo,
	windowsxp_gaim,
	windowsxp_msn,
	windowsxp_aim_triton,
	windowsxp_opera,
	windowsxp_firefox,
	windowsxp_iexplorer,
	windowsxp_wallpaper,
	windowsxp_userpicture,
	windowsxp_mydocuments,
	windowsxp_mymusic,
	windowsxp_mypictures,
	windowsxp_proxy,
        windowsxp_outlookexpress,
	NULL,
    };

    const char* (*linux_tests[])() = {
	linux_gaim,
        linux_firefox,
        linux_evolution,
	NULL,
    };

    static struct option long_options[] = {
	{ "path", required_argument, NULL, 'p' },
	{ "ostype", required_argument, NULL, 'o' },
	{ "user", required_argument, NULL, 'u' },
	{ NULL, 0, NULL, 0 }
    };

    int c;

    while(1) {
	c = getopt_long_only(argc, argv, "", long_options, NULL);
	if(c == -1) break;
	switch(c) {
	    case 'o' :
		if(strcmp(optarg, "linux") == 0) ostype = LINUX;
		else if(strcmp(optarg, "windowsxp") == 0) ostype = WINDOWSXP;
		else usage();
		break;
	    case 'p' :
		mount_location = strdup(optarg);
		break;
	    case 'u' :
		user = strdup(optarg);
		break;
	    default:
		usage();
	}
    }

    if(ostype == LINUX)
	tests = linux_tests;
    else if(ostype == WINDOWSXP)
	tests = windowsxp_tests;

    //fflush(stderr);
    //if (!freopen("/dev/null", "w", stderr)) {
    //    fprintf(stderr, "Failed to redirect stderr to /dev/null");
    //    return EXIT_FAILURE;
    //}

    while(tests[test]) {
	ret = tests[test]();
        if(ret) {
            
            if(passed)
                printf(", ");
            
            printf(ret);
            passed = 1;
        }
	test++;
    }
    if(passed)
        putchar('\n');


    return 0;
}
// vim:ai:et:sts=4:tw=80:sw=4:
