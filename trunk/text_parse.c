
/* These functions are just designed to strip HTML tags out of a file
   so the text can be parsed normally.
*/
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>

#include "log_output.h"

struct text_fp {
    char *data;
    char *pos;
    char *end;
    int html_mode;
    int tag_level;
};

void text_close(struct text_fp *textfp)
{
    if ( textfp->data ) {
        free(textfp->data);
    }
    free(textfp);
}

struct text_fp *text_open(const char *file)
{
    struct stat sb;
    struct text_fp *textfp;
    FILE *fp;
    int was_read;

    /* Get the size of the file */
    if ( stat(file, &sb) < 0 ) {
        fprintf(stderr, _("Unable to find %s\n"), file);
        return(NULL);
    }

    /* Allocate memory for the file structure */
    textfp = (struct text_fp *)malloc(sizeof *textfp);
    if ( ! textfp ) {
        fprintf(stderr, _("Out of memory\n"));
        return(NULL);
    }
    memset(textfp, 0, (sizeof *textfp));

    /* Allocate memory to hold the file */
    textfp->data = (char *)malloc(sb.st_size+1);
    if ( ! textfp->data ) {
        fprintf(stderr, _("Out of memory\n"));
        return(NULL);
    }

    /* Open and read the file */
    fp = fopen(file, "r");
    if ( ! fp ) {
        fprintf(stderr, _("Unable to open %s\n"), file);
        text_close(textfp);
        return(NULL);
    }
    was_read = fread(textfp->data, sb.st_size, 1, fp);
    fclose(fp);
    if ( ! was_read ) {
        fprintf(stderr, _("Unable to read %s\n"), file);
        text_close(textfp);
        return(NULL);
    }
    textfp->pos = textfp->data;
    textfp->end = textfp->data+sb.st_size;
    *textfp->end = '\0';

    /* See whether the file is in HTML mode */
    textfp->html_mode = (strstr(textfp->data, "<body") ||
                         strstr(textfp->data, "<BODY"));

    /* We're all set */
    return textfp;
}

char *text_line(char *line, int maxlen, struct text_fp *textfp)
{
    int len;
    int taglen;
    char tag[128];

    /* See if we've reached "EOF" */
    if ( ! *textfp->pos ) {
        return(NULL);
    }

    len = 0;
    if ( textfp->html_mode ) {
        while ( *textfp->pos ) {
            if ( (*textfp->pos == '<') ) {
                ++textfp->pos;
                ++textfp->tag_level;

                /* See what tag this is */
                while ( isspace(*textfp->pos) ) {
                    ++textfp->pos;
                }
                taglen = 0;
                while ( isalpha(*textfp->pos) && (taglen < (sizeof(tag)-1)) ) {
                    tag[taglen++] = *textfp->pos++;
                }
                tag[taglen] = '\0';

                /* See if this is the "end of line" tag */
                if ( (strcasecmp(tag, "br") == 0) ||
                     (strcasecmp(tag, "p") == 0) ||
                     (strcasecmp(tag, "li") == 0) ||
                     (strcasecmp(tag, "tr") == 0) ) {
                    break;
                }
            } else
            if ( (*textfp->pos == '>') ) {
                ++textfp->pos;
                --textfp->tag_level;
            } else {
                if ( ! textfp->tag_level ) {
                    if ( (*textfp->pos == '\r') || (*textfp->pos == '\n') ) {
                        line[len++] = ' ';
                    } else {
                        line[len++] = *textfp->pos;
                    }
                    if ( len >= (maxlen-1) ) {
                        break;
                    }
                }
                ++textfp->pos;
            }
        }
    } else {
        while ( *textfp->pos ) {
            /* End of line? */
            if ( (*textfp->pos == '\r') || (*textfp->pos == '\n') ) {
                line[len] = '\0';
                while ( (*textfp->pos == '\r') || (*textfp->pos == '\n') ) {
                    ++textfp->pos;
                }
                break;
            }
            line[len++] = *textfp->pos++;
            if ( len >= (maxlen-1) ) {
                break;
            }
        }
    }
    line[len] = '\0';
    return(line);
}

/* Parses a "key : value" pair out of a text file */
int text_parsefield(struct text_fp *textfp, char *key, int keylen,
                                            char *value, int valuelen)
{
    char line[4096];
    char *start, *mark;

    while ( text_line(line, sizeof(line), textfp) ) {
        mark = strchr(line, ':');
        if ( mark ) {
            *mark = '\0';

            /* Copy the key string, trimming whitespace */
            start = line;
            while ( isspace(*start) ) {
                ++start;
            }
            if ( ! *start || ! keylen ) {
                continue;
            }
            while ( (keylen > 0) && *start ) {
                *key++ = *start++;
            }
            while ( isspace(*(key-1)) ) {
                --key;
            }
            *key = '\0';
            
            /* Copy the value string, trimming whitespace */
            start = mark+1;
            while ( isspace(*start) ) {
                ++start;
            }
            if ( *start ) {
                while ( (valuelen > 0) && *start ) {
                    *value++ = *start++;
                }
                while ( isspace(*(value-1)) ) {
                    --value;
                }
            }
            *value = '\0';

            return(1);
        }
    }
    return(0);
}
