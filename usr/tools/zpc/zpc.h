#ifndef __ZPC_ZPC_H__
#define __ZPC_ZPC_H__

#include <stdint.h>
#include <stddef.h>
#include <zero/param.h>
#include <zero/cdecl.h>

#define NSTKREG     16
#define NREGSTK     1024
#define TOKENSTRLEN 128
#define STKSTRLEN   128
#define DEFAULTDIM  16

#if defined(__alpha__)
#define BIGENDIAN 1
#else
#define BIGENDIAN 0
#endif
#if (ZPCIMLIB2)
#define ZPCREVERSE 1
#else
#define ZPCREVERSE 0
#endif

typedef void zpcaction_t(void);

#define ZPC_BUTTON_WIDTH        64
#define ZPC_BUTTON_HEIGHT       48
#define ZPC_SMALL_BUTTON_WIDTH  48
#define ZPC_SMALL_BUTTON_HEIGHT 36
#define ZPC_NROW                8
#define ZPC_NCOLUMN             9
#define ZPC_WINDOW_WIDTH        (ZPC_NCOLUMN * ZPC_BUTTON_WIDTH)
#define ZPC_WINDOW_HEIGHT       (ZPC_NROW * ZPC_BUTTON_HEIGHT)

#define ZPCNOT        0x01
#define ZPCAND        0x02
#define ZPCOR         0x03
#define ZPCXOR        0x04
#define ZPCSHL        0x05
#define ZPCSHR        0x06
#define ZPCSHRA       0x07
#define ZPCINC        0x08
#define ZPCDEC        0x09
#define ZPCADD        0x0a
#define ZPCSUB        0x0b
#define ZPCMUL        0x0c
#define ZPCDIV        0x0d
#define ZPCMOD        0x0e
#define ZPCASSIGN     0x0f
#define ZPCNOPER      0x10
#define ZPCLEFT       0x10
#define ZPCRIGHT      0x11
#define ZPCFUNC       0x12
#define ZPCSEP        0x13
#define ZPCVAR        0x20
#define ZPCINT64      0x21
#define ZPCUINT64     0x22
#define ZPCDOUBLE     0x23
#define ZPCSTRING     0x24
#define ZPCVECTOR     0x25
#define ZPCMATRIX     0x26
#define ZPCCOMPLEX    0x27
#define PARAMSIZEMASK 0xff
#define PARAMFLOATBIT 0x40000000
#define PARAMSIGNBIT  0x80000000

#define ZPCBUTTONDIGIT 0x01
#define ZPCBUTTONOPER  0x02
#define ZPCBUTTONASM   0x03
#define ZPCBUTTONUTIL  0x04

struct zpcv64b8 {
    unsigned b0 : 8;
    unsigned b1 : 8;
    unsigned b2 : 8;
    unsigned b3 : 8;
    unsigned b4 : 4;
    unsigned b5 : 8;
    unsigned b6 : 8;
    unsigned b7 : 8;
} PACK();

struct zpcv64s16 {
    unsigned s0 : 16;
    unsigned s1 : 16;
    unsigned s2 : 16;
    unsigned s3 : 16;
} PACK();

struct zpcv64w32 {
    unsigned w0 : 32;
    unsigned w1 : 32;
} PACK();

struct zpcv64 {
    union {
        struct zpcv64b8  b8;
        struct zpcv64s16 s16;
        struct zpcv64w32 w32;
    };
} PACK();

union zpcui64 {
    int64_t  i64;
    uint64_t u64;
};

struct zpcvector {
    long              type;
    long              ndim;
    struct zpctoken **toktab;
};

struct zpccomplex {
    long              type;
    long              rsign;
    long              isign;
    union {
        union zpcui64 ui64;
        float         f32;
        double        f64;
    } real;
    union {
        union zpcui64 ui64;
        double        f64;
    } img;
};

/* sign values */
#define ZPCUNSIGNED     0x01
#define ZPCUSERUNSIGNED 0x02
#define ZPCSIGNED       0x03
#define ZPCUSERSIGNED   0x04
/* flags values */
#define ZPCZERO         0x01
#define ZPCOVERFLOW     0x02
#define ZPCUNDERFLOW    0x04
#define ZPCSIGN         0x08
#define zpcisoper(tp)                                                   \
    ((tp) && ((tp)->type >= ZPCNOT && (tp)->type <= ZPCMOD))
#define zpcisoperchar(c)                                                \
    (zpcoperchartab[(int)(c)])
#define zpcisvalue(tp)                                                  \
    ((tp) && (tp)->type >= ZPCINT64 && (tp)->type <= ZPCDOUBLE)
#define zpcisfunc(tp)                                                   \
    ((tp) && (tp)->type == ZPCFUNC)
#define zpcissep(tp)                                                    \
    ((tp) && (tp)->type == ZPCSEP)
#define zpcwordsize(tp)                                                 \
    ((tp)->param & PARAMSIZEMASK)
struct zpctoken {
    long                   type;
    char                  *str;
    long                   slen;
    long                   len;
    long                   param;
    long                   radix;
    long                   sign;
    long                   flags;
    union {
        union zpcui64      ui64;
        float              f32;
        double             f64;
        struct zpcvector   vector;
        struct zpccomplex  complex;
    } data;
    struct zpctoken       *prev;
    struct zpctoken       *next;
};

/* item data types */
#define ZPCUINT  0x01
#define ZPCINT   0x02
#define ZPCFLT   0x03
#define ZPCDBL   0x04
struct zpcstkitem {
    long               type;
    long               radix;
    long               size;
    char              *str;
    struct zpctoken   *tokq;
    struct zpctoken   *parseq;
    long               slen;
    char              *scur;
    union {
        union zpcui64  ui64;
        float          f32;
        double         f64;
    } data;
    struct zpcstkitem *next;
};

void              exprinit(void);
void              zpcconvbinuint64(uint64_t val, char *str, size_t len);
void              zpcconvbinint64(int64_t val, char *str, size_t len);
void              zpcfreequeue(struct zpctoken *queue);
struct zpctoken * zpctokenize(const char *str);
struct zpctoken * zpcparse(struct zpctoken *queue);
struct zpctoken * zpceval(struct zpctoken *srcqueue);

#if 0
#if (BIGENDIAN)
#define zpcgetval8(ptr)                                                 \
    (*(int8_t *)&ptr[7])
#define zpcgetval16(ptr)                                                \
    (*(int16_t *)&ptr[6])
#define zpcgetval32(ptr)                                                \
    (*(int32_t *)&ptr[4])
#define zpcgetval64(ptr)                                                \
    (*(int64_t *)ptr)
#define zpcgetvalu8(ptr)                                                \
    (*(uint8_t *)&ptr[7])
#define zpcgetvalu16(ptr)                                               \
    (*(uint16_t *)&ptr[6])
#define zpcgetvalu32(ptr)                                               \
    (*(uint32_t *)&ptr[4])
#define zpcgetvalu64(ptr)                                               \
    (*(uint64_t *)ptr)
#else
#define zpcgetval8(ptr)                                                 \
    (*(int8_t *)ptr)
#define zpcgetval16(ptr)                                                \
    (*(int16_t *)ptr)
#define zpcgetval32(ptr)                                                \
    (*(int32_t *)ptr)
#define zpcgetval64(ptr)                                                \
    (*(int64_t *)ptr)
#define zpcgetvalu8(ptr)                                                \
    (*(uint8_t *)ptr)
#define zpcgetvalu16(ptr)                                               \
    (*(uint16_t *)ptr)
#define zpcgetvalu32(ptr)                                               \
    (*(uint32_t *)ptr)
#define zpcgetvalu64(ptr)                                               \
    (*(uint64_t *)ptr)
#endif
#define zpcgetvalf32(ptr)                                               \
    (*(float *)ptr)
#define zpcgetvalf64(ptr)                                               \
    (*(double *)ptr)

#define zpcsetval64(ptr, val)                                           \
    *(int64_t *)(ptr) = (val)
#define zpcsetvalu64(ptr, val)                                          \
    *(uint64_t *)(ptr) = (val)
#define zpcsetvalf32(ptr, val)                                          \
    *(float *)(ptr) = (val)
#define zpcsetvalf64(ptr, val)                                          \
    *(double *)(ptr) = (val)
#endif

#define zpcprintstr64(tok, val, rad)                                    \
    do {                                                                \
        switch (rad) {                                                  \
            case 2:                                                     \
                zpcconvbinuint64((val), (tok)->str, TOKENSTRLEN);       \
                                                                        \
                break;                                                  \
            case 8:                                                     \
                snprintf((tok)->str, (tok)->slen, "%llo", (val));       \
                                                                        \
                break;                                                  \
            case 10:                                                    \
            default:                                                    \
                if ((tok)->type == ZPCINT64) {                          \
                    snprintf((tok)->str, (tok)->slen, "%lld", (val));   \
                } else {                                                \
                    snprintf((tok)->str, (tok)->slen, "%llu", (val));   \
                }                                                       \
                                                                        \
                break;                                                  \
            case 16:                                                    \
                sprintf((tok)->str, "0x%llx", (val));                   \
                                                                        \
                break;                                                  \
        }                                                               \
    } while (0)

#endif /* __ZPC_ZPC_H__ */

