#include <features.h>

#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE)

#if defined(_BSD_SOURCE)
#include <stddef.h>
#endif
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <zero/asm.h>

#define _BTPRINTFMT  "%a <%n%D> at %f"
#define _BTOFSSTRLEN 32
#define _BTTABLEN    4096

uintptr_t
_btprintf(__btsymadrlist_t buf, __btsize_t size, const char *fmt, int fd)
{
    Dl_info       info;
    FILE         *fp = (fd >= 0) ? fdopen(fd, "a") : NULL;
    __btsize_t    lim = size;
    __btsize_t    slen = 0;
    char         *mptr = NULL;
    size_t        len;
    uintptr_t     pdif;
    void         *ptr;
    const char   *fptr;
    char         *cptr;
    char         *tab;
    __btsize_t    ndx;
    long          nl;

    if (!fp) {
        len = _BTTABLEN;
        mptr = malloc(len);
        if (!mptr) {

            return (uintptr_t)-1;
        }
    }
    tab = mptr;
    cptr = tab;
    if (!fmt) {
        fmt = _BTPRINTFMT;
    }
    for (ndx = 0 ; ndx < lim ; ndx++) {
        nl = 0;
        fptr = fmt;
        ptr = buf[ndx];
        dladdr(ptr, &info);
        if (fp) {
            while (fptr[0] != '\0') {
                if (fptr[0] != '%') {
                    fprintf(fp, "%c", fptr[0]);
                    fptr++;
                } else if (fptr[1] == 'a') {
                    fprintf(fp, "%p", ptr);
                    fptr += 2;
                } else if (fptr[1] == 'n') {
                    fprintf(fp, "%s", info.dli_sname);
                    fptr += 2;
                } else if (fptr[1] == 'd') {
                    pdif = (uintptr_t)ptr - (uintptr_t)info.dli_saddr;
                    fprintf(fp, "0x%lx", ptr, (long)pdif);
                    fptr += 2;
                } else if (fptr[1] == 'D') {
                    pdif = (uintptr_t)ptr - (uintptr_t)info.dli_saddr;
                    if (pdif) {
                        fprintf(fp, "+0x%lx", (long)pdif);
                    }
                    fptr += 2;
                } else if (fptr[1] == 'f') {
                    fprintf(fp, "%s", info.dli_fname);
                    fptr += 2;
                }
            }
            if (fptr == '\0') {
                fprintf(fp, "%c", '\n');
                nl = 1;

                continue;
            }
        } else {
            while (fptr[0] != '\0') {
                if (fptr[0] != '%') {
                    cptr[0] = fptr[0];
                    cptr++;
                    fptr++;
                } else if (fptr[1] == 'a') {
                    slen += snprintf(cptr, len - slen, "%p", ptr);
                    fptr += 2;
                } else if (fptr[1] == 'n') {
                    slen += snprintf(cptr, len - slen, "%s", info.dli_sname);
                    fptr += 2;
                } else if (fptr[1] == 'd') {
                    pdif = (uintptr_t)ptr - (uintptr_t)info.dli_saddr;
                    slen += snprintf(cptr, len - slen, "0x%lx",
                                     ptr, (long)pdif);
                    fptr += 2;
                } else if (fptr[1] == 'D') {
                    pdif = (uintptr_t)ptr - (uintptr_t)info.dli_saddr;
                    if (pdif) {
                        slen += snprintf(cptr, len - slen, "+0x%lx",
                                         (long)pdif);
                    }
                    fptr += 2;
                } else if (fptr[1] == 'f') {
                    slen += snprintf(cptr, len - slen, "%s", info.dli_fname);
                    fptr += 2;
                }
                pdif = (uintptr_t)cptr - (uintptr_t)tab;
                if (pdif >= (len >> 1)) {
                    len <<= 1;
                    mptr = realloc(tab, len);
                    if (!mptr) {
                        free(tab);

                        return (uintptr_t)-1;
                    }
                    tab = mptr;
                    cptr = mptr + slen;
                }
            }
            if (fptr == '\0') {
                cptr[0] = '\n';
                nl = 1;
                cptr++;

                continue;
            }
        }
    }
    if (!nl) {
        if (fp) {
            fprintf(fp, "%c", '\n');
        } else {
            cptr[0] = '\n';
        }
    }
    
    return (uintptr_t)mptr;
}

uintptr_t
_backtrace(void **buf, __btsize_t size, long syms, int fd)
{
    Dl_info        info;
    FILE          *fp = (fd >= 0) ? fdopen(fd, "a") : NULL;
    void          *ptr = NULL;
    void          *oldptr;
    void          *fptr = NULL;
    __btsize_t     lim = size - 1;
    uintptr_t      ret = 0;
    void          *adr;
    void         **pptr;
    uintptr_t      delta;
    __btsize_t     ndx;

    if ((syms) && !fp) {
        buf = malloc(size * sizeof(void *));
    } else if (!buf || !size) {

        return ret;
    }
    pptr = buf;
    if (size) {
        m_getretfrmadr(&fptr);
        m_loadretadr(fptr, &ptr);
        if (syms) {
            for (ndx = 0 ; ndx < lim ; ndx++) {
                if ((ptr) && ptr != oldptr) {
                    if (!fp) {
                        pptr[ndx] = ptr;
                    } else {
                        dladdr(ptr, &info);
                        adr = info.dli_saddr;
                        if (ptr != info.dli_saddr) {
                            delta = (uintptr_t)ptr - (uintptr_t)adr;
                            fprintf(fp, "0x%p <%s+0x%llx> at %s\n",
                                    ptr, info.dli_sname,
                                    (unsigned long long)delta, info.dli_fname);
                        } else {
                            fprintf(fp, "0x%p <%s> at %s\n",
                                    ptr, info.dli_sname, info.dli_fname);
                        }
                        fflush(fp);
                    }
                } else {
                    ret = ndx;
                    
                    break;
                }
                oldptr = ptr;
                m_getretfrmadr(&fptr);
                m_loadretadr(fptr, &ptr);
            }
        } else {
            for (ndx = 0 ; ndx < lim ; ndx++) {
                if ((ptr) && ptr != oldptr) {
                    pptr[ndx] = ptr;
                } else {
                    ret = ndx;
                    
                    break;
                }
                oldptr = ptr;
                m_getretfrmadr(&fptr);
                m_loadretadr(fptr, &ptr);
            }
        }
    }
    if (fp) {
        fclose(fp);

        ret = 0;
    } else if (syms) {

        ret = (uintptr_t)buf;
    }

    return ret;
}

#endif /* defined(_GNU_SOURCE) */

