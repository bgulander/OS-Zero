#ifndef __UNIT_IA32_VM_H__
#define __UNIT_IA32_VM_H__

#include <stdint.h>
#include <kern/buf.h>

#define __KERNEL__ 1
#include <zero/mtx.h>
#define vmlklruq(pq)   mtxlk(&pq->lk, 0)
#define vmunlklruq(pq) mtxunlk(&pq->lk, 0)
#define vmlkbufq()     mtxlk(&vmbufq.lk, 0)
#define vmunlkbufq()   mtxunlk(&vmbufq.lk, 0)

extern uint32_t _kernpagedir[];

void *vmmapvirt(uint32_t *pagetab, void *virt, uint32_t size, uint32_t flags);
void  vmfreephys(void *virt, uint32_t size);

#define KERNVIRTBASE      0xc0000000U
#define vmphysadr(adr)    ((uint32_t)(adr) - KERNVIRTBASE)

#define vmpagedirnum(adr) ((uint32_t)(adr) >> PDSHIFT)
#define vmpagenum(adr)    ((uint32_t)(adr) >> PTSHIFT)
#define vmpageofs(adr)    ((uint32_t)(adr) & (PAGESIZE - 1))

#define vmpageid(adr)     ((uint32_t)(adr) >> PAGESIZELOG2)
#define vmbufid(adr)      ((uint32_t)(adr) >> BUFSIZELOG2)
#define vmisbufadr(adr)   (!((uint32_t)(adr) & (BUFSIZE - 1)))

#define vmpageadr(pg, pt) (((pg) - (pt)) << PAGESIZELOG2)

/* internal macros */

static __inline__ void
vmflushtlb(void *mp)
{
    __asm__ __volatile__ ("invlpg (%0)\n" : : "r" (mp) : "memory");
}

/* virtual memory parameters */
#define NPAGEMAX        (NPDE * NPTE)                   // # of virtual pages
#define NPDE            1024                            // per directory
#define NPTE            1024                            // per table
//#define PAGETAB         0x00700000U                     // physical address
#define PAGETABSIZE     (NPDE * NPTE * sizeof(uint32_t))
#define PDSHIFT         22
#define PTSHIFT         12
#define VMPDMASK        0xffc00000                      // top 10 bits
#define VMPTMASK        0x003ff000                      // bits 12..21
#define VMPGMASK        0xfffff000U                     // page frame; 22 bits

/* page structure setup */

/*
 * page flags
 */
/* standard IA-32 flags */
#define PAGEPRES     0x00000001U	// present
#define PAGEWRITE    0x00000002U	// writeable
#define PAGEUSER     0x00000004U	// user-accessible
#define PAGEBUSY     0x00000020U	// has been accessed
#define PAGEDIRTY    0x00000040U	// has been written to
#define PAGESUPER    0x00000080U	// 4M page
#define PAGEGLOBAL   0x00000100U	// global
#define PAGESYS1     0x00000200U	// reserved for system
#define PAGESYS2     0x00000400U	// reserved for system
#define PAGESYS3     0x00000800U	// reserved for system
/* custom flags */
#define PAGESWAPPED  PAGESYS1	// on disk
#define PAGEBUF      PAGESYS2	// buffer cache
#define PAGEWIRED    PAGESYS3	// wired
#define PAGESYSFLAGS (PAGESWAPPED | PAGEBUF | PAGEWIRED)

/* page fault management */

/* page fault exception */
#define NPAGEDEV     16
//#define pfdev(adr)  (((adr) & PFDEVMASK) >> 3)
#define pfadr(adr)   ((adr) & PFPAGEMASK)
#define PFPRES       0x00000001U	// page is present
#define PFWRITE      0x00000002U	// write fault
#define PFUSER       0x00000004U	// user fault
#define PFFLGMASK    0x00000007U
#define PFADRMASK    0xfffffff8U
#define PFPAGEMASK   0xfffff000U

struct vmpage {
    struct vmpage *prev;
    struct vmpage *next;
};

struct vmpageq {
    long           lk;
    struct vmpage *head;
    struct vmpage *tail;
};

#define VMBUFNREFMASK 0x07
#define vmsetbufnref(bp, npg)                                           \
    ((bp)->prev = (void *)((uint32_t)((bp)->prev)                       \
                           | (((npg) & VMBUFNREFMASK) << 4)),           \
     (bp)->next = (void *)((uint32_t)((bp)->next)                       \
                           | (((npg) >> 4) & VMBUFNREFMASK)))
#define vmgetbufnref(bp)                                                \
    (((uint32_t)((bp)->prev) & VMBUFNREFMASK)                           \
     | (((uint32_t)((bp)->next) & VMBUFNREFMASK) << 4))
#define vmgetprevbuf(bp)                                                \
    ((void *)((uint32_t)((bp)->prev) & ~VMBUFNREFMASK))
#define vmgetnextbuf(bp)                                                \
    ((void *)((uint32_t)((bp)->next) & ~VMBUFNREFMASK))
struct vmbuf {
    struct vmbuf *prev;
    struct vmbuf *next;
};

struct vmbufq {
    long           lk;
    struct vmbuf *head;
    struct vmbuf *tail;
};

#define vmaddpage(adr)                                                  \
    do {                                                                \
        struct vmpageq *_pageq = &vmpagelruq;                           \
        struct vmpage  *_pg = &vmpagetab[vmpageid(adr)];                \
        struct vmpage  *_head;                                          \
                                                                        \
        _pg->prev = NULL;                                               \
        vmlklruq(_pageq);                                               \
        _head = _pageq->head;                                           \
        _pg->next = _head;                                              \
        if (_head) {                                                    \
            _head->prev = _pg;                                          \
        } else {                                                        \
            _pageq->tail = _pg;                                         \
        }                                                               \
        _pageq->head = _pg;                                             \
        vmunlklruq(_pageq);                                             \
    } while (0)

#define vmrmpage(adr)                                                   \
    do {                                                                \
        struct vmpageq *_pageq = &vmpagelruq;                           \
        struct vmpage  *_pg = &vmpagetab[vmpageid(adr)];                \
        struct vmpage  *_tmp;                                           \
                                                                        \
        vmlklruq(_pageq);                                               \
        _tmp = _pg->prev;                                               \
        if (_tmp) {                                                     \
            _tmp->next = _pg->next;                                     \
        } else {                                                        \
            _tmp = _pg->next;                                           \
            _pageq->head = _tmp;                                        \
            if (_tmp) {                                                 \
                _tmp->prev = _pg->prev;                                 \
            } else {                                                    \
                _pageq->tail = _tmp;                                    \
            }                                                           \
            _pageq->head = _tmp;                                        \
        }                                                               \
        _tmp = _pg->next;                                               \
        if (_tmp) {                                                     \
            _tmp->prev = _pg->prev;                                     \
        } else {                                                        \
            _tmp = _pg->prev;                                           \
            _pageq->tail = _tmp;                                        \
            if (_tmp) {                                                 \
                _tmp->next = NULL;                                      \
            } else {                                                    \
                _pageq->head = _pageq->tail = _tmp;                     \
            }                                                           \
        }                                                               \
        vmunlklruq(_pageq);                                             \
    } while (0)

#define vmaddbuf(adr)                                                   \
    do {                                                                \
        struct vmbufq *_bufq = &vmbufq;                                 \
        struct vmbuf  *_hdrtab = vmbuftab;                              \
        struct vmbuf  *_buf = &_hdrtab[vmbufid(adr)];                   \
        struct vmbuf  *_head;                                           \
                                                                        \
        _buf->prev = NULL;                                              \
        vmlkbufq();                                                     \
        _head = _bufq->head;                                            \
        _buf->next = _head;                                             \
        if (_head) {                                                    \
            _head->prev = _buf;                                         \
        } else {                                                        \
            _bufq->tail = _buf;                                         \
        }                                                               \
        _bufq->head = _buf;                                             \
        vmunlkbufq();                                                   \
    } while (0)

#define vmrmbuf(adr)                                                    \
    do {                                                                \
        struct vmbufq *_bufq = &vmbufq;                                 \
        struct vmbuf  *_hdrtab = vmbuftab;                              \
        struct vmbuf  *_buf = &_hdrtab[vmbufid(adr)];                   \
        struct vmbuf  *_tmp;                                            \
                                                                        \
        vmlkbufq();                                                     \
        _tmp = _buf->prev;                                              \
        if (_tmp) {                                                     \
            _tmp->next = _buf->next;                                    \
        } else {                                                        \
            _tmp = _buf->next;                                          \
            _bufq->head = _tmp;                                         \
            if (_tmp) {                                                 \
                _tmp->prev = _buf->prev;                                \
            } else {                                                    \
                _bufq->tail = _tmp;                                     \
            }                                                           \
            _bufq->head = _tmp;                                         \
        }                                                               \
        _tmp = _buf->next;                                              \
        if (_tmp) {                                                     \
            _tmp->prev = _buf->prev;                                    \
        } else {                                                        \
            _tmp = _buf->prev;                                          \
            _bufq->tail = _tmp;                                         \
            if (_tmp) {                                                 \
                _tmp->next = NULL;                                      \
            } else {                                                    \
                _bufq->head = _bufq->tail = _tmp;                       \
            }                                                           \
        }                                                               \
        vmunlkbufq();                                                   \
    } while (0)

#define vmdeqpage(rpp)                                                  \
    do {                                                                \
        struct vmpageq *_pageq = &curproc->pagelruq;                    \
        struct vmpage  *_tail;                                          \
                                                                        \
        vmlklruq(_pageq);                                               \
        _tail = _pageq->tail;                                           \
        if (_tail) {                                                    \
            if (_tail->prev) {                                          \
                _tail->prev->next = NULL;                               \
            } else {                                                    \
                _pageq->head = NULL;                                    \
            }                                                           \
            _pageq->tail = _tail->prev;                                 \
        }                                                               \
        *(rpp) = _tail;                                                 \
        vmunlklruq(_pageq);                                             \
    } while (0)

#endif /* __UNIT_IA32_VM_H__ */

