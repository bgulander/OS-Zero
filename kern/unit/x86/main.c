#include <zero/cdefs.h>
#include <kern/conf.h>
#if (VBE)
#include <kern/io/drv/pc/vbe.h>
#endif
#include <kern/unit/x86/boot.h>
#include <kern/unit/x86/asm.h>
#include <kern/unit/x86/link.h>

extern uint8_t kernusrstktab[NCPU * KERNSTKSIZE];

extern void seginit(long id);
#if (VBE)
extern void vbeinit(void);
#endif
extern void trapinit(void);
extern void kinitprot(unsigned long pmemsz);
extern void kinitlong(unsigned long pmemsz);

long kernlongmode;

ASMLINK
void
kmain(unsigned long longmode, struct mboothdr *boothdr)
{
    unsigned long pmemsz;

    /* bootstrap kernel */
    /* INITIALISE BASE HARDWARE */
    seginit(0);
#if (VBE) && 0
    /* initialise VBE graphics subsystem */
    vbeinit();
#endif
    trapinit();
    kernlongmode = longmode;
    /* determine amount of RAM */
    pmemsz = grubmemsz(boothdr);
    kinitprot(pmemsz);

    /* kinitprot() should never return */
    k_halt();

    /* NOTREACHED */
    return;
}

