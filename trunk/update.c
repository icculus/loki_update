
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "patchset.h"
#include "setupdb.h"
#include "download.h"
#include "log_output.h"
#include "update.h"

/* Take a set of patches and apply all the updates for existing components */
int auto_update(patchset *patchset)
{
    patch_path *path;
    int applied = 0;

    for ( path=patchset->paths; path; path=path->next ) {
        if ( loki_find_component(patchset->product, path->leaf->component) ) {
            if ( perform_update(path->patches) < 0 ) {
                return(-1);
            }
            ++applied;
        }
    }
    return applied;
}

/* Apply a set of patches to a product */
int perform_update(patch *patch)
{
    char patch_file[PATH_MAX];
    char gpg_sig[1024];
    char *signed_by;
    int i;
    int patch_status;

    while ( patch ) {
        log(LOG_DEBUG, "Patch: %s at %s\n", patch->version, patch->url);
        log(LOG_NORMAL, "Downloading patch .. ");
        fflush(stdout);
        switch ( download_verify_file(patch->url,
                                      patch_file, sizeof(patch_file),
                                      gpg_sig, sizeof(gpg_sig)) ) {
            case DOWNLOAD_FAILED:
                log(LOG_NORMAL, "Download failed\n");
                unlink(patch_file);
                return(-1);
            case VERIFY_FAILED:
                log(LOG_NORMAL, "Download succeeded, verify failed\n");
                unlink(patch_file);
                return(-1);
            case VERIFY_UNKNOWN:
                log(LOG_NORMAL, "Download succeeded, verify unknown\n");
                break;
            case VERIFY_OK:
                log(LOG_NORMAL, "Download succeeded, verify OK\n");
                signed_by = strchr(gpg_sig, ' ');
                if ( signed_by ) {
                    *signed_by++ = '\0';
                    log(LOG_NORMAL, "File signed by: %s\n", signed_by);
                }
                log(LOG_NORMAL, "GPG fingerprint: ");
                for ( i=0; gpg_sig[i]; ++i ) {
                    log(LOG_NORMAL, "%c", gpg_sig[i]);
                    if ( ((i+1)%4) == 0 ) {
                        log(LOG_NORMAL, " ");
                    }
                }
                log(LOG_NORMAL, "\n");
                break;
        }
        /* Now run the patch */
        chmod(patch_file, 0700);
        patch_status = system(patch_file);
        unlink(patch_file);
        if ( patch_status != 0 ) {
            return(-1);
        }
        patch = patch->next;
    }
    return(0);
}
