#ifndef __KERN_IO_DRV_PC_PCI_H__
#define __KERN_IO_DRV_PC_PCI_H__

#include <stdint.h>
#include <zero/param.h>
#include <zero/cdecl.h>

#define PCICONFADR  0x0cf8
#define PCICONFDATA 0x0cfc

#define PCIRES1MASK 0x00000003
#define PCIREGMASK  0x000000fc
#define PCIFUNCMASK 0x00000700
#define PCIDEVMASK  0x0000f800
#define PCIBUSMASK  0x00ff0000
#define PCIRES2MASK 0x7f000000
#define PCICONFBIT  0x80000000

/* class codes */
#define PCINOCLASS    0x00
#define PCISTORAGE    0x01
#define PCINETWORK    0x02
#define PCIDISPLAY    0x03
#define PCIMULTIMEDIA 0x04
#define PCIMEMORY     0x05
#define PCIBRIDGE     0x06
#define PCICOMM       0x07
#define PCIPERIPHERAL 0x08
#define PCIINPUT      0x09
#define PCIDOCK       0x0a
#define PCIPROCESSOR  0x0b
#define PCISERIALBUS  0x0c
#define PCIWIRELESS   0x0d
#define PCIIOCTRL     0x0e
#define PCISATELLITE  0x0f
#define PCICRYPT      0x10
#define PCIDATASIGNAL 0x11

struct pcihdr00 {
    uint16_t vendor;
    uint16_t dev;
    uint16_t cmd;
    uint16_t status;
    uint8_t  rev;
    uint8_t  progif;
    uint8_t  subclass;
    uint8_t  class;
    uint8_t  clsize;
    uint8_t  lattmr;
    uint8_t  hdrtype;
    uint8_t  bist;
    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t bar4;
    uint32_t bar5;
    uint32_t cbcis;
    uint16_t subvendor;
    uint16_t subsys;
    uint32_t romadr;
    uint8_t  cap;
    uint8_t  res1;
    uint16_t res2;
    uint32_t res3;
    uint8_t  intrline;
    uint8_t  intrpin;
    uint8_t  mingrant;
    uint8_t  maxlat;
} PACK();

struct pcihdr01 {
    uint16_t vendor;
    uint16_t dev;
    uint16_t cmd;
    uint16_t status;
    uint8_t  rev;
    uint8_t  progif;
    uint8_t  subclass;
    uint8_t  class;
    uint32_t bar0;
    uint32_t bar1;
    uint8_t  primbus;
    uint8_t  secbus;
    uint8_t  subbus;
    uint8_t  seclattmr;
    uint8_t  iobase;
    uint8_t  iolim;
    uint16_t secstatus;
    uint16_t membase;
    uint16_t memlim;
    uint16_t prefmemlo;
    uint16_t preflimlo;
    uint32_t prefmemhi;
    uint32_t preflimhi;
    uint16_t iobasehi;
    uint16_t iolimhi;
    uint8_t  cap;
    uint8_t  res1;
    uint16_t res2;
    uint32_t romadr;
    uint8_t  intrline;
    uint8_t  intrpin;
    uint16_t bridgectl;
} PACK();

/* TODO: implement header 02 (pci-to-cardbus bridge) */

struct pcihdrtype {
    unsigned type : 7;
    unsigned mf   : 1;
} PACK();

struct pcibist {
    unsigned cmplcode  : 4;
    unsigned res       : 2;
    unsigned startbist : 1;
    unsigned bistcap   : 1;
};

struct pcicmd {
    unsigned iospace      : 1;
    unsigned memspace     : 1;
    unsigned busmaster    : 1;
    unsigned speccycles   : 1;
    unsigned wrinvldenab  : 1;
    unsigned vgapalsnoop  : 1;
    unsigned parity       : 1;
    unsigned res1         : 1;
    unsigned serrenab     : 1;
    unsigned fastbtobenab : 1;
    unsigned intrdisab    : 1;
    unsigned res2         : 1;
} PACK();

struct pcistatus {
    unsigned res1      : 3;
    unsigned intrstat  : 1;
    unsigned cap       : 1;
    unsigned mhz66     : 1;
    unsigned res2      : 1;
    unsigned fastbtob  : 1;
    unsigned masterpar : 1;
    unsigned devsel    : 2;
    unsigned sigabort  : 1;
    unsigned targabort : 1;
    unsigned mastabort : 1;
    unsigned syserr    : 1;
    unsigned parityerr : 1;
} PACK();

struct pcibaseadr {
    unsigned zero     : 1;
    unsigned type     : 2;
    unsigned prefetch : 1;
    unsigned adr      :28;
} PACK();

struct pciiobaseadr {
    unsigned one : 1;
    unsigned res : 1;
    unsigned adr : 30;
} PACK();

#endif /* __KERN_IO_DRV_PC_PCI_H__ */
