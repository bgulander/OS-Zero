#ifndef __KERN_BITS_SCHED_H__
#define __KERN_BITS_SCHED_H__

#include <kern/conf.h>
#include <zero/cdefs.h>
#include <zero/param.h>
#include <zero/trix.h>
#include <zero/fastidiv.h>
#include <zero/mtx.h>
//#include <kern/sched.h>
#include <kern/proc/task.h>

extern void schedsetready(struct task *task, long cpu);
extern void schedsetwait(struct task *task);
extern void schedsetsleep(struct task *task);

#define schedlkcpuntick(cpu)     (fmtxlk(&cpu->lk), (cpu)->ntick)
#define schedlkcpu(cpu)          (fmtxlk(&cpu->lk))
#define schedunlkcpu(cpu)        (fmtxunlk(&cpu->lk))
#define schedlktaskruntime(task) (fmtxlk(&task->lk), (task)->runtime)
#define schedlktask(task)        (fmtxlk(&task->lk))
#define schedunlktask(task)      (fmtxunlk(&task->lk))

#if defined(ZEROSCHED)

#if !defined(HZ)
#define HZ                  250
#endif

/* macros */
#define schedsetcpu(cpu)                                                \
    do {                                                                \
        k_curcpu = (cpu);                                               \
    } while (0)
#define schedsettask(task)                                              \
    do {                                                                \
        volatile struct m_cpu *_m_cpu = k_curcpu;                       \
                                                                        \
        _m_cpu->task = task;                                            \
        _m_cpu->proc = task->proc;                                      \
    } while (0)

#define schedcalctime(task)  ((task)->ntick >> SCHEDTICKSHIFT)
#define schedcalcticks(task) (max((task)->lastrun - (task)->firstrun, kgethz()))
#define schedcalcnice(val)   (schednicetab[(val)])
#if 0
#define schedsetprio(task, pri)                                         \
    ((task)->prio = schedprioqueueid(pri))
#define schedsetprioincr(task, pri, incr)                               \
    ((task)->prio = schedprioqueueid(pri) + (incr))
#endif
#define schedcalcuserprio(task)                                         \
    (fastu32div24(schedcalctime(task),                                  \
                  (roundup(schedcalcticks(task), SCHEDPRIORANGE)        \
                   / SCHEDPRIORANGE),                                   \
                  fastu32div24tab))
/* timeshare-tasks have interactivity scores */
#define schedistimeshare(sched)                                         \
    ((sched) >= SCHEDRESPONSIVE && (sched) <= SCHEDBATCH)
#define schedisinteract(score)                                          \
    ((score) < SCHEDSCOREINTLIM)
#define schedcalctrapqueue(pri)                                         \
    ((pri) >> SCHEDNQUEUESHIFT)
#define schedcalcqueue(tupe, pri)                                       \
    (((pri) - (type) * SCHEDNCLASSPRIO) >> 1)

/* interrupt priorities */
#define schedintrsoftprio(id)                                           \
    (SCHEDTRAPSOFTPRIO + (id) * SCHEDNQUEUEPRIO)
#define SCHEDTRAPRTPRIO     SCHEDTRAPPRIOMIN
#define SCHEDTRAPAVPRIO     (SCHEDTRAPPRIOMIN + SCHEDNQUEUEPRIO)
#define SCHEDTRAPHIDPRIO    (SCHEDTRAPPRIOMIN + 2 * SCHEDNQUEUEPRIO)
#define SCHEDTRAPNETPRIO    (SCHEDTRAPPRIOMIN + 3 * SCHEDNQUEUEPRIO)
#define SCHEDTRAPDISKPRIO   (SCHEDTRAPPRIOMIN + 4 * SCHEDNQUEUEPRIO)
#define SCHEDTRAPMISCPRIO   (SCHEDTRAPPRIOMIN + 5 * SCHEDNQUEUEPRIO)
#define SCHEDTRAPSOFTPRIO   (SCHEDTRAPPRIOMIN + 6 * SCHEDNQUEUEPRIO)

#define SCHEDNLVL0DL      (1U << 16)
#define SCHEDNLVL1DL      (1U << 8)
#define SCHEDNLVL2DL      (1U << 8)
#define SCHEDNDLKEY       3

/* 32-bit time_t values */
#define taskdlkey0(dl)    (((dl) >> 16) & 0xffff)
#define taskdlkey1(dl)    (((dl) >> 8) & 0xff)
#define taskdlkey2(dl)    ((dl) & 0xff)

#define SCHEDDEADLINEMAPNWORD                                           \
    (SCHEDNLVL0DL / (CHAR_BIT * sizeof(long)))
#define SCHEDREADYMAPNWORD                                              \
    max(SCHEDNQUEUE / (CHAR_BIT * sizeof(long)), CLSIZE / sizeof(long))
#define SCHEDIDLEMAPNWORD                                               \
    max(SCHEDNIDLE / (CHAR_BIT * sizeof(long)), CLSIZE / sizeof(long))
#define SCHEDLOADMAPNWORD                                               \
    max(SCHEDNTOTALQUEUE / (CHAR_BIT * sizeof(long)), CLSIZE / sizeof(long))
#define SCHEDIDLECOREMAPNWORD                                           \
    max(NCPU / (CHAR_BIT * sizeof(long)), CLSIZE / sizeof(long))

/* data structures */

#define __STRUCT_SCHEDQUEUESET_SIZE                                     \
    (sizeof(m_atomic_t) + 7 * sizeof(void *))
#define __STRUCT_SCHEDQUEUESET_PAD                                      \
    (roundup(__STRUCT_SCHEDQUEUESET_SIZE, CLSIZE) - __STRUCT_SCHEDQUEUESET_SIZE)
struct schedqueueset {
    m_atomic_t    lk;
    long         *curmap;
    long         *nextmap;
    long         *idlemap;
    long         *loadmap;
    struct task **cur;
    struct task **next;
    struct task **idle;
    uint8_t       _pad[__STRUCT_SCHEDQUEUESET_PAD];
};

extern struct m_cpu          m_cputab[NCPU];
extern long                  schedidlecoremap[SCHEDIDLECOREMAPNWORD];
extern struct task          *schedreadytab0[SCHEDNQUEUE];
extern struct task          *schedreadytab1[SCHEDNQUEUE];
extern struct schedqueueset  schedreadyset;
extern struct divu32         fastu32div24tab[rounduppow2(SCHEDHISTORYSIZE,
                                                         PAGESIZE)];
extern long                  schednicetab[SCHEDNICERANGE];
extern long                  schedslicetab[SCHEDNICERANGE];
extern long                 *schedniceptr;
extern long                 *schedsliceptr;

/* based on sched_pctcpu_update from ULE */
static __inline__ void
schedadjcpupct(struct task *task, long run)
{
    volatile struct m_cpu *m_cpu = k_curcpu;
    long                   tick = m_cpu->data.ntick;
    unsigned               last = task->lastrun;
    long                   diff = tick - last;
    long                   delta;
    long                   ntick;
    long                   val;
    long                   div;

    if (diff >= SCHEDHISTORYNTICK) {
        task->ntick = 0;
        task->firstrun = tick - SCHEDHISTORYNTICK;
    } else {
        unsigned long first = task->firstrun;

        delta = tick - first;
        if (delta >= SCHEDHISTORYSIZE) {
            ntick = task->ntick;
            div = last - first;
            val = tick - SCHEDHISTORYNTICK;
            last -= val;
            ntick = fastu32div24(ntick, div, fastu32div24tab);
            ntick *= last;
            task->firstrun = val;
        }
    }
    if (run) {
        ntick = diff >> SCHEDTICKSHIFT;
        task->ntick = ntick;
    }
    task->lastrun = tick;

    return;
}

static __inline__ void
schedswapqueues(long cpu)
{
    struct schedqueueset *set = &schedreadyset;

    fmtxlk(&set->lk);
    set->next = set->cur;
    set->cur = set->next;
    set->nextmap = set->curmap;
    set->curmap = set->nextmap;
    fmtxunlk(&set->lk);

    return;
}

static __inline__ struct m_cpu *
schedfindidlecore(long id, long *retcore)
{
    struct m_cpu *m_cpu = &m_cputab[id];
    long          nunit = NCPU;
    long          ndx = 0;
    long          val = 0;

    fmtxlk(&m_cpu->data.mtx);
    for (ndx = 0 ; ndx < nunit ; ndx++) {
        if (ndx != id && bitset(schedidlecoremap, ndx)) {
            fmtxunlk(&m_cpu->data.mtx);
            *retcore = ndx;

            return m_cpu;
        }
    }
    fmtxunlk(&m_cpu->data.mtx);

    return NULL;
}

static __inline__ void
schedsetnice(struct task *task, long val)
{
    struct proc *proc = task->proc;
    long         nice;
    
    val = max(-20, val);
    val = min(19, val);
    nice = schedniceptr[val];
    proc->nice = nice;
    proc->niceval = val;

    return;
}

/* CHECKED against sched_interact_score() in ULE :) */
static __inline__ long
schedcalcscore(struct task *task)
{
    long run = task->runtime;
    long slp = task->slptime;
    long div;
    long res;
    long tmp;

    if (SCHEDSCOREINTLIM <= SCHEDSCOREHALF
        && run >= slp) {
        res = SCHEDSCOREHALF;
        task->score = res;
        
        return res;
    }
    if (run > slp) {
#if (SCHEDSCOREHALF == 64)
        run >>= 6;
#else
        run = fastu32div24(run, SCHEDSCOREHALF, fastu32div24tab);
#endif
        res = SCHEDSCOREMAX;
        div = max(1, run);
        tmp = fastu32div24(slp, div, fastu32div24tab);
        res -= tmp;
        task->score = res;
        
        return res;
    }
    if (slp > run) {
#if (SCHEDSCOREHALF == 64)
        slp >>= 6;
#else
        slp = fastu32div24(slp, SCHEDHALFSCORE, s);
#endif
        div = max(1, slp);
        res = fastu32div24(run, div, fastu32div24tab);
        task->score = res;

        return res;
    }
    /* run == slp */
    if (run) {
        res = SCHEDSCOREHALF;
        task->score = res;

        return res;
    }
    task->score = 0;

    /* run == 0 && slp == 0 */
    return 0;
}

/* based on sched_priority() from ULE */
static __inline__ void
schedcalcprio(struct task *task)
{
    long score = schedcalcscore(task);
    long nice = schedcalcnice(task->proc->nice);
    long runprio = task->runprio;
    long prio;
    long ntick;
    long delta;
    long tmp;

    score += nice;
    score = max(0, score);
    if (score < SCHEDSCOREINTLIM) {
        prio = SCHEDINTPRIOMIN;
        delta = SCHEDINTRANGE;
#if (SCHEDSCOREINTLIM == 32)
        delta >>= 5;
#else
        delta = fastu32div24(delta, SCHEDSCOREINTLIM, fastu32div24tab);
#endif
        delta *= score;
        prio += delta;
    } else {
        ntick = task->ntick;
        prio = SCHEDUSERPRIOMIN;
        if (ntick) {
            tmp = schedcalcuserprio(task);
            prio += min(tmp, SCHEDUSERRANGE - 1);
        }
        prio += nice;
        prio = min(runprio, prio);
    }
    task->prio = prio;

    return;
}

/* based on sched_interact_update() in ULE :) */
/*
 * enforce maximum limit of scheduling history kept; call after either runtime
 * or slptime is adjusted
 */
static __inline__ void
schedadjintparm(struct task *task)
{
    long run = task->runtime;
    long slp = task->slptime;
    long sum = run + slp;

    if (sum < SCHEDRECTIMEMAX) {

        return;
    }
    if (sum > 2 * SCHEDRECTIMEMAX) {
        if (run > slp) {
            task->runtime = SCHEDRECTIMEMAX;
            task->slptime = 1;
        } else {
            task->runtime = 1;
            task->slptime = SCHEDRECTIMEMAX;
        }

        return;
    }
    if (sum > (SCHEDRECTIMEMAX >> 3) * 9) {
        /* exceeded by more than 1/8th, divide by 2 */
        run >>= 1;
        slp >>= 1;
    } else {
        /* multiply by 3 / 4; this gives us less than 0.85 * SCHEDRECTIMEMAX */
        run >>= 2;
        slp >>= 2;
        run *= 3;
        slp *= 3;
    }
    task->runtime = run;
    task->slptime = slp;

    return;
}

/* based on sched_interact_fork() in ULE */
static __inline__ void
schedadjforkintparm(struct task *task)
{
    long run = task->runtime;
    long slp = task->slptime;
#if !((HZ == 250) && (SCHEDHISTORYNSEC == 8))
    long ratio;
#endif
    long run2;
    long slp2;
    long sum = run + slp;

    if (sum > SCHEDRECTIMEFORKMAX) {
#if (HZ == 250) && (SCHEDHISTORYNSEC == 8)
        /* multiply run and slp by 3 / 8 */
        run2 = run;
        slp2 = slp;
        run >>= 3;
        slp >>= 3;
        run2 >>= 2;
        slp2 >>= 2;
        run += run2;
        slp += slp2;
#else
        ratio = fastu32div24(sum, SCHEDRECTIMEFORKMAX, fastu32div24tab);
        run = fastu32div24(run, ratio, fastu32div24tab);
        slp = fastu32div24(slp, ratio, fastu32div24tab);
#endif
        task->runtime = run;
        task->slptime = slp;
    }

    return;
}

/* applied for time-share tasks of classes SCHEDNORMAL and SCHEDBATCH */
/* return value is new priority */
static __inline__ long
schedcalcintparm(struct task *task, long *retscore)
{
    long range = SCHEDINTRANGE;
    long res = 0;
    long nice = task->proc->nice;
    long score;
    long diff;
    long ntick;
    long tickhz;
    long total;
    long div;
    long tmp;
    
    score = schedcalcscore(task);
    score += nice;
    score = max(0, score);
    if (score < SCHEDSCOREINTLIM) {
        /* map interactive tasks to priorities SCHEDRTMIN..SCHEDBATCHPRIOMIN */
        res = SCHEDINTPRIOMIN;
#if (SCHEDSCOREINTLIM == 32)
        range >>= 5;
#else
        range = fastu32div24(range, SCHEDSCOREINTLIM, fastu32div24tab);
#endif
        range *= score;
        res += range;
    } else {
        ntick = task->ntick;
        if (ntick) {
            /* map noninteractive tasks to priorities SCHEDSYSTEM..SCHEDBATCH */
            total = task->lastrun - task->firstrun;
            tickhz = ntick >> SCHEDTICKSHIFT;
            diff = SCHEDUSERPRIOMAX + SCHEDNICEHALF;
            res = SCHEDUSERPRIOMIN - SCHEDNICEHALF;
            total = max(total, kgethz());
            range = diff - res + 1;
            tmp = roundup(total, range);
            res += nice;
            div = fastu32div24(total, tmp, fastu32div24tab);
            range--;
            total = fastu32div24(tickhz, div, fastu32div24tab);
            diff = min(total, range);
            res += diff;
        }
    }
    *retscore = score;
    
    return res;
}

/* based on sched_wakeup() from ULE :) */
static __inline__ void
taskwakeup(struct task *task)
{
    long id = k_curcpu->data.id;
    long sched = task->sched;
    long slptick = task->slptick;
    long slp;
    long tick;
    long ntick;
    long diff;
#if (SMP)
    long core;
#endif

    task->slptick = 0;
    if (slptick) {
        tick = k_curcpu->data.ntick;
        if (slptick != tick) {
            diff = tick - slptick;
            slp = task->slptime;
            diff <<= SCHEDTICKSHIFT;
            task->slptime = diff;
            schedadjintparm(task);
            schedadjcpupct(task, 0);
        }
    }
    task->slice = 0;
    if (schedistimeshare(sched)) {
        schedcalcprio(task);
    }
#if (SMP) && 0
    id = schedfindidlecore(id, &core);
#endif
    task->state = TASKREADY;
    task->cpu = id;
    m_taskjmp(&task->m_task);
#if 0
    schedsetcpu(&m_cputab[id]);
    schedsetready(task, id);
#endif
    /* FIXME: sched_setpreempt() */
}

#endif /* defined(ZEROSCHED) */

#endif /* __KERN_BITS_SCHED_H__ */

