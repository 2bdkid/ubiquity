/* The data structures for the Windows NT registry and some of the code within
 * this source file was written by Aaron D. Brooks for the BeeHive project
 * (http://sourceforge.net/project/?group_id=1987) and licensed under the GPL
 * v2.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "registry.h"

char* findkey(const char* location, const char* path)
{

    FILE *hive;
    int numread;
    REGF myregf;
    char* hbinptr;

    char* ret;
    // FIXME
    char* arr;
    arr = strdup(path);

    hive = fopen(location,"r");
    if(hive)
    {		      
	    numread = fread(&myregf,sizeof(char),sizeof(REGF),hive);

	    char* key;
	    if((key = strrchr(arr, '\\')) != NULL){
		*key++ = '\0';
	    }

	    hbinptr = (char*)malloc(myregf.hivesize);
	    if(hbinptr)
	    {
		    numread = fread(hbinptr,sizeof(char),myregf.hivesize,hive);
		    
		    NK* back = getkey(hbinptr,(NK*)&(hbinptr[myregf.firstkey]), &arr);
		    if(back != NULL) {
			ret = printk(hbinptr, back, key);
		    }
		    free(hbinptr);
		//    if(key)
		//	free(key);
	    }

    } else {
	puts("Unable to open the registry file.");
	return NULL;
    }
    fclose(hive);

    if(ret)
	return ret;
    else
	return NULL;
}

char* printk(char* base, NK* thisnk, char* key) {
    int i;
    int* valptr;
	if(thisnk->numvalues > 0)
	{
		valptr=(int*) &(base[thisnk->myvallist]);
		for(i=1; i<thisnk->numvalues+1; i++)
		{
			if(valptr[i]>0)
			{
			    	VK* thisvk = (VK*) &(base[valptr[i]]);
				char* szname;
				//int vksize=sizeof(VK)+thisvk->namesize;

				szname = (char*) malloc (thisvk->namesize+1);
				strncpy(szname,(char*)(thisvk+1),thisvk->namesize+1);
				szname[thisvk->namesize]= 0;
				if(strcmp(szname,key) == 0){
				    if(thisvk->flag && 0x0000000000000001){
					// we only deal with strings for now.
					// FIXME: We need to support other
					// types, see notes.
					    if (thisvk->valtype == REG_SZ){
						char* tmp;
						int j = 0;
						tmp = malloc(thisvk->datasize-2);
						for(i=0; i<thisvk->datasize-2; i+=2) {
						    tmp[j] = (char)(base+4+(thisvk->mydata))[i];
						    j++;
						}
						tmp[j] = '\0';

						return tmp;
					    }
				    }
				}
				free(szname);

			}
		}
	}
	return NULL;
}

NK* getkey(char* base, NK* thisnk, char** path) {
    NK* tmp;
    LF* lfrec = (LF*) &(base[thisnk->mylfrec]);
    HASH* hasharray = (HASH*) lfrec +1;
    int i;
    char* element;

    if((element = str_token(path, "\\")) != NULL) {

	if(thisnk->numchildren > 0) {
	    for(i=0; i<thisnk->numchildren; i++) {
		tmp = (NK*)&(base[(hasharray+i)->nkrec]);

		char* szname;
		szname = (char*) malloc (tmp->namesize+1);
		strncpy(szname,(char*)(tmp+1),tmp->namesize+1);
		szname[tmp->namesize]= 0;

		if(strcmp(szname,element) == 0){
		    thisnk = getkey(base, tmp, path);
		    return thisnk;
		}
		free(szname);
	    }
	    return NULL;

	}

	free(element);
    }
    return thisnk;
}

/* Reentrant strtok, taken from a Usenet post:
 * http://groups.google.com/group/comp.lang.c/msg/9e92ba098baa55ac?dmode=source
 */
char *str_token( char **string, char *delimiters ) {

    char *rv, *wrk, empty[] = "";

    /******************************************************************/
    /* Beware the NULL pointer !                                      */
    /******************************************************************/

    if ( delimiters == NULL )
        delimiters = empty;

    if ( string == NULL || *string == NULL || **string == '\0' )
        rv = NULL;

    /******************************************************************/
    /* If there are no delimiters the string is the token.            */
    /******************************************************************/

    else if ( ( wrk = strpbrk( *string, delimiters ) ) == NULL ) {
        rv = *string;
        *string += strlen( rv );
    }

    /******************************************************************/
    /* Skip past any delimiters at the beginning of the string.       */
    /******************************************************************/

    else if ( wrk == *string ) {
        *string += strspn( wrk, delimiters );
        rv = str_token( string, delimiters );
    }

    /******************************************************************/
    /* Got a token! Null terminate it and prep for the next call.     */
    /******************************************************************/

    else {
        *wrk++ = '\0';
        rv = *string;
        *string = wrk + strspn( wrk, delimiters );
    }
    return rv;

}
