#ifndef __KERN_MEM_H__
#define __KERN_MEM_H__

#include <zero/param.h>
#include <kern/perm.h>
#include <kern/mem/mem.h>
#define  kwalloc(nb)   memalloc(nb, MEMWIRE)
#define  kmalloc(nb)   memalloc(nb, 0)
#define  kcalloc(nb)   memalloc(nb, MEMZERO)
#define  kcwalloc(nb)  memalloc(nb, MEMZERO | MEMWIRE)
#define  kvalloc(nb)   memalloc(PAGESIZE, 0)
#define  kcvalloc(nb)  memalloc(PAGESIZE, MEMZERO)
#define  kcvwalloc(nb) memalloc(PAGESIZE, MEMZERO | MEMWIRE)
void    *memalloc(unsigned long nb, long flg);
void     kfree(void *ptr);

struct memreg {
    struct perm  perm;		// permission structure
    long         flg;
    void        *adr;		// base address of region
    size_t       ofs;		// offset into region (for locks and such)
    size_t       len;		// length
};

#endif /* __KERN_MEM_H__ */

