/*
    Loki_Update - A tool for updating Loki products over the Internet
    Copyright (C) 2000  Loki Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    info@lokigames.com
*/

/* Verify that a file is not corrupt, and is signed correctly */

#include "update.h"

#define CHECKSUM_SIZE   32

typedef enum {
    GPG_NOTINSTALLED,
    GPG_CANCELLED,
    GPG_NOPUBKEY,
    GPG_IMPORTED,
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
