
/* Verify that a file is not corrupt, and is signed correctly */

#include "update.h"

#define CHECKSUM_SIZE   32

typedef enum {
    GPG_NOTINSTALLED,
    GPG_CANCELLED,
    GPG_NOPUBKEY,
    GPG_VERIFYFAIL,
    GPG_VERIFYOK
} gpg_result;

typedef enum {
    DOWNLOAD_FAILED,            /* Download failed */
    VERIFY_OK,                  /* Completely verified */
    VERIFY_UNKNOWN,             /* No GPG available */
    VERIFY_FAILED               /* Failed checksum */
} verify_result;

/* Verify the given signature */
extern gpg_result gpg_verify(const char *file, char *sig, int maxsig);

/* Get the given public key */
int get_publickey(const char *key, update_callback update, void *udata);