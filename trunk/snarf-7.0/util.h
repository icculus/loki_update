/* -*- mode: C; c-basic-offset: 8; indent-tabs-mode: nil; tab-width: 8 -*- */

#include <sys/types.h>
#include <stdio.h>
#include "url.h"
#include "config.h"

#ifndef UTIL_H
#define UTIL_H

typedef struct _Progress Progress;

struct _Progress {
        unsigned char tty;	/* if we have a tty */
        long int length;	/* total length */
        long int current;	/* current position */
        long int offset;	/* if we're resuming, save the offset */
        int max_hashes;		/* max number of hashes to print */
        int cur_hashes;		/* total hashes printed so far*/
        int overflow;		/* save the remainder */
        unsigned char frame;	/* frame for the spinny animation */
        double start_time;	/* for calculating k/sec */
        UrlResource *rsrc;	/* Info such as file name and offset */
};

enum report_levels { DEBUG, WARN, ERR };

#ifdef PROTOTYPES

Progress *progress_new(void);
int progress_init(Progress *, UrlResource *, long int);
int progress_update(Progress *, long int);
void progress_destroy(Progress *, int);
double double_time(void);

char *string_lowercase(char *);
char *get_proxy(const char *);
int dump_data(UrlResource *, int, FILE *);
char *strconcat(const char *, ...);
char *base64(char *, int);
void report(enum report_levels, char *, ...);
int tcp_connect(char *, int);
int tcp_connect_async(char *remote_host, int port, int (*update)(int status_level, const char *status, float percentage, int size, int total, void *udata), void *udata);
off_t get_file_size(const char *);
void repchar(FILE *fp, char ch, int count);
int transfer(UrlResource *rsrc);

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif

#endif /* PROTOTYPES */

extern int debug_enabled;

#define open_outfile(x)  (((x)->outfile[0] == '-') ? stdout : real_open_outfile(x))
#define real_open_outfile(x)  (((x)->options & OPT_RESUME && !((x)->options & OPT_NORESUME)) ? (fopen((x)->outfile, "a")) : (fopen((x)->outfile, "w")))

#define safe_free(x)		if(x) free(x)
#define safe_strdup(x)		( (x) ? strdup(x) : NULL )
#define BUFSIZE (5*2048)



#endif
