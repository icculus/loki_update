
/* Verify that a file is not corrupt, and is signed correctly */

#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "get_url.h"
#include "md5.h"
#include "download.h"
#include "gpg_verify.h"

verify_result download_verify_file(const char *url, char *file, int maxpath,
                                                    char *sig, int maxsig)
{
    verify_result result;
    char sum_file[PATH_MAX];
    char md5_real[CHECKSUM_SIZE+1];
    char md5_calc[CHECKSUM_SIZE+1];
    FILE *fp;

    /* First download the base file */
    if ( get_url(url, file, maxpath) != 0 ) {
        return DOWNLOAD_FAILED;
    }

    result = VERIFY_UNKNOWN;

    /* First check the GPG signature */
    if ( result == VERIFY_UNKNOWN ) {
        sprintf(sum_file, "%s.sig", url);
        if ( get_url(sum_file, sum_file, sizeof(sum_file)) == 0 ) {
            result = gpg_verify(sum_file, sig, maxsig);
            unlink(sum_file);
        }
    }

    /* Now download the MD5 checksum file */
    if ( result == VERIFY_UNKNOWN ) {
        sprintf(sum_file, "%s.md5", url);
        if ( get_url(sum_file, sum_file, sizeof(sum_file)) == 0 ) {
            fp = fopen(sum_file, "r");
            if ( fp ) {
                if ( fgets(md5_calc, sizeof(md5_calc), fp) ) {
                    md5_compute(file, md5_real, 0);
                    if ( strcmp(md5_calc, md5_real) != 0 ) {
                        result = VERIFY_FAILED;
                    }
                }
                fclose(fp);
            }
            unlink(sum_file);
        }
    }
    return result;
}
