
/* These functions are just designed to strip HTML tags out of a file
   so the text can be parsed normally.
*/

struct text_fp;

extern void text_close(struct text_fp *textfp);

extern struct text_fp *text_open(const char *file);

extern char *text_line(char *line, int maxlen, struct text_fp *textfp);

extern int text_parsefield(struct text_fp *textfp, char *key, int keylen,
                                                char *value, int valuelen);
