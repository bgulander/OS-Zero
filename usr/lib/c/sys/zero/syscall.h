#ifndef __SYS_ZERO_SYSCALL_H__
#define __SYS_ZERO_SYSCALL_H__

#if defined(__KERNEL__)

#include <zero/cdefs.h>
#include <zero/param.h>
#include <kern/syscall.h>

ASMLINK
sysreg_t _syscall(sysreg_t num, sysreg_t arg1, sysreg_t arg2, sysreg_t arg3);

#endif /* defined(__ZEROKERNEL__) */

#endif /* __SYS_ZERO_SYSCALL_H__ */

