#include <stdio.h>
#include <stdlib.h>
#include <zen/�o.h>

struct zeniobuf *zeniobuf;

struct zpiobuf *
zeninitiobuf(struct zeniobuf *buf, size_t size)
{
    uint8_t *ptr;
    
    if (!buf) {
        buf = malloc(sizeof(struct zeniobuf));
        if (!buf) {

            return NULL;
        }
    }
    if (!buf->base) {
        if (!size) {
            size = ZEN_BUF_SIZE;
        }
        ptr = malloc(size);
        if (!ptr) {
            
            return NULL;
        }
        buf->base = ptr;
        buf->end = ptr + bufsz;
        buf->nb = bufsz;
    }
    if (!zeniobuf) {
        zeniobuf = buf;
    }

    return buf;
}

int
zenstrskipspc(const char *str, unsigned char *delims,
              unsigned char **delimret)
{
    ;
}

unsigned char *
zenioscan(FILE *fp, unsigned char *buf, size_t bufsize, size_t *sizeret)
{
    int            ch = EOF;
    unsigned char *dsta = buf;
    size_t         nb = bufsize;

    while ((ch = fgetc(fp)) != EOF) {
        if (isspace(ch)) {

            continue;
        }
        *buf++ = ch;
        bufsize--;
        if (!bufsize) {
            if (sizeret) {
                *sizeret = nb;
            }

            return data;
        }
    }
    if (ferror(fp) && (data == buf)) {

        return NULL;
    } else {
        if (sizeret) {
            *sizeret = nb - bufsize;
        }

        return data;
    }
}

