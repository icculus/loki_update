
/* Verify that a file is not corrupt, and is signed correctly */

#define CHECKSUM_SIZE   32

typedef enum {
    DOWNLOAD_FAILED = -1,       /* Download failed */
    VERIFY_OK,                  /* Completely verified */
    VERIFY_UNKNOWN,             /* No GPG available */
    VERIFY_FAILED               /* Failed checksum */
} verify_result;

extern verify_result download_verify_file(const char *url,
                                            char *file, int maxpath,
                                            char *sig, int maxsig);
