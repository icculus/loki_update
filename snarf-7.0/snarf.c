/* -*- mode: C; c-basic-offset: 8; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This program and all accompanying files are copyright 1998 Zachary
 * Beane <xach@xach.com>. They come with NO WARRANTY; see the file COPYING
 * for details.
 * 
 * This program is licensed to you under the terms of the GNU General
 * Public License. You should have recieved a copy of it with this
 * program; if you did not, you can view the terms at http://www.gnu.org/.
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include "options.h"
#include "llist.h"
#include "url.h"
#include "http.h"
#include "ftp.h"
#include "gopher.h"
#include "util.h"

#define LATEST_URL "http://www.xach.com/snarf/snarf-latest.tar.gz"

#ifdef PROGRESS_DEFAULT_OFF
#define PROGRESS_SETTING "off"
#else
#define PROGRESS_SETTING "on"
#endif

static void
usage(int verbose)
{
        if (!verbose) {
                fprintf(stderr, "Use `snarf --help' for help\n");
                exit(1);
        }

        printf("This is snarf, version %s\n", VERSION);
        printf("usage: snarf [OPTIONS] URL [OUTFILE] ...\n");
        printf("Options:\n"
               "    -a     Force active FTP (default is passive)\n"
               "    -v     Verbose; print anything the server sends\n" 
               "    -q     Don't print progress bars (compiled default is %s)\n"
               "    -p     Force printing of progress bars (overrides -q)\n"
               "    -r     Resume downloading a partially transferred file\n"
               "    -n     Ignore '-r' and transfer file in its entirety\n"
               "    -m     Spoof MSIE user-agent string\n"
               "    -z     Spoof Navigator user-agent string\n"
               "\n"
               "Lowercase option letters only affect the URLs that "
               "immediately follow them.\n"
               "If you give an option in caps, it will be the "
               "default option for all URLs\nthat follow it.\n"
               "\n"
               "If you specify the outfile as '-', the file will be "
               "printed to standard\noutput as it downloads.\n"
               "\n"
               "You can have as many URLs and outfiles as you want\n"
               "\n"
               "You can specify a username and password for ftp or "
               "http authentication. The\nformat is:\n"
               "\n"
               "    ftp://username:password@host/\n"
               "\n"
               "If you don't specify a password, you will be prompted "
                "for one.\n"
               "\n"
               "snarf checks the SNARF_PROXY, FTP_PROXY, GOPHER_PROXY, "
               "HTTP_PROXY, and PROXY\nenvironment variables.\n"
               "\n"
               "snarf is free software and has NO WARRANTY. See the file "
               "COPYING for details.\n", PROGRESS_SETTING);

        exit(1);
}
                

int
main(int argc, char *argv[])
{
        List *arglist		= NULL;
        UrlResource *rsrc	= NULL;
        Url *u			= NULL;
        int retval		= 0;
        int i;

        arglist = list_new();
        rsrc 	= url_resource_new();

        if(argc < 2) {
                fprintf(stderr, "snarf: not enough arguments\n");
                usage(0); /* usage() exits */
        }
        
        if (strcmp(argv[1], "--version") == 0) {
                printf("snarf %s\n", VERSION);
                exit(0);
        }

        if (strcmp(argv[1], "--help") == 0) {
                usage(1);
        }

        for(i = 1; --argc; i++) {
                if( strcmp(argv[i], "LATEST") == 0 ) {
                        u = url_new();
                        if( !url_init(u, LATEST_URL) ) {
                                report(ERR, "`%s' is not a valid URL");
                                continue;
                        }

                        rsrc->url = u;
                        continue;
                }

                /* options */
                if( argv[i][0] == '-' && argv[i][1] ) {
                        if( rsrc->url ) {
                                list_append(arglist, rsrc);
                                rsrc = url_resource_new();
                        }

                        rsrc->options = set_options(rsrc->options, argv[i]);
                        continue;
                }

                /* everything else */
                if( !rsrc->url ) {
                        u = url_new();
                        if( !url_init(u, argv[i]) ) {
                                report(ERR, "bad url `%s'", argv[i]);
                                continue;
                        }
                        rsrc->url = u;
                } else if( is_probably_an_url(argv[i]) ) {
                        list_append(arglist, rsrc);
                        rsrc = url_resource_new();

                        u = url_new();
                        if( !url_init(u, argv[i]) ) {
                                report(ERR, "bad url `%s'", argv[i]);
                                continue;
                        }
                        rsrc->url = u;
                } else {
                        if( rsrc->outfile )
                                report(WARN, "ignoring `%s' for outfile",
                                       argv[i]);
                        else
                                rsrc->outfile = strdup(argv[i]);
                }
        }

        if( rsrc->url ) {
                list_append(arglist, rsrc);
        }

        if (arglist->data == NULL) {
                fprintf(stderr, "snarf: not enough arguments\n");
                usage(0);
        }

        /* walk through the arglist and output and do that thing you
           do so well */

        while (arglist != NULL) {
                rsrc = arglist->data;
                arglist = arglist->next;
                u = rsrc->url;

                if( !rsrc->outfile && u->file )
                        rsrc->outfile = strdup(u->file);

                if( rsrc->outfile ) {
                        rsrc->outfile_offset = get_file_size(rsrc->outfile);
                        
                        if (rsrc->outfile_offset && 
                            !(rsrc->options & OPT_NORESUME)) {
                                fprintf(stderr, "setting resume on "
                                        "existing file `%s' at %ld bytes\n", 
                                        rsrc->outfile, rsrc->outfile_offset);
                                rsrc->options |= OPT_RESUME;
                        }
                }
                        

                if (u->username && !u->password) {
                        char *prompt = NULL;
                        
                        prompt = strconcat("Password for ",
                                           u->username, "@",
                                           u->host, ": ", NULL);
                        
                        u->password = strdup(getpass(prompt));
                        free(prompt);
                }

                i = transfer(rsrc);
                if (i == 0)
                        retval++;

                url_resource_destroy(rsrc);
        }
                
        return retval;
}
        
