/*
 * IA-32 page tables are accessed as a flat 4-megabyte set of page table
 * entries. The table is mapped to the address PAGETAB declared in vm.h.
 */

#define PAGEDEV 0

#include <kern/conf.h>
#include <stddef.h>
#include <stdint.h>
#include <zero/cdefs.h>
#include <zero/param.h>
#include <zero/trix.h>
//#include <kern/io/dev.h>
#include <kern/util.h>
#include <kern/mem/vm.h>
#include <kern/mem/page.h>
//#include <kern/proc/task.h>
// #include <kern/io/buf.h>
#include <kern/io/drv/pc/dma.h>
#include <kern/unit/x86/link.h>
#include <kern/unit/ia32/seg.h>
#if (SMP)
#include <kern/unit/ia32/mp.h>
#endif

#define HICORE (1024 * 1024)

void pginit(void);

extern uint8_t       kernsysstktab[NCPU * KERNSTKSIZE];
extern uint8_t       kernusrstktab[NCPU * KERNSTKSIZE];
extern pde_t         kernpagedir[NPDE];
extern pde_t         usrpagedir[NPDE];
#if (VMFLATPHYSTAB)
struct page          vmphystab[NPAGEMAX] ALIGNED(PAGESIZE);
#endif
//m_atomic_t         vmlrulktab[PTRBITS];
struct physlruqueue  vmlrutab[PTRBITS];

//static struct vmpage  vmpagetab[NPAGEMAX] ALIGNED(PAGESIZE);
#if (PAGEDEV)
static struct dev    vmdevtab[NPAGEDEV];
static m_atomic_t    vmdevlktab[NPAGEDEV];
#endif
VM_LK_T              vmphyslk;
struct page         *vmphysqueue;
struct page         *vmshmqueue;
struct vmpagestat    vmpagestat;

/*
 * 32-bit page directory is flat 4-megabyte table of page-tables.
 * for virtual address virt,
 *
 *     vmphysadr = pagetab[vmpagenum(virt)]; // physical address
 */
void
vmmapseg(void *pagetab, uint32_t virt, uint32_t phys, uint32_t lim,
         uint32_t flg)
{
    pte_t *pte;
    long   n;

    n = rounduppow2(lim - virt, PAGESIZE) >> PAGESIZELOG2;
    pte = (pte_t *)pagetab + vmpagenum(virt);
    vmpagestat.nmap += n;
    while (n--) {
        *pte = phys | flg;
        phys += PAGESIZE;
        pte++;
    }

    return;
}

/*
 * initialise virtual memory
 * - no locking; call before going multiprocessor
 * - zero page tables
 * - initialize page directory
 * - map page directory index page
 * - map our segments
 * - initialise paging
 */
void
vminit(void *pagetab)
{
    pde_t    *pde;
    pte_t    *pte;
    uint32_t  adr;
    long      n;

    /* PHYSICAL MEMORY */

    /* initialize page directory index page */
    pde = kernpagedir;
    adr = (uint32_t)pagetab;
    n = NPDE;
    while (n--) {
        *pde = adr | PAGEPRES | PAGEWRITE;
        adr += NPTE * sizeof(uint32_t);
        pde++;
    }

    /* map page directory index page */
    pde = (pde_t *)pagetab + vmpagenum(kernpagedir);
    adr = (uint32_t)&kernpagedir;
    *pde = adr | PAGEUSER | PAGEPRES | PAGEWRITE;

    /* zero page tables */
    kbzero(pagetab, PAGETABSIZE);

    /* zero stacks */
    kbzero(kernsysstktab, NCPU * KERNSTKSIZE);
    kbzero(kernusrstktab, NCPU * KERNSTKSIZE);
    /* map kernel-mode stacks */
    vmmapseg(pagetab,
             (uint32_t)kernsysstktab, (uint32_t)kernsysstktab,
             (uint32_t)kernsysstktab + NCPU * KERNSTKSIZE,
             PAGEPRES | PAGEWRITE);
    /* map user-mode stacks */
    vmmapseg(pagetab,
             (uint32_t)kernusrstktab, (uint32_t)kernusrstktab,
             (uint32_t)kernusrstktab + NCPU * KERNSTKSIZE,
             PAGEUSER | PAGEPRES | PAGEWRITE);
#if defined(__x86_64__) || defined(__amd64__)
    /* zero page structures */
    kbzero(kernpagetab4, 4 * PAGESIZE);
#endif

    /* identity-map 0..1M */
    vmmapseg(pagetab, 0, 0,
             HICORE,
             PAGEPRES | PAGEWRITE | PAGENOCACHE | PAGEWIRED);
#if (SMP)
    vmmapseg(pagetab, (uint32_t)MPENTRY, (uint32_t)MPENTRY,
             (uint32_t)&mpend - (uint32_t)&mpentry + MPENTRY,
             PAGEPRES);
#endif

    /* identity-map kernel low-half boot segment */
    vmmapseg(pagetab, HICORE, HICORE,
             (uint32_t)&_eboot,
             PAGEPRES | PAGEWRITE);

#if (USYSINFO)
    /* identity-map USYSINFO */
    vmmapseg(pagetab, (uint32_t)&_usysinfo, (uint32_t)&_usysinfo,
             (uint32_t)&_eusysinfo,
             PAGEPRES | PAGEWRITE);
#endif

    /* identity-map kernel DMA buffers */
    vmmapseg(pagetab, (uint32_t)&_dmabuf, DMABUFBASE,
             (uint32_t)&_dmabuf + DMABUFSIZE,
             PAGEPRES | PAGEWRITE | PAGENOCACHE | PAGEWIRED);

    /* identity-map page tables */
    vmmapseg(pagetab, (uint32_t)pagetab, (uint32_t)pagetab,
             (uint32_t)pagetab + PAGETABSIZE,
             PAGEPRES | PAGEWRITE | PAGEWIRED);

#if 0
    /* identity map free RAM */
    vmmapseg(pagetab, (uint32_t)&_epagetab, (uint32_t)&_epagetab,
             KERNVIRTBASE - (uint32_t)&_epagetab,
             PAGEWRITE);
#endif
//    kbzero(&_epagetab, lim - (uint32_t)&_epagetab);

    /* VIRTUAL MEMORY */

    /* map kernel text/read-only segments */
    vmmapseg(pagetab, (uint32_t)&_text, vmlinkadr((uint32_t)&_textvirt),
             (uint32_t)&_etextvirt,
             PAGEPRES);

    /* map kernel DATA and BSS segments */
    vmmapseg(pagetab, (uint32_t)&_data, vmlinkadr((uint32_t)&_datavirt),
             (uint32_t)&_ebssvirt,
             PAGEPRES | PAGEWRITE);

    /* identity-map 3.5G..4G */
// devmap(pagetab, DEVMEMBASE, 512 * 1024 * 1024);

    /* initialize paging */
    pginit();

    return;
}

void
vminitphys(uintptr_t base, unsigned long nbphys)
{
    unsigned long nb = min(nbphys, KERNVIRTBASE);

    /* initialise physical memory manager */
    pageinitphys(base, nb);

    return;
}

void
vminitvirt(void *pagetab, void *virt, uint32_t size, uint32_t flg)
{
    void  *adr;
    pte_t *pte;
    long   n;

    n = rounduppow2(size, PAGESIZE) >> PAGESIZELOG2;
    pte = (pte_t *)pagetab + vmpagenum(virt);
    while (n--) {
        *pte = flg;
        pte++;
    }

    return;
}

void
vmfreephys(void *virt, uint32_t size)
{
//    struct vmbuf *buf;
    uint32_t  adr;
    pte_t    *pte;
    long      n;
//    long          nref;
//    struct page  *pg;

    n = rounduppow2(size, PAGESIZE) >> PAGESIZELOG2;
    pte = (pte_t *)&_pagetab + vmpagenum(virt);
    while (n--) {
        adr = (uint32_t)*pte;
        adr &= VMPAGEMASK;
        if (!adr) {

            continue;
        }

        if (*pte & PAGESWAPPED) {
//            swapfree(adr);
        } else if (!(*pte & PAGEWIRED)) {
#if 0
            pg = pagefind(adr);
            pagerm(pg);
#endif
            vmpagestat.nmap--;
        } else {
//                kprintf("UNWIRE\n");
            vmpagestat.nwire--;
        }
        pagefreephys((void *)adr);
        *pte = 0;
        pte++;
    }

    return;
}

FASTCALL
void
vmpagefault(uint32_t pid, uint32_t adr, uint32_t error)
{
    pte_t         *pte = (pte_t *)&_pagetab + vmpagenum(adr);
    uint32_t       flg = (uint32_t)*pte & (PAGEFLTFLGMASK | PAGESYSFLAGS);
    struct page   *page = NULL;
    unsigned long  qid;

    kprintf("PAGEFAULT: pid == %lx, adr == %lx, error == %lx\n",
            pid, adr, error);
    if (!(adr & ~(PAGEFLTADRMASK | PAGESYSFLAGS))) {
        page = pageallocphys();
        if (page) {
            vmspinlk(&page->lk);
            page->nref++;
            if (flg & PAGEWIRED) {
                vmpagestat.nwire++;
            } else {
                vmpagestat.nmap++;
                page->nflt++;
                if (!(adr & PAGEWIRED)) {
                    qid = pagecalcqid(page);
                    vmspinlk(&vmlrutab[qid].lk);
                    deqpush(page, &vmlrutab[qid].list);
                    vmrellk(&vmlrutab[qid].lk);
                }
            }
            vmrellk(&page->lk);
            *pte = adr | flg | PAGEPRES;
        }
#if (PAGEDEV)
    } else if (!(page & PAGEPRES)) {
        // pageout();
        page = vmpagein(page);
        if (page) {
            vmspinlk(&page->lk);
            page->nflt++;
            qid = pagecalcqid(page);
            vmspinlk(&vmlrutab[qid].lk);
            deqpush(page, &vmlrutab[qid].list);
            vmrellk(&vmlrutab[qid].lk);
            vmrellk(&page->lk);
        }
#endif
    }
#if 0
    if (page) {
        pageaddset(page);
    }
#endif

    return;
}

#if 0
void
vmseekdev(uint32_t dev, uint64_t ofs)
{
    devseek(vmdevtab[dev], ofs, SEEK_SET);
}

uint32_t
vmpagein(uint32_t adr)
{
    uint32_t     pageid = vmpagenum(adr);
    uint32_t     blk = vmblkid(pageid);
    struct page *page = pagefind(adr);
    void        *data;

    vmgetlk(&vmdevlktab[dev], MEMPID);
    vmseekdev(dev, blk * PAGESIZE);
    page->nflt++;
//    data = pageread(dev, PAGESIZE);
    vmrellk(&vmdevlktab[pagedev], MEMPID);
}

void
vmpagefree(uint32_t adr)
{
    ;
}
#endif /* 0 */

