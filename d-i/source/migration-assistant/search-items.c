#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <dirent.h>

#include "registry.h"

const char* mount_location;
const char* user;


/*		Utility Functions			*/
/* These really belong in a header file somewhere.	*/
/* (utils.c and utils.h)				*/
/* At which point they should take arguments, not	*/
/* assume that mount_location and user exist.		*/

char* windows_get_user_registry(void) {
    char* ret;
    asprintf(&ret, "%s/%s/%s/%s", mount_location,
	    "Documents and Settings", user, "NTUSER.DAT");

    return ret;
}

char* gaim_get_accounts_file_windows(void) {
    char* ret;
    asprintf(&ret, "%s/%s/%s/%s", mount_location,
	    "Documents and Settings", user,
	    "Application Data/.gaim/accounts.xml");

    return ret;
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
    char* dirname;

    asprintf(&dirname, "%s/%s/%s/%s", mount_location, "Documents and Settings",
	    user, "Local Settings/Application Data/AOL/UserProfiles");
    dir = opendir(dirname);
    if(!dir) return NULL;
    free(dirname);

    while((entry = readdir(dir)) != NULL) {
	if(entry->d_type == DT_DIR) {
	    if(strcmp(entry->d_name,"All Users") != 0)
		return "AIM Triton";
	}
    }
    return NULL;
}

/*		Web Browsers				*/

const char* windowsxp_opera (void) {
    char* filename;
    asprintf(&filename, "%s/%s/%s/%s", mount_location, "Documents and Settings",
	    user, "Application Data/Opera/Opera/profile/opera6.adr");
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
    char* dirname;

    asprintf(&dirname, "%s/%s/%s/%s", mount_location, "Documents and Settings",
	    user, "Application Data/Mozilla/Firefox/Profiles");
    dir = opendir(dirname);
    if(!dir) return NULL;
    free(dirname);

    while((entry = readdir(dir)) != NULL) {
	if(entry->d_type == DT_DIR) {
	    return "Mozilla Firefox";
	}
    }
    return NULL;
}

const char* windowsxp_iexplorer (void) {
    return "Internet Explorer";
}

/*		Windows Settings			*/

const char* windowsxp_wallpaper (void) {
    // Copyright issue?
    // If so, check to make sure it's not a default.
    return "Wallpaper";
}

const char* windowsxp_userpicture (void) {
    // Copyright issue?
    // If so, check to make sure it's not a default.
    // What does Microsoft call it?
    // Doesn't necessarily exist.
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
	NULL,
    };

    const char* (*linux_tests[])() = {
	linux_gaim,
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
