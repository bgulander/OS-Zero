#ifndef __ZERO_RING_H__
#define __ZERO_RING_H__

#if !defined(RINGSHAREBUF)
#define RINGSHAREBUF 0
#endif

#include <stdint.h>
#include <limits.h>
#include <zero/cdecl.h>
#include <zero/param.h>
#include <zero/mtx.h>
#if !defined(__KERNEL__)
#if !defined(MALLOC) || !defined(FREE)
#include <stdlib.h>
#endif
#if !defined(MEMCPY)
#include <string.h>
#endif
#if !defined(MALLOC)
#define MALLOC(n)            malloc(n)
#endif
#if !defined(FREE)
#define FREE(ptr)            free(ptr)
#endif
#if !defined(MEMCPY)
#define MEMCPY(dest, src, n) memcpy(dest, src, n)
#endif
#endif /* !defined(__KERNEL__) */

#if (RINGSHAREBUF) && !defined(__KERNEL__)
#if defined(_ISOC11_SOURCE) && (_ISOC11_SOURCE)
#defined VALLOC(n)           aligned_alloc(PAGESIZE, n)
#elif (((defined(_BSD_SOURCE) && (_BSD_SOURCE))                         \
        || (defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE >= 500             \
                                       || ((defined(_XOPEN_SOURCE_EXTENDED) \
                                            && (_XOPEN_SOURCE_EXTENDED)))))) \
       && !((USEPOSIX200112)                                            \
            || (defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE >= 600))))     \
#define VALLOC(n)            valloc(n)
#elif (USEPOSIX200112 || (defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE >= 600)))
static __inline__ void *
VALLOC(size_t n)
{
    void * _ptr;

    if (posix_memalign(&_ptr, PAGESIZE, n)) {

        return NULL;
    }

    return _ptr;
}
#else
#define VALLOC(n)            memalign(PAGESIZE, n)
#endif
#endif /* RINGSHAREBUF && !__KERNEL__ */

/* MALLOC       - function used to allocate data buffer */
/* FREE         - function used to free buffers */
/* MEMCPY       - function used to copy data */
/* RING_ITEM    - type of items in ring buffer */
/* RING_INVAL   - invalid/non-present item value */

struct ringbuf {
    zeromtx        lk;
    volatile long  init;
    long           n;
    RING_ITEM     *base;
    RING_ITEM     *lim;
    RING_ITEM     *inptr;
    RING_ITEM     *outptr;
    long           pad;
    /* data buffer */
#if (RINGSHAREBUF)
    uint8_t       *data;
#else
    uint8_t        data[EMPTY];
#endif
} ALIGNED(PAGESIZE);

/*
 * initialise ring buffer
 * - if base == NULL, allocate the data buffer
 */
static __inline__ long
ringinit(void *ptr, void *base, long n)
{
    struct ringbuf *buf = ptr; 
    long            retval = 0;

    mtxlk(&buf->lk);
    if (buf->init) {
        mtxunlk(&buf->lk);

        return 1;
    }
    if (!base) {
#if (RINGSHAREBUF)
        base = VALLOC(n * sizeof(RING_ITEM));
#else
        base = MALLOC(n * sizeof(RING_ITEM));
#endif
        if (base) {
            retval++;
        }
    } else {
        retval++;
    }
    if (base) {
        buf->init = 1;
        buf->n = n;
        buf->base = base;
        buf->lim = (RING_ITEM *)((uint8_t *)base + n * sizeof(RING_ITEM));
        buf->inptr = base;
        buf->outptr = base;
    }
    mtxunlk(&buf->lk);

    return retval;
}

/* fetch next item from ring buffer */
static __inline__ RING_ITEM
ringget(struct ringbuf *buf)
{
    RING_ITEM item = RING_INVAL;

    mtxlk(&buf->lk);
    if (buf->inptr == buf->lim) {
        buf->inptr = buf->base;
    }
    if (buf->inptr != buf->outptr) {
        item = *buf->inptr++;
    }
    mtxunlk(&buf->lk);

    return item;
}

/* queue item into ring buffer */
static __inline__ RING_ITEM
ringput(struct ringbuf *buf, RING_ITEM val)
{
    RING_ITEM item = RING_INVAL;

    mtxlk(&buf->lk);
    if (buf->outptr == buf->lim) {
        buf->outptr = buf->base;
    }
    if (buf->outptr != buf->inptr) {
        *buf->outptr++ = val;
        item = val;
    } else if (!buf->init) {
        *buf->outptr++ = val;
        item = val;
        buf->init = 1;
    }
    mtxunlk(&buf->lk);

    return item;
}

/* resize data buffer */
static __inline__ void *
ringresize(struct ringbuf *buf, long n)
{
    void *ptr = NULL;
    void *src;

    mtxlk(&buf->lk);
    src = buf->base;
    ptr = MALLOC(n * sizeof(RING_ITEM));
    if (ptr) {
        if (src && (buf->init)) {
            MEMCPY(ptr, src, buf->n);
        }
        FREE(src);
        buf->base = ptr;
        buf->n = n;
    }
    mtxunlk(&buf->lk);

    return ptr;
}

#endif /* __ZERO_RING_H__ */

