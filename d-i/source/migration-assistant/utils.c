#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

// For gconf utils
#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

// for copyfile and rcopy
#include <sys/mman.h>
#include <unistd.h>

#include "utils.h"
// Taken from comp.lang.c:
// http://groups.google.com/group/comp.lang.c/msg/8d7be5a7387de73f?dmode=source
char *strrep(const char *str, const char *old, const char *new)
{
    char *ret, *r;
    const char *p, *q;
    size_t len_str = strlen(str);
    size_t len_old = strlen(old);
    size_t len_new = strlen(new);
    size_t count;

    for(count = 0, p = str; (p = strstr(p, old)); p += len_old)
        count++;

    ret = malloc(count * (len_new - len_old) + len_str + 1);
    if(!ret)
        return NULL;

    for(r = ret, p = str; (q = strstr(p, old)); p = q + len_old) {
        count = q - p;
        memcpy(r, p, count);
        r += count;
        strcpy(r, new);
        r += len_new;
    }
    strcpy(r, p);
    return ret;

} 
char* reformat_path(const char* from) {
    /* takes a string of the form:
     * C:\Windows\Something
     * and returns the following string that must be free()'d:
     * Windows/Something
     */
    if(strstr(from, "C:\\") == NULL) {
        fprintf(stderr, "Pointed to a location other than C:");
        return NULL;
    }
    /* +1 for \0, -3 for C: */
    char* ret = malloc(strlen(from)-2);
    strcpy(ret, from+3);
    char* tmp = ret;
    while(*tmp != '\0') {
        if(*tmp == '\\') *tmp = '/';
        tmp++;
    }
    return ret;
}
// Modified from Advanced Programming in the Unix Environment.
void copyfile(const char* from, const char* to) {
    int         fdin, fdout;
    void        *src, *dst;
    struct stat statbuf;
    const int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    create_file(to);

    if ((fdin = open(from, O_RDONLY)) < 0) {
        printf("can't open %s for reading\n", from);
	    exit(EXIT_FAILURE);
    }
    // if file exists, check to see if the files are the same (byte by byte,
    // returning on the first inconsistant byte) and if not append a .1 at the
    // end (followed by .2, .3, etc).

    if ((fdout = open(to, O_RDWR | O_CREAT | O_TRUNC, mode)) < 0) {
        printf("can't creat %s for writing\n", to);
	    exit(EXIT_FAILURE);
    }

    if (fstat(fdin, &statbuf) < 0) {  /* need size of input file */
        puts("fstat error");
	    exit(EXIT_FAILURE);
    }
    if(statbuf.st_size == 0)
        return;

    /* set size of output file */
    if (lseek(fdout, statbuf.st_size - 1, SEEK_SET) == -1) {
        puts("lseek error");
	    exit(EXIT_FAILURE);
    }
    if (write(fdout, "", 1) != 1) {
        puts("write error");
	    exit(EXIT_FAILURE);
    }

    if ((src = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED,
      fdin, 0)) == MAP_FAILED) {
        puts("mmap error for input");
	exit(EXIT_FAILURE);
    }

    if ((dst = mmap(0, statbuf.st_size, PROT_READ | PROT_WRITE,
      MAP_SHARED, fdout, 0)) == MAP_FAILED) {
        puts("mmap error for output");
	exit(EXIT_FAILURE);
    }

    memcpy(dst, src, statbuf.st_size); /* does the file copy */
}

void rcopy(const char* from, const char* to) {
    struct dirent *de;
    DIR *d;
    char* fpn, *tpn;
    char* extension = NULL;

    d = opendir(from);
    if(!d) {
	printf("could not open dir, %s\n", from);
	exit(EXIT_FAILURE);
    }

    mkdir(to, 0755);

    while((de = readdir(d)) != NULL) {
        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        asprintf(&fpn, "%s/%s", from, de->d_name);
        asprintf(&tpn, "%s/%s", to, de->d_name);

        if(de->d_type == DT_REG) {
            extension = de->d_name;
            while(*extension != '\0') extension++;
            while(extension != de->d_name && *extension != '.') extension--;
            if(extension == de->d_name) extension = NULL;
            else extension++;

            // TODO: Make a array of ignored extensions instead.
            if(!((strcmp(extension, "ini") == 0)
                || (strcmp(extension, "lnk") == 0))) {
                    copyfile(fpn, tpn);
            }
	    } else if(de->d_type == DT_DIR) {
	        mkdir(tpn, 0755); // I think we can axe this.  See above.
    	    rcopy(fpn, tpn);
    	}
        free(fpn);
        free(tpn);
    }
    closedir(d);
}

/* taken from partconf/partconf.c */
void makedirs(const char *dir)
{
    DIR *d;
    char *dirtmp, *basedir;

    if ((d = opendir(dir)) != NULL) {
        closedir(d);
        return;
    }
    if (mkdir(dir, 0755) < 0) {
        dirtmp = strdup(dir);
        basedir = dirname(dirtmp);
        makedirs(basedir);
        free(dirtmp);
        mkdir(dir, 0755);
    }
}

void create_file(const char* file) {
    FILE* fp;
    char* tmp;
    char* filename;

    if((fp = fopen(file, "r")) != NULL) {
	fclose(fp);
	return;
    }

    tmp = strdup(file);
    filename = dirname(tmp);
    makedirs(filename);
    free(tmp);

    creat(file, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}
void add_wallpaper (const char* path) {
    xmlDoc* doc = NULL;
    xmlNode* root, *wallpaper, *ptr = NULL;
    char* file, *name, *orig_name;
    const int opts = XML_PARSE_NOBLANKS | XML_PARSE_NOERROR | XML_PARSE_RECOVER;

    orig_name = strdup(path);
    name = basename(orig_name);

    asprintf(&file, "%s/home/%s/.gnome2/backgrounds.xml", to_location, to_user);
    create_file(file);
    doc = xmlReadFile(file, NULL, opts);
    root = xmlDocGetRootElement(doc);

    if(!root) {
	// FIXME: set DTD.
	root = xmlNewNode(NULL, (xmlChar*) "wallpapers");
	xmlDocSetRootElement(doc, root);
    } else {
	wallpaper = root->children;
	while(wallpaper != NULL) {
	    ptr = wallpaper->children;
	    while(ptr != NULL) {
		if((xmlStrcmp(ptr->name, (xmlChar*) "filename") == 0) &&
			(xmlStrcmp(ptr->children->content, (xmlChar*) path) == 0)) {
		    return; // Already set.
		}
		ptr = ptr->next;
	    }
	    wallpaper = wallpaper->next;
	}

    }
    
    wallpaper = xmlNewChild(root, NULL, (xmlChar*) "wallpaper", NULL);
    xmlNewProp(wallpaper, (xmlChar*) "deleted", (xmlChar*) "false");
    xmlNewTextChild(wallpaper, NULL, (xmlChar*) "name", (xmlChar*) name);
    xmlNewTextChild(wallpaper, NULL, (xmlChar*) "filename", (xmlChar*) path);
    xmlNewTextChild(wallpaper, NULL, (xmlChar*) "options", (xmlChar*) "scaled");
    free(orig_name);

    xmlSaveFormatFile(file, doc, 1);
    free(file);
}
// yikes, get rid of this.
void makegconfdirs(const char *dir)
{
    DIR *d;
    char *dirtmp, *basedir, *gconf;

    if ((d = opendir(dir)) != NULL) {
        closedir(d);
        return;
    }
    if (mkdir(dir, 0755) < 0) {
        dirtmp = strdup(dir);
        basedir = dirname(dirtmp);
        makegconfdirs(basedir);
        free(dirtmp);

        mkdir(dir, 0755);
	
    }
    dirtmp = strdup(dir);
    basedir = basename(dirtmp);
    if(strcmp(basedir,".gconf") != 0) {
	asprintf(&gconf, "%s/%%gconf.xml", dir);
	creat(gconf, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	free(gconf);
    }
    free(dirtmp);
}


void set_gconf_key (const char* path, const char* name, gconf_type type, const char* value) {
    char* file, *gconf;
    xmlDoc* doc = NULL;
    xmlNode* root, *entry = NULL;
    const int opts = XML_PARSE_NOBLANKS | XML_PARSE_NOERROR | XML_PARSE_RECOVER;

    asprintf(&gconf, "%s/home/%s/.gconf/%s", to_location, to_user, path);
    asprintf(&file, "%s/%%gconf.xml", gconf);

    makegconfdirs(gconf);
    free(gconf);
    create_file(file);

    doc = xmlReadFile(file, NULL, opts);
    root = xmlDocGetRootElement(doc);
    if(!root) {
	root = xmlNewNode(NULL, (xmlChar*) "gconf");
	xmlDocSetRootElement(doc, root);
	entry = xmlNewChild(root, NULL, (xmlChar*) "entry", NULL);
	xmlNewProp(entry, (xmlChar*) "name", (xmlChar*) name);
    } else {
	entry = root->children;
	while(entry != NULL) {
	    if(xmlStrcmp(xmlGetProp(entry, (xmlChar*) "name"),(xmlChar*) name) == 0)
		break;
	    entry = entry->next;
	}
	if(!entry) {
	    entry = xmlNewChild(root, NULL, (xmlChar*) "entry", NULL);
	    xmlNewProp(entry, (xmlChar*) "name", (xmlChar*) name);
	}
    }


    if(type == GCONF_STRING) {

	xmlSetProp(entry, (xmlChar*) "type", (xmlChar*) "string");
	if(entry->children == NULL)
	    xmlNewTextChild(entry, NULL, (xmlChar*) "stringvalue", (xmlChar*) value);
	else
	    xmlNodeSetContent(entry->children, (xmlChar*) value);

    } else if (type == GCONF_BOOLEAN) {

	xmlSetProp(entry, (xmlChar*) "type", (xmlChar*) "bool");
	xmlSetProp(entry, (xmlChar*) "value", (xmlChar*) value);

    } else if (type == GCONF_LIST_STRING) {
	xmlNode* li, *sv = NULL;
	xmlSetProp(entry, (xmlChar*) "type", (xmlChar*) "list");
	xmlSetProp(entry, (xmlChar*) "ltype", (xmlChar*) "string");

	if(entry->children == NULL)
	    li = xmlNewChild(entry, NULL, (xmlChar*) "li", NULL);
	else
	    li = entry->children;

	xmlSetProp(li, (xmlChar*) "type", (xmlChar*) "string");
	sv = li->children;
	while(sv != NULL) {
	    // Already exists.
	    if(xmlStrcmp(sv->children->content, (xmlChar*) value) == 0)
		return;
	    sv = sv->next;
	}
	xmlNewTextChild(li, NULL, (xmlChar*) "stringvalue", (xmlChar*) value);
    }

    xmlSaveFormatFile(file, doc, 1);

}
