#include <kern/conf.h>
#include <zero/cdecl.h>
#include <zero/param.h>
#include <zero/types.h>
#include <zero/mtx.h>
#include <kern/proc/thr.h>
#include <kern/unit/x86/asm.h>

FASTCALL struct thr *(*schedpickthr)(void);

void
schedinit(void)
{
#if (ZEROSCHED)
    schedpickthr = thrpick;
#else
#error define supported scheduler such as ZEROSCHED
#endif

    return;
}

#if 0
void
schedyield(void)
{
    struct thr *thr = schedpickthr();
    
    thrjmp(thr);
}
#endif

void
schedyield(void)
{
    struct thr *thr;

    if (schedpickthr) {
        thr = schedpickthr();
        thrjmp(thr);
    }

    return;
}

void
schedloop(void)
{
    /* scheduler loop; interrupted by timer [and other] interrupts */
    do {
        /* enable all interrupts */
#if !(APIC)
        outb(0x00, PICMASK1);
        outb(0x00, PICMASK2);
#endif
        /* wait for interrupt */
        k_waitint();
    } while (1);

    /* NOTREACHED */
    return;
}

