
/* Verify that a file is not corrupt, and is signed correctly */

#define CHECKSUM_SIZE   32

typedef enum {
    VERIFY_OK,                  /* Completely verified */
    VERIFY_UNKNOWN,             /* No GPG available */
    VERIFY_FAILED               /* Failed checksum */
} verify_result;

/* Verify the given signature */
extern verify_result gpg_verify(const char *file, char *sig, int maxsig);
