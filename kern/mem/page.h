#ifndef __KERN_MEM_PAGE_H__
#define __KERN_MEM_PAGE_H__

#include <kern/conf.h>
#include <stdint.h>
#include <zero/param.h>
#include <zero/trix.h>
#include <zero/asm.h>
#include <zero/tktlk.h>
#include <kern/types.h>
#include <kern/perm.h>
#include <kern/time.h>
#include <kern/cpu.h>
#include <kern/mem/vm.h>

#define PAGENODEV (-1)
#define PAGENOOFS (-1)

/* page ID */
#define pagenum(adr)  ((adr) >> PAGESIZELOG2)
//#define swapblknum(sp, pg) ((pg) - (sp)->pgtab)
#define pageadr(pg, pt)                                                 \
    ((!(pg)) ? NULL : ((void *)(((pg) - (pt)) << PAGESIZELOG2)))

/* index into table of LRU-queues */
#define pagecalcqid(pg)   max(PTRBITS - 1, lzerol(pg->nflt))

/* working sets */
#if 0
#define pageinset(pg)  (vmsetmap[pagenum((pg)->adr)])
#define pageaddset(pg) (vmsetmap[pagenum((pg)->adr)] = k_curpid)
extern pid_t           vmsetmap[NPAGEPHYS];
#endif
#define pageinset(pg)  bitset(vmsetbitmap, pagenum((pg)->adr))
#define pageaddset(pg) setbit(vmsetbitmap, pagenum((pg)->adr))
#define pageclrset(pg) clrbit(vmsetbitmap, pagenum((pg)->adr))

#if 0
#define PAGEWIREBIT 0x00000001
#define PAGEBUFBIT  0x00000002
struct virtpage {
    void            *adr;     // virtual or physical address
    uintptr_t        link;    // prev XOR next
#if 0
    unsigned long    flg;
    struct virtpage *prev;
    struct virtpage *next;
#endif
};
#endif

#define __STRUCT_PHYSLRUQUEUE_SIZE                                      \
    (sizeof(long) + sizeof(void *))
#define __STRUCT_PHYSLRUQUEUE_PAD                                       \
    (roundup(__STRUCT_PHYSLRUQUEUE_SIZE, CLSIZE) - __STRUCT_PHYSLRUQUEUE_SIZE)
struct physlruqueue {
    VM_LK_T          lk;
    struct page *list;
    uint8_t          _pad[__STRUCT_PHYSLRUQUEUE_PAD];
};

#define __STRUCT_PHYSPAGE_SIZE                                          \
    (sizeof(long) + 3 * sizeof(void *) + 4 * sizeof(m_ureg_t))
#define __STRUCT_PHYSPAGE_PAD                                           \
    (roundup(__STRUCT_PHYSPAGE_SIZE, CLSIZE) - __STRUCT_PHYSPAGE_SIZE)
struct page {
    VM_LK_T      lk;        // mutual exclusion lock
    m_ureg_t     nref;      // reference count
    m_ureg_t     pid;       // owner process ID
    m_ureg_t     adr;       // page address
    struct perm *perm;      // permissions
    m_ureg_t     nflt;      // # of page-fault exceptions triggered
    struct page *prev;      // previous on queue
    struct page *next;      // next on queue
    uint8_t          _pad[__STRUCT_PHYSPAGE_PAD];
};

typedef uint64_t swapoff_t;

#define PAGEDEVMASK        ((UINT64_C(1) << PAGESIZELOG2) - 1)
#define swapsetblk(u, blk) ((u) |= (blk) << PAGESIZELOG2)
#define swapsetdev(u, dev) ((u) |= (dev))
#define swapblkid(adr)     ((adr) >> PAGESIZELOG2)
#define swapdevid(adr)     ((adr) & PAGEDEVMASK)
#define __STRUCT_SWAPDEV_SIZE                                           \
    (sizeof(long) + 2 * sizeof(void *) + sizeof(swapoff_t))
#define __STRUCT_SWAPDEV_PAD                                            \
    (roundup(__STRUCT_SWAPDEV_SIZE, CLSIZE) - __STRUCT_SWAPDEV_SIZE)
struct swapdev {
    m_atomic_t   lk;
    swapoff_t    npg;
    swapoff_t   *pgmap;
    struct page *pgtab;
    struct page *freeq;
    uint8_t      _pad[__STRUCT_SWAPDEV_PAD];
};

#define DEQ_SINGLE_TYPE
#define DEQ_TYPE struct page
#include <zero/deq.h>

unsigned long     pageinitphys(uintptr_t base, unsigned long nb);
struct page *     pageallocphys(void);
void              pagefreephys(void *adr);
#if 0
void              pagefree(void *adr);
void              swapfree(uintptr_t adr);
#endif

#endif /* __KERN_MEM_PAGE_H__ */

