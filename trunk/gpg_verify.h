
/* Verify that a file is not corrupt, and is signed correctly */

#define CHECKSUM_SIZE   32

typedef enum {
    GPG_NOTINSTALLED,
    GPG_CANCELLED,
    GPG_NOPUBKEY,
    GPG_VERIFYFAIL,
    GPG_VERIFYOK
} gpg_result;

typedef enum {
    VERIFY_OK,                  /* Completely verified */
    VERIFY_UNKNOWN,             /* No GPG available */
    VERIFY_FAILED               /* Failed checksum */
} verify_result;

/* Verify the given signature */
extern gpg_result gpg_verify(const char *file, char *sig, int maxsig);

/* Get the given public key */
int get_publickey(const char *key,
    int (*update)(float percentage, int size, int total, void *udata),
                                                void *udata);
