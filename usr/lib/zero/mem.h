#ifndef __ZERO_MEM_H__
#define __ZERO_MEM_H__

#if !defined(MEMLFDEQ)
#define MEMLFDEQ    0
#endif
#if !defined(MEMTABNREF)
#define MEMTABNREF  0
#endif
#if !defined(MEMDEBUG)
#define MEMDEBUG    0
#endif

#if defined(MEMDEBUG)
#define crash(expr)                                                     \
    do {                                                                \
        if (!(expr)) {                                                  \
            *(uint8_t *)NULL = 0x00;                                    \
        }                                                               \
    } while (0)
#endif

/* generic memory manager definitions for libzero */

#define MEM_LK_NONE 0x01        // don't use locks; single-thread
#define MEM_LK_PRIO 0x02        // priority-based locklessinc.com lock
#define MEM_LK_FMTX 0x04        // anonymous non-recursive mutex
#define MEM_LK_SPIN 0x08        // spinlock

#define MEM_LK_TYPE MEM_LK_PRIO // type of locks to use

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <zero/cdefs.h>
#include <zero/param.h>
#include <zero/asm.h>
#include <zero/unix.h>
#include <zero/trix.h>
#include <zero/spin.h>
#if (MEM_LK_TYPE == MEM_LK_PRIO)
#define PRIOLKUSEMMAP
#include <zero/priolk.h>
#elif (MEM_LK_TYPE == MEM_LK_FMTX)
#include <zero/mtx.h>
#endif
#if defined(MEMVALGRIND) && (MEMVALGRIND)
#define ZEROVALGRIND 1
#include <zero/valgrind.h>
#endif

/* types */

#if (WORDSIZE == 4)
typedef int32_t   MEMWORD_T;    // machine word
typedef uint32_t  MEMUWORD_T;   // unsigned machine word
#elif (WORDSIZE == 8)
typedef int64_t   MEMWORD_T;
typedef uint64_t  MEMUWORD_T;
#endif
typedef uintptr_t MEMADR_T;     // address (with possible flag/lock-bits)
typedef intptr_t  MEMADRDIFF_T; // for possible negative values
typedef uint8_t * MEMPTR_T;     // could be char * too; needs to be single-byte

#if (MEM_LK_TYPE == MEM_LK_PRIO)
typedef struct priolk MEMLK_T;
#elif (MEM_LK_TYPE == MEM_LK_FMTX)
typedef zerofmtx      MEMLK_T;
#elif (MEM_LK_TYPE == MEM_LK_SPIN)
typedef zerospin      MEMLK_T;
#endif

/* macros */

#if (WORDSIZE == 4)
#define MEMWORD(i)  INT32_C(i)
#define MEMUWORD(u) UINT32_C(u)
#elif (WORDSIZE == 8)
#define MEMWORD(i)  INT64_C(i)
#define MEMUWORD(u) UINT64_C(u)
#endif

#if (MEM_LK_TYPE == MEM_LK_PRIO)
#define memgetlk(lp) priolkget(lp)
#define memrellk(lp) priolkrel(lp)
#elif (MEM_LK_TYPE == MEM_LK_FMTX)
#define memgetlk(lp) fmtxlk(lp)
#define memrellk(lp) fmtxunlk(lp)
#elif (MEM_LK_TYPE == MEM_LK_SPIN)
#define memgetlk(lp) spinlk(lp)
#define memrellk(lp) spinunlk(lp)
#endif

//#define MEMPAGEBIT      (MEMUWORD(1) << (PAGESIZELOG2 - 1))
//#define MEMPAGEIDMASK   (MEMPAGEBIT - 1)
#define MEMPAGEINFOMASK ((MEMUWORD(1) << PAGESIZELOG2) - 1)
/* use the low-order bit of the word or pointer to lock data */
#define MEMLKBITID      0
#define MEMLKBIT        (MEMUWORD(1) << MEMLKBITID)
#if (MEMDEBUGDEADLOCK)
#define memlkbitln(ptr)                                                 \
    do {                                                                \
        do {                                                            \
            ;                                                           \
        } while (m_cmpsetbit((m_atomic_t *)&(ptr)->list, MEMLKBITID));  \
        (ptr)->line = __LINE__;                                         \
    } while (0)
#define memrelbitln(ptr) \
    ((ptr)->line = __LINE__,                                            \
     m_cmpclrbit((m_atomic_t *)&(ptr)->list, MEMLKBITID))
#endif
#if (MEMDEBUGLOCK)
#define memlkbit(ptr)                                                   \
    do {                                                                \
        fprintf(stderr, "LK: %s: %d\n", __FILE__, __LINE__);            \
    } while (m_cmpsetbit((m_atomic_t *)ptr, MEMLKBITID))
#define memrelbit(ptr) (fprintf(stderr, "UNLK: %s: %d\n", __FILE__, __LINE__), \
                        m_cmpclrbit((m_atomic_t *)ptr, MEMLKBITID))

#else
#define memtrylkbit(ptr)                                                \
    (!m_cmpsetbit((m_atomic_t *)ptr, MEMLKBITID))
#define memlkbit(ptr)                                                   \
    do {                                                                \
        ;                                                               \
    } while (m_cmpsetbit((m_atomic_t *)ptr, MEMLKBITID))
#define memrelbit(ptr) m_cmpclrbit((m_atomic_t *)ptr, MEMLKBITID)
#endif

#if (WORDSIZE == 4)
#define memcalcslot(sz, slot)                                           \
    do {                                                                \
        long _tmp;                                                      \
        long _slot;                                                     \
                                                                        \
        ceilpow2_32(sz, _tmp);                                          \
        _slot = tzerol(_tmp);                                           \
        (slot) = _slot;                                                 \
    } while (0)
#elif (WORDSIZE == 8)
#define memcalcslot(sz, slot)                                           \
    do {                                                                \
        long _tmp;                                                      \
        long _slot;                                                     \
                                                                        \
        ceilpow2_64(sz, _tmp);                                          \
        _slot = tzeroll(_tmp);                                          \
        (slot) = _slot;                                                 \
    } while (0)
#endif
#define memcalcpageslot(sz, slot)                                       \
    do {                                                                \
        MEMUWORD_T _res = sz;                                           \
                                                                        \
        _res--;                                                         \
        _res >>= PAGESIZELOG2;                                          \
        (slot) = _res;                                                  \
    } while (0)

/* determine minimal required alignment for blocks */
#if defined(__BIGGEST_ALIGNMENT__)
#define MEMMINALIGN         max(__BIGGEST_ALIGNMENT__, 2 * PTRSIZE)
#else
#define MEMMINALIGN         (2 * PTRSIZE) // allow for dual-word tagged pointers
#endif
#if (MEMMINALIGN == 8)
#define MEMALIGNSHIFT       3
#elif (MEMMINALIGN == 16)
#define MEMALIGNSHIFT       4
#elif (MEMMINALIGN == 32)
#define MEMALIGNSHIFT       5
#endif
/* maximum small buf size (MEMBUFMAXBLKS << MEMMAXSMALLSHIFT) + bookkeeping */
#define MEMMAXSMALLSHIFT    (PAGESIZELOG2 - 1)
#define MEMSMALLSLOTS       PAGESIZELOG2
/* NOTES
 * -----
 * - all allocations except those from pagebin are power-of-two sizes
 * - pagebin allocations are PAGESIZE * slot
 */
/* minimum allocation block size */
#define MEMMINBLK           (MEMUWORD(1) << MEMALIGNSHIFT)
/* maximum allocation block size */
#define MEMMAXBUFBLK        (MEMUWORD(1) << MEMMAXBUFSLOT)
/* maximum slot # */
#define MEMMAXBUFSLOT       (PTRBITS - 1)
/* number of words in buf freemap */
//#define MEMBUFFREEMAPWORDS  (CLSIZE / WORDSIZE)
#define MEMBUFFREEMAPWORDS  16
/* number of block-bits in buf freemap */
#define MEMBUFMAXBLKS       (MEMBUFFREEMAPWORDS * WORDSIZE * CHAR_BIT)
/* minimum allocation block size in bigbins */
//#define MEMBIGMINSIZE      (2 * PAGESIZE)
#define MEMPAGESLOTS        (MEMWORD(1) << MEMBUFSLOTBITS)

#define MEMSMALLSLOT        7
#define MEMMIDSLOT          10
//#define MEMBIGSLOT          (MEMSMALLSLOTS - 1)
#define MEMSMALLPAGESLOT    32
#define MEMMIDPAGESLOT      64
#define MEMBIGPAGESLOT      128
//#define MEMSMALLBLKSHIFT    (PAGESIZELOG2 - 1)
#define MEMSMALLMAPSHIFT    23
//#define MEMBUFMIDMAPSHIFT 22
#define MEMBIGMAPSHIFT      26
//#define MEMBUFHUGEMAPSHIFT  26

struct membkt {
#if (MEMLFDEQ)
    struct lfdeq   list;
#else
    struct membuf *list;        // bi-directional list of bufs + lock-bit
#endif
#if (MEMDEBUGDEADLOCK)
    MEMUWORD_T     line;
#endif
    MEMWORD_T      nblk;        // # of per-buffer blocks; 0 for default
    MEMWORD_T      nbuf;        // number of bufs in list
    MEMWORD_T      nmax;        // max # to buffer; 0 for default
    uint8_t        _pad[CLSIZE
#if (MEMLFDEQ)
                        - sizeof(struct lfdeq)
#else
                        - sizeof(struct membuf *)
#endif
#if (MEMDEBUGDEADLOCK)
                        - sizeof(MEMUWORD_T)
#endif
                        - 3 * sizeof(MEMUWORD_T)];
};

/* type-bits for allocation buffers */
#define MEMSMALLBUF      0x00
#define MEMPAGEBUF       0x01
#define MEMBIGBUF        0x02
#define MEMBUFTYPES      3
#define MEMBUFTYPEBITS   2
#define MEMHEAPBIT       (MEMUWORD(1) << MEMBUFFLGSHIFT)
#define MEMBUFFLGMASK    (MEMHEAPBIT)
#define MEMBUFFLGSHIFT   (sizeof(MEMUWORD_T) * CHAR_BIT - 1)
#define MEMBUFFLGBITS    1
#define MEMBUFSLOTBITS   8
#define MEMBUFSLOTMASK   (((MEMUWORD(1) << MEMBUFSLOTBITS) - 1)         \
                          << MEMBUFSLOTSHIFT)
#define MEMBUFSLOTSHIFT  (2 * MEMBUFNBLKBITS)
#define MEMBUFNBLKBITS   12
#define MEMBUFNBLKMASK   ((MEMWORD(1) << MEMBUFNBLKBITS) - 1)
#define MEMBUFNFREEBITS   MEMBUFNBLKBITS
#define MEMBUFNFREEMASK  (MEMBUFNBLKMASK << MEMBUFNFREESHIFT)
#define MEMBUFNFREESHIFT  MEMBUFNBLKBITS

#define memsetbufflg(buf, flg) ((buf)->flg |= (flg))
#define memsetbufnblk(buf, n)                                           \
    ((buf)->info |= (n))
#define memsetbuftype(buf, t)                                           \
    ((buf)->flg |=  (t))
#define memsetbufslot(buf, slot)                                        \
    ((buf->info)  |= (slot) << MEMBUFSLOTSHIFT)
#define memsetbufnfree(buf, n)                                          \
    ((buf)->info = ((buf)->info & ~MEMBUFNFREEMASK) | ((n) << MEMBUFNBLKBITS))
#define memgetbufflg(buf)                                               \
    ((buf)->info & (flg))
#define memgetbufnblk(buf)                                              \
    ((buf)->info & MEMBUFNBLKMASK)
#define memgetbufnfree(buf)                                             \
    (((buf)->info >> MEMBUFNFREESHIFT) & ((MEMUWORD(1) << MEMBUFNFREEBITS) - 1))
#define memgetbuftype(buf)                                              \
    ((buf)->flg & ((MEMUWORD(1) << MEMBUFTYPEBITS) - 1))
#define memgetbufslot(buf)                                              \
    (((buf)->info >> MEMBUFSLOTSHIFT) & ((MEMWORD(1) << MEMBUFSLOTBITS) - 1))

struct membufvals {
    MEMWORD_T *nblk[MEMBUFTYPES];
    MEMWORD_T *ntls[MEMBUFTYPES];
    MEMWORD_T *nglob[MEMBUFTYPES];
};

#define MEMINITBIT   (1L << 0)
#define MEMNOHEAPBIT (1L << 1)
struct mem {
    struct membkt       smallbin[MEMSMALLSLOTS]; // blocks of 1 << slot
    struct membkt       bigbin[PTRBITS];         // mapped blocks of 1 << slot
    struct membkt       pagebin[MEMPAGESLOTS];   // maps of PAGESIZE * slot
    struct membufvals   bufvals;
#if (MEMMULTITAB)
    struct memtabl0    *tab;     // allocation lookup structure
#elif (MEMNEWHASH)
    struct memhashlist *hash;    // hash table
    struct memhash     *hashbuf; // buffer for hash items
#endif
    MEMUWORD_T          flg;     // memory interface flags
    struct membuf      *heap;    // heap allocations (try sbrk(), then mmap())
    struct membuf      *maps;    // mapped blocks
#if (MEM_LK_TYPE == MEM_LK_PRIO)
    zerospin            priolk;
    unsigned long       prioval; // locklessinc priority locks
#endif
    zerospin            initlk;  // lock for initialisation
    MEMLK_T             heaplk;  // lock for sbrk()
};

#if (MEMBITFIELD)
struct membufinfo {
    unsigned int nblk  : MEMBUFNBLKBITS;
    unsigned int nfree : MEMBUFNBLKBITS;
    unsigned int slot  : MEMBUFSLOTBITS;
    unsigned int type  : MEMBUFTYPEBITS;
    unsigned int flg   : MEMBUFFLGBITS;
};
#endif

/*
 * buf structure for allocating runs of pages; crafted to fit in 8 words
 * (typical cacheline size)
 * - a second cacheline is used for the bitmap; 1-bits denote free blocks
 * - the actual buffers will be prefixed by this structure
 * - allocation shall take place with sbrk() or mmap()
 */

#define MEMNOBLK (MEMWORD(-1))

struct membuf {
#if (MEMBITFIELD)
    struct membufinfo       info;
#else
    MEMWORD_T               info; // slot + # of total & free blks
#endif
    struct membuf          *heap; // previous buf in heap for bufs from sbrk()
    struct membuf          *prev; // previous buf in chain
    struct membuf          *next; // next buf in chain
    volatile struct membkt *bkt;  // pointer to parent bucket
//    MEMWORD_T      slot;        // bucket slot #
    MEMUWORD_T              size; // buffer bookkeeping + allocation blocks
    MEMPTR_T                base; // base address for allocations
    MEMUWORD_T              flg;  // flags + buffer type
//    MEMPTR_T               *ptrtab; // original pointers for aligned blocks
    MEMUWORD_T              freemap[MEMBUFFREEMAPWORDS];
    MEMPTR_T                ptrtab[MEMBUFMAXBLKS];
};

#if (MEMMULTITAB)

/* toplevel lookup table item */
struct memtabl0 {
    MEMLK_T        lk;
    struct memtab *tab;
};

/* lookup table item */
struct memtab {
    struct memtab *tab;
};

struct memitem {
    MEMADR_T val;
};

#elif (MEMNEWHASH)

struct memhashlist {
#if (MEMHASHLOCK)
    MEMLK_T         lk;
#endif
    struct memhash *chain;
};

#define MEMHASHDEL (-1)
#define MEMHASHCHK (0)
#define MEMHASHADD (1)

#if (MEMHASHMURMUR)
#define memhashptr(page) MurmurHash3Mixer((MEMADR_T)page)
#else
#if (WORDSIZE == 4)
#define memhashptr(page)                                                \
    (tmhash32((MEMADR_T)page))
#elif (WORDSIZE == 8)
#define memhashptr(page)                                                \
    (tmhash64((MEMADR_T)page))
#endif
#endif
#define MEMHASHNOTFOUND  0
#define MEMHASHFOUND     (~(MEMADR_T)0)

/*
 * - we have a 4-word header; adding total of 52 words as 13 hash-table entries
 *   lets us cache-color the table by adding a modulo-9 value to the pointer
 */
#define MEMHASHBITS     20
#define MEMHASHITEMS    (1U << MEMHASHBITS)
#if (MEMBIGHASHTAB)
#define MEMHASHSIZE     (256 * WORDSIZE)
#if defined(MEMHASHNREF) && (MEMHASHNREF)                               \
    && defined(MEMHASHNACT) && (MEMHASHNACT)
#define MEMHASHTABITEMS 32      // allow a bit of table-address randomization
#elif ((defined(MEMHASHNREF) && (MEMHASHNREF))                          \
       ||  defined(MEMHASNACT) && (MEMHASHNACT))
#define MEMHASHTABITEMS 42      // allow a bit of table-address randomization
#else
#define MEMHASHTABITEMS 64      // allow a bit of table-address randomization
#endif
#else
#define MEMHASHSIZE     (128 * WORDSIZE)
#if defined(MEMHASHNREF) && (MEMHASHNREF)                               \
    && defined(MEMHASHNACT) && (MEMHASHNACT)
#define MEMHASHTABITEMS 24
#elif ((defined(MEMHASHNREF) && (MEMHASHNREF))                          \
       ||  defined(MEMHASNACT) && (MEMHASHNACT))
#define MEMHASHTABITEMS 32
#else
#define MEMHASHTABITEMS 48
#endif
#endif
#define memhashsize()   MEMHASHSIZE

struct memhashitem {
#if defined(MEMHASHNREF) && (MEMHASHNREF)
    MEMUWORD_T nref;            // reference count for the page
#endif
#if defined(MEMHASHNACT) && (MEMHASHNACT)
    MEMUWORD_T nact;            // number of inserts, finds, and deletes
#endif
    MEMADR_T   page;            // page address
    MEMADR_T   val;             // stored value
};

struct memhash {
    struct memhash     *chain;  // next array in this chain
    MEMUWORD_T          ntab;   // number of occupied slots in this table
    struct memhashitem *tab;    // pointer to the item table
    struct memhashlist *list;   // pointer for head of list
    MEMUWORD_T          data;   // base address for generating table address
};

#endif

/*
 * NOTE: the arenas are mmap()'d as PAGESIZE-allocations so there's going
 * to be some room in the end for arbitrary data
 */
#define memtlssize() rounduppow2(sizeof(struct memtls), 2 * PAGESIZE)
struct memtls {
    struct membkt     smallbin[MEMSMALLSLOTS]; // blocks of size 1 << slot
    struct membkt     pagebin[MEMPAGESLOTS]; // maps of PAGESIZE * slot
#if (MEM_LK_TYPE & MEM_LK_PRIO)
    struct priolkdata priolkdata;
#endif
    MEMUWORD_T        flg;
};

/* mark the first block of buf as allocated */
#define _memfillmap0(ptr, ofs, mask)                                    \
    ((ptr)[(ofs)] = (mask) & ~MEMUWORD(1),                              \
     (ptr)[(ofs) + 1] = (mask),                                         \
     (ptr)[(ofs) + 2] = (mask),                                         \
     (ptr)[(ofs) + 3] = (mask))
#define _memfillmap(ptr, ofs, mask)                                     \
    ((ptr)[(ofs)] = (mask),                                             \
     (ptr)[(ofs) + 1] = (mask),                                         \
     (ptr)[(ofs) + 2] = (mask),                                         \
     (ptr)[(ofs) + 3] = (mask))

static __inline__ void
membufinitfree(struct membuf *buf)
{
    MEMUWORD_T  bits = ~MEMUWORD(0);      // all 1-bits
    MEMUWORD_T *ptr = buf->freemap;

    _memfillmap0(ptr, 0, bits);
#if (MEMBUFFREEMAPWORDS >= 8)
    _memfillmap(ptr, 4, bits);
#endif
#if (MEMBUFFREEMAPWORDS == 16)
    _memfillmap(ptr, 8, bits);
    _memfillmap(ptr, 12, bits);
#endif

    return;
}

static __inline__ MEMWORD_T
membufgetfree(struct membuf *buf)
{
    MEMWORD_T   nblk = memgetbufnblk(buf);
    MEMUWORD_T *map = buf->freemap;
    MEMWORD_T   ndx = 0;
    MEMUWORD_T  word;
    MEMUWORD_T  mask;
    MEMUWORD_T  res;

    do {
        word = *map;
        if (word) {                             // skip 0-words
            res = tzerol(word);                 // count trailing zeroes
            ndx += res;                         // add to ndx
            if (ndx < nblk) {
                mask = MEMUWORD(1) << res;      // create bit
                mask = ~mask;                   // invert for mask
                word &= mask;                   // mask the bit out
                *map = word;                    // update

                return ndx;                     // return index of first 1-bit
            }

            return MEMNOBLK;                    // 1-bit not found
        }
        ndx += sizeof(MEMUWORD_T) * CHAR_BIT;
        map++;                                  // try next word in freemap
    } while (ndx < nblk);

    return MEMNOBLK;                            // 1-bit not found
}

#define memalignptr(ptr, pow2)                                          \
    ((MEMPTR_T)rounduppow2((uintptr_t)(ptr), (pow2)))
#define memadjptr(ptr, sz)                                              \
    (&((MEMPTR_T)(ptr))[sz])

/* compute adr + adr % 9 (# of cachelines in offset, aligned to cl boundary) */
static __inline__ MEMUWORD_T *
memgentlsadr(MEMUWORD_T *adr)
{
    /* division by 9 */
    MEMADR_T res = (MEMADR_T)adr;
    MEMADR_T q;
    MEMADR_T r;
    MEMADR_T div9;
    MEMADR_T dec;

    res >>= 16;
    /* divide by 9 */
    q = res - (res >> 3);
    q = q + (q >> 6);
    q = q + (q >> 12) + (q >> 24);
    q = q >> 3;
    r = res - q * 9;
    div9 = q + ((r + 7) >> 4);
    /* calculate res -= res/9 * 9 i.e. res % 9 (max 8) */
    dec = div9 * 9;
    res -= dec;
    /* scale to 0..512 (machine words) */
    res <<= 6;
    /* align to cacheline */
    res &= ~(CLSIZE - 1);
    /* add to original pointer */
    adr += res;

    return adr;
}

static __inline__ MEMPTR_T
memgenadr(MEMPTR_T ptr, MEMUWORD_T blksz, MEMUWORD_T size)
{
    MEMPTR_T   adr = ptr;
    MEMADR_T   res = (MEMADR_T)ptr;
    MEMUWORD_T lim = blksz - size;
    MEMWORD_T  shift;
    MEMADR_T   q;
    MEMADR_T   r;
    MEMADR_T   div9;
    MEMADR_T   dec;

    if (lim <= 2 * CLSIZE) {

        return ptr;
    } else if (lim >= 4 * CLSIZE) {
#if (CLSIZE == 32)
        shift = 4;
#elif (CLSIZE == 64)
        shift = 5;
#endif
    } else {
#if (CLSIZE == 32)
        shift = 3;
#elif (CLSIZE == 64)
        shift = 4;
#endif
    }
    /* shift out some [mostly-aligned] low bits */
    res >>= 16;
    /* divide by 9 */
    q = res - (res >> 3);
    q = q + (q >> 6);
    q = q + (q >> 12) + (q >> 24);
    q = q >> 3;
    r = res - q * 9;
    div9 = q + ((r + 7) >> 4);
    /* calculate res -= res/9 * 9 i.e. res % 9 (max 8) */
    dec = div9 * 9;
    res -= dec;
    /* scale by shifting the result of the range 0..8 */
    res <<= shift;
    /* round down to a multiple of cacheline */
    res &= ~(CLSIZE - 1);
    /* add offset to original address */
    adr += res;

    return adr;
}

/* compute adr + adr % 9 (# of words in offset, aligned to word boundary) */
static __inline__ MEMUWORD_T *
memgenhashtabadr(MEMUWORD_T *adr)
{
    MEMADR_T res = (MEMADR_T)adr;
    MEMADR_T q;
    MEMADR_T r;
    MEMADR_T div9;
    MEMADR_T dec;

    /* shift out some [mostly-aligned] low bits */
    res >>= 16;
    /* divide by 9 */
    q = res - (res >> 3);
    q = q + (q >> 6);
    q = q + (q >> 12) + (q >> 24);
    q = q >> 3;
    r = res - q * 9;
    div9 = q + ((r + 7) >> 4);
    /* calculate res -= res/9 * 9 i.e. res % 9 (max 8) */
    dec = div9 * 9;
    res -= dec;
#if (MEMBIGHASHTAB)
    /* scale res to 0..32 (machine words) */
    res <<= 2;
#else
    /* scale res to 0..16 (machine words) */
    res <<= 1;
#endif
    /* add to original pointer */
    adr += res;
    /* align to machine word boundary */

    return adr;
}

/*
 * for 32-bit pointers, we can use a flat lookup table for bookkeeping pointers
 * - for bigger pointers, we use a hash (or multilevel) table
 */
#if ((MEMNEWHASH) || (MEMHASH)) && !defined(MEMHASHITEMS)
#define MEMHASHBITS   20
#define MEMHASHITEMS  (MEMUWORD(1) << MEMHASHBITS)
#elif (MEMMULTITAB)
#if (PTRBITS > 32)
#define MEMADRBITS    (ADRBITS - PAGESIZELOG2)
#define MEMADRSHIFT   (PAGESIZELOG2)
#define MEMLVL1BITS   (MEMADRBITS - 3 * MEMLVLBITS)
#define MEMLVLBITS    10
#define MEMLVL1ITEMS  (MEMWORD(1) << MEMLVL1BITS)
#define MEMLVLITEMS   (MEMWORD(1) << MEMLVLBITS)
#define MEMLVL1MASK   ((MEMWORD(1) << MEMLVL1BITS) - 1)
#define MEMLVLMASK    ((MEMWORD(1) << MEMLVLBITS) - 1)
#define memgetkeybits(p, k1, k2, k3, k4)                                \
    do {                                                                \
        MEMADR_T _p1 = (MEMADR_T)(p) >> MEMADRSHIFT;                    \
        MEMADR_T _p2 = (MEMADR_T)(p) >> (MEMADRSHIFT + MEMLVLBITS);     \
                                                                        \
        (k4) = _p1 & MEMLVLMASK;                                        \
        (k3) = _p2 & MEMLVLMASK;                                        \
        _p1 >>= 2 * MEMLVLBITS;                                         \
        _p2 >>= 2 * MEMLVLBITS;                                         \
        (k2) = _p1 & MEMLVLMASK;                                        \
        (k1) = _p2 & MEMLVL1MASK;                                       \
    } while (0)
#if 0
/* compute level keys in reverse order */
#define memgetkeybits(p, k1, k2, k3, k4)                                \
    do {                                                                \
        MEMADR_T _p1 = (MEMADR_T)(p) >> MEMADRSHIFT;                    \
        MEMADR_T _p2 = (MEMADR_T)(p) >> (MEMADRSHIFT + MEMLVLBITS);     \
                                                                        \
        (k1) = _p1 & MEMLVLMASK;                                        \
        (k2) = _p2 & MEMLVLMASK;                                        \
        _p1 >>= 2 * MEMLVLBITS;                                         \
        _p2 >>= 2 * MEMLVLBITS;                                         \
        (k3) = _p1 & MEMLVLMASK;                                        \
        (k4) = _p2 & MEMLVL1MASK;                                       \
    } while (0)
#endif
#endif /* PTRBITS > 32 */
#endif /* (MEMNEWHASH || MEMHASH) && !defined(MEMHASHITEMS) */

/*
 * allocation headers
 * - 3 allocation classes:
 *   - small; block size is 1 << slot
 *   - page; block size is PAGESIZE + PAGESIZE * slot
 *   - big; block size is 1 << slot
 */
#define membufhdrsize()        (sizeof(struct membuf))
//#define membufptrtabsize(nblk) (MEMBUFMAXBLKS * sizeof(MEMPTR_T))
//#define membufslot(buf)       (memgetbufslot(buf))
#define membufblkofs()                                                  \
    (rounduppow2(membufhdrsize(), PAGESIZE))
#define memusesmallbuf(sz)     ((sz) <= (MEMUWORD(1) << MEMMAXSMALLSHIFT))
#define memusepagebuf(sz)      ((sz) <= (PAGESIZE * MEMPAGESLOTS))
/* allocations of PAGESIZE * slot */
#define memsmallbufsize(slot, nblk)                                     \
    (rounduppow2(membufblkofs() + ((nblk) << (slot)),                   \
                 PAGESIZE))
#define mempagebufsize(slot, nblk)                                      \
    (rounduppow2(membufblkofs()                                         \
                 + (MEMUWORD(PAGESIZE) + MEMUWORD(PAGESIZE) * (slot))   \
                 * (nblk),                                              \
                 PAGESIZE))
#define membigbufsize(slot, nblk)                                       \
    (rounduppow2(membufblkofs() + (MEMUWORD(1) << (slot)) * (nblk),     \
                 PAGESIZE))
#define memnbufblk(slot, type)                                          \
    (((type) == MEMSMALLBUF)                                            \
     ? (((slot) <= MEMSMALLSLOT)                                        \
        ? (MEMBUFMAXBLKS)                                               \
        : (((slot) <= MEMMIDSLOT)                                       \
           ? (MEMBUFMAXBLKS >> 1)                                       \
           : (MEMBUFMAXBLKS >> 2)))                                     \
     : (((type) == MEMPAGEBUF)                                          \
        ? (((slot) <= MEMMIDPAGESLOT)                                   \
           ? 8                                                          \
           : (((slot) <= MEMBIGPAGESLOT)                                \
              ? 4                                                       \
              : 2))                                                     \
        : (((slot) <= MEMSMALLMAPSHIFT)                                 \
           ? 4                                                          \
           : (((slot) <= MEMBIGMAPSHIFT)                                \
              ? 2                                                       \
              : 1))))
#define memnbuftls(slot, type)                                          \
    (((type) == MEMSMALLBUF)                                            \
     ? 4                                                                \
     : (((type) == MEMPAGEBUF)                                          \
        ? (((slot) <= MEMMIDPAGESLOT)                                   \
           ? 8                                                          \
           : (((slot) <= MEMBIGPAGESLOT)                                \
              ? 4                                                       \
              : 2))                                                     \
        : (((slot) <= MEMSMALLMAPSHIFT)                                 \
           ? 4                                                          \
           : (((slot) <= MEMBIGMAPSHIFT)                                \
              ? 2                                                       \
              : 1))))
#define memnbufglob(slot, type)                                         \
    (((type) == MEMSMALLBUF)                                            \
     ? 32                                                               \
     : (((type) == MEMPAGEBUF)                                          \
        ? 16                                                            \
        : 8))

#define membufblkadr(buf, ndx)                                          \
    ((buf)->base + ((ndx) << memgetbufslot(buf)))
#define membufblkid(buf, ptr)                                           \
    (((MEMPTR_T)(ptr) - (buf)->base) >> memgetbufslot(buf))
#define membufpageadr(buf, ndx)                                         \
    ((buf)->base + (ndx) * (MEMUWORD(PAGESIZE) + MEMUWORD(PAGESIZE) * memgetbufslot(buf)))
#define membufblksize(buf, type, slot)                                  \
    ((type != MEMPAGEBUF)                                               \
     ? (MEMUWORD(1) << (slot))                                          \
     : (MEMUWORD(PAGESIZE) + MEMUWORD(PAGESIZE) * (slot)))
#define membufgetptr(buf, ptr)                                          \
    ((buf)->ptrtab[membufblkid(buf, ptr)])
#define membufsetadr(buf, ptr, adr)                                     \
    ((buf)->ptrtab[membufblkid(buf, ptr)] = (adr))
#define membufgetpageadr(buf, ndx)                                      \
    ((buf)->ptrtab[(ndx)])
#define membufsetpageadr(buf, ndx, adr)                                 \
    ((buf)->ptrtab[(ndx)] = (adr))

#define memgetnbufblk(bkt, type, slot)                                  \
    ((bkt)->nblk ? (bkt)->nblk : g_mem.bufvals.nblk[type][slot])
#define memgetnbuftls(bkt, type, slot)                                  \
    ((bkt)->nmax ? (bkt)->nmax : g_mem.bufvals.ntls[type][slot])
#define memgetnbufglob(bkt, type, slot)                                 \
    ((bkt)->nmax ? (bkt)->nmax : g_mem.bufvals.nglob[type][slot])

void            meminit(void);
struct memtls * meminittls(void);
MEMPTR_T        memgetblk(MEMWORD_T slot, MEMWORD_T type,
                          MEMUWORD_T size, MEMUWORD_T align);
MEMPTR_T        memsetbuf(MEMPTR_T ptr, struct membuf *buf, MEMWORD_T id);
#if (MEMMULTITAB)
struct membuf * memfindbuf(void *ptr, MEMWORD_T incr);
#elif (MEMNEWHASH)
MEMADR_T        membufop(MEMPTR_T ptr, MEMWORD_T op, struct membuf *buf,
                         MEMWORD_T id);
#elif (MEMHASH)
MEMADR_T        memfindbuf(void *ptr, MEMWORD_T incr, MEMADR_T *keyret);
#else
struct membuf * memfindbuf(void *ptr, long rel);
#endif
void            memrelblk(void *ptr, struct membuf *buf, MEMWORD_T id);
#if (MEMTEST)
void            memprintbuf(struct membuf *buf, const char *func);
long            _memchkptr(struct membuf *buf, MEMPTR_T ptr);
long            _memchkbuf(struct membuf *buf, MEMWORD_T slot, MEMWORD_T type,
                           MEMWORD_T nblk, MEMUWORD_T flg, const char *func);
#endif

#if (MEMSTAT)
void            memprintstat(void);
struct memstat {
    MEMUWORD_T nbsmall;
    MEMUWORD_T nbpage;
    MEMUWORD_T nbbig;
    MEMUWORD_T nbheap;
    MEMUWORD_T nbmap;
    MEMUWORD_T nbbook;
    MEMUWORD_T nbhash;
};
#endif

#endif /* __ZERO_MEM_H__ */

