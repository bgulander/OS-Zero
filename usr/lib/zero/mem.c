#include <stddef.h>
#include <stdio.h>
#if (MEMDEBUG)
#include <stdio.h>
//#include <assert.h>
//#include <crash.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <zero/cdefs.h>
#include <zero/param.h>
#include <zero/unix.h>
#include <zero/mem.h>
#include <zero/hash.h>
#if (MEM_LK_TYPE & MEM_LK_PRIO)
#include <zero/priolk.h>
#endif

static pthread_once_t               g_thronce = PTHREAD_ONCE_INIT;
static pthread_key_t                g_thrkey;
THREADLOCAL volatile struct memtls *g_memtls;
struct mem                          g_mem;
#if (MEMSTAT)
struct memstat                      g_memstat;
#endif

static void
memfreetls(void *arg)
{
    struct memtls *tls = g_memtls;
    void          *adr = arg;
    struct membkt *src;
    struct membkt *dest;
    MEMUWORD_T     slot;
    MEMUWORD_T     n;
    struct membuf *head;
    struct membuf *buf;
    MEMADR_T       upval;

    src = &tls->smallbin[0];
    dest = &g_mem.smallbin[0];
    for (slot = 0 ; slot < PTRBITS ; slot++) {
        memlkbit(&src->list);
        upval = (MEMADR_T)src->list;
        upval &= ~MEMLKBIT;
        head = (struct membuf *)upval;
        if (head) {
            buf = head;
            buf->bkt = dest;
            while (buf->next) {
                buf = buf->next;
                buf->bkt = dest;
            }
            n = src->nbuf;
            memlkbit(&dest->list);
            upval = (MEMADR_T)dest->list;
            upval &= ~MEMLKBIT;
            dest->nbuf += n;
            buf->prev = NULL;
            buf->next = (struct membuf *)upval;
            if (upval) {
                ((struct membuf *)upval)->prev = buf;
            }
            m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t)head);
        }
        memrelbit(&src->list);
        src++;
        dest++;
    }
    src = &tls->pagebin[0];
    dest = &g_mem.pagebin[0];
    for (slot = 0 ; slot < MEMPAGESLOTS ; slot++) {
        memlkbit(&src->list);
        upval = (MEMADR_T)src->list;
        upval &= ~MEMLKBIT;
        head = (struct membuf *)upval;
        if (head) {
            buf = head;
            buf->bkt = dest;
            while (buf->next) {
                buf = buf->next;
                buf->bkt = dest;
            }
            n = src->nbuf;
            memlkbit(&dest->list);
            upval = (MEMADR_T)dest->list;
            upval &= ~MEMLKBIT;
            dest->nbuf += n;
            buf->prev = NULL;
            buf->next = (struct membuf *)upval;
            if (upval) {
                ((struct membuf *)upval)->prev = buf;
            }
            m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t)head);
        }
        memrelbit(&src->list);
        src++;
        dest++;
    }
    unmapanon(adr, memtlssize());

    return;
}

static unsigned long
memgetprioval(void)
{
    unsigned long val;

//    fmtxlk(&g_mem.priolk);
    val = g_mem.prioval;
    val++;
    val &= sizeof(long) * CHAR_BIT - 1;
    g_mem.prioval = val;
//    fmtxunlk(&g_mem.priolk);

    return val;
}

struct memtls *
meminittls(void)
{
    struct memtls *tls = NULL;
    MEMPTR_T       adr;
    unsigned long  val;

    adr = mapanon(0, memtlssize());
    if (adr != MAP_FAILED) {
        tls = (struct memtls *)memgenptrcl(adr, memtlssize(),
                                           sizeof(struct memtls));
        val = memgetprioval();
#if (MEM_LK_TYPE & MEM_LK_PRIO)
        priolkinit(&tls->priolkdata, val);
#endif
        pthread_once(&g_thronce, meminit);
        pthread_setspecific(g_thrkey, adr);
        g_memtls = tls;
    }

    return tls;
}

static void
memprefork(void)
{
    MEMUWORD_T slot;

//    fmtxlk(&g_mem.priolk);
    memgetlk(&g_mem.heaplk);
    for (slot = 0 ; slot < PTRBITS ; slot++) {
        memlkbit(&g_mem.smallbin[slot].list);
        memlkbit(&g_mem.bigbin[slot].list);
    }
    for (slot = 0 ; slot < MEMPAGESLOTS ; slot++) {
        memlkbit(&g_mem.pagebin[slot].list);
    }
#if (MEMMULTITAB)
    for (slot = 0 ; slot < MEMLVL1ITEMS ; slot++) {
        memlkbit(&g_mem.tab[slot].tab);
    }
#else
    for (slot = 0 ; slot < MEMHASHITEMS ; slot++) {
        memlkbit(&g_mem.hash[slot].chain);
    }
#endif

    return;
}

static void
mempostfork(void)
{
    MEMUWORD_T slot;

#if (MEMMULTITAB)
    for (slot = 0 ; slot < MEMLVL1ITEMS ; slot++) {
        memrelbit(&g_mem.tab[slot].tab);
    }
#else
    for (slot = 0 ; slot < MEMHASHITEMS ; slot++) {
        memrelbit(&g_mem.hash[slot].chain);
    }
#endif
    for (slot = 0 ; slot < MEMPAGESLOTS ; slot++) {
        memrelbit(&g_mem.pagebin[slot].list);
    }
    for (slot = 0 ; slot < PTRBITS ; slot++) {
        memrelbit(&g_mem.bigbin[slot].list);
        memrelbit(&g_mem.smallbin[slot].list);
    }
    memrellk(&g_mem.heaplk);
//    fmtxunlk(&g_mem.priolk);

    return;
}

void
meminit(void)
{
    void       *heap;
    long        ofs;
    void       *ptr;
    MEMPTR_T   *adr;
    MEMUWORD_T  slot;

    pthread_atfork(memprefork, mempostfork, mempostfork);
    pthread_key_create(&g_thrkey, memfreetls);
#if (MEMSTAT)
    atexit(memprintstat);
#endif
    adr = mapanon(0, 3 * (2 * PTRBITS + MEMPAGESLOTS) * sizeof(long));
    if (adr == MAP_FAILED) {

        abort();
    }
    g_mem.bufvals.nblk[MEMSMALLBUF] = (MEMUWORD_T *)&adr[0];
    g_mem.bufvals.nblk[MEMPAGEBUF] = (MEMUWORD_T *)&adr[PTRBITS];
    g_mem.bufvals.nblk[MEMBIGBUF] = (MEMUWORD_T *)&adr[PTRBITS + MEMPAGESLOTS];
    g_mem.bufvals.ntls[MEMSMALLBUF] = (MEMUWORD_T *)&adr[2 * PTRBITS + MEMPAGESLOTS];
    g_mem.bufvals.ntls[MEMPAGEBUF] = (MEMUWORD_T *)&adr[3 * PTRBITS + MEMPAGESLOTS];
    g_mem.bufvals.ntls[MEMBIGBUF] = (MEMUWORD_T *)&adr[3 * PTRBITS + 2 * MEMPAGESLOTS];
    g_mem.bufvals.nglob[MEMSMALLBUF] = (MEMUWORD_T *)&adr[4 * PTRBITS + 2 * MEMPAGESLOTS];
    g_mem.bufvals.nglob[MEMPAGEBUF] = (MEMUWORD_T *)&adr[5 * PTRBITS + 2 * MEMPAGESLOTS];
    g_mem.bufvals.nglob[MEMBIGBUF] = (MEMUWORD_T *)&adr[5 * PTRBITS + 3 * MEMPAGESLOTS];
    for (slot = 0 ; slot < PTRBITS ; slot++) {
        g_mem.bufvals.nblk[MEMSMALLBUF][slot] = memnbufblk(MEMSMALLBUF, slot);
        g_mem.bufvals.nblk[MEMBIGBUF][slot] = memnbufblk(MEMSMALLBUF, slot);
        g_mem.bufvals.ntls[MEMSMALLBUF][slot] = memnbuftls(MEMSMALLBUF, slot);
//        g_mem.bufvals.ntls[MEMBIGBUF][slot] = memnbufblk(MEMSMALLBUF, slot);
        g_mem.bufvals.nglob[MEMSMALLBUF][slot] = memnbufglob(MEMSMALLBUF, slot);
        g_mem.bufvals.nglob[MEMBIGBUF][slot] = memnbufglob(MEMSMALLBUF, slot);
    }
    for (slot = 0 ; slot < MEMPAGESLOTS ; slot++) {
        g_mem.bufvals.nblk[MEMPAGEBUF][slot] = memnbufblk(MEMPAGEBUF, slot);
        g_mem.bufvals.ntls[MEMPAGEBUF][slot] = memnbuftls(MEMPAGEBUF, slot);
        g_mem.bufvals.nglob[MEMPAGEBUF][slot] = memnbufglob(MEMPAGEBUF, slot);
    }
#if (MEMARRAYHASH) || (MEMNEWHASH)
    ptr = mapanon(0, MEMHASHITEMS * sizeof(struct memhashlist));
#elif (MEMHASH)
    ptr = mapanon(0, MEMHASHITEMS * sizeof(struct memhash));
#if (MEMSTAT)
    g_memstat.nbhash += MEMHASHITEMS * sizeof(struct memhash);
#endif
#elif (MEMHUGELOCK)
    ptr = mapanon(0, MEMLVL1ITEMS * sizeof(struct memtabl0));
#else
    ptr = mapanon(0, MEMLVL1ITEMS * sizeof(struct memtab));
#endif
    if (ptr == MAP_FAILED) {

        abort();
    }
#if (MEMHASH) || (MEMARRAYHASH) || (MEMNEWHASH)
    g_mem.hash = ptr;
#else
    g_mem.tab = ptr;
#endif
    memgetlk(&g_mem.heaplk);
    heap = growheap(0);
    ofs = (1UL << PAGESIZELOG2) - ((long)heap & (PAGESIZE - 1));
    if (ofs != PAGESIZE) {
        growheap(ofs);
    }
    memrellk(&g_mem.heaplk);

    return;
}

static struct membuf *
memallocsmallbuf(MEMUWORD_T slot)
{
    MEMPTR_T       adr = SBRK_FAILED;
    MEMWORD_T      bufsz = memsmallbufsize(slot);
    MEMUWORD_T     nblk = MEMBUFBLKS;
    MEMUWORD_T     type = MEMSMALLBUF;
    MEMUWORD_T     info = 0;
    struct membuf *buf;

    if (!(g_mem.flg & MEMNOHEAPBIT)) {
        /* try to allocate from heap (sbrk()) */
        memgetlk(&g_mem.heaplk);
        adr = growheap(bufsz);
        if (adr != SBRK_FAILED) {
#if (MEMSTAT)
            g_memstat.nbheap += bufsz;
#endif
            info = MEMHEAPBIT;
        } else {
            g_mem.flg |= MEMNOHEAPBIT;
            memrellk(&g_mem.heaplk);
        }
    }
    if (adr == SBRK_FAILED) {
        /* sbrk() failed, let's try mmap() */
        adr = mapanon(0, bufsz);
        if (adr == MAP_FAILED) {
#if defined(ENOMEM)
            errno = ENOMEM;
#endif

            return NULL;
        }
#if (MEMSTAT)
        g_memstat.nbsmall += bufsz;
        g_memstat.nbmap += bufsz;
#endif
    }
    buf = (struct membuf *)adr;
#if (MEMBITFIELD)
    memsetbufflg(buf, 1);
#else
    buf->info = info;             // possible MEMHEAPBIT
#endif
    memsetbufslot(buf, slot);
    memsetbufnblk(buf, nblk);
    memsetbuftype(buf, type);
    buf->size = bufsz;
    buf->ptrtab = (MEMPTR_T *)((MEMPTR_T)buf + membufhdrsize());
#if (MEMSTAT)
    g_memstat.nbsmall += bufsz;
    g_memstat.nbheap += bufsz;
    g_memstat.nbbook += membufblkofs();
#endif
#if (MEMTEST)
    _memchkbuf(buf, slot, type, nblk, info, __FUNCTION__);
#endif

    return buf;
}

static void *
meminitsmallbuf(struct membuf *buf)
{
    MEMUWORD_T nblk = MEMBUFBLKS;
    MEMPTR_T   adr = (MEMPTR_T)buf;
    MEMPTR_T   ptr = adr + membufblkofs();

    /* initialise freemap */
    membufinitfree(buf, nblk);
    buf->base = ptr;
    nblk--;
    VALGRINDMKPOOL(ptr, 0, 0);
    memsetbufnfree(buf, nblk);

    return ptr;
}

static struct membuf *
memallocpagebuf(MEMUWORD_T slot, MEMUWORD_T nblk)
{
    MEMUWORD_T     mapsz = mempagebufsize(slot, nblk);
    MEMUWORD_T     type = MEMPAGEBUF;
    MEMPTR_T       adr;
    struct membuf *buf;

    /* mmap() blocks */
    adr = mapanon(0, mapsz);
    buf = (struct membuf *)adr;
#if (MEMBITFIELD)
    memclrbufflg(buf, 1);
#else
    buf->info = 0;
#endif
    memsetbufslot(buf, slot);
    memsetbufnblk(buf, nblk);
    memsetbuftype(buf, type);
    buf->size = mapsz;
    if (adr == MAP_FAILED) {
        
        return NULL;
    }
#if (MEMSTAT)
    g_memstat.nbpage += mapsz;
    g_memstat.nbmap += mapsz;
    g_memstat.nbbook += membufblkofs();
#endif
    buf->ptrtab = (MEMPTR_T *)((MEMPTR_T)buf + membufhdrsize());
#if (MEMTEST)
    _memchkbuf(buf, slot, type, nblk, 0, __FUNCTION__);
#endif

    return buf;
}

static void *
meminitpagebuf(struct membuf *buf, MEMUWORD_T nblk)
{
    MEMPTR_T  adr = (MEMPTR_T)buf;
    MEMPTR_T  ptr = adr + membufblkofs();

    /* initialise freemap */
    membufinitfree(buf, nblk);
    buf->base = ptr;
    nblk--;
    VALGRINDMKPOOL(ptr, 0, 0);
    memsetbufnfree(buf, nblk);

    return ptr;
}

static struct membuf *
memallocbigbuf(MEMUWORD_T slot, MEMUWORD_T nblk)
{
    MEMUWORD_T     mapsz = membigbufsize(slot, nblk);
    MEMUWORD_T     type = MEMBIGBUF;
    MEMPTR_T       adr;
    struct membuf *buf;

    /* mmap() blocks */
    adr = mapanon(0, mapsz);
    if (adr == MAP_FAILED) {
        
        return NULL;
    }
    buf = (struct membuf *)adr;
#if (MEMBITFIELD)
    memclrbufflg(buf, 1);
#else
    buf->info = 0;
#endif
    memsetbufslot(buf, slot);
    memsetbufnblk(buf, nblk);
    memsetbuftype(buf, type);
#if (MEMSTAT)
    g_memstat.nbbig += mapsz;
    g_memstat.nbmap += mapsz;
    g_memstat.nbbook += membufblkofs();
#endif
    buf->size = mapsz;
    buf->ptrtab = (MEMPTR_T *)((MEMPTR_T)buf + membufhdrsize());
#if (MEMTEST)
    _memchkbuf(buf, slot, type, nblk, 0, __FUNCTION__);
#endif

    return buf;
}

static void *
meminitbigbuf(struct membuf *buf, MEMUWORD_T nblk)
{
    MEMPTR_T adr = (MEMPTR_T)buf;
    MEMPTR_T ptr = adr + membufblkofs();

    membufinitfree(buf, nblk);
    buf->base = ptr;
    nblk--;
    VALGRINDMKPOOL(ptr, 0, 0);
    memsetbufnfree(buf, nblk);

    return ptr;
}

void *
memgetbufblk(struct membuf *head, struct membkt *bkt)
{
    void          *ptr = NULL;
    MEMUWORD_T     nfree = memgetbufnfree(head);
    MEMUWORD_T     type = memgetbuftype(head);
    MEMUWORD_T     id;

    id = membufgetfree(head);
    nfree--;
    if (type != MEMPAGEBUF) {
        ptr = membufblkadr(head, id);
    } else {
        ptr = membufpageadr(head, id);
    }
    memsetbufnfree(head, nfree);
    if (!nfree) {
        if (head->next) {
            head->next->prev = NULL;
        }
        m_syncwrite((m_atomic_t *)&bkt->list, (m_atomic_t)head->next);
        head->bkt = NULL;
        head->prev = NULL;
        head->next = NULL;
        bkt->nbuf--;
    } else {
        memrelbit(&bkt->list);
    }

    return ptr;
}

#if (MEMNEWHASH)

static void
meminithashitem(MEMPTR_T data)
{
    struct memhash *item = (struct memhash *)data;
    MEMUWORD_T     *uptr;

    data += offsetof(struct memhash, data);
    item->chain = NULL;
    uptr = (MEMUWORD_T *)data;
    uptr = memgenhashtabadr(uptr);
    item->ntab = 0;
    item->tab = (struct memhashitem *)uptr;
    item->list = NULL;

    return;
}

static struct memhash *
memgethashitem(void)
{
    struct memhash *item = NULL;
    uint8_t        *first;
    struct memhash *prev;
    struct memhash *cur;
    uint8_t        *next;
    MEMUWORD_T      bsz;
    MEMADR_T        upval;
    long            n;

    memlkbit(&g_mem.hashbuf);
    upval = (MEMADR_T)g_mem.hashbuf;
    upval &= ~MEMLKBIT;
    if (upval) {
        item = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)item->chain);
//        meminithashitem(item);
    } else {
        n = 4 * PAGESIZE / memhashsize();
        bsz = 4 * PAGESIZE;
#if (MEMSTAT)
        g_memstat.nbhash += bsz;
#endif
        item = mapanon(0, bsz);
        first = item;
        if (item == MAP_FAILED) {

            abort();
        }
        meminithashitem(first);
        upval = (MEMADR_T)g_mem.hashbuf;
        first += memhashsize();
        n--;
        next = first;
        meminithashitem(next);
        upval &= ~MEMLKBIT;
        while (--n) {
            prev = (struct memhash *)next;
            next += memhashsize();
            meminithashitem(next);
            prev->chain = (struct memhash *)next;
        }
        cur = (struct memhash *)next;
        cur->chain = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)first);
    }

    return item;
}

static void
membufhashitem(struct memhash *item)
{
    MEMADR_T upval;

    memlkbit(&g_mem.hashbuf);
    upval = (MEMADR_T)g_mem.hashbuf;
    upval &= ~MEMLKBIT;
    item->chain = (struct memhash *)upval;
    m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)item);

    return;
}

MEMADR_T
membufop(MEMPTR_T ptr, MEMWORD_T op, struct membuf *buf)
{
    MEMADR_T            adr = (MEMADR_T)ptr;
    MEMADR_T            page = adr >> PAGESIZELOG2;
    MEMUWORD_T          key = memhashptr(page) & (MEMHASHITEMS - 1);
    MEMADR_T            desc;
    MEMADR_T            val;
    MEMADR_T            upval;
    struct memhash     *blk;
    struct memhash     *prev;
    struct memhashitem *slot;
    struct memhashitem *src;
    MEMUWORD_T          lim;
    MEMUWORD_T          n;
    MEMUWORD_T          found;

    memlkbit(&g_mem.hash[key].chain);
    upval = (MEMADR_T)g_mem.hash[key].chain;
    upval &= ~MEMLKBIT;
    found = 0;
    blk = (struct memhash *)upval;
    while ((blk) && !found) {
        lim = blk->ntab;
        src = blk->tab;
        prev = NULL;
        do {
            n = min(lim, 4);
            switch (n) {
                case 4:
                    slot = &src[3];
                    if (slot->page == page) {
                        found++;
                        
                        break;
                    }
                case 3:
                    slot = &src[2];
                    if (slot->page == page) {
                        found++;
                        
                        break;
                    }
                case 2:
                    slot = &src[1];
                    if (slot->page == page) {
                        found++;
                        
                        break;
                    }
                case 1:
                    slot = &src[0];
                    if (slot->page == page) {
                        found++;
                        
                        break;
                    }
                case 0:
                default:
                    
                    break;
            }
            lim -= n;
            src += n;
        } while ((lim) && !found);
        if (!found) {
            prev = blk;
            blk = blk->chain;
        }
    }
    upval = (MEMADR_T)g_mem.hash[key].chain;
    if (!found) {
        if (op == MEMHASHDEL || op == MEMHASHCHK) {
            memrelbit(&g_mem.hash[key].chain);

            return MEMHASHNOTFOUND;
        } else {
            desc = (MEMADR_T)buf;
            upval &= ~MEMLKBIT;
            blk = (struct memhash *)upval;
            if (blk) {
                prev = NULL;
                do {
                    n = blk->ntab;
                    if (n < MEMHASHITEMS) {
                        slot = blk->tab;
                        found++;
                        n++;
                        slot->nref = 1;
                        slot->nact = 1;
                        slot->page = page;
                        blk->ntab = n;
                        slot->val = desc;
#if (MEMDEBUG)
                        crash(slot != NULL);
#endif
                        slot->val = desc;
                        if (prev) {
                            prev->chain = blk->chain;
                            blk->chain = (struct memhash *)upval;
#if (MEMHASHLOCK)
                            g_mem.hash[key].chain = blk;
#elif (MEMNEWHASH)
                            memrelbit(&g_mem.hash[key].chain);
#else
                            m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                        (m_atomic_t)blk);
#endif
                        } else {
                            memrelbit(&g_mem.hash[key].chain);
                        }

                        return desc;
                    }
                    if (!found) {
                        prev = blk;
                        blk = blk->chain;
                    }
                } while (!found && (blk));
            }
            if (!found) {
                blk = memgethashitem();
                slot = blk->tab;
                blk->ntab = 1;
                slot->nref = 1;
                slot->nact = 1;
                blk->chain = (struct memhash *)upval;
                blk->list = &g_mem.hash[key];
                slot->page = page;
                slot->val = desc;
#if (MEMHASHLOCK)
                g_mem.hash[key].chain = blk;
                memrellk(&g_mem.hash[key].lk);
#else
                m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                            (m_atomic_t)blk);
#endif
            } else {
                memrelbit(&g_mem.hash[key].chain);
            }
#if (MEMDEBUG)
            crash(desc != 0);
#endif
            
            return desc;
        }
    }
    desc = slot->val;
    upval &= ~MEMLKBIT;
    slot->nact++;
    if (op == MEMHASHDEL) {
        slot->nref--;
        if (!slot->nref) {
            if (blk->ntab == 1) {
                if (prev) {
                    prev->chain = blk->chain;
                    memrelbit(&g_mem.hash[key].chain);
                } else {
                    m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                (m_atomic_t)blk->chain);
                }
                membufhashitem(blk);
            } else {
                src = &blk->tab[n];
                slot->nref = src->nref;
                slot->nact = src->nact;
                upval &= ~MEMLKBIT;
                slot->page = src->page;
                slot->val = src->val;
                blk->ntab = n;
            }
        }
#if (MEMDEBUG)
        crash(desc != 0);
#endif
        memrelbit(&g_mem.hash[key].chain);

        return desc;
    } else if (op == MEMHASHADD) {
#if 0
        if (op == MEMHASHCHK) {
            desc = MEMHASHFOUND;
        }
#endif
        slot->nref += op;
    }
    upval &= ~MEMLKBIT;
    if (prev) {
        prev->chain = blk->chain;
        blk->chain = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                    (m_atomic_t)blk);
    } else {
        memrelbit(&g_mem.hash[key].chain);
    }
#if (MEMDEBUG)
    crash(desc != 0);
#endif
    
    return desc;
}

MEMPTR_T
memsetbuf(MEMPTR_T ptr, struct membuf *buf)
{
    MEMADR_T desc = membufop(ptr, MEMHASHADD, buf);

    return (MEMPTR_T)desc;
}

#elif (MEMHASH) || (MEMARRAYHASH)

static struct memhash *
memgethashitem(void)
{
    struct memhash *item = NULL;
    uint8_t        *first;
    uint8_t        *cur;
    struct memhash *prev;
    MEMUWORD_T      bsz;
    MEMADR_T        upval;
    long            n;

    memlkbit(&g_mem.hashbuf);
    upval = (MEMADR_T)g_mem.hashbuf;
    upval &= ~MEMLKBIT;
    if (upval) {
        item = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)item->chain);
    } else {
#if (MEMNEWHASH)
        n = 4 * PAGESIZE / memhashsize();
        bsz = 4 * PAGESIZE;
#elif (MEMBIGARRAYHASH)
        n = 4 * PAGESIZE / memhashsize();
        bsz = 4 * PAGESIZE;
#elif (MEMARRAYHASH)
        n = 2 * PAGESIZE / memhashsize();
        bsz = 2 * PAGESIZE;
#else
        n = PAGESIZE / sizeof(struct memhash);
        bsz = PAGESIZE;
#endif
#if (MEMSTAT)
        g_memstat.nbhash += bsz;
#endif
        item = mapanon(0, bsz);
        first = item;
        if (item == MAP_FAILED) {

            abort();
        }
        first += memhashsize();
        n--;
        cur = first;
        while (--n) {
            prev = (struct memhash *)cur;
            cur += memhashsize();
            prev->chain = (struct memhash *)cur;
        }
        upval = (MEMADR_T)g_mem.hashbuf;
        upval &= ~MEMLKBIT;
        cur->chain = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)first);
    }

    return item;
}

static void
membufhashitem(struct memhash *item)
{
    MEMADR_T upval;

#if (MEMARRAYHASH)
    item->ntab = 0;
#else
    item->adr = 0;
    item->val = 0;
#endif
    memlkbit(&g_mem.hashbuf);
    upval = (MEMADR_T)g_mem.hashbuf;
    upval &= ~MEMLKBIT;
    item->chain = (struct memhash *)upval;
    m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)item);

    return;
}

#endif

#if (MEMARRAYHASH) && !(MEMNEWHASH)

MEMADR_T
memfindbuf(void *ptr, MEMWORD_T incr)
{
    MEMADR_T            adr = (MEMADR_T)ptr;
    struct memhashitem *slot = NULL;
    MEMADR_T            ret = 0;
    MEMADR_T            upval;
    long                key;
    struct memhash     *head;
    struct memhash     *blk;
    struct memhash     *prev;
    struct memhashitem *src;
    MEMUWORD_T          n;
#if (MEMHASHLOOP)
    struct memhash     *hptr;
    MEMUWORD_T          lim;
#endif
    MEMUWORD_T          found;
    MEMUWORD_T          gone;

    adr >>= PAGESIZELOG2;
    key = razohash((void *)adr, sizeof(void *), MEMHASHBITS);
//    key = adr & ((MEMWORD(1) << MEMHASHBITS) - 1);
    memlkbit(&g_mem.hash[key].chain);
    upval = (MEMADR_T)g_mem.hash[key].chain;
    upval &= ~MEMLKBIT;
    head = (struct memhash *)upval;
    blk = head;
    found = 0;
    gone = 0;
    if (blk) {
        prev = NULL;
        do {
            n = blk->ntab;
            src = blk->tab;
            switch (n) {
#if (MEMBIGARRAYHASH)
                case 20:
                    slot = &src[19];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 19:
                    slot = &src[18];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 18:
                    slot = &src[17];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 17:
                    slot = &src[16];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 16:
                    slot = &src[15];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 15:
                    slot = &src[14];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 14:
                    slot = &src[13];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 13:
                    slot = &src[12];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 12:
                    slot = &src[11];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 11:
                    slot = &src[10];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
#endif
                case 10:
                    slot = &src[9];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 9:
                    slot = &src[8];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 8:
                    slot = &src[7];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 7:
                    slot = &src[6];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 6:
                    slot = &src[5];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 5:
                    slot = &src[4];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 4:
                    slot = &src[3];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 3:
                    slot = &src[2];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 2:
                    slot = &src[1];
                    if (slot->adr == adr) {
                        found++;
                        
                        break;
                    }
                case 1:
                    slot = &src[0];
                if (slot->adr == adr) {
                    found++;
                    
                    break;
                }
                case 0:
                default:

                    break;
            }
            if (!found) {
                prev = blk;
                blk = blk->chain;
            }
        } while (!found && (blk));
    }
    if (found) {
        ret = slot->val;
        if (incr) {
            slot->nref += incr;
            if (incr == MEMHASHDEL) {
                memputblk(ptr, (struct membuf *)ret);
                n--;
                if (!slot->nref) {
                    if (blk->ntab == 1) {
                        if (prev) {
                            prev->chain = blk->chain;
                            memrelbit(&g_mem.hash[key].chain);
                        } else {
                            m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                        (m_atomic_t)blk->chain);
                        }
                        membufhashitem(blk);
                        gone = 1;
                    } else {
                        src = &blk->tab[n];
                        slot->nref = src->nref;
                        slot->adr = src->adr;
                        slot->val = src->val;
                        blk->ntab = n;
                    }
                }
            } else if (incr == MEMHASHADD) {
                if (prev) {
                    prev->chain = blk->chain;
                    blk->chain = head;
                    m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                (m_atomic_t)blk);
                } else {
                    memrelbit(&g_mem.hash[key].chain);
                }

                return MEMHASHPRESENT;
            }
        }
    } else if (incr == MEMHASHADD && (head)) {
        blk = head;
        prev = NULL;
        do {
            n = blk->ntab;
            if (n < MEMHASHTABITEMS) {
                slot = &blk->tab[n];
                n++;
                slot->nref = 1;
                slot->adr = adr;
                blk->ntab = n;
                if (prev) {
                    prev->chain = blk->chain;
                    blk->chain = head;
                    blk = (struct memhash *)((MEMADR_T)ptr | MEMLKBIT);
                    m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                (m_atomic_t)blk);
                }

                return (MEMADR_T)slot;
            }
            prev = blk;
            blk = blk->chain;
        } while (blk);
    }
    if (!gone) {
        if ((prev) && (blk)) {
            prev->chain = blk->chain;
            blk->chain = head;
            m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                        (m_atomic_t)blk);
        } else {
            memrelbit(&g_mem.hash[key].chain);
        }
    }

    return ret;
}

void *
memsetbuf(void *ptr, struct membuf *buf, MEMUWORD_T info)
{
    MEMADR_T            adr = (MEMADR_T)ptr;
    MEMADR_T            key;
    MEMADR_T            val = (MEMADR_T)buf;
    MEMADR_T            ret;
    struct memhash     *item = NULL;
    struct memhashitem *slot;
    MEMADR_T            upval;

    adr >>= PAGESIZELOG2;
    ret = memfindbuf(ptr, MEMHASHADD);
    val |= info;
    slot = (struct memhashitem *)ret;
    if (ret == MEMHASHPRESENT) {

        return ptr;
    } else {
        key = razohash((void *)adr, sizeof(void *), MEMHASHBITS);
//        key = adr & ((MEMWORD(1) << MEMHASHBITS) - 1);
        if (ret) {
            slot->val = val;
            memrelbit(&g_mem.hash[key].chain);
            
            return ptr;
        } else {
            item = memgethashitem();
            memlkbit(&g_mem.hash[key].chain);
            upval = (MEMADR_T)g_mem.hash[key].chain;
            slot = &item->tab[0];
            item->ntab = 1;
            upval &= ~MEMLKBIT;
            slot->nref = 1;
            slot->adr = adr;
            slot->val = val;
            item->chain = (struct memhash *)upval;
            m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain, (m_atomic_t)item);
        }
    }

    return ptr;
}

#elif (MEMHASH) && !(MEMNEWHASH)

MEMADR_T
memfindbuf(void *ptr, MEMWORD_T incr, MEMADR_T *keyret)
{
    MEMADR_T        adr = (MEMADR_T)ptr;
    MEMADR_T        upval;
    long            key;
    struct memhash *head;
    struct memhash *item;
    struct memhash *prev;
    MEMADR_T        val;

    adr >>= PAGESIZELOG2;
//    key = razohash((void *)adr, sizeof(void *), MEMHASHBITS);
    key = adr & ((MEMWORD(1) << MEMHASHBITS) - 1);
    if (keyret) {
        *keyret = key;
    }
    memlkbit(&g_mem.hash[key].chain);
    upval = (MEMADR_T)g_mem.hash[key].chain;
    upval &= ~MEMLKBIT;
    head = (struct memhash *)upval;
    prev = NULL;
    item = head;
    while (item) {
        if (item->adr == adr) {
            val = item->val;
            if (incr) {
                item->nref += incr;
#if (MEMDEBUG)
                crash(item->nref >= 0);
#endif
                if (item->nref) {
                    if (prev) {
                        prev->chain = item->chain;
                        item->chain = head;
                        m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                    (m_atomic_t)item);
                    } else {
                        memrelbit(&g_mem.hash[key].chain);
                    }
                } else {
                    if (prev) {
                        prev->chain = item->chain;
                        memrelbit(&g_mem.hash[key].chain);
                    } else {
                        m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                    (m_atomic_t)item->chain);
                    }
                    membufhashitem(item);
                }
            } else {
                memrelbit(&g_mem.hash[key].chain);
            }
            
            return val;
        }
        prev = item;
        item = item->chain;
    }
    memrelbit(&g_mem.hash[key].chain);

    return 0;
}

void *
memsetbuf(void *ptr, struct membuf *buf, MEMUWORD_T info)
{
    MEMADR_T        adr = (MEMADR_T)ptr;
    MEMADR_T        key;
    MEMADR_T        val = memfindbuf(ptr, 1, &key);
    MEMADR_T        upval;
    struct memhash *item;

    adr >>= PAGESIZELOG2;
    if (!val) {
        item = memgethashitem();
        val = (MEMADR_T)buf;
        item->nref = 1;
        val |= info;
        item->adr = adr;
        item->val = val;
        memlkbit(&g_mem.hash[key].chain);
        upval = (MEMADR_T)g_mem.hash[key].chain;
        upval &= ~MEMLKBIT;
        item->chain = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain, (m_atomic_t)item);
    }

    return ptr;
}

#elif (MEMHUGELOCK) && (MEMMULTITAB)

void *
memsetbuf(void *ptr, struct membuf *buf, MEMUWORD_T info)
{
    MEMADR_T        val = (MEMADR_T)buf | info;
    struct memtab  *itab;
    struct memtab  *tab;
    struct memitem *item;
    long             k1;
    long             k2;
    long             k3;
    long             k4;
    void            *pstk[2] = { NULL };
    
    memgetkeybits(ptr, k1, k2, k3, k4);
    memgetlk(&g_mem.tab[k1].lk);
    itab = g_mem.tab[k1].tab;
    if (!itab) {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memtab));
        if (itab == MAP_FAILED) {
            memrelbit(&g_mem.tab[k1].lk);
            
            return NULL;
        }
        pstk[0] = itab;
        g_mem.tab[k1].tab = itab;
    }
    tab = &itab[k2];
    itab = tab->tab;
    if (!itab) {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memtab));
        if (itab == MAP_FAILED) {
            unmapanon(pstk[0], MEMLVLITEMS * sizeof(struct memtab));
            memrellk(&g_mem.tab[k1].lk);

            return NULL;
        }
        pstk[1] = itab;
        tab->tab = itab;
    }
    tab = &itab[k3];
    itab = tab->tab;
    if (!itab) {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memitem));
        if (itab == MAP_FAILED) {
            unmapanon(pstk[0], MEMLVLITEMS * sizeof(struct memtab));
            unmapanon(pstk[1], MEMLVLITEMS * sizeof(struct memtab));
            memrellk(&g_mem.tab[k1].lk);
            
            return NULL;
        }
        tab->tab = itab;
    }
    item = (struct memitem *)&itab[k4];
    item->nref++;
    item->val = val;
    memrellk(&g_mem.tab[k1].lk);
    
    return ptr;
}

struct membuf *
memfindbuf(void *ptr, long rel)
{
    MEMADR_T        ret = 0;
    struct membuf  *buf = NULL;
    struct memtab  *itab;
    struct memtab  *tab;
    struct memitem *item;
    long            k1;
    long            k2;
    long            k3;
    long            k4;

    memgetkeybits(ptr, k1, k2, k3, k4);
    memgetlk(&g_mem.tab[k1].lk);
    itab = g_mem.tab[k1].tab;
    if (itab) {
        tab = &itab[k2];
        itab = tab->tab;
        if (itab) {
            tab = &itab[k3];
            itab = tab->tab;
            if (itab) {
                item = (struct memitem *)&itab[k4];
                ret = item->val;
                if (rel) {
                    if (!--item->nref) {
                        item->val = 0;
                    }
                }
            }
        }
    }
    memrellk(&g_mem.tab[k1].lk);

    return (struct membuf *)ret;
}

#elif (MEMMULTITAB)

void *
memsetbuf(void *ptr, struct membuf *buf, MEMUWORD_T info)
{
    MEMADR_T        val = (MEMADR_T)buf | info;
    struct memtab  *itab;
    struct memtab  *tab;
    struct memitem *item;
    m_atomic_t     *lk1;
    m_atomic_t     *lk2;
    long             k1;
    long             k2;
    long             k3;
    long             k4;
    void            *pstk[2] = { NULL };
    
    memgetkeybits(ptr, k1, k2, k3, k4);
    lk1 = &g_mem.tab[k1].tab;
    memlkbit(lk1);
    itab = (struct memtab *)((MEMADR_T)g_mem.tab[k1].tab & ~MEMLKBIT);
    if (!itab) {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memtab));
        if (itab == MAP_FAILED) {
            memrelbit(lk1);
            
            return NULL;
        }
        pstk[0] = itab;
        g_mem.tab[k1].tab = (struct memtab *)((MEMADR_T)itab | MEMLKBIT);
    }
    tab = &itab[k2];
    lk2 = &tab->tab;
    if (*lk2) {
        memlkbit(lk2);
        memrelbit(lk1);
        itab = (struct memtab *)((MEMADR_T)tab->tab & ~MEMLKBIT);
    } else {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memtab));
        if (itab == MAP_FAILED) {
            unmapanon(pstk[0], MEMLVLITEMS * sizeof(struct memtab));
            memrelbit(lk1);
            
            return NULL;
        }
        pstk[1] = itab;
        lk2 = &tab->tab;
        m_syncwrite(lk2, (struct memtab *)((MEMADR_T)itab | MEMLKBIT));
        memrelbit(lk1);
    }
    tab = &itab[k3];
    lk1 = &tab->tab;
    if (tab->tab) {
        memlkbit(lk1);
        memrelbit(lk2);
        itab = (struct memtab *)((MEMADR_T)tab->tab & ~MEMLKBIT);
    } else {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memitem));
        if (itab == MAP_FAILED) {
            unmapanon(pstk[0], MEMLVLITEMS * sizeof(struct memtab));
            unmapanon(pstk[1], MEMLVLITEMS * sizeof(struct memtab));
            
            return NULL;
        }
        m_syncwrite(lk1, (struct memtab *)((MEMADR_T)itab | MEMLKBIT));
        memrelbit(lk2);
    }
    item = (struct memitem *)&itab[k4];
    item->nref++;
    item->val = val;
    memrelbit(lk1);
    
    return ptr;
}

struct membuf *
memfindbuf(void *ptr, long rel)
{
    MEMADR_T        ret = 0;
    struct membuf  *buf = NULL;
    struct memtab  *itab;
    struct memtab  *tab;
    struct memitem *item;
    m_atomic_t     *lk1;
    m_atomic_t     *lk2;
    long            k1;
    long            k2;
    long            k3;
    long            k4;

    memgetkeybits(ptr, k1, k2, k3, k4);
//    memgetlk(&g_mem.tab[k1].lk);
    lk1 = &g_mem.tab[k1].tab;
    memlkbit(lk1);
    itab = (struct memtab *)((MEMADR_T)g_mem.tab[k1].tab & ~MEMLKBIT);
    if (itab) {
        tab = &itab[k2];
        lk2 = &tab->tab;
        memlkbit(lk2);
        memrelbit(lk1);
        itab = (struct memtab *)((MEMADR_T)tab->tab & ~MEMLKBIT);
        if (itab) {
            tab = &itab[k3];
            lk1 = &tab->tab;
            memlkbit(lk1);
            memrelbit(lk2);
            itab = (struct memtab *)((MEMADR_T)tab->tab & ~MEMLKBIT);
            if (itab) {
                item = (struct memitem *)&itab[k4];
                ret = item->val;
                if (rel) {
                    if (!--item->nref) {
                        item->val = 0;
                    }
                }
            }
            memrelbit(lk1);
        } else {
            memrelbit(lk2);
        }
    } else {
        memrelbit(lk1);
    }
//    memrellk(&g_mem.tab[k1].lk);

    return buf;
}

#endif

static MEMADR_T
memopenbuf(struct membkt *bkt)
{
    MEMADR_T upval;

    if (!bkt->list) {

        return 0;
    }
    memlkbit(&bkt->list);
    upval = (MEMADR_T)bkt->list;
    upval &= ~MEMLKBIT;
    if (!upval) {
        memrelbit(&bkt->list);
    }

    return upval;
}

/* for pagebin, val is the allocation index */
MEMPTR_T
memputptr(struct membuf *buf, void *ptr, size_t size, size_t align)
{
    MEMUWORD_T type = memgetbuftype(buf);
    MEMUWORD_T slot = memgetbufslot(buf);
    MEMUWORD_T bsz = membufblksize(buf, type, slot);
    MEMUWORD_T id;
    MEMADR_T   adr = ptr;
    MEMADR_T   delta;

    if (align <= MEMMINALIGN) {
        ptr = memgenptr(adr, bsz, size);
    }
    if ((MEMADR_T)ptr & (align - 1)) {
        ptr = memalignptr(adr, align);
        delta = (MEMADR_T)ptr - adr;
        bsz -= delta;
        if (bsz < size) {
            ptr = memgenptr(ptr, bsz, size);
        }
    }
    if (type != MEMPAGEBUF) {
        membufsetptr(buf, ptr, adr);
    } else {
        id = membufpageid(buf, ptr);
        membufsetpage(buf, id, adr);
    }

    return ptr;
}

MEMPTR_T
memtryblk(MEMUWORD_T slot, MEMUWORD_T type,
          struct membkt *bkt1, struct membkt *bkt2,
          struct membuf **bufret)
{
    MEMADR_T       upval = memopenbuf(bkt1);
    struct membuf *buf = (struct membuf *)upval;
    struct membkt *bkt = bkt1;
    MEMPTR_T       ptr = NULL;
    MEMUWORD_T     info = 0;
    MEMUWORD_T     nblk = memgetnbufblk(slot, type);
    struct membkt *dest;

#if 0
    if (type == MEMSMALLBUF) {
        dest = &g_memtls->smallbin[slot];
    } else if (type == MEMPAGEBUF) {
        dest = &g_memtls->pagebin[slot];
    } else {
        dest = &g_mem.bigbin[slot];
    }
#endif
    if (buf) {
        ptr = memgetbufblk(buf, bkt1);
    } else {
        if (bkt2) {
            upval = memopenbuf(bkt2);
            buf = (struct membuf *)upval;
            bkt = bkt2;
        }
        if (buf) {
            ptr = memgetbufblk(buf, bkt2);
        } else {
            if (type == MEMSMALLBUF) {
                buf = memallocsmallbuf(slot);
                if (buf) {
                    ptr = meminitsmallbuf(buf);
                }
            } else if (type == MEMPAGEBUF) {
                buf = memallocpagebuf(slot, nblk);
                if (buf) {
                    ptr = meminitpagebuf(buf, nblk);
                }
            } else {
                buf = memallocbigbuf(slot, nblk);
                if (buf) {
                    ptr = meminitbigbuf(buf, nblk);
                }
            }
        }
    }
#if 0
    if (buf) {
        memlkbit(&dest->list);
        upval = (MEMADR_T)dest->list;
        upval &= ~MEMLKBIT;
        buf->prev = NULL;
        buf->next = (struct membuf *)upval;
        if (upval) {
            ((struct membuf *)upval)->prev = buf;
        }
        buf->bkt = dest;
        m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t)buf);
    } else {
        memrelbit(&dest->list);
    }
#endif
    *bufret = buf;

    return ptr;
}

MEMPTR_T
memgetblk(MEMUWORD_T slot, MEMUWORD_T type, MEMUWORD_T size, MEMUWORD_T align)
{
    struct membuf *buf;
    MEMPTR_T       ptr = NULL;
    MEMUWORD_T     info;
    MEMADR_T       upval = 0;
    
    if (type == MEMSMALLBUF) {
        ptr = memtryblk(slot, MEMSMALLBUF,
                        &g_memtls->smallbin[slot], &g_mem.smallbin[slot],
                        &buf);
#if (MEMDEBUG)
        crash(ptr != NULL);
        crash(buf != NULL);
#endif
        if (ptr) {
#if (MEMBITFIELD)
            info = memgetbufflg(buf, 1);
#else
            info = buf->info;
#endif
            if (info & MEMHEAPBIT && !(g_mem.flg & MEMNOHEAPBIT)) {
                memlkbit(&g_mem.heap);
                upval = (MEMADR_T)g_mem.heap;
                upval &= ~MEMLKBIT;
                buf->heap = (struct membuf *)upval;
                /* this unlocks the global heap (low-bit becomes zero) */
                m_syncwrite(&g_mem.heap, buf);
                memrellk(&g_mem.heaplk);
            }
        }
    } else if (type == MEMPAGEBUF) {
        ptr = memtryblk(slot, MEMPAGEBUF,
                        &g_memtls->pagebin[slot], &g_mem.pagebin[slot],
                        &buf);
#if (MEMDEBUG)
        crash(ptr != NULL);
        crash(buf != NULL);
#endif
    } else {
        ptr = memtryblk(slot, MEMBIGBUF,
                        &g_mem.bigbin[slot], NULL,
                        &buf);
#if (MEMDEBUG)
        crash(ptr != NULL);
        crash(buf != NULL);
#endif
    }
    if (ptr) {
        ptr = memputptr(buf, ptr, size, align);
        memsetbuf(ptr, buf);
#if (MEMTEST)
        _memchkptr(buf, ptr);
#endif
    }
#if (MEMDEBUG)
    crash(ptr != NULL);
#endif

    return ptr;
}

void
memrelbuf(MEMUWORD_T slot, MEMUWORD_T type,
          struct membuf *buf, struct membkt *src)
{
    struct membkt *dest;
    MEMADR_T       upval;
    
    if (type == MEMSMALLBUF) {
        dest = &g_mem.smallbin[slot];
        if (src == &g_memtls->smallbin[slot]
            && src->nbuf >= memgetnbuftls(slot, MEMSMALLBUF)) {
            if (buf->prev) {
                buf->prev->next = buf->next;
                memrelbit(&src->list);
            } else {
                m_syncwrite((m_atomic_t *)&src->list,
                            (m_atomic_t)buf->next);
            }
        } else if (src != &g_mem.smallbin[slot]) {
            memlkbit(&dest->list);
            upval = (MEMADR_T)dest->list;
            upval &= ~MEMLKBIT;
            buf->prev = NULL;
            buf->next = (struct membuf *)upval;
            if (upval) {
                ((struct membuf *)upval)->prev = buf;
            }
            dest->nbuf++;
            m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t *)buf);
        }
    } else if (type == MEMPAGEBUF) {
        dest = &g_mem.pagebin[slot];
        if (src == &g_memtls->pagebin[slot]
            && src->nbuf >= memgetnbuftls(slot, MEMPAGEBUF)) {
            if (buf->prev) {
                buf->prev->next = buf->next;
                memrelbit(&src->list);
            } else {
                m_syncwrite((m_atomic_t *)&src->list,
                            (m_atomic_t *)buf->next);
            }
        }
        if (dest->nbuf >= memgetnbufglob(slot, MEMPAGEBUF)) {
            unmapanon(buf->base, buf->size);
#if (MEMSTAT)
            g_memstat.nbpage -= buf->size;
            g_memstat.nbmap -= buf->size;
            g_memstat.nbbook -= membufblkofs();
#endif
        } else if (src != &g_mem.pagebin[slot]) {
            memlkbit(&dest->list);
            upval = (MEMADR_T)dest->list;
            upval &= ~MEMLKBIT;
            buf->prev = NULL;
            buf->next = (struct membuf *)upval;
            if (upval) {
                ((struct membuf *)upval)->prev = buf;
            }
            dest->nbuf++;
            m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t *)buf);
        }
    } else if (type == MEMBIGBUF) {
        if (src->nbuf >= memgetnbufglob(slot, MEMBIGBUF)) {
            if (buf->prev) {
                buf->prev->next = buf->next;
                memrelbit(&src->list);
            } else {
                m_syncwrite((m_atomic_t *)&src->list, (m_atomic_t *)buf->next);
            }
            unmapanon(buf, buf->size);
#if (MEMSTAT)
            g_memstat.nbmap -= buf->size;
            g_memstat.nbbig -= buf->size;
            g_memstat.nbbook -= membufblkofs();
#endif
        }
    }

    return;
}

void
memputblk(void *ptr, struct membuf *buf)
{
    struct membkt *bkt = buf->bkt;
    struct membkt *dest;
    MEMUWORD_T     slot = memgetbufslot(buf);
    MEMUWORD_T     nblk = memgetbufnblk(buf);
    MEMUWORD_T     nfree = memgetbufnfree(buf);
    MEMUWORD_T     type = memgetbuftype(buf);
    MEMPTR_T       adr;
    MEMUWORD_T     id;
    MEMADR_T       upval;

#if (MEMTEST)
    _memchkptr(buf, ptr);
#endif
    if (bkt) {
        memlkbit(&bkt->list);
    }
    nfree++;
    if (type != MEMPAGEBUF) {
        id = membufblkid(buf, ptr);
        adr = membufblkadr(buf, id);
        membufsetptr(buf, ptr, NULL);
    } else {
        id = membufpageid(buf, ptr);
        adr = membufpageadr(buf, id);
        membufsetpage(buf, id, NULL);
    }
    setbit(buf->freemap, id);
    memsetbufnfree(buf, nfree);
    if (nfree == nblk) {
        memrelbuf(slot, type, buf, bkt);
        if (bkt) {
            memrelbit(&bkt->list);
        }
    } else if (nfree == 1) {
        if (type == MEMSMALLBUF) {
            dest = &g_memtls->smallbin[slot];
            if (dest->nbuf >= memgetnbuftls(slot, MEMSMALLBUF)) {
                dest = &g_mem.smallbin[slot];
            }
        } else if (type == MEMPAGEBUF) {
            dest = &g_memtls->pagebin[slot];
            if (dest->nbuf >= memgetnbuftls(slot, MEMPAGEBUF)) {
                dest = &g_mem.smallbin[slot];
            }
        } else {
            dest = &g_mem.bigbin[slot];
        }
        memlkbit(&dest->list);
        upval = (MEMADR_T)dest->list;
        upval &= ~MEMLKBIT;
        buf->prev = NULL;
        buf->next = (struct membuf *)upval;
        if (upval) {
            ((struct membuf *)upval)->prev = buf;
        }
        buf->bkt = dest;
        dest->nbuf++;
        m_syncwrite((m_atomic_t *)&dest->list, buf);
    } else if (bkt) {
        memrelbit(&bkt->list);
    }

    return;
}

