/*
 * Copyright (C) 2008-2014 Tuomo Petteri Ven�l�inen. All rights
 * reserved.
 *
 * See the file LICENSE for more information about using this software.
 */

#define MAPFALLBACK   0
#define VARSIZEBUF    1
#define MALLOCCCTUNE  0
#define MALLOCBUFMAP  1
#define NEWBUF        0
#define ARNREFCNT     1
#define MALLOCTRIM    0 /* TODO: try to fix heap trim */
#define MALLOCZEROSTK 0
#define MALLOCBUFHDR  1

#if !defined(VALGRIND)
#define VALGRIND   0
#endif

#define DEBUGMTX   0
#define GNUMALLOCHOOKS 1

#if !defined(MTSAFE)
#define MTSAFE     1
#endif

#define SMALLBUF   1
#define CONSTBUF   0
#define NOSBRK     0
#define TAILFREE   0
#define TESTING    1

#define NEWMAP     1
#define NEWSLAB    1
#define FREEBUF    0
#define ISTK       1
#define NOSTK      0
#define BIGSLAB    0
#define BIGHDR     1
#define INTSTAT    0
#define STAT       0
#define STDIO      1
#define FREEBITMAP 0

#define ZEROMTX    1
#define PTHREAD    1

#include <features.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#if (STDIO)
#include <stdio.h>
#endif
#if (ZEROMTX)
#include <zero/mtx.h>
typedef volatile long   LK_T;
#elif (SPINLK)
#include <zero/spin.h>
typedef long            LK_T;
#elif (PTHREAD)
typedef pthread_mutex_t LK_T;
#endif
#if (MALLOCHASH)
#include <zero/hash.h>
#endif
#if (VALGRIND)
#include <valgrind/valgrind.h>
#endif
#include <zero/param.h>
#include <zero/cdecl.h>
#include <zero/trix.h>
//#include <mach/param.h>
#include <zero/unix.h>
//#include "_malloc.h"
#if (MTSAFE)
#include <unistd.h>
#if defined(_GNU_SOURCE)
#include <sys/sysinfo.h>
#endif
#endif /* MTSAFE */
#if (ZEROFUTEX)
#include <zero/futex.h>
#endif

/*
 * TODO
 * ----
 * - tune nmbuf() and other behavior
 * - implement mallopt()
 * - improve fault handling
 */

/*
 * THANKS
 * ------
 * - Matthew 'kinetik' Gregan for pointing out bugs, giving me cool routines to
 *   find more of them, and all the constructive criticism etc.
 * - Thomas 'Freaky' Hurst for patience with early crashes, 64-bit hints, and
 *   helping me find some bottlenecks.
 * - Henry 'froggey' Harrington for helping me fix issues on AMD64.
 * - Dale 'swishy' Anderson for the enthusiasm, encouragement, and everything
 *   else.
 * - Martin 'bluet' Stensg�rd for an account on an AMD64 system for testing
 *   earlier versions.
 */

/*
 *        malloc buffer layers
 *        --------------------
 *
 *                --------
 *                | mag  |
 *                --------
 *                    |
 *                --------
 *                | slab |
 *                --------
 *        --------  |  |   -------
 *        | heap |--|  |---| map |
 *        --------         -------
 *
 *        mag
 *        ---
 *        - magazine cache with allocation stack of pointers into the slab
 *          - LIFO to reuse freed blocks of virtual memory
 *
 *        slab
 *        ----
 *        - slab allocator bottom layer
 *        - power-of-two size slab allocations
 *          - supports both heap and mapped regions
 *
 *        heap
 *        ----
 *        - process heap segment
 *          - sbrk() interface; needs global lock
 *
 *        map
 *        ---
 *        - process map segment
 *          - mmap() interface; thread-safe
 */

#define SBRK_FAILED ((void *)-1L)

#define HACKS   0
//#define ZEROMTX 1
//#define STAT    0

#define SPINLK  0
/* NOT sure if FreeBSD still needs spinlocks */
#if defined(__FreeBSD__)
#undef SPINLK
#define SPINLK  1
#endif

#ifdef _REENTRANT
#ifndef MTSAFE
#define MTSAFE  1
#endif
#endif

/* experimental */
#define TUNEBUF 0

/* basic allocator parameters */
#define BLKMINLOG2    CLSIZELOG2  /* minimum-size allocation */
#if (SMALLBUF)
#if (VARSIZEBUF)
#define SLABLOG2      18
#define SLABBIGLOG2   16
#define SLABTINYLOG2  12
#define SLABTEENYLOG2 10
#define MAPMIDLOG2    22
#define MAPBIGLOG2    24
#else
#define SLABLOG2      20
#define SLABBIGLOG2   16
#define SLABTINYLOG2  13
#define SLABTEENYLOG2 8
#define MAPMIDLOG2    22
#define MAPBIGLOG2    24
#endif
#elif (!BIGSLAB)
#define SLABLOG2      19
#define SLABBIGLOG2   16
#define SLABTINYLOG2  13
#define SLABTEENYLOG2 8
#define MAPMIDLOG2    20
#define MAPBIGLOG2    24
#else
#define SLABLOG2      23 /* base size for heap allocations */
#define SLABBIGLOG2   20
#define MAPMIDLOG2    26
#define MAPBIGLOG2    28
#endif
#define MINSZ         (1UL << BLKMINLOG2)
#define HQMAX         SLABLOG2
//#define HQMAX         20
#define NBKT          (8 * PTRSIZE)
#if (MTSAFE)
#if defined(_SC_NROCESSORS_CONF)
#define NARN          (2 * sysconf(_SC_NPROCESSORS_CONF))
#elif defined(_GNU_SOURCE)
#define NARN          (2 * get_nprocs_conf())
#else
#define NARN          16
#endif
#else
#define NARN          1
#endif

/* lookup tree of tables */

#define l1ndx(ptr) (((uintptr_t)ptr >> L1NDX) & ((1 << NL1BIT) - 1))
#define l2ndx(ptr) (((uintptr_t)ptr >> L2NDX) & ((1 << NL2BIT) - 1))
#define l3ndx(ptr) (((uintptr_t)ptr >> L3NDX) & ((1 << NL3BIT) - 1))

#if (PTRBITS > 32)

#define NL1KEY     (1UL << NL1BIT)
#define NL2KEY     (1UL << NL2BIT)
#define NL3KEY     (1UL << NL3BIT)
#define L1NDX      (L2NDX + NL2BIT)
#define L2NDX      (L3NDX + NL3BIT)
//#define L3NDX      SLABLO2G
#define L3NDX      BLKMINLOG2
#define NL1BIT     16

#if (PTRBITS > 48)

#define NL2BIT     16
#if (ADRBITS < PTRBITS)
#define NL3BIT     (ADRBITS - BLKMINLOG2 - NL1BIT - NL2BIT)
#else
#define NL3BIT     (PTRBITS - BLKMINLOG2 - NL1BIT - NL2BIT)
#endif

#else

#define NL2BIT     (PTRBITS - BLKMINLOG2 - NL1BIT)
#define NL3BIT     0

#endif /* PTRBITS > 48 */

#endif /* PTRBITS <= 32 */

/* macros */

#if (MALLOCBUFMAP)
#define nbufmap(bid) (1L << nbufmaplog2(bid))
#define nbufmaplog2(bid)                                                \
    (((bid) <= MAPMIDLOG2)                                              \
     ? 2                                                                \
     : (((bid) <= MAPBIGLOG2)                                           \
        ? 1                                                             \
        : 0))
#endif
#if 0
#define narnbufmag(bid)   (1L << narnbufmaglog2(bid))
#define narnbufmaglog2(bid)                                             \
    (((bid) <= SLABTEENYLOG2)                                           \
     ? 2                                                                \
     : (((bid) <= SLABTINYLOG2)                                         \
        ? 3                                                             \
        : (((bid) <= MAPMIDLOG2)                                        \
           ? 2                                                          \
           : (((bid) <= MAPBIGLOG2)                                     \
              ? 1                                                       \
              : 0))))
#endif
#if (NOSBRK)
#define ismapbkt(bid)     0
#else
#define ismapbkt(bid)     ((bid) > HQMAX)
#endif
#if (TUNEBUF)
//#define isbufbkt(bid)     ((bid) <= MAPMIDLOG2)
//#define isbufbkt(bid)     0
#if (!CONSTBUF)
#define nbufinit(bid)     0
#endif
#define isbufbkt(bid)     (_nbuftab[(bid)])
#define nmagslablog2(bid) (_nslablog2tab[(bid)])
//#define nmagslablog2(bid) 0
#else
#define isbufbkt(bid)     0
#define nmagslablog2(bid) (ismapbkt(bid) ? nmaplog2(bid) : nslablog2(bid))
#define nslablog2(bid)    0
#define nmaplog2(bid)     0
#define nslablog2(bid)    0
#define nmaplog2(bid)     0
#endif

#if (MALLOCCCTUNE)
#undef SLABTEENYLOG2
#define SLABTEENYLOG2     8
#define nblklog2(bid)                                                   \
    (((!ismapbkt(bid))                                                  \
    ? (((bid) < SLABTEENYLOG2)                                          \
       ? (SLABLOG2 - (bid) + 2)                                         \
       : (((bid) < SLABTINYLOG2)                                        \
          ? (SLABLOG2 - (bid) + 1)                                      \
          : (SLABLOG2 - (bid))))                                        \
      : (((bid) < MAPMIDLOG2)                                           \
         ? 2                                                            \
         : (((bid) < MAPBIGLOG2)                                        \
            ? 1                                                         \
            : 0))))
#elif (VARSIZEBUF)
#define nblklog2(bid)                                                   \
    (((!ismapbkt(bid))                                                  \
      ? (((bid) < SLABTEENYLOG2)                                        \
         ? (SLABTINYLOG2 - (bid))                                       \
         : (((bid) < SLABTINYLOG2)                                      \
            ? (SLABBIGLOG2 - (bid))                                     \
            : (SLABLOG2 - (bid))))                                      \
      : 0))

#elif (NEWBUF)
#define nblklog2(bid)                                                   \
    (((!ismapbkt(bid))                                                  \
      ? (((bid) < SLABTEENYLOG2)                                        \
         ? (SLABLOG2 - (bid))                                           \
         : (((bid) < SLABTINYLOG2)                                      \
            ? (SLABLOG2 - (bid) + 1)                                    \
            : (((bid) < SLABBIGLOG2)                                    \
               ? (SLABLOG2 - (bid) + 2)                                 \
               : (SLABLOG2 - (bid)))))                                  \
      : (((bid) < MAPMIDLOG2)                                           \
         ? 2                                                            \
         : (((bid) < MAPBIGLOG2)                                        \
            ? 1                                                         \
            : 0))))
#elif (TUNEBUF)
#if (CONSTBUF)
#define nbufinit(bid)     0
#define nmagslablog2init(bid) 0
#else /* !CONSTBUF */
/* adjust how much is buffered based on current use */
#define nmagslablog2init(bid) 0
#define nmagslablog2up(m, v, t)                                         \
    do {                                                                \
        if (t >= (v)) {                                                 \
            for (t = 0 ; t < NBKT ; t++) {                              \
                _nslablog2tab[(t)] = m(t);                              \
            }                                                           \
        }                                                               \
    } while (0)
#if (NEWSLAB)
//#define nmagslablog2init(bid) 0
#define nmagslablog2m64(bid)                                            \
    (((ismapbkt(bid))                                                   \
      ? (((bid) <= MAPMIDLOG2)                                          \
         ? 1                                                            \
         : (((bid) <= MAPBIGLOG2)                                       \
            ? 0                                                         \
            : 0))                                                       \
      : (((bid) <= SLABTEENYLOG2)                                       \
         ? 0                                                            \
         : (((bid) <= SLABTINYLOG2)                                     \
            ? 1                                                         \
            : 1))))
#define nmagslablog2m128(bid)                                           \
    (((ismapbkt(bid))                                                   \
      ? (((bid) <= MAPMIDLOG2)                                          \
         ? 1                                                            \
         : (((bid) <= MAPBIGLOG2)                                       \
            ? 1                                                         \
            : 0))                                                       \
      : (((bid) <= SLABTEENYLOG2)                                       \
         ? 0                                                            \
         : (((bid) <= SLABTINYLOG2)                                     \
            ? 1                                                         \
            : 2))))
#define nmagslablog2m256(bid)                                           \
    (((ismapbkt(bid))                                                   \
      ? (((bid) <= MAPMIDLOG2)                                          \
         ? 2                                                            \
         : (((bid) <= MAPBIGLOG2)                                       \
            ? 1                                                         \
            : 0))                                                       \
      : (((bid) <= SLABTEENYLOG2)                                       \
         ? 0                                                            \
         : (((bid) <= SLABTINYLOG2)                                     \
            ? 1                                                         \
            : 2))))
#define nmagslablog2m512(bid)                                           \
    (((ismapbkt(bid))                                                   \
      ? (((bid) <= MAPMIDLOG2)                                          \
         ? 3                                                            \
         : (((bid) <= MAPBIGLOG2)                                       \
            ? 1                                                         \
            : 0))                                                       \
      : (((bid) <= SLABTEENYLOG2)                                       \
         ? 0                                                            \
         : (((bid) <= SLABTINYLOG2)                                     \
            ? 1                                                         \
            : 2))))
#elif (BIGSLAB)
//#define nmagslablog2init(bid) 0
#define nmagslablog2m64(bid)                                            \
    (((ismapbkt(bid))                                                   \
      ? 0                                                               \
      : (((bid) <= SLABTEENYLOG2)                                       \
         ? -3                                                           \
         : (((bid) <= SLABTINYLOG2)                                     \
            ? -3                                                        \
            : 0))))
#define nmagslablog2m128(bid)                                           \
    (((ismapbkt(bid))                                                   \
      ? 0                                                               \
      : (((bid) <= SLABTEENYLOG2)                                       \
         ? -2                                                           \
         : (((bid) <= SLABTINYLOG2)                                     \
            ? -3                                                        \
            : 0))))
#define nmagslablog2m256(bid)                                           \
    (((ismapbkt(bid))                                                   \
      ? 0                                                               \
      : (((bid) <= SLABTEENYLOG2)                                       \
         ? -1                                                           \
         : (((bid) <= SLABTINYLOG2)                                     \
            ? -2                                                        \
            : 0))))
#define nmagslablog2m512(bid)                                           \
    (((ismapbkt(bid))                                                   \
      ? 0                                                               \
      : (((bid) <= SLABTEENYLOG2)                                       \
         ? 0                                                            \
         : (((bid) <= SLABTINYLOG2)                                     \
            ? -1                                                        \
            : 0))))
#else
#define nmagslablog2init(bid)                                           \
    ((ismapbkt(bid))                                                    \
     ? (((bid) <= 23)                                                   \
        ? 2                                                             \
        : 1)                                                            \
     : (((bid) <= SLABTINYLOG2)                                         \
        ? 1                                                             \
        : (((bid) <= SLABBIGLOG2)                                       \
           ? 1                                                          \
           : 2)))
#define nmagslablog2m64(bid)                                            \
    ((ismapbkt(bid))                                                    \
     ? 0                                                                \
     : (((bid) <= SLABTINYLOG2)                                         \
        ? 0                                                             \
        : (((bid) <= SLABBIGLOG2)                                       \
           ? 1                                                          \
           : 2)))
#define nmagslablog2m128(bid)                                           \
    ((ismapbkt(bid))                                                    \
     ? (((bid) <= 23)                                                   \
        ? 1                                                             \
        : 0)                                                            \
     : (((bid) <= SLABTINYLOG2)                                         \
        ? 1                                                             \
        : (((bid) <= SLABBIGLOG2)                                       \
           ? 1                                                          \
           : 2)))
#define nmagslablog2m256(bid)                                           \
    ((ismapbkt(bid))                                                    \
     ? (((bid) <= 24)                                                   \
        ? 1                                                             \
        : 0)                                                            \
     : (((bid) <= SLABTINYLOG2)                                         \
        ? 1                                                             \
        : (((bid) <= SLABBIGLOG2)                                       \
           ? 1                                                          \
           : 2)))
#define nmagslablog2m512(bid)                                           \
    ((ismapbkt(bid))                                                    \
     ? (((bid) <= 24)                                                   \
        ? 1                                                             \
        : 0)                                                            \
     : (((bid) <= SLABTINYLOG2)                                         \
        ? 0                                                             \
        : (((bid) <= SLABBIGLOG2)                                       \
           ? 1                                                          \
           : 2)))
#endif
#endif /* !CONSTBUF */
#if (FREEBUF)
#define nblklog2(bid)                                                   \
    ((!ismapbkt(bid))                                                   \
     ? (SLABLOG2 - (bid))                                               \
     : 0)
#else
#define nblklog2(bid)                                                   \
    ((!ismapbkt(bid))                                                   \
     ? (SLABLOG2 - (bid) + nmagslablog2(bid))                           \
     : nmagslablog2(bid))
#endif
#else
#define nblklog2(bid)     (ismapbkt(bid) ? 1UL : SLABLOG2 - (bid))
#endif /* TUNEBUF */
#define nblk(bid)         (1UL << nblklog2(bid))
#define NBSLAB            (1UL << SLABLOG2)
#define nbmap(bid)        (1UL << (nblklog2(bid) + (bid)))
#define nbmag(bid)        (1UL << (nblklog2(bid) + (bid)))

#if (MALLOCHASH)
#define NHASHBIT          20
#define NHASH             (1U << NHASHBIT)
#elif (PTRBITS <= 32)
#define NSLAB             (1UL << (PTRBITS - SLABLOG2))
#define slabid(ptr)       ((uintptr_t)(ptr) >> SLABLOG2)
#endif
#if (BIGHDR)
#define NBHDR             (16 * PAGESIZE)
#else
#define NBHDR             (4 * PAGESIZE)
//#define NBHDR             PAGESIZE
#endif
#if (BIGHDR)
#define NBUFHDR           16
#else
#define NBUFHDR           8
#endif
#define NBUFMAP           8

#define thrid()           ((_aid >= 0) ? _aid : (_aid = getaid()))
#define blksz(bid)        (1UL << (bid))
#define usrsz(bid)        (blksz(bid) - RZSZ)
#define magfull(mag)      (!(mag)->cur)
#define magempty(mag)     ((mag)->cur == (mag)->max)
#if (ALNSTK)
#define nbstk(bid)        max(nblk(bid) * sizeof(void *), PAGESIZE)
#define nbalnstk(bid)     nbstk(bid)
#else
#define nbstk(bid)        max((nblk(bid) << 1) * sizeof(void *), PAGESIZE)
#endif
#if (FREEBITMAP)
#define mapfmap(n)        mapanon(_mapfd,                               \
                                  rounduppow2(((n) * sizeof(uint8_t)) >> 3, \
                                              PAGESIZE))
#define unmapfmap(mag)    unmapanon((mag)->fmap,                        \
                                    rounduppow2(((mag)->max * sizeof(uint8_t)) >> 3, \
                                                PAGESIZE))
#endif
#define mapstk(n)         mapanon(_mapfd,                               \
                                  rounduppow2(((n) << 1) * sizeof(void *), \
                                              PAGESIZE))
#define unmapstk(mag)     unmapanon((mag)->bptr,                        \
                                    rounduppow2((mag->max << 1) * sizeof(void *), PAGESIZE))
#define putblk(mag, ptr)                                                \
    ((gtpow2(mag->max, 1)                                               \
      ? (((void **)(mag)->bptr)[--(mag)->cur] = (ptr))                  \
      : ((mag)->cur = 0, (mag)->adr = (ptr))))
#if !(MALLOCZEROSTK)
#define getblk(mag)                                                     \
    ((gtpow2(mag->max, 1)                                               \
      ? (((void **)(mag)->bptr)[(mag)->cur++])                          \
      : ((mag)->cur = 1, ((mag)->adr))))
#endif
#define NPFBIT BLKMINLOG2
#define BPMASK (~((1UL << NPFBIT) - 1))
#define BDIRTY 0x01UL
#define BALIGN 0x02UL
#define BMAP   0x04UL
#define clrptr(ptr)       ((void *)((uintptr_t)(ptr) & BPMASK))
#define setflg(ptr, flg)  ((void *)((uintptr_t)(ptr) | (flg)))
#define chkflg(ptr, flg)  ((uintptr_t)(ptr) & (flg))
#define blkid(mag, ptr)                                                 \
    ((mag)->max + (((uintptr_t)(ptr) - ((uintptr_t)(mag)->adr & ~BMAP)) >> (mag)->bid))
#define putptr(mag, ptr1, ptr2)                                         \
    ((gtpow2((mag)->max, 1))                                            \
     ? (((void **)(mag)->bptr)[blkid(mag, ptr1)] = (ptr2))              \
     : ((mag)->bptr = (ptr2)))
#define getptr(mag, ptr)                                                \
    ((gtpow2((mag)->max, 1))                                            \
     ? (((void **)(mag)->bptr)[blkid(mag, ptr)])                        \
     : ((mag)->bptr))

/* red-zones haven't been implemented completely yet... some bugs. */
#define RZSZ     0
#if (RZSZ == 8)
#define markred(p) (*(uint64_t *)(p) = UINT64_C(0xb4b4b4b4b4b4b4b4))
#define chkred(p)                                                       \
    ((*(uint64_t *)(p) == UINT64_C(0xb4b4b4b4b4b4b4b4))                 \
     ? 0                                                                \
     : 1)
#endif

static void   initmall(void);
static void   relarn(void *arg);
static void * getmem(size_t size, size_t align, long zero);
static void   putmem(void *ptr);
static void * _realloc(void *ptr, size_t size, long rel);

#include <string.h>
#if (MTSAFE)
#define PTHREAD  1
#include <pthread.h>
#endif
#if (VALGRIND)
#include <valgrind/valgrind.h>
#endif
//#include <mach/mach.h>

#if (STAT)
#include <stdio.h>
#endif

/* synchronisation */

#if (ZEROMTX)
#if (DEBUGMTX)
#define mlk(mp)           fprintf(stderr, "LK: %d\n", __LINE__), mtxlk2(mp, _aid + 1)
#define munlk(mp)         fprintf(stderr, "UNLK: %d\n", __LINE__), mtxunlk2(mp, _aid + 1)
#define mtrylk(mp)        fprintf(stderr, "TRYLK: %d\n", __LINE__), mtxtrylk2(mp, _aid + 1)
#else
#define mlk(mp)           mtxlk2(mp, _aid + 1)
#define munlk(mp)         mtxunlk2(mp, _aid + 1)
#define mtrylk(mp)        mtxtrylk2(mp, _aid + 1)
#endif
#elif (SPINLK)
#define mlk(sp)           spinlk2(sp, _aid + 1)
#define munlk(sp)         spinunlk2(sp, _aid + 1)
#define mtrylk(sp)        spintrylk2(sp, _aid + 1)
#elif (MTSAFE)
#if (PTHREAD)
#define mlk(sp)           pthread_mutex_lock(sp)
#define munlk(sp)         pthread_mutex_unlock(sp)
#define mtrylk(sp)        pthread_mutex_trylock(sp)
#else
#define mlk(sp)           spinlk(sp)
#define munlk(sp)         spinunlk(sp)
#define mtrylk(sp)        spintrylk(sp)
#endif
#else
#define mlk(sp)
#define munlk(sp)
#define mtrylk(sp)
#endif
#define mlkspin(sp)       spinlk(sp)
#define munlkspin(sp)     spinunlk(sp)
#define mtrylkspin(sp)    spintry(sp)

#if (MALLOCHASH)
#define MPTRSIZE (8 * max(sizeof(long), sizeof(void *)))
struct mptr {
    void        *ptr;
    long         n;
    long         ntab;
    struct mptr *tab;
    struct mag  *mag;
    long         nbtab;
    long         pad[2]; /* pad to 8 long-words */
};
#endif

/* configuration */

#define CONF_INIT 0x00000001
#define VIS_INIT  0x00000002
struct mconf {
    long        flags;
#if (MTSAFE)
    LK_T        lk;
    LK_T        initlk;
    LK_T        arnlk;
    LK_T        heaplk;
#if (MALLOCTRIM)
    LK_T        trimlk;
#endif
#endif
    long        scur;
    long        acur;
    long        narn;
};

#if (ISTK) && (!NOSTK)
#define istk(bid)                                                       \
    ((nblk(bid) << 1) * sizeof(void *) <= NBHDR - offsetof(struct mag, data))
#else
#define istk(bid) 0
#endif
struct mag {
#if (MALLOCTRIM)
    struct mag *prevheap;
#endif
    long        cur;
    long        max;
    long        aid;
    long        bid;
    long        glob;
    void       *adr;
    void       *bptr;
    struct mag *prev;
    struct mag *next;
#if (FREEBITMAP)
    uint8_t    *fmap;
#endif
#if (ISTK)
    uint8_t     data[EMPTY];
#elif (!NOSTK)
    struct mag *stk[EMPTY];
#else
    struct mag *stk;
#endif
};

//#define nbarn() (blksz(bktid(sizeof(struct arn))))
#define nbarn() PAGESIZE
struct arn {
    struct mag  *btab[NBKT];
//    long         nref;
    long         hcur;
    long         nhdr;
    struct mag **htab;
    long         scur;
#if (MALLOCBUFHDR)
    LK_T         hdrlk;
#endif
    LK_T         lktab[NBKT];
};

struct mtree {
#if (MTSAFE)
    LK_T         lk;
#endif
    struct mag **tab;
    long         nblk;
};

/* globals */

#if (MALLOCHASH)
static LK_T                 _hlktab[NHASH] ALIGNED(PAGESIZE);
static struct mptr          _mtab[NHASH];
#endif
#if (INTSTAT) || (STAT)
static uint64_t             nalloc[NARN][NBKT];
static long                 nhdrbytes[NARN];
static long                 nstkbytes[NARN];
static long                 nmapbytes[NARN];
static long                 nheapbytes[NARN];
static unsigned long long   nheapreq[NBKT];
static unsigned long long   nmapreq[NBKT];
#endif
#if (TUNEBUF)
static long                 _nbuftab[NBKT];
#endif
#if (TUNEBUF)
static long                 _nslablog2tab[NBKT];
#endif
#if (MTSAFE)
static LK_T                 _flktab[NBKT];
static LK_T                 _blktab[NBKT];
#endif
#if (TAILFREE)
static struct mag          *_ftail[NBKT];
static struct mag          *_btail[NBKT];
#endif
static struct mag          *_ftab[NBKT];
static struct mag          *_btab[NBKT];
#if (HACKS) || (TUNEBUF) || (MALLOCBUFMAP)
static long                 _fcnt[NBKT];
#endif
#if (MALLOCBUFHDR)
static LK_T                 _hdrlktab[NBKT];
static long                 _nhdrtab[NBKT];
static struct mag          *_hdrtab[NBKT];
#endif
static void               **_mdir;
static struct arn         **_atab;
static struct mconf         _conf;
#if (MTSAFE) && (PTHREAD)
static pthread_key_t        _akey;
static __thread long        _aid = -1;
#else
static long                 _aid = -1;
#endif
static int64_t              _nbheap;
static int64_t              _nbmap;
static int                  _mapfd = -1;
#if (MALLOCTRIM)
//static LK_T                 _toplk;
static struct mag          *_topmag;
//static uint8_t             *_heaptop;
#endif
#if (ARNREFCNT)
static LK_T                *_nreflktab;
static unsigned long       *_nreftab;
#endif

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
void *(*__malloc_hook)(size_t size, const void *caller);
void *(*__realloc_hook)(void *ptr, size_t size, const void *caller);
void *(*__memalign_hook)(size_t align, size_t size, const void *caller);
void  (*__free_hook)(void *ptr, const void *caller);
void *(*__malloc_initialize_hook)(void);
void  (*__after_morecore_hook)(void);
#endif

/* utility functions */

static __inline__ long
bktid(size_t size)
{
    unsigned long bid = 1UL << (LONGSIZELOG2 + 3);
    unsigned long nlz;

    nlz = lzerol(size);
    bid -= nlz;
    if (!powerof2(size)) {
        bid++;
    }

    return bid;
}

#if (MALLOCZEROSTK)
static __inline__ void *
getblk(struct mag *mag)
{
    void *ptr;

    if (gtpow2(mag->max, 1)) {
        ptr = ((void **)(mag->bptr))[mag->cur];
        if (!ptr) {
            ptr = (uint8_t *)mag->adr + blksz(mag->bid) * mag->cur;
        }
        mag->cur++;
    } else {
        ptr = mag->adr;
        mag->cur = 1;
    }

    return ptr;
}
#endif

#if (MALLOCTRIM)
static long
trimheap(void)
{
    long           sz;
    unsigned long  bid;
    struct mag    *top;
    struct mag    *mag;
    struct mag    *low;
    struct mag    *tmp;

    fprintf(stderr, "trimheap()\n");
#if 0
    if (!mtrylk(&_conf.trimlk)) {
        
        return 0;
    }
#endif
#if 0
    if (!mtrylk(&_conf.heaplk)) {
        munlk(&_conf.trimlk);

        return 0;
    }
#endif
#if 0
    if (!mtrylk(&_toplk)) {
        munlk(&_conf.trimlk);

        return;
    }
#endif
    top = _topmag;
#if 0
    if (mtrylk(&top->lk)) {
        munlk(&_conf.trimlk);

        return;
    }
#endif
#if 0
    if (!mtrylk(&top->lk)) {
        munlk(&_conf.heaplk);
        munlk(&_conf.trimlk);
        
        return;
    }
#endif
    sz = 0;
    mag = top;
    while (mag) {
#if 0
        if (!mtrylk(&mag->lk)) {
            tmp = top;
            while (tmp != mag) {
                munlk(&tmp->lk);
                tmp = tmp->prevheap;

                return 0;
            }

            break;
        }
#endif
        bid = mag->bid;
        if (magfull(mag)) {
            if (mag->prev) {
                mag->prev->next = mag->next;
            } else if (mag->glob) {
                _btab[bid] = mag->next;
            } else {
                _ftab[mag->bid] = mag->next;
            }
            if (mag->next) {
                mag->next->prev = NULL;
#if (TAILFREE)
            } else if (mag->glob) {
                _btail[bid] = mag->prev;
            } else {
                _ftail[bid] = mag->prev;
#endif
            }
            sz += mag->max * blksz(bid);
            mag = mag->prevheap;
        } else {
//            munlk(&mag->lk);

            break;
        }
    }
    low = mag;
    if (low != top) {
//        sz = _heaptop - (uint8_t *)low->adr;
        fprintf(stderr, "TRIM: %ld\n", (unsigned long)sz);
        sbrk(-sz);
        mag = top;
        while (mag != low) {
//            munlk(&mag->lk);
#if (VALGRIND)
            if (RUNNING_ON_VALGRIND) {
                VALGRIND_FREELIKE_BLOCK(mag->adr, 0);
            }
#endif
            mag = mag->prevheap;
        }
        _topmag = low;
//        _heaptop = low->adr + low->max * blksz(low->bid);
    }
//    munlk(&_toplk);
//    munlk(&_conf.trimlk);
//    munlk(&top->lk);

    return sz;
}
#endif

#if (MTSAFE)
static long
getaid(void)
{
    long aid;

    mlk(&_conf.arnlk);
    aid = _conf.acur++;
    _conf.acur &= NARN - 1;
#if (ARNREFCNT)
    mlk(&_nreflktab[aid]);
    _nreftab[aid]++;
    munlk(&_nreflktab[aid]);
#endif
    pthread_setspecific(_akey, _atab[aid]);
    munlk(&_conf.arnlk);

    return aid;
}
#endif

/* fork() management */
/* TODO: acquire and release MALLOCHASH locks if feasible */

#if (MTSAFE)

static void
prefork(void)
{
    long        aid;
    long        bid;
    struct arn *arn;

    mlk(&_conf.initlk);
    mlk(&_conf.arnlk);
#if (MALLOCTRIM)
    mlk(&_conf.trimlk);
#endif
    mlk(&_conf.heaplk);
    mlk(&_conf.lk);
    aid = _conf.narn;
    while (aid--) {
        arn = _atab[aid];
        for (bid = 0 ; bid < NBKT ; bid++) {
            mlk(&arn->lktab[bid]);
        }
#if (MALLOCBUFHDR)
        mlk(&arn->hdrlk);
#endif
    }
    bid = NBKT;
    while (bid--) {
#if (MALLOCBUFHDR)
        mlk(&_hdrlktab[bid]);
#endif
        mlk(&_flktab[bid]);
        mlk(&_blktab[bid]);
    }

    return;
}

static void
postfork(void)
{
    long        aid;
    long        bid;
    struct arn *arn;

    bid = NBKT;
    while (bid--) {
        munlk(&_blktab[bid]);
        munlk(&_flktab[bid]);
#if (MALLOCBUFHDR)
        munlk(&_hdrlktab[bid]);
#endif
    }
    aid = _conf.narn;
    munlk(&_conf.lk);
    while (aid--) {
        arn = _atab[aid];
#if (MALLOCBUFHDR)
        munlk(&arn->hdrlk);
#endif
        for (bid = 0 ; bid < NBKT ; bid++) {
            munlk(&arn->lktab[bid]);
        }
    }
    munlk(&_conf.heaplk);
#if (MALLOCTRIM)
    munlk(&_conf.trimlk);
#endif
    munlk(&_conf.arnlk);
    munlk(&_conf.initlk);

    return;
}

static void
relarn(void *arg)
{
    struct arn *arn = arg;
#if (TUNEBUF) || (MALLOCBUFMAP)
    long        n = 0;
#endif
#if (ARNREFCNT)
    long        aid = thrid();
    long        nref;
#endif
    long        bid;
    struct mag *mag;
    struct mag *head;

#if (ARNREFCNT)
    mlk(&_nreflktab[aid]);
    nref = --_nreftab[aid];
    munlk(&_nreflktab[aid]);
#endif
//    nref = --arn->nref;
    if (!nref) {
        bid = NBKT;
#if (MALLOCTRIM)
        mlk(&_conf.trimlk);
#endif
        while (bid--) {
            mlk(&arn->lktab[bid]);
            head = arn->btab[bid];
            if (head) {
#if (TUNEBUF) || (MALLOCBUFMAP)
                n++;
#endif
                mag = head;
                while (mag->next) {
                    mag->glob = 1;
#if (HACKS)
                    n++;
#endif
                    mag = mag->next;
                }
                mlk(&_blktab[bid]);
                mag->glob = 1;
#if (TAILFREE)
                if (!_btab[bid]) {
                    _btab[bid] = head;
                    _btail[bid] = mag;
                } else {
                    head->prev = _btail[bid];
                    if (_btail[bid]) {
                        _btail[bid]->next = head;
                    } else {
                        _btab[bid] = head;
                    }
                    _btail[bid] = mag;
                }
#else
                mag->next = _btab[bid];
                if (_btab[bid]) {
                    _btab[bid]->prev = mag;
                }
                _btab[bid] = head;
#endif
#if (TUNEBUF) || (MALLOCBUFMAP)
                _fcnt[bid] += n;
#endif
                munlk(&_blktab[bid]);
                arn->btab[bid] = NULL;
            }
            munlk(&arn->lktab[bid]);
        }
#if (MALLOCTRIM)
        munlk(&_conf.trimlk);
#endif
    }

    return;
}

/* statistics */

#if (STAT) && (STDIO)
void
printstat(void)
{
    long l;

    for (l = 0 ; l < NBKT ; l++) {
        fprintf(stderr, "%ld\t%lu\t%lu\n", l, nheapreq[l], nmapreq[l]);
    }

    exit(0);
}
#elif (INTSTAT) && (STDIO)
void
printintstat(void)
{
    FILE *fp = NULL;
    long  aid;
    long  bkt;
    long  nbhdr = 0;
    long  nbstk = 0;
    long  nbheap = 0;
    long  nbmap = 0;

    fp = fopen("/tmp/malloc.log", "a");
    if (fp) {
        for (bkt = 0 ; bkt < NBKT ; bkt++) {
            if (nheapreq[bkt]) {
                fprintf(fp, "NREQ[%lx]: %llu\n", bkt, nheapreq[bkt]);
            } else if (nmapreq[bkt]) {
                fprintf(fp, "NREQ[%lx]: %llu\n", bkt, nmapreq[bkt]);
            }
        }
        for (aid = 0 ; aid < NARN ; aid++) {
            for (bkt = 0 ; bkt < NBKT ; bkt++) {
                if (nalloc[aid][bkt]) {
                    fprintf(fp, "NALLOC[%lx][%lx]: %lld\n",
                            aid, bkt, (long long)nalloc[aid][bkt]);
                } else {
                    fprintf(fp, "\n");
                }
            }
        }
#if 0
        fprintf(fp, "TOTAL: hdr: %ld, stk: %ld, heap: %ld, map: %ld\n",
                nbhdr, nbstk, nbheap, nbmap);
#endif
        fclose(fp);
    }
}
#endif

#if (X11VIS)
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/Xlocale.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>

static LK_T  x11visinitlk;
long         x11visinit = 0;
Display     *x11visdisp = NULL;
Window       x11viswin = None;
Pixmap       x11vispmap = None;
GC           x11visinitgc = None;
GC           x11visfreedgc = None;
GC           x11visusedgc = None;
GC           x11visresgc = None;

#define x11vismarkfreed(ptr)                                            \
    do {                                                                \
        if (x11visinit) {                                               \
            int y = ((uintptr_t)(ptr) >> (BLKMINLOG2 + 10)) & 0x3ff;    \
            int x = ((uintptr_t)(ptr) >> BLKMINLOG2) & 0x3ff;           \
            XDrawPoint(x11visdisp, x11vispmap, x11visfreedgc, x, y);    \
        }                                                               \
} while (0)
#define x11vismarkres(ptr)                                              \
    do {                                                                \
        if (x11visinit) {                                               \
            int y = ((uintptr_t)(ptr) >> (BLKMINLOG2 + 10)) & 0x3ff;    \
            int x = ((uintptr_t)(ptr) >> BLKMINLOG2) & 0x3ff;           \
            XDrawPoint(x11visdisp, x11vispmap, x11visresgc, x, y);      \
        }                                                               \
    } while (0)
#define x11vismarkused(ptr)                                             \
    do {                                                                \
        if (x11visinit) {                                               \
            int y = ((uintptr_t)(ptr) >> (BLKMINLOG2 + 10)) & 0x3ff;    \
            int x = ((uintptr_t)(ptr) >> BLKMINLOG2) & 0x3ff;           \
            XDrawPoint(x11visdisp, x11vispmap, x11visusedgc, x, y);     \
        }                                                               \
    } while (0)

void
initx11vis(void)
{
    XColor col;
    XGCValues gcval;

//    mlk(&x11vislk);
    mlk(&x11visinitlk);
    if (x11visinit) {
        munlk(&x11visinitlk);

        return;
    }
    XInitThreads();
    x11visdisp = XOpenDisplay(NULL);
    if (x11visdisp) {
        x11viswin = XCreateSimpleWindow(x11visdisp,
                                        DefaultRootWindow(x11visdisp),
                                        0, 0,
                                        1024, 1024, 0,
                                        BlackPixel(x11visdisp,
                                                   DefaultScreen(x11visdisp)),
                                        WhitePixel(x11visdisp,
                                                   DefaultScreen(x11visdisp)));
        if (x11viswin) {
            XEvent ev;

            x11vispmap = XCreatePixmap(x11visdisp,
                                       x11viswin,
                                       1024, 1024,
                                       DefaultDepth(x11visdisp,
                                                    DefaultScreen(x11visdisp)));

            gcval.foreground = WhitePixel(x11visdisp,
                                          DefaultScreen(x11visdisp));
            x11visinitgc = XCreateGC(x11visdisp,
                                     x11viswin,
                                     GCForeground,
                                     &gcval);

            XFillRectangle(x11visdisp,
                           x11vispmap,
                           x11visinitgc,
                           0, 0,
                           1024, 1024);

            col.red = 0x0000;
            col.green = 0x0000;
            col.blue = 0xffff;
            if (!XAllocColor(x11visdisp,
                             DefaultColormap(x11visdisp,
                                             DefaultScreen(x11visdisp)),
                             &col)) {

                return;
            }
            gcval.foreground = col.pixel;
            x11visfreedgc = XCreateGC(x11visdisp,
                                      x11viswin,
                                      GCForeground,
                                      &gcval);

            col.red = 0xffff;
            col.green = 0x0000;
            col.blue = 0x0000;
            if (!XAllocColor(x11visdisp,
                             DefaultColormap(x11visdisp,
                                             DefaultScreen(x11visdisp)),
                             &col)) {

                return;
            }
            gcval.foreground = col.pixel;
            x11visusedgc = XCreateGC(x11visdisp,
                                     x11viswin,
                                     GCForeground,
                                     &gcval);

            col.red = 0x0000;
            col.green = 0xffff;
            col.blue = 0x0000;
            if (!XAllocColor(x11visdisp,
                             DefaultColormap(x11visdisp,
                                             DefaultScreen(x11visdisp)),
                             &col)) {

                return;
            }
            gcval.foreground = col.pixel;
            x11visresgc = XCreateGC(x11visdisp,
                                    x11viswin,
                                    GCForeground,
                                    &gcval);
            
            XSelectInput(x11visdisp, x11viswin, ExposureMask);
            XMapRaised(x11visdisp, x11viswin);
            do {
                XNextEvent(x11visdisp, &ev);
            } while (ev.type != Expose);
            XSelectInput(x11visdisp, x11viswin, NoEventMask);
        }
    }
    x11visinit = 1;
    munlk(&x11visinitlk);
//    munlk(&x11vislk);
}
#endif

static void
initmall(void)
{
    long        bid = NBKT;
    long        aid = NARN;
    long        ofs;
    uint8_t    *ptr;
#if (MALLOCHASH)
    long        l;
    long        n;
#endif

    mlk(&_conf.initlk);
    if (_conf.flags & CONF_INIT) {
        munlk(&_conf.initlk);
        
        return;
    }
#if (STAT)
    atexit(printstat);
#elif (INTSTAT)
    atexit(printintstat);
#endif
#if (MMAP_DEV_ZERO)
    _mapfd = open("/dev/zero", O_RDWR);
#endif
#if (MTSAFE)
#if (MALLOCTRIM) && (PTHREAD) && !(ZEROMTX)
    pthread_mutex_init(&_conf.trimlk, NULL);
#endif
    mlk(&_conf.arnlk);
    _atab = mapanon(_mapfd, NARN * sizeof(struct arn **));
    ptr = mapanon(_mapfd, NARN * nbarn());
    aid = NARN;
    while (aid--) {
        _atab[aid] = (struct arn *)ptr;
        ptr += nbarn();
    }
#if (ARNREFCNT)
    _nreftab = mapanon(_mapfd, NARN * sizeof(unsigned long));
    _nreflktab = mapanon(_mapfd, NARN * sizeof(LK_T));
#endif
    aid = NARN;
    while (aid--) {
        for (bid = 0 ; bid < NBKT ; bid++) {
#if (ZEROMTX)
            mtxinit(&_atab[aid]->lktab[bid]);
#elif (PTHREAD) && !SPINLK
            pthread_mutex_init(&_atab[aid]->lktab[bid], NULL);
#endif
        }
        _atab[aid]->hcur = NBUFHDR;
    }
    _conf.narn = NARN;
    pthread_key_create(&_akey, relarn);
    munlk(&_conf.arnlk);
#endif
#if (PTHREAD)
    pthread_atfork(prefork, postfork, postfork);
#endif
#if (PTHREAD)
    while (bid--) {
#if (ZEROMTX)
        mtxinit(&_flktab[bid]);
        mtxinit(&_blktab[bid]);
#if (MALLOCBUFHDR)
        mtxinit(&_hdrlktab[bid]);
#endif
#elif (PTHREAD) && !SPINLK
        pthread_mutex_init(&_flktab[bid], NULL);
        pthread_mutex_init(&_blktab[bid], NULL);
#if (MALLOCBUFHDR)
        pthread_mutex_init(&_hdrlktab[bid], NULL);
#endif
#endif
    }
#endif
#if (!NOSBRK)
#if (MALLOCTRIM)
    mlk(&_conf.trimlk);
#endif
    mlk(&_conf.heaplk);
    ofs = NBSLAB - ((long)growheap(0) & (NBSLAB - 1));
    if (ofs != NBSLAB) {
        growheap(ofs);
    }
#if (MALLOCTRIM)
//    _heaptop = growheap(0);
#endif
    munlk(&_conf.heaplk);
#if (MALLOCTRIM)
    munlk(&_conf.trimlk);
#endif
#endif
#if !(MALLOCHASH)
#if (PTRBITS <= 32)
    _mdir = mapanon(_mapfd, NSLAB * sizeof(void *));
#else
    _mdir = mapanon(_mapfd, NL1KEY * sizeof(void *));
#endif
#endif /* !MALLOCHASH */
#if (TUNEBUF)
    for (bid = 0 ; bid < NBKT ; bid++) {
        _nbuftab[bid] = nbufinit(bid);
        _nslablog2tab[bid] = nmagslablog2init(bid);
    }
#endif
    _conf.flags |= CONF_INIT;
    munlk(&_conf.initlk);
#if (X11VIS)
    initx11vis();
#endif
#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__malloc_initialize_hook) {
        __malloc_initialize_hook();
    }
#endif

    return;
}

#if (MTSAFE)

#if (MALLOCHASH)

static struct mag *
findmag(void *ptr)
{
    uintptr_t    key = (uintptr_t)ptr;
    struct mptr *mptr;
    struct mag  *mag;

    key = hashq128(&key,sizeof(uintptr_t), NHASHBIT);
    mlk(&_hlktab[key]);
    mptr = &_mtab[key];
    if (mptr->ptr == ptr) {
        mag = mptr->mag;
        munlk(&_hlktab[key]);

        return mag;
    } else {
        struct mptr *lim;

#if 0
        mptr = mptr->tab;
        if (mptr) {
            lim = mptr + mptr->ntab;
            do {
                if (mptr->ptr == ptr) {
                    mag = mptr->mag;
                    munlk(&_hlktab[key]);
                    
                    return mag;
                }
                mptr++;
            } while ((mptr->ptr) && mptr < lim);
            if (mptr == lim) {
                munlk(&_hlktab[key]);
                
                return NULL;
            }
        } else {
            munlk(&_hlktab[key]);
            
            return NULL;
        }
#endif
        lim = mptr->tab;
        mptr = mptr->tab + mptr->n;
        if (mptr) {
            do {
                if (mptr->ptr == ptr) {
                    mag = mptr->mag;
                    munlk(&_hlktab[key]);
                    
                    return mag;
                }
                mptr--;
            } while ((mptr->ptr) && mptr >= lim);
            if (mptr < lim) {
                munlk(&_hlktab[key]);
                
                return NULL;
            }
        } else {
            munlk(&_hlktab[key]);
            
            return NULL;
        }
    }
    munlk(&_hlktab[key]);

    return NULL;
}

static void
setmag(void *ptr,
       struct mag *mag)
{
    uintptr_t    key = (uintptr_t)ptr;
    struct mptr *mptr;

    key = hashq128(&key, sizeof(uintptr_t), NHASHBIT);
    mlk(&_hlktab[key]);
    mptr = &_mtab[key];
    if (!mag) {
        if (mptr->ptr == ptr) {
            mptr->ptr = NULL;
            mptr->mag = mag;
        } else {
            struct mptr *tmp = mptr->tab;
            long         ndx;

            if (tmp) {
                for (ndx = mptr->n - 1 ; ndx >= 0 ; ndx++) {
                    if (tmp[ndx].ptr == ptr) {
                        mptr->n--;
                        tmp[ndx] = tmp[mptr->n];
                    }
                }
            }
        }
    } else if (!mptr->ptr) {
        mptr->ptr = ptr;
        mptr->mag = mag;
    } else {
        if (!mptr ||mptr->n == mptr->ntab) {
            long         n = max(mptr->ntab << 1,
                                 PAGESIZE / MPTRSIZE);
            long         nb = rounduppow2(n * MPTRSIZE, PAGESIZE);
            struct mptr *tab = mapanon(_mapfd, nb);

            if (tab == MAP_FAILED) {
                munlk(&_hlktab[key]);

                exit(1);
            }
            if (mptr->tab) {
                memcpy(tab, mptr->tab, mptr->ntab * MPTRSIZE);
                bzero(tab + mptr->ntab, mptr->ntab * MPTRSIZE);
                unmapanon(mptr->tab,
                          mptr->nbtab);
            }
            mptr->tab = tab;
            mptr->ntab = n;
            mptr->nbtab = nb;
            mptr = &mptr->tab[mptr->n];
        } else {
            mptr = &mptr->tab[mptr->n];
        }
        mptr->n++;
        mptr->ptr = ptr;
        mptr->mag = mag;
    }
    munlk(&_hlktab[key]);

    return;
}

#else /* !MALLOCHASH */

#if (PTRBITS > 32)

#if (PTRBITS > 48)

static struct mag *
findmag(void *ptr)
{
    uintptr_t  l1 = l1ndx(ptr);
    uintptr_t  l2 = l2ndx(ptr);
    uintptr_t  l3 = l3ndx(ptr);
    void       *ptr1;
    void       *ptr2;
    struct mag *mag = NULL;

    ptr1 = _mdir[l1];
    if (ptr1) {
        ptr2 = ((void **)ptr1)[l2];
        if (ptr2) {
            mag = ((struct mag **)ptr2)[l3];
        }
    }

    return mag;
}

static void
setmag(void *ptr,
       struct mag *mag)
{
    uintptr_t    l1 = l1ndx(ptr);
    uintptr_t    l2 = l2ndx(ptr);
    uintptr_t    l3 = l3ndx(ptr);
    void        *ptr1;
    void        *ptr2;
    void       **pptr;
    struct mag **item;

    ptr1 = _mdir[l1];
    if (!ptr1) {
        _mdir[l1] = ptr1 = mapanon(_mapfd,
                                           NL2KEY * sizeof(void *));
        if (ptr1 == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif

            exit(1);
        }
    }
    pptr = ptr1;
    ptr2 = pptr[l2];
    if (!ptr2) {
        pptr[l2] = ptr2 = mapanon(_mapfd,
                                  NL3KEY * sizeof(struct mag *));
        if (ptr2 == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif

            exit(1);
        }
    }
    item = &((struct mag **)ptr2)[l3];
    *item = mag;

    return;
}

#elif (PTRBITS > 32)

static struct mag *
findmag(void *ptr)
{
    uintptr_t   l1 = l1ndx(ptr);
    uintptr_t   l2 = l2ndx(ptr);
    void       *ptr1;
    struct mag *mag = NULL;

    ptr1 = _mdir[l1];
    if (ptr1) {
        mag = ((struct mag **)ptr1)[l2];
    }

    return mag;
}

static void
setmag(void *ptr,
       struct mag *mag)
{
    uintptr_t    l1 = l1ndx(ptr);
    uintptr_t    l2 = l2ndx(ptr);
    void        *ptr1;
    struct mag **item;

    ptr1 = _mdir[l1];
    if (!ptr1) {
        _mdir[l1] = ptr1 = mapanon(_mapfd, NL2KEY * sizeof(struct mag *));
        if (ptr1 == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif

            exit(1);
        }
    }
    item = &((struct mag **)ptr1)[l2];
    *item = mag;

    return;
}

#else /* PTRBITS <= 32 */

#define findmag(ptr)     (_mdir[slabid(ptr)])
#define setmag(ptr, mag) (_mdir[slabid(ptr)] = (mag))

#endif

#endif

#endif /* MALLOCHASH */

#endif /* MTSAFE */

#if (MALLOCBUFHDR)
static struct mag *
gethdr(long aid, long bid)
#else
static struct mag *
gethdr(long aid)
#endif
{
    struct arn  *arn;
    long         cur;
    struct mag **hbuf;
    struct mag  *mag = NULL;
    uint8_t     *ptr;
    
    arn = _atab[aid];
#if (MALLOCBUFHDR)
    mlk(&arn->hdrlk);
    mlk(&_hdrlktab[bid]);
    mag = _hdrtab[bid];
    if (mag) {
#if (MALLOCTRIM)
//        mlk(&mag->lk);
#endif
        _hdrtab[bid] = mag->next;
        if (mag->next) {
            mag->next->prev = NULL;
        }
        _nhdrtab[bid]--;
        munlk(&_hdrlktab[bid]);
        munlk(&arn->hdrlk);
        
        return mag;
    }
    munlk(&_hdrlktab[bid]);
#endif /* MALLOCBUFHDR */
    hbuf = arn->htab;
    if (!arn->nhdr) {
        hbuf = mapanon(_mapfd, rounduppow2(NBUFHDR * NBHDR, PAGESIZE));
        if (hbuf != MAP_FAILED) {
#if (INTSTAT) || (STAT)
            nhdrbytes[aid] += rounduppow2(NBUFHDR * NBHDR, PAGESIZE);
#endif
            arn->htab = hbuf;
            arn->hcur = NBUFHDR;
            arn->nhdr = NBUFHDR;
        }
    }
    cur = arn->hcur;
    if (gtepow2(cur, NBUFHDR)) {
        mag = mapanon(_mapfd, rounduppow2(NBUFHDR * NBHDR, PAGESIZE));
        if (mag == MAP_FAILED) {
#ifdef ENOMEM
            errno = ENOMEM;
#endif
            
            return NULL;
#if (VALGRIND)
        } else if (RUNNING_ON_VALGRIND) {
            VALGRIND_MALLOCLIKE_BLOCK(mag,
                                      rounduppow2(NBUFHDR * NBHDR, PAGESIZE),
                                      0, 0);
#endif
        }
        ptr = (uint8_t *)mag;
        while (cur) {
            mag = (struct mag *)ptr;
            *hbuf++ = mag;
#if (!ISTK) && (!NOSTK)
            mag->bptr = mag->stk;
#endif
            cur--;
            ptr += NBHDR;
        }
    }
    hbuf = arn->htab;
#if (SYSDBG)
    _nhbuf++;
#endif
    mag = hbuf[cur++];
    arn->hcur = cur;
#if (MALLOCBUFHDR)
    munlk(&arn->hdrlk);
#endif

    return mag;
}

#if (TUNEBUF) && (!CONSTBUF)
static void
tunebuf(long val)
{
    static long tunesz = 0;
    long        nb = _nbheap + _nbmap;

    return;

    if (!tunesz) {
        tunesz = val;
    }
    if (val == 64 && nb >= 64 * 1024) {
        nmagslablog2up(nmagslablog2m64, val, nb);
    } else if (val == 128 && nb >= 128 * 1024) {
        nmagslablog2up(nmagslablog2m128, val, nb);
    } else if (val == 256 && nb >= 256 * 1024) {
        nmagslablog2up(nmagslablog2m256, val, nb);
    } else if (val == 512 && nb >= 512 * 1024) {
        nmagslablog2up(nmagslablog2m512, val, nb);
    }

    return;
}
#endif

static void *
getslab(long aid,
        long bid)
{
    uint8_t     *ptr = NULL;
    long         nb;
#if (TUNEBUF) && (!CONSTBUF)
    unsigned long tmp;
    static long tunesz = 0;
#endif

    if (!ismapbkt(bid)) {
        nb = nbmag(bid);
#if (MALLOCTRIM)
//        mlk(&_conf.trimlk);
#endif
        mlk(&_conf.heaplk);
        ptr = growheap(nb);
#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
        if (__after_morecore_hook) {
            __after_morecore_hook();
        }
#endif
        if (ptr != SBRK_FAILED) {
#if (MALLOCTRIM)
//            _heaptop = ptr + nb;
#endif
            munlk(&_conf.heaplk);
#if (MALLOCTRIM)
//            munlk(&_conf.trimlk);
#endif
#if (INTSTAT) || (STAT)
            nheapbytes[aid] += nb;
#endif
#if (INTSTAT) || (STAT)
            _nbheap += nb;
            nheapreq[bid]++;
#endif
        } else {
#if (STDIO)
            fprintf(stderr, "failed to grow heap %ld bytes: bkt %ld\n", nb, bid);
#endif
            abort();
        }
    } else {
        nb = nbmap(bid);
        ptr = mapanon(_mapfd, nb);
        if (ptr != MAP_FAILED) {
#if (INTSTAT) || (STAT)
            nmapbytes[aid] += nb;
#endif
#if (STAT)
            nmapreq[bid]++;
#endif
        } else {
#if (STDIO)
            fprintf(stderr, "failed to map %ld bytes: bkt %ld\n", nb, bid);
#endif
            abort();
        }
    }
#if (TUNEBUF) && (!CONSTBUF)
    tmp = _nbmap + _nbheap;
    if (!tunesz) {
        tunesz = 64;
    }
    if ((tmp >> 10) >= tunesz) {
        tunebuf(tunesz);
    }
#endif

    return ptr;
}

static void
freemap(struct mag *mag)
{
    struct arn  *arn;
    long         cur;
    long         aid = mag->aid;
    long         bid = mag->bid;
    long         bsz = blksz(bid);
    long         max = mag->max;
#if (TUNEBUF) || (MALLOCBUFHDR)
    long         queue;
#endif
#if (TUNEBUF) || (MALLOCBUFMAP)
    long         nfree;
#endif
#if (HACKS)
    struct mag  *mptr1;
    struct mag  *mptr2;
#endif
#if !(MALLOCBUFHDR)
    struct mag **hbuf;
#endif

    arn = _atab[aid];
    mlk(&arn->lktab[bid]);
#if (HACKS)
    mlk(&_flktab[bid]);
    mptr1 = _ftab[bid];
    while (mptr1) {
        mptr2 = mptr1->next;
        if (ismapbkt(mptr1->bid)) {
            unmapanon(clrptr(mag->adr), max * bsz);
#if (VALGRIND)
            if (RUNNING_ON_VALGRIND) {
                VALGRIND_FREELIKE_BLOCK(clrptr(mag->adr), 0);
            }
#endif
#if (INTSTAT) || (STAT)
            nmapbytes[aid] -= max * bsz;
#endif
#if (TUNEBUF)
            _nbmap -= max * bsz;
#endif
#if !(MALLOCBUFHDR)
            if (gtpow2(max, 1)) {
                if (!istk(bid)) {
#if (INTSTAT) || (STAT)
                    nstkbytes[aid] -= (max << 1) * sizeof(void *); 
#endif
#if (NOSTK)
                    unmapstk(mag);
#if (VALGRIND)
                    if (RUNNING_ON_VALGRIND) {
                        VALGRIND_FREELIKE_BLOCK(mag->bptr, 0);
                    }
#endif
#endif
                    mag->bptr = NULL;
#if (FREEBITMAP)
                    unmapfmap(mag);
                    mag->fmap = NULL;
#if (VALGRIND)
                    if (RUNNING_ON_VALGRIND) {
                        VALGRIND_FREELIKE_BLOCK(mag->fmap, 0);
                    }
#endif
#endif
                }
            }
#else /* MALLOCBUFHDR */
            if (!istk(bid)) {
                mlk(&_hdrlktab[bid]);
                if (mptr1->prev) {
                    mptr1->prev->next = mptr2;
                }
                mptr1->next = _hdrtab[bid];
                if (mptr1->next) {
                    mptr1->next->prev = mptr1;
                }
                _hdrtab[bid] = mptr1;
                _nhdrtab[bid]++;
                munlk(&_hdrlktab[bid]);
            }
#endif
            if (mptr1->prev) {
                mptr1->prev->next = mptr2;
            }
        }
        mptr1 = mptr2;
    }
    munlk(&_flktab[bid]);
#endif /* HACKS */

#if (TUNEBUF) || (MALLOCBUFMAP)
    nfree = _fcnt[bid];
#endif
#if (MALLOCBUFHDR)
    cur = 0;
    queue = 0;
#else
    cur = arn->hcur;
    hbuf = arn->htab;
#endif
#if (MALLOCBUFMAP)
    queue = nfree < nbufmap(bid);
#elif (TUNEBUF)
    queue = nfree < _nbuftab[bid];
#endif
    if (!cur // || !ismapbkt(bid)
#if (TUNEBUF) || (MALLOCBUFHDR) || (MALLOCBUFMAP)
        || (queue)
#endif
        ) {
        mag->prev = NULL;
        mag->next = NULL;
        mlk(&_flktab[bid]);
#if (TAILFREE)
        if (!_ftab[bid]) {
            _ftab[bid] = mag;
            _ftail[bid] = mag;
        } else {
            mag->prev = _ftail[bid];
            if (_ftail[bid]) {
                _ftail[bid]->next = mag;
            }
            _ftail[bid] = mag;
        }
#else
        mag->next = _ftab[bid];
#if (TESTING)
        if (_ftab[bid]) {
            _ftab[bid]->prev = mag;
        }
#endif
        _ftab[bid] = mag;
#endif
#endif
#if (TUNEBUF) || (MALLOCBUFMAP)
        _fcnt[bid]++;
#endif
        munlk(&_flktab[bid]);
    } else {
        unmapanon(clrptr(mag->adr), max * bsz);
#if (VALGRIND)
        if (RUNNING_ON_VALGRIND) {
            VALGRIND_FREELIKE_BLOCK(clrptr(mag->adr), 0);
        }
#endif
#if (INTSTAT) || (STAT)
        nmapbytes[aid] -= max * bsz;
#endif
#if (TUNEBUF)
        _nbmap -= max * bsz;
#endif
#if !(MALLOCBUFHDR)
        if (gtpow2(max, 1)) {
            if (!istk(bid)) {
#if (INTSTAT) || (STAT)
                nstkbytes[aid] -= (mag->max << 1) * sizeof(void *); 
#endif
                unmapstk(mag);
#if (VALGRIND)
                if (RUNNING_ON_VALGRIND) {
                    VALGRIND_FREELIKE_BLOCK(mag->bptr, 0);
                }
#endif
                mag->bptr = NULL;
#if (FREEBITMAP)
                unmapfmap(mag);
#if (VALGRIND)
                if (RUNNING_ON_VALGRIND) {
                    VALGRIND_FREELIKE_BLOCK(mag->fmap, 0);
                }
#endif
                mag->fmap = NULL;
#endif
            }
        }
        mag->adr = NULL;
        hbuf[--cur] = mag;
        arn->hcur = cur;
#else /* MALLOCBUFHDR */
        mlk(&_hdrlktab[bid]);
        mag->next = _hdrtab[bid];
        if (mag->next) {
            mag->next->prev = NULL;
        }
        _hdrtab[bid] = mag->next;
        _nhdrtab[bid]++;
        munlk(&_hdrlktab[bid]);
#endif
    }
    munlk(&arn->lktab[bid]);

    return;
}

#define blkalnsz(sz, aln)                                               \
    (((aln) <= MINSZ)                                                   \
     ? max(sz, aln)                                                     \
     : (sz) + (aln))
static void *
getmem(size_t size,
       size_t align,
       long zero)
{
    struct arn  *arn;
    long         aid;
    long         sz = blkalnsz(max(size, MINSZ), align);
    long         bid = bktid(sz);
    uint8_t     *retptr = NULL;
    long         bsz = blksz(bid);
    uint8_t     *ptr = NULL;
    long         max = nblk(bid);
    struct mag  *mag = NULL;
    void       **stk;
#if !(MALLOCZEROSTK)
    long         l;
#endif
    long         get = 0;
    long         glob = 0;
#if (MAPFALLBACK)
    long         mapped = 0;
#endif
#if (MALLOCTRIM)
//    long         top = 0;
    long         lk = 0;
#endif
    
    if (!(_conf.flags & CONF_INIT)) {
        initmall();
    }
    aid = thrid();
    arn = _atab[aid];
    mlk(&arn->lktab[bid]);
    mag = arn->btab[bid];
    if (!mag) {
        glob = 1;
#if (MALLOCTRIM)
        if (!ismapbkt(bid)) {
            mlk(&_conf.trimlk);
            lk = 1;
        }
#endif
        mlk(&_blktab[bid]);
        mag = _btab[bid];
#if (MALLOCTRIM)
        if (mag) {
//            mlk(&mag->lk);
        }
#endif
    }
    if (!mag) {
#if (MALLOCTRIM)
        if (!ismapbkt(bid)) {
            mlk(&_conf.trimlk);
            lk = 1;
        }
#endif
        mlk(&_flktab[bid]);
        mag = _ftab[bid];
        if (mag) {
#if (MALLOCTRIM)
//            mlk(&mag->lk);
#endif
            mag->aid = aid;
#if (TESTING)
            if (mag->next) {
                mag->next->prev = NULL;
            }
#endif
            _ftab[bid] = mag->next;
#if (TESTING)
            if (mag->next) {
                mag->next->prev = NULL;
            }
#endif
#if (TAILFREE)
            if (!mag->next) {
                _ftail[bid] = NULL;
            }
#endif
            mag->next = NULL;
#if (TUNEBUF) || (MALLOCBUFMAP)
            _fcnt[bid]--;
#endif
#if (MALLOCTRIM)
//            munlk(&mag->lk);
#endif
        }
        munlk(&_flktab[bid]);
        if (mag) {
            if (gtpow2(max, 1)) {
                mag->glob = 0;
                mag->next = arn->btab[bid];
                if (mag->next) {
                    mag->next->prev = mag;
                }
                arn->btab[bid] = mag;
            }
        }
    } else if (mag->cur == mag->max - 1) {
        if (mag->next) {
            mag->next->prev = NULL;
        }
        if (glob) {
            _btab[bid] = mag->next;
#if (TESTING)
            if (mag->next) {
                mag->next->prev = NULL;
            }
#endif
#if (TAILFREE)
            if (!mag->next) {
                _btail[bid] = NULL;
            }
#endif
        } else {
            arn->btab[bid] = mag->next;
        }
        mag->next = NULL;
    }
    if (glob) {
        munlk(&_blktab[bid]);
    }
    if (!mag) {
        get = 1;
#if (MALLOCTRIM)
        if (!ismapbkt(bid)) {
            mtrylk(&_conf.trimlk);
            lk = 1;
        }
#endif
        if (!ismapbkt(bid)) {
            ptr = getslab(aid, bid);
#if (MAPFALLBACK)
            if (ptr == SBRK_FAILED) {
                ptr = mapanon(_mapfd, nbmag(bid));
                if (ptr == MAP_FAILED) {
                    ptr = NULL;
                } else {
                    mapped = 1;
                }
            }
#else
            if (ptr == SBRK_FAILED) {
                ptr = NULL;
            }
#endif
        } else {
            ptr = mapanon(_mapfd, nbmap(bid));
            if (ptr == MAP_FAILED) {
                ptr = NULL;
            }
#if (INTSTAT) || (STAT)
            else {
                nmapbytes[aid] += nbmap(bid);
            }
#endif
        }
#if (MALLOCBUFHDR)
        mag = gethdr(aid, bid);
#else
        mag = gethdr(aid);
#endif
#if (FREEBUF)
        if (mag) {
            sz = rounduppow2((max << 1) * sizeof(void *), PAGESIZE);
            mag->aid = aid;
            mag->cur = 0;
            mag->max = max;
            mag->bid = bid;
#if (MAPFALLBACK)
            if (mapped) {
                mag->adr = (uint8_t *)((uintptr_t)ptr | BMAP);
            }
#endif
            mag->adr = ptr;
            if (ptr) {
                if (gtpow2(max, 1)) {
                    if (istk(bid)) {
#if (ISTK)
                        stk = (void **)(&mag->data);
#elif (!NOSTK)
                        stk = (void **)mag->stk;
#else
                        stk = mapstk(max);
#endif
                    } else {
                        stk = mapstk(max);
                    }
                    mag->bptr = stk;
                    if (stk != MAP_FAILED) {
#if (FREEBITMAP)
                        mag->fmap = mapfmap(max);
                        if (mag->fmap == MAP_FAILED) {
                            unmapstk(mag);
                            
                            return NULL;
                        }
#endif
#if (INTSTAT) || (STAT)
                        nstkbytes[aid] += sz;
#endif
#if (VALGRIND)
                        if (RUNNING_ON_VALGRIND) {
                            VALGRIND_MALLOCLIKE_BLOCK(stk, sz, 0, 0);
                        }
#endif
//                        n = max << nmagslablog2(bid);
#if (MALLOCZEROSTK)
                        memset(stk,
                               0,
                               max * sizeof(void *));
#else
                        for (l = 0 ; l < max ; l++) {
                            stk[l] = ptr;
                            ptr += bsz;
                        }
#endif
                        mag->prev = NULL;
//                        _fcnt[bid]++;
                        mag->glob = 0;
                        mag->next = arn->btab[bid];
                        if (mag->next) {
                            mag->next->prev = mag;
                        }
                        arn->btab[bid] = mag;
                    } else {
#if (STDIO)
                        fprintf(stderr, "failed to map stk: bkt %ld\n", bid);
#endif
                        abort();
                    }
                }
            }
        }
#else /* !FREEBUF */
#if (MALLOCBUFHDR)
        mag = gethdr(aid, bid);
#else
        mag = gethdr(aid);
#endif
        if (mag) {
            mag->aid = aid;
            mag->cur = 0;
            mag->max = max;
            mag->bid = bid;
#if (MAPFALLBACK)
            if (mapped) {
                mag->adr = (uint8_t *)((uintptr_t)ptr | BMAP);
            } else {
                mag->adr = ptr;
            }
#else
            mag->adr = ptr;
#endif
            if (ptr) {
                if (gtpow2(max, 1)) {
                    if (istk(bid)) {
#if (ISTK)
                        stk = (void **)(&mag->data);
#elif (NOSTK)
//                        stk = mag->bptr;
                        stk = mapstk(max);
#else
                        stk = (void **)mag->stk;
#endif
                    } else {
                        stk = mapstk(max);
                    }
                    mag->bptr = stk;
                    if (stk != MAP_FAILED) {
                        sz = rounduppow2((max << 1) * sizeof(void *), PAGESIZE);
#if (FREEBITMAP)
                        mag->fmap = mapfmap(max);
                        if (mag->fmap == MAP_FAILED) {
                            unmapstk(mag);
                            
                            return NULL;
                        }
#endif
#if (INTSTAT) || (STAT)
                        nstkbytes[aid] += sz; 
#endif
#if (VALGRIND)
#if (NOSTK) && !(ISTK)
                        if (istk(bid)) {
                            if (RUNNING_ON_VALGRIND) {
                                VALGRIND_MALLOCLIKE_BLOCK(stk, sz, 0, 0);
                            }
                        }
#endif
#endif
//                        n = max << nmagslablog2(bid);
#if (MALLOCZEROSTK)
                        memset(stk,
                               0,
                               max * sizeof(void *));
#else
                        for (l = 0 ; l < max ; l++) {
                            stk[l] = ptr;
                            ptr += bsz;
                        }
#endif
                        mag->prev = NULL;
#if (HACKS)
//                        _fcnt[bid]++;
#endif
                        mag->glob = 0;
                        mag->next = arn->btab[bid];
                        if (mag->next) {
                            mag->next->prev = mag;
                        }
                        arn->btab[bid] = mag;
                    } else {
#if (STDIO)
                        fprintf(stderr, "failed to allocate stack: bkt %ld\n", bid);
#endif
                        abort();
                    }
                }
#if (MALLOCTRIM)
                if  (!ismapbkt(bid)) {
                    mag->prevheap = _topmag;
                    _topmag = mag;
                }
#endif
            }
        }
#endif /* FREEBUF */
    }
#if (MALLOCTRIM)
    if (lk) {
        munlk(&_conf.trimlk);
    }
#endif
    if (mag) {
        ptr = getblk(mag);
        retptr = clrptr(ptr);
#if (VALGRIND)
        if (RUNNING_ON_VALGRIND) {
            if (retptr) {
                VALGRIND_MALLOCLIKE_BLOCK(retptr, bsz, 0, 0);
            }
        }
#endif
        if ((zero) && chkflg(ptr, BDIRTY)) {
            memset(retptr, 0, bsz);
        }
        ptr = retptr;
#if (FREEBITMAP)
        if (bitset(mag->fmap, blkid(mag, ptr))) {
            fprintf(stderr, "free block already marked as allocated\n");

            abort();
        }
        setbit(mag->fmap, blkid(mag, ptr));
#endif
#if (RZSZ)
        markred(ptr);
        markred(ptr + RZSZ + size);
#endif
        if (retptr) {
#if (RZSZ)
            retptr = ptr + RZSZ;
#endif
            if (align) {
                if ((uintptr_t)(retptr) & (align - 1)) {
                    retptr = (uint8_t *)rounduppow2((uintptr_t)ptr, align);
                }
                ptr = setflg(retptr, BALIGN);
            }
            putptr(mag, retptr, ptr);
            setmag(retptr, mag);
        }
#if (MALLOCTRIM)
        if (lk) {
            munlk(&_conf.trimlk);
        }
#endif
    }    
#if (!DEVEL)
    if ((get) && ismapbkt(bid)) {
        munlk(&_flktab[bid]);
    }
#endif
    munlk(&arn->lktab[bid]);
#if (X11VIS)
//    mlk(&x11vislk);
    if (x11visinit) {
//        ptr = clrptr(ptr);
        ptr = retptr;
        if (ptr) {
            long     l = blksz(bid) >> BLKMINLOG2;
            uint8_t *vptr = ptr;
            
            while (l--) {
                x11vismarkres(vptr);
                vptr += MINSZ;
            }
        }
        if (retptr) {
            long     l = sz >> BLKMINLOG2;
            uint8_t *vptr = retptr;
            
            while (l--) {
                x11vismarkused(ptr);
                vptr += MINSZ;
            }
        }
        XSetWindowBackgroundPixmap(x11visdisp,
                                   x11viswin,
                                   x11vispmap);
        XClearWindow(x11visdisp,
                     x11viswin);
        XFlush(x11visdisp);
    }
//    munlk(&x11vislk);
#endif
    if (!retptr) {
#ifdef ENOMEM
        errno = ENOMEM;
#endif
#if (STDIO)
        fprintf(stderr, "%lx failed to allocate %ld bytes (heap: %ld, map: %ld\n) - initialised == %ld", aid, 1UL << bid, (long)_nbheap, (long)_nbmap, _conf.flags & CONF_INIT);
#endif

//        abort();
    }
#if (INTSTAT) || (STAT)
    nalloc[aid][bid]++;
#endif

    return retptr;
}

static void
putmem(void *ptr)
{
#if (RZSZ)
    uint8_t    *u8ptr = ptr;
#endif
    struct arn *arn;
    void       *mptr;
    struct mag *mag = (ptr) ? findmag(ptr) : NULL;
    long        aid = -1;
    long        tid = thrid();
    long        bid = -1;
    long        max;
//    long        glob = 0;
    long        freed = 0;

#if (MALLOCTRIM)
    mlk(&_conf.trimlk);
#endif
    if (mag) {
#if (VALGRIND)
        if (RUNNING_ON_VALGRIND) {
            VALGRIND_FREELIKE_BLOCK(ptr, 0);
        }
#endif
        aid = mag->aid;
        if (aid < 0) {
            mag->glob = 1;
//            glob = 1;
            mag->aid = aid = tid;
        }
        bid = mag->bid;
        max = mag->max;
        arn = _atab[aid];
        mlk(&arn->lktab[bid]);
        if (gtpow2(max, 1) && magempty(mag)) {
            mag->glob = 0;
            mag->next = arn->btab[bid];
            if (mag->next) {
                mag->next->prev = mag;
            }
            arn->btab[bid] = mag;
        }
        mptr = getptr(mag, ptr);
        if (mptr) {
            putptr(mag, ptr, NULL);
#if (MALLOCHASH)
            setmag(ptr, NULL);
#endif
#if (RZSZ)
            if (!chkflg(mptr, BALIGN)) {
                u8ptr = mptr - RZSZ;
                if (chkred(u8ptr) || chkred(u8ptr + blksz(bid) - RZSZ)) {
#if (STDIO)
                    fprintf(stderr, "red-zone violation\n");
#endif
                    abort();
                }
//                ptr = clrptr(mptr);
            }
#endif
#if (FREEBITMAP)
            if (!bitset(mag->fmap, blkid(mag, mptr))) {
                fprintf(stderr, "duplicate free: %p\n", mag->adr);
                
                return;
            }
            clrbit(mag->fmap, blkid(mag, mptr));
#endif
            ptr = clrptr(mptr);
            putblk(mag, setflg(ptr, BDIRTY));
            if (magfull(mag)) {
                if (gtpow2(max, 1)) {
//                    mlk(&_flktab[bid]);
                    if (mag->glob) {
                        mlk(&_blktab[bid]);
                    } else {
                        mlk(&_flktab[bid]);
                    }
                    if (mag->prev) {
                        mag->prev->next = mag->next;
//                    } else if ((glob) && (mag->glob)) {
                    } else if (mag->glob) {                        
                        _btab[bid] = mag->next;
                    } else {
                        arn->btab[bid] = mag->next;
                    }
                    if (mag->next) {
                        mag->next->prev = mag->prev;
#if (TAILFREE)
                    } else if (mag->glob) {
                        _btail[bid] = mag->prev;
#endif
                    }
//                    munlk(&_flktab[bid]);
                    if (mag->glob) {
                        munlk(&_blktab[bid]);
                    } else {
                        munlk(&_flktab[bid]);
                    }
                }
#if (MALLOCTRIM)
                if (mag == _topmag && !ismapbkt(bid)) {
                    if (trimheap()) {
                        munlk(&_conf.trimlk);
//                        munlk(&mag->lk);
                        munlk(&arn->lktab[bid]);

                        return;
                    }
//                    munlk(&mag->lk);
                }
#endif
//                setmag(mptr, NULL);
#if !(MALLOCHASH)
                setmag(ptr, NULL);
#endif
                if ((ismapbkt(bid)
#if (MAPFALLBACK)
                    || ((uintptr_t)mag->adr & BMAP)
#endif
                        )
                    && (!isbufbkt(bid)
#if (MALLOCBUFMAP)
                        && (_fcnt[bid] >= NBUFMAP)
#elif (TUNEBUF)
                        && (_fcnt[bid] < _nbuftab[(bid)])
#endif
                        )) {
                    freed = 1;
                } else {
                    mag->prev = mag->next = NULL;
                    mlk(&_flktab[bid]);
#if (TAILFREE)
                    if (!_ftab[bid]) {
                        _ftab[bid] = mag;
                        _ftail[bid] = mag;
                    } else {
                        mag->prev = _ftail[bid];
                        _ftail[bid]->next = mag;
                        _ftail[bid] = mag;
                    }
#else
                    mag->next = _ftab[bid];
#if (TESTING)
                    if (_ftab[bid]) {
                        _ftab[bid]->prev = mag;
                    }
#endif
                    _ftab[bid] = mag;
#endif
#if (TUNEBUF) || (MALLOCBUFMAP)
                    _fcnt[bid]++;
#endif
                    munlk(&_flktab[bid]);
                }
            }
            munlk(&arn->lktab[bid]);
            if (freed) {
                freemap(mag);
            }
#if (X11VIS)
//    mlk(&x11vislk);
            if (x11visinit) {
                ptr = mptr;
                if (ptr) {
                    if (freed) {
                        long     l = nbmap(bid) >> BLKMINLOG2;
                        uint8_t *vptr = ptr;
                        
                        while (l--) {
                            x11vismarkfreed(vptr);
                            vptr += MINSZ;
                        }
                } else {
                        long     l = blksz(bid) >> BLKMINLOG2;
                        uint8_t *vptr = ptr;
                        
                        while (l--) {
                            x11vismarkfreed(vptr);
                            vptr += MINSZ;
                        }
                    }
                }
                XSetWindowBackgroundPixmap(x11visdisp,
                                           x11viswin,
                                           x11vispmap);
                XClearWindow(x11visdisp,
                             x11viswin);
                XFlush(x11visdisp);
            }
//    munlk(&x11vislk);
#endif
        } else {
            munlk(&arn->lktab[bid]);
            fprintf(stderr, "invalid free %p\n", ptr);
            abort();
        }
    }
#if (MALLOCTRIM)
    munlk(&_conf.trimlk);
#endif

    return;
}

/* STD: ISO/POSIX */

void *
malloc(size_t size)
{
    void *ptr;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__malloc_hook) {
        void *caller;

        caller = m_getretadr();
        __malloc_hook(size, (const void *)caller);;
    }
#endif
    ptr = getmem(size, 0, 0);

    return ptr;
}

void *
calloc(size_t n, size_t size)
{
    size_t  sz = n * (size + (RZSZ << 1));
    void   *ptr = getmem(sz, 0, 1);

    return ptr;
}

void *
_realloc(void *ptr,
         size_t size,
         long rel)
{
    void       *retptr = ptr;
    long        sz = blkalnsz(max(size + (RZSZ << 1), MINSZ), 0);
    struct mag *mag = (ptr) ? findmag(ptr) : NULL;
    long        bid = bktid(sz);
    uintptr_t   bsz = (mag) ? blksz(mag->bid) : 0;

    if (!ptr) {
        retptr = getmem(size, 0, 0);
    } else if ((mag) && mag->bid != bid) {
        retptr = getmem(size, 0, 0);
        if (retptr) {
            memcpy(retptr, ptr, min(sz, bsz));
            putmem(ptr);
            ptr = NULL;
        }
    }
    if ((rel) && (ptr)) {
        putmem(ptr);
    }

    return retptr;
}

void *
realloc(void *ptr,
        size_t size)
{
    void *retptr = NULL;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__realloc_hook) {
        void *caller;

        caller = m_getretadr();
        __realloc_hook(ptr, size, (const void *)caller);
    }
#endif
    if (!size && (ptr)) {
        free(ptr);
    } else {
        retptr = _realloc(ptr, size, 0);
    }

    return retptr;
}

void
free(void *ptr)
{
#if (_BNU_SOURCE)
    if (__free_hook) {
        void *caller;

        caller = m_getretadr();
        __free_hook(ptr, (const void *)caller);
    }
#endif
    if (ptr) {
        putmem(ptr);
    }

    return;
}

#if (_ISOC11_SOURCE)
void *
aligned_alloc(size_t align,
              size_t size)
{
    void *ptr = NULL;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__memalign_hook) {
        void *caller;
        
        caller = m_getretadr();
        __memalign_hook(align, size, (const void *)caller);
    }
#endif
    if (!powerof2(align) || (size & (align - 1))) {
        errno = EINVAL;
    } else {
        ptr = getmem(size, align, 0);
    }

    return ptr;
}

#endif

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
int
posix_memalign(void **ret,
               size_t align,
               size_t size)
{
    void *ptr = getmem(size, align, 0);
    int   retval = -1;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__memalign_hook) {
        void *caller;
        
        caller = m_getretadr();
        __memalign_hook(align, size, (const void *)caller);
    }
#endif
    if (!powerof2(align) || (size & (sizeof(void *) - 1))) {
        errno = EINVAL;
    } else {
        ptr = getmem(size, align, 0);
        if (ptr) {
            retval ^= retval;
        }
    }
    *ret = ptr;

    return retval;
}
#endif

/* STD: UNIX */

#if ((_BSD_SOURCE)                                                      \
    || (_XOPEN_SOURCE >= 500 || ((_XOPEN_SOURCE) && (_XOPEN_SOURCE_EXTENDED))) \
    && !(_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600))
void *
valloc(size_t size)
{
    void *ptr;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__memalign_hook) {
        void *caller;
        
        caller = m_getretadr();
        __memalign_hook(PAGESIZE, size, (const void *)caller);
    }
#endif
    ptr = getmem(size, PAGESIZE, 0);
    
    return ptr;
}
#endif

void *
memalign(size_t align,
         size_t size)
{
    void *ptr = NULL;

#if defined(_GNU_SOURCE) && (GNUMALLOCHOOKS)
    if (__memalign_hook) {
        void *caller;
        
        caller = m_getretadr();
        __memalign_hook(align, size, (const void *)caller);
    }
#endif
    if (!powerof2(align)) {
        errno = EINVAL;
    } else {
        ptr = getmem(size, align, 0);
    }

    return ptr;
}

#if (_BSD_SOURCE)
void *
reallocf(void *ptr,
         size_t size)
{
    void *retptr = _realloc(ptr, size, 1);

    return retptr;
}
#endif

#if defined(_GNU_SOURCE)
void *
pvalloc(size_t size)
{
    size_t  sz = rounduppow2(size, PAGESIZE);
    void   *ptr = getmem(sz, PAGESIZE, 0);

    return ptr;
}
#endif

void
cfree(void *ptr)
{
    if (ptr) {
        free(ptr);
    }

    return;
}

size_t
malloc_usable_size(void *ptr)
{
    struct mag *mag = findmag(ptr);
    size_t      sz = usrsz(mag->bid);

    return sz;
}

size_t
malloc_good_size(size_t size)
{
    size_t rzsz = RZSZ;
    size_t sz = usrsz(bktid(size)) - (rzsz << 1);

    return sz;
}

size_t
malloc_size(void *ptr)
{
    struct mag *mag = findmag(ptr);
    size_t      sz = (mag) ? blksz(mag->bid) : 0;

    return sz;
}

