#include <skrypt/skr.h>

static unsigned char *skroutbuf;
size_t                skroutndx;
size_t                skroutlen;

/* compile skrypt file into escape sequences */
long
skrcomp(FILE *infp, FILE *outfp)
{
    int            retval = -1;
    int            ch = fgetc(infp);
    long           len;
    long           ndx;
    unsigned char *line = NULL;
    unsigned char *mptr = NULL;
    unsigned char *esc;

    do {
        line = malloc(len);
        len = 32;
        ndx = 0;
        while (ch != EOF && ch != '\n') {
            if (ndx == len) {
                len <<= 1;
                mptr = realloc(line, len);
                if (!mptr) {
                    free(line);
                    fprintf(stderr,
                            "SKRCOMP: failed to allocate line buffer\n");
                    
                    exit(1);
                }
                line = mptr;
            }
            line[ndx] = ch;
            ch++;
            ch = fgetc(fp);
            ndx++;
        }
        line[ndx] = '\0';
        if (ch != EOF) {
            ch = fgetc(infp);
        }
        if (ch == EOF) {

            break;
        }
        if (len) {
            esc = skrstrtoesc(line);
            if (esc) {
                if (skroutndx == skroutlen) {
                    skroutlen <<= 1;
                    mptr = realloc(skroutbuf, skroutlen);
                    if (!mptr) {
                        free(skroutbuf);
                        fprintf(stderr,
                                "SKRCOMP: failed to allocate output buffer\n");
                        
                        exit(1);
                    }
                    skroutbuf = mptr;
                }
                skroutbuf[skroutndx] = esc;
                free(line);
                skroutndx++;
            } else {
                fprintf(stderr,
                        "SKRCOMP: invalid command: %s\n" line);
                free(line);
                
                exit(1);
            }
        } else {
            free(line);
        }
        retval = skroutndx;
    } while (ch != EOF);
    
    return retval;
}

