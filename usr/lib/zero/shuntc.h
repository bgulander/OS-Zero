#ifndef __ZERO_SHUNTC_H__
#define __ZERO_SHUNTC_H__

#include <stdint.h>
#include <zero/bits/shuntc.h>

#define SHUNT_NOPER      SHUNTCNOPER
#define SHUNT_INTSIZE    SHUNTCINTSIZE
#define SHUNT_INT64      SHUNTCINT
#define SHUNT_UINT64     SHUNTCUINT
#define SHUNT_RADIX      shuntcradix
#define SHUNT_TOKEN      struct shuntctoken
#define SHUNT_INT        int64_t
#define SHUNT_UINT       uint64_t
#define SHUNT_OP         shuntcop_t
#define SHUNT_NARGTAB    shuntcopnargtab
#define SHUNT_EVALTAB    shuntcevaltab
#define SHUNT_LEFTPAREN  SHUNTCLEFTPAREN
#define SHUNT_RIGHTPAREN SHUNTCRIGHTPAREN

struct shuntctoken {
    uint_fast8_t  type;         // token type
    char         *str;          // string presentation of value
    uint_fast8_t  slen;         // length of string buffer in characters
    uint_fast8_t  len;          // length of string in characters
    uint_fast8_t  parm;         // e.g. type size in bytes/octets - 1
    uint_fast8_t  radix;        // string presentation, e.g. SHUNT_HEX
//    uint_fast8_t  sign;         // sign-bit
    uint_fast8_t  flg;          // token flags, e.g. SHUNTCZERO
    /* value union */
    union {
        int8_t       i8;
        uint8_t      ui8;
        int16_t      i16;
        uint16_t     ui16;
        int32_t      i32;
        uint32_t     ui32;
        int64_t      i64;
        uint64_t     ui64;
        float        f;
        double       d;
        long double  ld;
        void        *ptr;
    } data;
    /* previous and next in queue/list */
    struct shuntctoken *prev;
    struct shuntctoken *next;
};

typedef SHUNT_INT shuntcop_t(struct shuntctoken *, struct shuntctoken *);

extern uint_fast8_t  shuntcopchartab[256];
extern uint_fast8_t  shuntcopprectab[SHUNTCNTAB];
extern uint_fast8_t  shuntcopnargtab[SHUNTCNTAB];
extern shuntcop_t   *shuntcevaltab[SHUNTCNTAB];
extern uint_fast8_t  shuntcradix;

#define SHUNTCPARMSIZEMASK 0xff
#define shuntcisopchar(c)                                               \
    (shuntcopchartab[(int)(c)])
#define shuntcopprec(tok)                                               \
    (shuntcopprectab[(tok)->type] & ~SHUNTCRTOL)
#define shuntcisrtol(tok)                                               \
    (shuntcopprectab[(tok)->type] & SHUNTCRTOL)
#define shuntcisvalue(tok)                                              \
    ((tok) && ((tok)->type == SHUNTCINT || (tok)->type == SHUNTCUINT))
#define shuntctypesize(tok)                                             \
    (((tok)->parm & SHUNTCPARMSIZEMASK) + 1)
#define shuntcisfunc(tok)                                               \
    ((tok) && (tok)->type == SHUNTCFUNC)
#define shuntcissep(tok)                                                \
    ((tok) && (tok)->type == SHUNTCSEP)
#define shuntcisop(tok)                                                 \
    ((tok) && ((tok)->type >= SHUNTCNOT && (tok)->type <= SHUNTCASSIGN))

SHUNT_INT shuntcnot64(struct shuntctoken *arg1, struct shuntctoken *dummy);
SHUNT_INT shuntcand64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcor64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcxor64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcshl64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcshra64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcshr64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcror64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcrol64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcinc64(struct shuntctoken *arg1, struct shuntctoken *dummy);
SHUNT_INT shuntcdec64(struct shuntctoken *arg1, struct shuntctoken *dummy);
SHUNT_INT shuntcadd64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcsub64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcmul64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcdiv64(struct shuntctoken *arg1, struct shuntctoken *arg2);
SHUNT_INT shuntcmod64(struct shuntctoken *arg1, struct shuntctoken *arg2);

#endif /* __ZERO_SHUNTC_H__ */
