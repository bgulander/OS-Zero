#include <stdint.h>
#include <stddef.h>
#include <zero/cdefs.h>
#include <zero/param.h>
#include <gfx/rgb.h>
#include <kern/conf.h>
#include <kern/util.h>
#include <kern/mem/vm.h>
#include <kern/io/drv/chr/cons.h>
#include <kern/io/drv/pc/vga.h>
#include <kern/unit/x86/link.h>

#if (VBE)

#if (NEWFONT)

#include "progcsz.h"
void *vgafontbuf = proggycleansztab;

#else /* !NEWFONT */

const unsigned char vgafont8[VGAFONTSIZE] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0x40, 0x52, 0x40, 0x5e, 0x4c, 0x40, 0x3f,
    0x3f, 0x7f, 0x6d, 0x7f, 0x61, 0x73, 0x7f, 0x3f,
    0x36, 0x7f, 0x7f, 0x7f, 0x3e, 0x1c, 0x08, 0x00,
    0x08, 0x1c, 0x3e, 0x7f, 0x3e, 0x1c, 0x08, 0x00,
    0x1c, 0x3e, 0x1c, 0x7f, 0x7f, 0x3e, 0x1c, 0x3e,
    0x08, 0x08, 0x1c, 0x3e, 0x7f, 0x3e, 0x1c, 0x3e,
    0x00, 0x00, 0x0c, 0x1e, 0x1e, 0x0c, 0x00, 0x00,
    0x7f, 0x7f, 0x73, 0x61, 0x61, 0x73, 0x7f, 0x7f,
    0x00, 0x1e, 0x33, 0x21, 0x21, 0x33, 0x1e, 0x00,
    0x7f, 0x61, 0x4c, 0x5e, 0x5e, 0x4c, 0x61, 0x7f,
    0x07, 0x03, 0x07, 0x3e, 0x66, 0x66, 0x66, 0x3c,
    0x1e, 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x3f, 0x0c,
    0x1f, 0x19, 0x1f, 0x18, 0x18, 0x38, 0x78, 0x70,
    0x3f, 0x31, 0x3f, 0x31, 0x31, 0x33, 0x73, 0x60,
    0x4c, 0x2d, 0x1e, 0x73, 0x73, 0x1e, 0x2d, 0x4c,
    0x40, 0x70, 0x7c, 0x7f, 0x7c, 0x70, 0x40, 0x00,
    0x01, 0x07, 0x1f, 0x7f, 0x1f, 0x07, 0x01, 0x00,
    0x0c, 0x1e, 0x3f, 0x0c, 0x0c, 0x3f, 0x1e, 0x0c,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x00, 0x33, 0x00,
    0x3f, 0x6d, 0x6d, 0x3d, 0x0d, 0x0d, 0x0d, 0x00,
    0x1f, 0x31, 0x1c, 0x36, 0x36, 0x1c, 0x66, 0x3c,
    0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x3f, 0x00,
    0x0c, 0x1e, 0x3f, 0x0c, 0x3f, 0x1e, 0x0c, 0x7f,
    0x0c, 0x1e, 0x3f, 0x0c, 0x0c, 0x0c, 0x0c, 0x00,
    0x0c, 0x0c, 0x0c, 0x0c, 0x3f, 0x1e, 0x0c, 0x00,
    0x00, 0x0c, 0x06, 0x7f, 0x06, 0x0c, 0x00, 0x00,
    0x00, 0x18, 0x30, 0x7f, 0x30, 0x18, 0x00, 0x00,
    0x00, 0x00, 0x60, 0x60, 0x60, 0x7f, 0x00, 0x00,
    0x00, 0x12, 0x33, 0x7f, 0x33, 0x12, 0x00, 0x00,
    0x00, 0x0c, 0x1e, 0x3f, 0x7f, 0x7f, 0x00, 0x00,
    0x00, 0x7f, 0x7f, 0x3f, 0x1e, 0x0c, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x18, 0x00,
    0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00,
    0x18, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x18, 0x00,
    0x00, 0x63, 0x66, 0x0c, 0x18, 0x33, 0x63, 0x00,
    0x1c, 0x36, 0x1c, 0x3b, 0x6e, 0x66, 0x3b, 0x00,
    0x30, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0c, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00,
    0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00,
    0x00, 0x33, 0x1e, 0x7f, 0x1e, 0x33, 0x00, 0x00,
    0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30,
    0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x40, 0x00,
    0x3e, 0x63, 0x67, 0x6f, 0x7b, 0x73, 0x3e, 0x00,
    0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00,
    0x3c, 0x66, 0x06, 0x1c, 0x30, 0x66, 0x7e, 0x00,
    0x3c, 0x66, 0x06, 0x1c, 0x06, 0x66, 0x3c, 0x00,
    0x0e, 0x1e, 0x36, 0x66, 0x7f, 0x06, 0x0f, 0x00,
    0x7e, 0x60, 0x7c, 0x06, 0x06, 0x66, 0x3c, 0x00,
    0x1c, 0x30, 0x60, 0x7c, 0x66, 0x66, 0x3c, 0x00,
    0x7e, 0x66, 0x06, 0x0c, 0x18, 0x18, 0x18, 0x00,
    0x3c, 0x66, 0x66, 0x3c, 0x66, 0x66, 0x3c, 0x00,
    0x3c, 0x66, 0x66, 0x3e, 0x06, 0x0c, 0x38, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30,
    0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00,
    0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00,
    0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00,
    0x3c, 0x66, 0x06, 0x0c, 0x18, 0x00, 0x18, 0x00,
    0x3e, 0x63, 0x6f, 0x6f, 0x6f, 0x60, 0x3c, 0x00,
    0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00,
    0x7e, 0x33, 0x33, 0x3e, 0x33, 0x33, 0x7e, 0x00,
    0x1e, 0x33, 0x60, 0x60, 0x60, 0x33, 0x1e, 0x00,
    0x7c, 0x36, 0x33, 0x33, 0x33, 0x36, 0x7c, 0x00,
    0x7f, 0x31, 0x34, 0x3c, 0x34, 0x31, 0x7f, 0x00,
    0x7f, 0x31, 0x34, 0x3c, 0x34, 0x30, 0x78, 0x00,
    0x1e, 0x33, 0x60, 0x60, 0x67, 0x33, 0x1f, 0x00,
    0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00,
    0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
    0x0f, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00,
    0x73, 0x33, 0x36, 0x3c, 0x36, 0x33, 0x73, 0x00,
    0x78, 0x30, 0x30, 0x30, 0x31, 0x33, 0x7f, 0x00,
    0x63, 0x77, 0x7f, 0x7f, 0x6b, 0x63, 0x63, 0x00,
    0x63, 0x73, 0x7b, 0x6f, 0x67, 0x63, 0x63, 0x00,
    0x1c, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1c, 0x00,
    0x7e, 0x33, 0x33, 0x3e, 0x30, 0x30, 0x78, 0x00,
    0x3c, 0x66, 0x66, 0x66, 0x6e, 0x3c, 0x0e, 0x00,
    0x7e, 0x33, 0x33, 0x3e, 0x36, 0x33, 0x73, 0x00,
    0x3c, 0x66, 0x70, 0x38, 0x0e, 0x66, 0x3c, 0x00,
    0x7e, 0x5a, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x00,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00,
    0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00,
    0x63, 0x63, 0x36, 0x1c, 0x1c, 0x36, 0x63, 0x00,
    0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x3c, 0x00,
    0x7f, 0x63, 0x46, 0x0c, 0x19, 0x33, 0x7f, 0x00,
    0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00,
    0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x01, 0x00,
    0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00,
    0x08, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f,
    0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3b, 0x00,
    0x70, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6e, 0x00,
    0x00, 0x00, 0x3c, 0x66, 0x60, 0x66, 0x3c, 0x00,
    0x0e, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x3b, 0x00,
    0x00, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00,
    0x1c, 0x36, 0x30, 0x78, 0x30, 0x30, 0x78, 0x00,
    0x00, 0x00, 0x3b, 0x66, 0x66, 0x3e, 0x06, 0x7c,
    0x70, 0x30, 0x36, 0x3b, 0x33, 0x33, 0x73, 0x00,
    0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
    0x06, 0x00, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c,
    0x70, 0x30, 0x33, 0x36, 0x3c, 0x36, 0x73, 0x00,
    0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
    0x00, 0x00, 0x66, 0x7f, 0x7f, 0x6b, 0x63, 0x00,
    0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x00,
    0x00, 0x00, 0x6e, 0x33, 0x33, 0x3e, 0x30, 0x78,
    0x00, 0x00, 0x3b, 0x66, 0x66, 0x3e, 0x06, 0x0f,
    0x00, 0x00, 0x6e, 0x3b, 0x33, 0x30, 0x78, 0x00,
    0x00, 0x00, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x00,
    0x08, 0x18, 0x3e, 0x18, 0x18, 0x1a, 0x0c, 0x00,
    0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3b, 0x00,
    0x00, 0x00, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00,
    0x00, 0x00, 0x63, 0x6b, 0x7f, 0x7f, 0x36, 0x00,
    0x00, 0x00, 0x63, 0x36, 0x1c, 0x36, 0x63, 0x00,
    0x00, 0x00, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x7c,
    0x00, 0x00, 0x7e, 0x4c, 0x18, 0x32, 0x7e, 0x00,
    0x0e, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0e, 0x00,
    0x0c, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x0c, 0x00,
    0x70, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x70, 0x00,
    0x3b, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x1c, 0x36, 0x63, 0x63, 0x7f, 0x00,
    0x3c, 0x66, 0x60, 0x66, 0x3c, 0x0c, 0x06, 0x3c,
    0x00, 0x66, 0x00, 0x66, 0x66, 0x66, 0x3f, 0x00,
    0x0e, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00,
    0x3f, 0x61, 0x1e, 0x03, 0x1f, 0x33, 0x1f, 0x00,
    0x66, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3f, 0x00,
    0x70, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3f, 0x00,
    0x18, 0x18, 0x3c, 0x06, 0x3e, 0x66, 0x3f, 0x00,
    0x00, 0x00, 0x3c, 0x60, 0x60, 0x3c, 0x06, 0x1c,
    0x3f, 0x61, 0x1e, 0x33, 0x3f, 0x30, 0x1e, 0x00,
    0x66, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00,
    0x70, 0x00, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00,
    0x66, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
    0x3e, 0x63, 0x1c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00,
    0x70, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
    0x63, 0x1c, 0x36, 0x63, 0x7f, 0x63, 0x63, 0x00,
    0x18, 0x18, 0x00, 0x3c, 0x66, 0x7e, 0x66, 0x00,
    0x0e, 0x00, 0x7e, 0x30, 0x3c, 0x30, 0x7e, 0x00,
    0x00, 0x00, 0x3f, 0x06, 0x3f, 0x66, 0x3f, 0x00,
    0x1f, 0x36, 0x66, 0x7f, 0x66, 0x66, 0x67, 0x00,
    0x3c, 0x66, 0x00, 0x3c, 0x66, 0x66, 0x3c, 0x00,
    0x00, 0x66, 0x00, 0x3c, 0x66, 0x66, 0x3c, 0x00,
    0x00, 0x70, 0x00, 0x3c, 0x66, 0x66, 0x3c, 0x00,
    0x3c, 0x66, 0x00, 0x66, 0x66, 0x66, 0x3f, 0x00,
    0x00, 0x70, 0x00, 0x66, 0x66, 0x66, 0x3f, 0x00,
    0x00, 0x66, 0x00, 0x66, 0x66, 0x3e, 0x06, 0x7c,
    0x61, 0x0c, 0x1e, 0x33, 0x33, 0x1e, 0x0c, 0x00,
    0x66, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00,
    0x0c, 0x0c, 0x3f, 0x60, 0x60, 0x3f, 0x0c, 0x0c,
    0x1c, 0x36, 0x32, 0x78, 0x30, 0x73, 0x7e, 0x00,
    0x66, 0x66, 0x3c, 0x7e, 0x18, 0x7e, 0x18, 0x18,
    0x7c, 0x66, 0x66, 0x7d, 0x63, 0x67, 0x63, 0x63,
    0x07, 0x0d, 0x0c, 0x1e, 0x0c, 0x0c, 0x6c, 0x38,
    0x0e, 0x00, 0x3c, 0x06, 0x3e, 0x66, 0x3f, 0x00,
    0x1c, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00,
    0x00, 0x0e, 0x00, 0x3c, 0x66, 0x66, 0x3c, 0x00,
    0x00, 0x0e, 0x00, 0x66, 0x66, 0x66, 0x3f, 0x00,
    0x00, 0x7c, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x00,
    0x7e, 0x00, 0x66, 0x76, 0x7e, 0x6e, 0x66, 0x00,
    0x1e, 0x36, 0x36, 0x1f, 0x00, 0x3f, 0x00, 0x00,
    0x1c, 0x36, 0x36, 0x1c, 0x00, 0x3e, 0x00, 0x00,
    0x18, 0x00, 0x18, 0x30, 0x60, 0x66, 0x3c, 0x00,
    0x00, 0x00, 0x00, 0x7e, 0x60, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x7e, 0x06, 0x06, 0x00, 0x00,
    0x61, 0x63, 0x66, 0x6f, 0x19, 0x33, 0x66, 0x07,
    0x61, 0x63, 0x66, 0x6d, 0x1b, 0x37, 0x67, 0x01,
    0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x00,
    0x00, 0x19, 0x33, 0x66, 0x33, 0x19, 0x00, 0x00,
    0x00, 0x66, 0x33, 0x19, 0x33, 0x66, 0x00, 0x00,
    0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44,
    0x2a, 0x55, 0x2a, 0x55, 0x2a, 0x55, 0x2a, 0x55,
    0x6d, 0x3b, 0x6d, 0x77, 0x6d, 0x3b, 0x6d, 0x77,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x7c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x7c, 0x0c, 0x7c, 0x0c, 0x0c, 0x0c,
    0x1b, 0x1b, 0x1b, 0x1b, 0x7b, 0x1b, 0x1b, 0x1b,
    0x00, 0x00, 0x00, 0x00, 0x7f, 0x1b, 0x1b, 0x1b,
    0x00, 0x00, 0x7c, 0x0c, 0x7c, 0x0c, 0x0c, 0x0c,
    0x1b, 0x1b, 0x7b, 0x03, 0x7b, 0x1b, 0x1b, 0x1b,
    0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
    0x00, 0x00, 0x7f, 0x03, 0x7b, 0x1b, 0x1b, 0x1b,
    0x1b, 0x1b, 0x7b, 0x03, 0x7f, 0x00, 0x00, 0x00,
    0x1b, 0x1b, 0x1b, 0x1b, 0x7f, 0x00, 0x00, 0x00,
    0x0c, 0x0c, 0x7c, 0x0c, 0x7c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x7c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0f, 0x00, 0x00, 0x00,
    0x0c, 0x0c, 0x0c, 0x0c, 0x7f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x7f, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0f, 0x0c, 0x0c, 0x0c,
    0x00, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00,
    0x0c, 0x0c, 0x0c, 0x0c, 0x7f, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0f, 0x0c, 0x0f, 0x0c, 0x0c, 0x0c,
    0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x1b,
    0x1b, 0x1b, 0x1b, 0x18, 0x1f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1f, 0x18, 0x1b, 0x1b, 0x1b, 0x1b,
    0x1b, 0x1b, 0x7b, 0x00, 0x7f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7f, 0x00, 0x7b, 0x1b, 0x1b, 0x1b,
    0x1b, 0x1b, 0x1b, 0x18, 0x1b, 0x1b, 0x1b, 0x1b,
    0x00, 0x00, 0x7f, 0x00, 0x7f, 0x00, 0x00, 0x00,
    0x1b, 0x1b, 0x7b, 0x00, 0x7b, 0x1b, 0x1b, 0x1b,
    0x0c, 0x0c, 0x7f, 0x00, 0x7f, 0x00, 0x00, 0x00,
    0x1b, 0x1b, 0x1b, 0x1b, 0x7f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x7f, 0x00, 0x7f, 0x0c, 0x0c, 0x0c,
    0x00, 0x00, 0x00, 0x00, 0x7f, 0x1b, 0x1b, 0x1b,
    0x1b, 0x1b, 0x1b, 0x1b, 0x1f, 0x00, 0x00, 0x00,
    0x0c, 0x0c, 0x0f, 0x0c, 0x0f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0f, 0x0c, 0x0f, 0x0c, 0x0c, 0x0c,
    0x00, 0x00, 0x00, 0x00, 0x1f, 0x1b, 0x1b, 0x1b,
    0x1b, 0x1b, 0x1b, 0x1b, 0x7f, 0x1b, 0x1b, 0x1b,
    0x0c, 0x0c, 0x7f, 0x0c, 0x7f, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x7c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0f, 0x0c, 0x0c, 0x0c,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x7f, 0x7f,
    0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3b, 0x6e, 0x64, 0x6e, 0x3b, 0x00,
    0x00, 0x3c, 0x66, 0x7c, 0x66, 0x7c, 0x60, 0x60,
    0x00, 0x7e, 0x66, 0x60, 0x60, 0x60, 0x60, 0x00,
    0x00, 0x7f, 0x36, 0x36, 0x36, 0x36, 0x36, 0x00,
    0x7e, 0x66, 0x30, 0x18, 0x30, 0x66, 0x7e, 0x00,
    0x00, 0x00, 0x3f, 0x6c, 0x6c, 0x6c, 0x38, 0x00,
    0x00, 0x33, 0x33, 0x33, 0x33, 0x3e, 0x30, 0x60,
    0x00, 0x3b, 0x6e, 0x0c, 0x0c, 0x0c, 0x0c, 0x00,
    0x7e, 0x18, 0x3c, 0x66, 0x66, 0x3c, 0x18, 0x7e,
    0x1c, 0x36, 0x63, 0x7f, 0x63, 0x36, 0x1c, 0x00,
    0x1c, 0x36, 0x63, 0x63, 0x36, 0x36, 0x77, 0x00,
    0x0e, 0x18, 0x0c, 0x3e, 0x66, 0x66, 0x3c, 0x00,
    0x00, 0x00, 0x3f, 0x6d, 0x6d, 0x3f, 0x00, 0x00,
    0x03, 0x06, 0x3f, 0x6d, 0x6d, 0x3f, 0x30, 0x60,
    0x1c, 0x30, 0x60, 0x7c, 0x60, 0x30, 0x1c, 0x00,
    0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00,
    0x00, 0x7e, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00,
    0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x7e, 0x00,
    0x30, 0x18, 0x0c, 0x18, 0x30, 0x00, 0x7e, 0x00,
    0x0c, 0x18, 0x30, 0x18, 0x0c, 0x00, 0x7e, 0x00,
    0x07, 0x0d, 0x0d, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x6c, 0x6c, 0x38,
    0x18, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x3b, 0x6e, 0x00, 0x3b, 0x6e, 0x00, 0x00,
    0x1c, 0x36, 0x36, 0x1c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x07, 0x06, 0x06, 0x06, 0x76, 0x36, 0x1e, 0x0e,
    0x3c, 0x36, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00,
    0x38, 0x0c, 0x18, 0x30, 0x3c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1e, 0x1e, 0x1e, 0x1e, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#endif /* NEWFONT */

#endif /* VBE */

#if (VGAGFX)

void
vgainitgfx(void)
{
    ;
}

void
vgaputpix(int32_t pix, int32_t x, int32_t y)
{
    ;
}

#endif /* VGAGFX */

#if (VGAGFX) || (VBE)

/* copy font from VGA RAM */
void
vgagetfont(void *bufadr)
{
    uint16_t  port = 0x03ce;
    uint32_t *src;
    uint32_t *dest;
    int       i;

    /* set buffer address */
    __asm__ __volatile__ ("movl %0, %%edi\n" : : "rm" (0xa0000));
    outw(0x0005, port);         // clear even/odd mode
    outw(0x0406, port);         // map VGA memory to 0xa0000
    port = 0x03c4;
    outw(0x0402, port);         // set bitplane 2
    outw(0x0604, port);         // clear even/odd mode
    src = (uint32_t *)0xa0000;
    dest = bufadr;
    for (i = 0 ; i < VGANGLYPH ; i++) {
        dest[0] = src[0];
        dest[1] = src[1];
        dest[2] = src[2];
        dest[3] = src[3];
        src += 8;               // skip 16 bytes
        dest += 4;
    }
    vgareset();

    return;
}

#endif /* VGAGFX || VBE */

#if (!VBE)

void
vgainitcons(int w, int h)
{
    struct cons *cons = constab;
    uint8_t     *ptr = (uint8_t *)VGABUFADR;
    long         l;

    for (l = 0 ; l < NCONS ; l++) {
        kbzero(ptr, PAGESIZE);
        cons->buf = (uint16_t *)ptr;
        cons->col = 0;
        cons->row = 0;
        cons->ncol = w;
        cons->nrow = h;
        cons->chatr = vgasetfg(0, VGAWHITE);
        cons->nbufrow = 0;
        /* TODO: allocate scrollback buffer */
        //        cons->data = NULL;
        cons++;
    }
    conscur = 0;
    vgamoveto(0, 0);

    return;
}

/* output string on a given console */
void
vgaputs2(struct cons *cons, char *str)
{
    uint16_t *ptr;
    int       x;
    int       y;
    int       w;
    int       h;
    uint8_t   ch;
    uint8_t   atr;

    x = cons->col;
    y = cons->row;
    w = cons->ncol;
    h = cons->nrow;
    atr = cons->chatr;
    while (*str) {
        ptr = (uint16_t *)cons->buf + y * w + x;
        ch = *str;
        if (ch == '\n') {
            if (++y == h) {
                y = 0;
            }
            x = 0;
        } else {
            if (++x == w) {
                x = 0;
                if (++y == h) {
                    y = 0;
                }
            }
            vgaputch3(ptr, ch, atr);
        }
        str++;
        cons->col = x;
        cons->row = y;
    }

    return;
}

void
vgaputchar(int ch)
{
    struct cons *cons;
    uint16_t    *ptr;

    cons = &constab[conscur];
    ptr = (uint16_t *)cons->buf + cons->ncol * cons->col + cons->row;
    *ptr = _vgamkch(ch, cons->chatr);

    return;
}

void
vgasyncscr(void)
{
    ;
}

#endif /* !VBE */

