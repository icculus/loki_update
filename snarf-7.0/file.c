/* -*- mode: C; c-basic-offset: 8; indent-tabs-mode: nil; tab-width: 8 -*- */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include "url.h"
#include "options.h"
#include "util.h"

extern int default_opts;

int
file_transfer(UrlResource *rsrc)
{
	char path[PATH_MAX];
	struct stat sb;
        int in 			= 0;
        FILE *out 		= NULL;
        Url *u			= NULL;
        int retval		= 0;

        /* make sure everything's initialized to something useful */
        u = rsrc->url;
     
        if( !u->path )
                u->path = strdup(".");

        if( !u->file ) {
                report(ERR, "No file specified");
                return 0;
        }

        if( !rsrc->outfile )
                rsrc->outfile = strdup(u->file);

        rsrc->options |= default_opts;

        sprintf(path, "%s/%s", u->path, u->file);
        in = open(path, O_RDONLY, 0);
        if( in < 0 ) {
		if ( rsrc->progress ) {
			char msg[PATH_MAX+128];
			sprintf(msg, "%s: %s\n", path, strerror(errno));
			rsrc->progress(ERROR, msg, 0.0f, 0, 0,
			               rsrc->progress_udata);
		} else {
                	report(ERR, "opening %s: %s", path, strerror(errno));
		}
                return 0;
        }
	if ( stat(path, &sb) == 0 ) {
		rsrc->outfile_size = sb.st_size;
	}
	if ((rsrc->options & OPT_RESUME) && rsrc->outfile_offset) {
		if ( lseek(in, rsrc->outfile_offset, SEEK_SET) < 0 ) {
			if ( rsrc->progress ) {
				char msg[PATH_MAX+256];
                		sprintf(msg, "seeking to %d in %s: %s\n",
				  rsrc->outfile_offset, path, strerror(errno));
				rsrc->progress(ERROR, msg, 0.0f, 0, 0,
				               rsrc->progress_udata);
			} else {
                		report(ERR, "seeking to %d in %s: %s",
				  rsrc->outfile_offset, path, strerror(errno));
			}
                	return 0;
		}
	}

        out = open_outfile(rsrc);
        if( !out ) {
		if ( rsrc->progress ) {
			char msg[PATH_MAX+128];
			sprintf(msg, "%s: %s\n", rsrc->outfile, strerror(errno));
			rsrc->progress(ERROR, msg, 0.0f, 0, 0,
			               rsrc->progress_udata);
		} else {
                	report(ERR, "opening %s: %s",
			       rsrc->outfile, strerror(errno));
		}
                return 0;
        }

        if( ! dump_data(rsrc, in, out) )
                retval = 0;
        else
                retval = 1;
                        
 cleanup:
        close(in); fclose(out);
        return retval;

}

