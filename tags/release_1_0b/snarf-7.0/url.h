/* -*- mode: C; c-basic-offset: 8; indent-tabs-mode: nil; tab-width: 8 -*- */

#ifndef URL_H
#define URL_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


typedef struct _UrlResource 	UrlResource;
typedef struct _Url		Url;

struct _Url {
        char *full_url;
	int service_type;
	char *username;
	char *password;
	char *host;
	int port;
	char *path;
	char *file;
};

/* These are used by the progress update callback */
#define STATUS	2
#define ERROR	5

struct _UrlResource {
	Url *url;
	char *outfile;
        char *proxy;
        char *proxy_username;
        char *proxy_password;
	unsigned char options;
        off_t outfile_size;
        off_t outfile_offset;
        int (*progress)(int status_level, const char *status,
			float percentage, int size, int total, void *udata);
	float progress_percent;
        void *progress_udata;
};


/* Service types */
enum url_services {
        SERVICE_FILE = 1,
        SERVICE_HTTP,
        SERVICE_FTP,
        SERVICE_GOPHER,
        SERVICE_FINGER
};
                
/* Error string */

extern char *url_error;

/* Funcs */

#ifdef PROTOTYPES

Url *url_new(void);
void url_destroy(Url *);
Url *url_init(Url *, const char *);
UrlResource *url_resource_new(void);
void url_resource_destroy(UrlResource *);
int is_probably_an_url(char *);

#endif /* PROTOTYPES */

#endif /* URL_H */

	
