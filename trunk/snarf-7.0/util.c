/* -*- mode: C; c-basic-offset: 8; indent-tabs-mode: nil; tab-width: 8 -*- */

#include "util.h"

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>

#ifdef USE_SOCKS5
#define SOCKS
#include <socks.h>
#endif

#if (defined(HAVE_SYS_IOCTL_H) && defined(GUESS_WINSIZE))
/* Needed for SunOS */
#ifndef BSD_COMP
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#endif

#if defined(HAVE_ARES_H)
#include <ares.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "url.h"
#include "options.h"


char output_buf[BUFSIZ];


#ifndef HAVE_STRERROR

extern int sys_nerr;
extern char *sys_errlist[];

const char *
strerror(int index)
{
        if( (index > 0) && (index <= sys_nerr) ) {
                return sys_errlist[index];
        } else if(index==0) {
                return "No error";
        } else {
                return "Unknown error";
        }
}

#endif /* !HAVE_STRERROR */

void
repchar(FILE *fp, char ch, int count)
{
        while(count--)
                fputc(ch, fp);
}


int
guess_winsize()
{
#if (defined(HAVE_SYS_IOCTL_H) && defined(GUESS_WINSIZE))
        int width;
        struct winsize ws;

        if( ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 ||
            ws.ws_col == 0 )
                width = 79;
        else
                width = ws.ws_col - 1;

        return width;
#else
        return 79;
#endif /* defined(HAVE_SYS_IOCTL_H) && defined(GUESS_WINSIZE) */
}


double
double_time(void)
{
#ifdef HAVE_GETTIMEOFDAY
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec + (tv.tv_usec / 1000000.00);
#else
        return time();
#endif
}


char *
string_lowercase(char *string)
{
        char *start = string;

        if( string == NULL )
                return NULL;

        while( *string ) {
                *string = tolower(*string);
                string++;
        }

        return start;
}


char *
get_proxy(const char *firstchoice)
{
        char *proxy;
        char *help;

        if( (proxy = getenv(firstchoice)) )
                return proxy;

        help = safe_strdup(firstchoice);
        string_lowercase(help);
        proxy = getenv(help);
        safe_free(help);
        if( proxy )
                return proxy;

        if( (proxy = getenv("SNARF_PROXY")) )
                return proxy;

        if( (proxy = getenv("PROXY")) )
                return proxy;

        return NULL;
}


int
dump_data(UrlResource *rsrc, int sock, FILE *out)
{
	int okay		= 1;
        int out_fd 		= fileno(out);
        Progress *p		= NULL;
        int bytes_read		= 0;
        ssize_t written		= 0;
        char buf[BUFSIZE];

        /* if we already have all of it */
        if( !(rsrc->options & OPT_NORESUME) ) {
                if( rsrc->outfile_size && 
                    (rsrc->outfile_offset >= rsrc->outfile_size) ) {
                        report(WARN, "you already have all of `%s', skipping", 
                             rsrc->outfile);
                        close(sock);
                        return 1;
                }
        }

        p = progress_new();
        progress_init(p, rsrc, rsrc->outfile_size);
        if (!(rsrc->options & OPT_NORESUME)) {
                progress_update(p, rsrc->outfile_offset);
                p->offset = rsrc->outfile_offset;
        }


	okay = 1;
        while( okay && (bytes_read = read(sock, buf, BUFSIZE)) ) {
                if ( progress_update(p, bytes_read) ) {
			/* Cancelled? */
			okay = 0;
		}
                written = write(out_fd, buf, bytes_read);
                if( written == -1 ) {
                        perror("write");
                        okay = 0;
                }
        }

        progress_destroy(p);
        return okay;
}


off_t
get_file_size(const char *file)
{
        struct stat file_info;

        if( !(file || *file) )
                return 0;

        if( file[0] == '-' )
                return 0;

        if( stat(file, &file_info) == -1 )
                return 0;
        else
                return(file_info.st_size);
}



int debug_enabled = 0;

Progress *
progress_new(void)
{
        Progress *new_progress;

        new_progress = malloc(sizeof(Progress));

        new_progress->tty	= 0;
        new_progress->length 	= 0;
        new_progress->current 	= 0;
        new_progress->offset	= 0;
        new_progress->overflow	= 0;
        new_progress->max_hashes= 0;
        new_progress->cur_hashes= 0;

        new_progress->start_time = double_time();

        return new_progress;
}


int
progress_init(Progress *p, UrlResource *rsrc, long int len)
{
        char *filename 	= NULL;
        int win_width	= 0;
        int total_units	= 0;

        if( !p )
                return 0;

        p->rsrc = rsrc;
        p->length = len;


#ifndef PROGRESS_DEFAULT_OFF
        if( rsrc->options & OPT_PROGRESS ) {
                p->tty = 1;
        } else {
                if( (!isatty(2)) || (rsrc->outfile[0] == '-') || 
                    (rsrc->options & OPT_QUIET) || rsrc->progress ) {
            
                        p->tty = 0;
                        return 1;
                } else {
                        p->tty = 1;
                }
        }
#else
        if( rsrc->options & OPT_PROGRESS )
                p->tty = 1;
        else {
                p->tty = 0;
                return 1;
        }
#endif /* PROGRESS_DEFAULT_OFF */
        
        win_width = guess_winsize();

        if( win_width > 30 )
                total_units = win_width - (30 + 9 + 16);
        else {
                /* the window is pathetically narrow; bail */
                total_units = 20;
        }

        /* buffering stderr: no flicker! yay! */
        setbuf(stderr, (char *)&output_buf);

        fprintf(stderr, "%s (", rsrc->url->full_url);

        if( total_units && len ) {
                p->length = len;
                p->max_hashes = total_units;
                fprintf(stderr, "%dK", (int )(len / 1024));
        } else {
                fprintf(stderr, "unknown size");
        }

        fprintf(stderr, ")\n");

        p->current = 0;

        filename = strdup(rsrc->outfile);
        
        if( strlen(filename) > 24 )
                filename[24] = '\0';

        fprintf(stderr, "%-25s[", filename);

        if( p->length )
                repchar(stderr, ' ', p->max_hashes);
        else
                fputc('+', stderr);

        fprintf(stderr, "] %7dK", (int )p->current);
        fflush(stderr);
        return 1;
}


int
progress_update(Progress *	p, 
                long int 	increment)
{
        unsigned int units;
        char *anim = "-\\|/";
	int cancelled = 0;

        p->current += increment;

	if( p->tty ) {
        	fprintf(stderr, "\r");
        	fprintf(stderr, "%-25.25s [", p->rsrc->outfile);
	}
        

        if( p->length ) {
                float percent_done = (float )p->current / p->length;
                double elapsed;
                float rate;

		if( p->tty ) {
                	units = percent_done * p->max_hashes;
                	if( units )
                        	repchar(stderr, '#', units);
                	repchar(stderr, ' ', p->max_hashes - units);
                	fprintf(stderr, "] ");
                	fprintf(stderr, "%7dK", (int )(p->current / 1024));
		}

		if( p->rsrc->progress ) {
                        p->rsrc->progress_percent = percent_done*100.0;
			cancelled = p->rsrc->progress(
                                           p->rsrc->progress_percent,
			                   p->current/1024, p->length/1024,
			                   p->rsrc->progress_udata);
		}
       
                elapsed = double_time() - p->start_time;

                if (elapsed) 
                        rate = ((p->current - p->offset) / elapsed) / 1024;
                else
                        rate = 0;

                /* The first few runs give extra-high values, so skip them */
                if (rate > 999999)
                        rate = 0;

		if( p->tty ) {
                	fprintf(stderr, " | %7.2fK/s", rate);
		}
        } else {
                /* length is unknown, so just draw a little spinny thingy */
		if( p->tty ) {
                	fprintf(stderr, "%c]", anim[p->frame++ % 4]);
                	fprintf(stderr, " %7dK", (int )(p->current / 1024));
		}
        }

	if( p->tty ) {
        	fflush(stderr);
	}
        return cancelled;
}


void 
progress_destroy(Progress *p)
{
        double elapsed = 0;
        double kbytes;
        if( p && (!p->tty) ) {
		safe_free(p);
                return;
	}

        elapsed = double_time() - p->start_time;

        fprintf(stderr, "\n");

        if( elapsed ) {
                kbytes = ((float )(p->current - p->offset) / elapsed) / 1024.0;
                fprintf(stderr, "%ld bytes transferred " 
                        "in %.2f sec (%.2fk/sec)\n",
                        p->current - p->offset, elapsed, kbytes);
        } else {
                fprintf(stderr, "%ld bytes transferred " 
                        "in less than a second\n", p->current);
        }

        fflush(stderr);

        safe_free(p);
}
                



/* taken from glib */
char *
strconcat (const char *string1, ...)
{
        unsigned int   l;
        va_list args;
        char   *s;
        char   *concat;
  
        l = 1 + strlen (string1);
        va_start (args, string1);
        s = va_arg (args, char *);

        while( s ) {
                l += strlen (s);
                s = va_arg (args, char *);
        }
        va_end (args);
  
        concat = malloc(l);
        concat[0] = 0;
  
        strcat (concat, string1);
        va_start (args, string1);
        s = va_arg (args, char *);
        while (s) {
                strcat (concat, s);
                s = va_arg (args, char *);
        }
        va_end (args);
  
        return concat;
}


/* written by lauri alanko */
char *
base64(char *bin, int len)
{
	char *buf= (char *)malloc((len+2)/3*4+1);
	int i=0, j=0;

        char BASE64_END = '=';
        char base64_table[64]= "ABCDEFGHIJKLMNOPQRSTUVWXYZ" 
                "abcdefghijklmnopqrstuvwxyz"
                "0123456789+/";
	
	while( j < len - 2 ) {
		buf[i++]=base64_table[bin[j]>>2];
		buf[i++]=base64_table[((bin[j]&3)<<4)|(bin[j+1]>>4)];
		buf[i++]=base64_table[((bin[j+1]&15)<<2)|(bin[j+2]>>6)];
		buf[i++]=base64_table[bin[j+2]&63];
		j+=3;
	}

	switch ( len - j ) {
        case 1:
		buf[i++] = base64_table[bin[j]>>2];
		buf[i++] = base64_table[(bin[j]&3)<<4];
		buf[i++] = BASE64_END;
		buf[i++] = BASE64_END;
		break;
        case 2:
		buf[i++] = base64_table[bin[j] >> 2];
		buf[i++] = base64_table[((bin[j] & 3) << 4) 
                                       | (bin[j + 1] >> 4)];
		buf[i++] = base64_table[(bin[j + 1] & 15) << 2];
		buf[i++] = BASE64_END;
		break;
        case 0:
	      break;
	}
	buf[i]='\0';
	return buf;
}



void
report(enum report_levels lev, char *format, ...)
{
        switch( lev ) {
        case DEBUG:
                fprintf(stderr, "debug: ");
                break;
        case WARN:
                fprintf(stderr, "warning: ");
                break;
        case ERR:
                fprintf(stderr, "error: ");
                break;
        default:
                break;
        }

        if( format ) {
                va_list args;
                va_start(args, format);
                vfprintf(stderr, format, args);
                va_end(args);
                fprintf(stderr, "\n");
        }
}


int
tcp_connect(char *remote_host, int port) 
{
        struct hostent *host;
        struct sockaddr_in sa;
        int sock_fd;

        if((host = (struct hostent *)gethostbyname(remote_host)) == NULL) {
                herror(remote_host);
                return 0;
        }

        /* get the socket */
        if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("socket");
                return 0;
        }

        /* connect the socket, filling in the important stuff */
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        memcpy(&sa.sin_addr, host->h_addr,host->h_length);
  
        if(connect(sock_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0){
                perror(remote_host);
                return 0;
        }

        return sock_fd;
}

#if defined(HAVE_ARES_H)
static struct hostent *hostentry;

static void snarf_host_callback(void *arg, int status, struct hostent *host)
{
	struct sockaddr_in *sa = (struct sockaddr_in *)arg;

	if ( status == ARES_SUCCESS ) {
		memcpy(&sa->sin_addr, host->h_addr, host->h_length);
	}
}
	
static int
gethostbyname_async(const char *remote_host, struct sockaddr_in *sa,
                    int (*update)(float percentage, int size, int total, void *udata), void *udata)
{
	ares_channel channel;
	int nfds;
	fd_set read_fds;
	fd_set write_fds;
	struct timeval tv, maxtv;
	struct timeval *tvp;
	int cancelled;
	int status;

	/* Initialize to no host entry */
	sa->sin_addr.s_addr = INADDR_NONE;
	if ( ares_init(&channel) != ARES_SUCCESS ) {
		return(-1);
	}

	/* Perform the lookup */
	ares_gethostbyname(channel, remote_host, AF_INET, snarf_host_callback, sa);

	/* Drive the lookup, periodically calling the UI update */
	cancelled = 0;
	while ( ! cancelled ) {
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		nfds = ares_fds(channel, &read_fds, &write_fds);
		if ( nfds == 0 ) {
			break;
		}
		maxtv.tv_sec = 0;
		maxtv.tv_usec = 100000;
		tvp = ares_timeout(channel, &maxtv, &tv);
		if ( select(nfds, &read_fds, &write_fds, NULL, tvp) == 0 ) {
			/* No activity, run UI update */
			cancelled = update(0.0f, 0, 0, udata);
		}
		ares_process(channel, &read_fds, &write_fds);
	}
	ares_destroy(channel);

	if ( sa->sin_addr.s_addr == INADDR_NONE ) {
		status = -1;
	} else {
		status = 0;
	}
	return status;
}
#else
static int
gethostbyname_async(const char *remote_host, struct sockaddr_in *sa,
                    int (*update)(float percentage, int size, int total, void *udata), void *udata)
{
        struct hostent *host;
	int status;

	host = (struct hostent *)gethostbyname(remote_host);
	if ( host ) {
		memcpy(&sa->sin_addr, host->h_addr, host->h_length);
		status = 0;
	} else {
		status = -1;
	}
	return(status);
}
#endif /* HAVE_ARES */

static int set_blocking(int sock_fd, int blocking)
{
	int flags;

	// Let's do it the Posix.1g way!
	flags = fcntl(sock_fd, F_GETFL, 0);
	if( flags < 0 ) {
		return(-1);
	}
	if( blocking ) {
		flags &= ~O_NONBLOCK;
	} else {
		flags |= O_NONBLOCK;
	}
	if( fcntl(sock_fd, F_SETFL, flags) < 0 ) {
		return(-1);
	}
	return(0);
}

int
tcp_connect_async(char *remote_host, int port,
    int (*update)(float percentage, int size, int total, void *udata),
                                                void *udata)
{
	struct sockaddr_in sa;
	int sock_fd;
	fd_set fdset;
	struct timeval tv;
	int was_error;
	int error_size;
	int cancelled;
	int connected;

	/* No need for async code if there's no UI update function */
	if( ! update ) {
		return tcp_connect(remote_host, port);
	}

        /* Start off with zero percent complete (opening connection) */
	cancelled = update(0.0f, 0, 0, udata);
        if ( cancelled ) {
                return(0);
        }

	/* Look up the remote host address */
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(remote_host);
	if ( sa.sin_addr.s_addr == INADDR_NONE ) {
		if( gethostbyname_async(remote_host, &sa, update, udata) < 0 ) {
			/* FIXME: Print an error message */
			return(0);
		}
	}
	
	/* create the socket */
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if( sock_fd < 0 ) {
		/* FIXME: Print an error message */
		return 0;
	}

	/* connect the socket asynchronously */
	if( set_blocking(sock_fd, 0) < 0 ) {
		/* FIXME: Print an error message */
		close(sock_fd);
		return 0;
	}
	if( connect(sock_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0 ) {
		if ( errno != EINPROGRESS ) {
			/* FIXME: Print an error message */
			close(sock_fd);
			return 0;
		}
		cancelled = 0;
		connected = 0;
		while ( ! cancelled && ! connected ) {
			FD_ZERO(&fdset);
			FD_SET(sock_fd, &fdset);
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
			switch (select(sock_fd+1, NULL, &fdset, NULL, &tv)) {
			    case -1:  /* Socket broken? */
				/* FIXME: Print an error message */
				cancelled = 1;
				break;
			    case 0:
				/* No activity, run UI update */
				cancelled = update(0.0f, 0, 0, udata);
				break;
			    default:
				error_size = sizeof(was_error);
				getsockopt(sock_fd, SOL_SOCKET, SO_ERROR,
					   &was_error, &error_size);
				if ( was_error ) {
					/* FIXME: Print an error message */
					cancelled = 1;
				} else {
					connected = 1;
				}
				break;
			}
		}
		if ( cancelled ) {
			close(sock_fd);
			return 0;
		}
	}
	if( set_blocking(sock_fd, 1) < 0 ) {
		/* FIXME: Print an error message */
		close(sock_fd);
		return 0;
	}
	return sock_fd;
}

#ifndef HAVE_STRDUP
char *
strdup(const char *s)
{
        char *new_string = NULL;

        if (s == NULL)
                return NULL;

        new_string = malloc(strlen(s) + 1);

        strcpy(new_string, s);

        return new_string;
}
#endif /* HAVE_STRDUP */


int
transfer(UrlResource *rsrc)
{
        int i;

        switch (rsrc->url->service_type) {
        case SERVICE_HTTP:
                i = http_transfer(rsrc);
                break;
        case SERVICE_FTP:
                i = ftp_transfer(rsrc);
                break;
        case SERVICE_GOPHER:
                i = gopher_transfer(rsrc);
                break;
        default:
                report(ERR, "bad url: %s", rsrc->url->full_url);
        }

        return i;
}
