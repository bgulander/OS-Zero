#ifndef __SIGNAL_H__
#define __SIGNAL_H__

/* TODO: sysconf(): _SC_SIGRT_MIN, _SC_SIGRT_MAX */

#include <features.h>
//#include <sys/types.h>
#include <zero/param.h>
//#include <unistd.h>
#include <zero/param.h>
#if (_ZERO_SOURCE)
//#include <kern/signal.h>
#endif
#if (USEPOSIX199506) || (_XOPEN_SOURCE >= 500) && 0
#include <pthread.h>
#endif
#include <bits/signal.h>

#if (_POSIX_SOURCE) && (USEPOSIX199309)
#include <time.h>
#endif

#if !defined(__KERNEL__)

/* set handler for signal sig; returns old handler */
extern void (*signal(int sig, void (*func)(int)))(int);
extern __sighandler_t __sysv_signal(int sig, __sighandler_t func);
#if (_GNU_SOURCE)
extern __sighandler_t sysv_signal(int sig, __sighandler_t func);
#endif
#if (_XOPEN_SOURCE)
extern __sighandler_t bsd_signal(int sig, __sighandler_t func);
#endif

#if (_BSD_SOURCE) || (USEXOPENEXT)
extern void psiginfo(const siginfo_t *info, const char *msg);
void        psignal(int sig, const char *message);
#endif

#if (_POSIX_SOURCE)
/*
 * send signal sig to process or group described by pid
 * - if pid is zero, send sig to all processes in the current one's process group
 * - if pid < -1, send sig to all processes in the process group -pid
 */
extern int kill(pid_t pid, int sig);

#if ((USEPOSIX199506) || (_XOPEN_SOURCE >= 500)) && 0
int pthread_kill(pthread_t thr, int sig);
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);
#endif

#endif /* _POSIX_SOURCE */

#if (_BSD_SOURCE) || (USEXOPENEXT)
/*
 * send signal sig to all processes in the group pgrp
 * - if pid is zero, send sig to all processes in the current one's group
 */
extern int killpg(pid_t pgrp, int sig);
#endif /* _BSD_SOURCE || USEXOPENEXT */

extern int raise(int sig);
#if (USESVID)
extern __sighandler_t ssignal(int sig, __sighandler_t func);
extern int            gsignal(int sig);
#endif /* USESVID */
//extern void           psignal(int sig);
#if (FAVORBSD)
/* set mask to blocked signals, wait for signal, restore the mask */
extern int            sigpause(int mask);
#elif (_XOPEN_SOURCE)
/* remove sig from signal mask and suspend the process */
#if defined(__GNUC__)
extern int            sigpause(int sig) __asm__ ("__xpg_sigpause\n");
#else
//#define               sigpause(sig) __sigpause(sig, 1);
#endif
#endif

#if (_BSD_SOURCE)

/* none of these functions should be used any longer */
#define    sigmask(sig)   (1L << ((sig) - 1))
/* block signals in mask, return old mask */
extern int sigblock(int mask); 
/* set mask of blocked signals, return old mask */
extern int sigsetmask(int mask);
/* return current signal mask */
extern int siggetmask(void);

#endif /* !defined(__KERNEL__) */

#if !defined(__KERNEL__)

#if (USEBSD) && (!USEPOSIX)
int sigvec(int sig, const struct sigvec *vec, struct sigvec *oldvec);
#endif

// extern int sigreturn(struct sigcontext *scp);

#endif /* !defined(__KERNEL__) */

#endif /* _BSD_SOURCE */

#if !defined(__KERNEL__)

#if (USEPOSIX)

/* get and/or change set of blocked signals */
extern int sigprocmask(int how, const sigset_t *restrict set,
                       sigset_t *restrict oldset);
/* change blocked signals to set, wait for a signal, restore the set */
extern int sigsuspend(const sigset_t *set);
/* set or examine signal behavior */
extern int sigaction(int sig, const struct sigaction *restrict act,
                     struct sigaction *restrict oldact);
/* fetch pending blocked signals */
extern int sigpending(sigset_t *set);
/* wait for a signal in set */
extern int sigwait(const sigset_t *set, int *restrict sig);
#if (USEPOSIX199309)
extern int sigwaitinfo(const sigset_t *restrict set,
                       siginfo_t *restrict info);
extern int sigtimedwait(const sigset_t *restrict set,
                        siginfo_t *restrict info,
                        const struct timespec *restrict timeout);
extern int sigqueue(pid_t pid, int sig, const union sigval val);
#endif /* USEPOSIX199309 */

#endif /* USEPOSIX */

#if (_BSD_SOURCE)
extern const char *const _sys_siglist[_NSIG];
extern const char *const sys_siglist[_NSIG];
#endif

#endif /* !defined(__KERNEL__) */

#if !defined(__KERNEL__)

/*
 * if intr is nonzero, make signal sig interrupt system calls (causing them
 * to fail with EINTR); if intr is zero, make system calls be restarted
 * after signal sig
 */
extern int siginterrupt(int sig, int intr);

#if (_BSD_SOURCE) || (USEXOPENEXT)
extern int sigstack(struct sigstack *stk, struct sigstack *oldstk);
extern int sigaltstack(const stack_t *restrict stk,
                       stack_t *restrict oldstk);
#else
extern int sigaltstack(const struct sigaltstack *stk,
                       struct sigaltstack *oldstk);
#endif

#if (_XOPEN_SOURCE)
#include <ucontext.h>
#endif

#if (USEXOPENEXT)
extern int            sighold(int sig);
extern int            sigrelse(int sig);
extern int            sigignore(int sig);
extern __sighandler_t sigset(int sig, __sighandler_t func);
#endif /* USEXOPENEXT */

#endif /* !defined(__KERNEL__) */

#endif /* __SIGNAL_H__ */

