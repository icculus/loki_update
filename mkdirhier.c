
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "mkdirhier.h"

/* Create the directories in the heirarchy above this path, if necessary */
int mkdirhier(const char *path)
{
	int retval;
	char *bufp, *new_path;
	struct stat sb;

	retval = 0;
	if ( path && *path ) {
		new_path = strdup(path);
		for ( bufp=new_path+1; *bufp; ++bufp ) {
			if ( *bufp == '/' ) {
				*bufp = '\0';
				if ( stat(new_path, &sb) < 0 ) {
					retval = mkdir(new_path, 0755);
				}
				*bufp = '/';
			}
		}
		free(new_path);
	}
	return(retval);
}
