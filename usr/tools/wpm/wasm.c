#define ASMDEBUG    0
#define ASMBUF      0
#define ASMPROF     0
#define READBUFSIZE 65536

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <zero/cdecl.h>
#include <zero/param.h>
#include <zero/trix.h>
#if (ASMBUF)
#include <errno.h>
#include <fcntl.h>
#endif
#if (ASMPROF)
#include <zero/prof.h>
#endif
#include <wpm/asm.h>
#if (ZPC)
#include <zpc/zpc.h>
#include <zpc/asm.h>
#include <wpm/mem.h>
#elif (WPM)
#include <wpm/wpm.h>
#endif

typedef struct asmtoken * tokfunc_t(struct asmtoken *, asmadr_t, asmadr_t *);

static asmuword_t        asmgetreg(uint8_t *str, uint8_t **retptr);
static uint8_t         * asmgetlabel(uint8_t *str, uint8_t **retptr);
static struct op       * asmgetinst(uint8_t *str, uint8_t **retptr);
static uint8_t         * asmgetsym(uint8_t *str, uint8_t **retptr);
static asmuword_t        asmgetvalue(uint8_t *str, asmword_t *retval,
                                     uint8_t **retptr);
static uint8_t           asmgetchar(uint8_t *str, uint8_t **retptr);
static struct asmtoken * asmgettoken(uint8_t *str, uint8_t **retptr);

#if (ASMPREPROC)
typedef asmword_t exprfunc_t(asmword_t, asmword_t);

asmword_t exprnot(asmword_t val1, asmword_t val2);
asmword_t exprand(asmword_t val1, asmword_t val2);
asmword_t expror(asmword_t val1, asmword_t val2);
asmword_t exprxor(asmword_t val1, asmword_t val2);
asmword_t exprshl(asmword_t val1, asmword_t val2);
asmword_t exprshr(asmword_t val1, asmword_t val2);
asmword_t expradd(asmword_t val1, asmword_t val2);
asmword_t exprsub(asmword_t val1, asmword_t val2);
asmword_t exprmul(asmword_t val1, asmword_t val2);
asmword_t exprdiv(asmword_t val1, asmword_t val2);
asmword_t exprmod(asmword_t val1, asmword_t val2);
#endif

static struct asmtoken * asmprocvalue(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmproclabel(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmprocinst(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmprocchar(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmprocdata(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmprocglobl(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmprocspace(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmprocorg(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmprocalign(struct asmtoken *, asmadr_t, asmadr_t *);
static struct asmtoken * asmprocasciz(struct asmtoken *, asmadr_t, asmadr_t *);

#if (ZPC)
extern char      *opnametab[ZPCNASMOP];
#elif (WPM)
extern char      *opnametab[256];
#endif

struct asmsym {
    struct asmsym *next;
    uint8_t       *name;
    asmadr_t       adr;
};

#define NHASHITEM 1024
static struct op     *ophash[NHASHITEM] ALIGNED(PAGESIZE);
static struct asmsym *symhash[NHASHITEM];
static struct val    *valhash[NHASHITEM];
static struct label  *globhash[NHASHITEM];
#if (ASMDB)
struct asmline       *linehash[NHASHITEM];
#endif

#if (ZPC)
static uint8_t        opnargtab[ZPCNASMOP]
= {
    0,  // ILL
    1,  // NOT
    2,  // XOR
    2,  // OR
    2,  // AND
    2,  // SHR
    2,  // SHRA
    2,  // SHL
    2,  // ROR
    2,  // ROL
    1,  // INC
    1,  // DEC
    2,  // ADD
    2,  // SUB
    2,  // MUL
    2,  // DIV
    2,  // MOD
    1,  // BZ
    1,  // BNZ
    1,  // BLT
    1,  // BLE
    1,  // BGT
    1,  // BGE
    2,  // MOV
    1,  // CALL
    1,  // RET
    1   // TRAP
};
#elif (WPM)
static uint8_t        opnargtab[256]
= {
    0,
    1,  // NOT
    2,  // AND
    2,  // OR
    2,  // XOR
    2,  // SHL
    2,  // SHR
    2,  // SRHL
    2,  // ROR
    2,  // ROL
    1,  // INC
    1,  // DEC
    2,  // ADD
    2,  // SUB
    2,  // CMP
    2,  // MUL
    2,  // DIV
    2,  // MOD
    1,  // BZ
    1,  // BNZ
    1,  // BLT
    1,  // BLE
    1,  // BGT
    1,  // BGE
    1,  // BO
    1,  // BNO
    1,  // BC
    1,  // BNC
    1,  // POP
    1,  // PUSH
    2,  // MOV
    2,  // MOVB
    2,  // MOVW
    1,  // JMP
    1,  // CALL
    0,  // ENTER
    0,  // LEAVE
    0,  // RET
    1,  // LMSW
    1,  // SMSW
    0,  // RESET
    0,  // NOP
    0,  // HLT
    0,  // BRK
    1,  // TRAP
    0,  // CLI
    0,  // STI
    0,  // IRET
    1,  // THR
    2,  // CMPSWAP
    1,  // INB
    2,  // OUTB
    1,  // INW
    2,  // OUTW
    1,  // INL
    2,  // OUTL
    1   // HOOK
};
#endif

#if (ASMPREPROC)
static asmuword_t  exproptab[256];
exprfunc_t      *exprfunctab[256]
= {
    NULL,
    exprnot,
    exprand,
    expror,
    exprxor,
    exprshl,
    exprshr,
    expradd,
    exprsub,
    exprmul,
    exprdiv,
    exprmod
};
#endif

tokfunc_t *tokfunctab[NTOK]
= {
    NULL,
    asmprocvalue,
    asmproclabel,
    asmprocinst,
    NULL,               // REG
    NULL,               // SYM
    asmprocchar,
    NULL,               // IMMED
    NULL,               // INDIR
    NULL,               // ADR
    NULL,               // INDEX
    asmprocdata,
    asmprocglobl,
    asmprocspace,
    asmprocorg,
    asmprocalign,
    asmprocasciz
};

#if (ASMPREPROC)
#define NEXPRSTK 1024
#define EXPRNOT   0x01
#define EXPRAND   0x02
#define EXPROR    0x03
#define EXPRXOR   0x04
#define EXPRSHL   0x05
#define EXPRSHR   0x06
#define EXPRADD   0x07
#define EXPRSUB   0x08
#define EXPRMUL   0x09
#define EXPRDIV   0x0a
#define EXPRMOD   0x0b
struct expr {
    struct expr   *next;
    unsigned long  type;
    asmword_t      val;
};
static struct expr     *exprstk[NEXPRSTK];
#endif
static struct asmtoken *tokenqueue;
static struct asmtoken *tokentail;
static struct asmsym   *symqueue;
static asmadr_t         _start;
static asmadr_t         _startset;
static unsigned long    inputread;
static uint8_t         *linebuf;
static uint8_t         *strbuf;
#if (ASMBUF)
struct readbuf {
    void    *data;
    uint8_t *cur;
    uint8_t *lim;
};
struct readbuf       *readbuftab;
static long           nreadbuf = 16;
static long           readbufcur = 0;
#endif

#if (ASMBUF)
static int
asmgetc(int fd, int bufid)
{
    struct readbuf *buf = &readbuftab[bufid];
    ssize_t         nleft = READBUFSIZE;
    ssize_t         n;
    int             ch = EOF;
    long            l = nreadbuf;

    if (bufid >= nreadbuf) {
        nreadbuf <<= 1;
        readbuftab = realloc(readbuftab, nreadbuf * sizeof(struct readbuf));
        for ( ; l < nreadbuf ; l++) {
            readbuftab[l].data = malloc(READBUFSIZE);
        }
    }
    if (buf->cur < buf->lim) {
        ch = *buf->cur++;
    } else if (buf->cur == buf->lim) {
        n = 0;
        while (nleft) {
            n = read(fd, buf->data, READBUFSIZE);
            if (n < 0) {
                if (errno == EINTR) {
                    
                    continue;
                } else {
                    
                    return EOF;
                }
            } else if (n == 0) {

                break;
            } else {
                nleft -= n;
            }
        }
        if (nleft == READBUFSIZE) {

            return EOF;
        }
        buf->cur = buf->data;
        buf->lim = (uint8_t *)buf->data + READBUFSIZE - nleft;
        ch = *buf->cur++;
    }

    return ch;
}
#endif

void
printtoken(struct asmtoken *token)
{
    switch (token->type) {
        case TOKENVALUE:
#if (ZPC)
            zpcprinttoken(token->data.token);
#elif (WPM)
            fprintf(stderr, "value 0x%08x (size == %d)\n",
                    token->data.value.val, token->data.value.size);
#endif

            break;
        case TOKENLABEL:
            fprintf(stderr, "label %s (adr == 0x%08x)\n",
                    token->data.label.name,
                    (unsigned long)token->data.label.adr);

            break;
        case TOKENINST:
            fprintf(stderr, "instruction %s (op == 0x%02x)\n",
                    token->data.inst.name, token->data.inst.op);

            break;
        case TOKENREG:
            fprintf(stderr, "register r%1x\n", token->data.ndx.reg);

            break;
        case TOKENSYM:
            fprintf(stderr, "symbol %s (adr == 0x%08x)\n",
                    token->data.sym.name, token->data.sym.adr);

            break;
        case TOKENCHAR:
            fprintf(stderr, "character 0x%02x\n", token->data.ch);

            break;
        case TOKENIMMED:
            fprintf(stderr, "immediate (val == 0x%08x)\n", token->val);

            break;
        case TOKENINDIR:
            fprintf(stderr, "indirection\n");

            break;
        case TOKENADR:
            fprintf(stderr, "address (sym == %s, adr == 0x%08x)\n",
                    token->data.adr.name, token->data.adr.val);

            break;
        case TOKENINDEX:
            fprintf(stderr, "index %d(%%r%d)\n", token->data.ndx.val, token->data.ndx.reg);

            break;
    }
}

void
asmfreetoken(struct asmtoken *token)
{
#if (ASMDB)
    free(token->file);
#endif
    free(token);

    return;
}

static void
asmqueuetoken(struct asmtoken *token)
{
    token->next = NULL;
    if (!tokenqueue) {
        token->prev = NULL;
        tokenqueue = token;
    } else if (tokentail) {
        token->prev = tokentail;
        tokentail->next = token;
        tokentail = token;
    } else {
        tokenqueue->next = token;
        token->prev = tokenqueue;
        tokentail = token;
    }

    return;
}

static void
asmaddop(struct op *op)
{
    uint8_t       *str = op->name;
    unsigned long  key = 0;
    unsigned long  len = 0;

    while (*str) {
        key += *str++;
        len++;
    }
    op->len = len;
    key &= (NHASHITEM - 1);
    op->next = ophash[key];
    ophash[key] = op;

    return;
}

struct op *
asmfindop(uint8_t *name)
{
    struct op     *op = NULL;
    uint8_t       *str = name;
    unsigned long  key = 0;

    while ((*str) && isalpha(*str)) {
        key += *str++;
    }
    key &= (NHASHITEM - 1);
    op = ophash[key];
    while ((op) && strncmp((char *)op->name, (char *)name, op->len)) {
        op = op->next;
    }

    return op;
}

#if (ASMDB)
void
asmaddline(asmadr_t adr, uint8_t *data, uint8_t *filename, unsigned long line)
{
    struct asmline *newline = malloc(sizeof(struct asmline));
    unsigned long   key;

    key = (adr & 0xff) + ((adr >> 8) & 0xff) + ((adr >> 16) & 0xff) + ((adr >> 24) & 0xff);
    newline->adr = adr;
    newline->file = (uint8_t *)strdup((char *)filename);
    newline->num = line;
    newline->data = data;
    newline->next = linehash[key];
    key &= (NHASHITEM - 1);
    linehash[key] = newline;

    return;
}

struct asmline *
asmfindline(asmadr_t adr)
{
    struct asmline *line;
    unsigned long   key;

    key = (adr & 0xff) + ((adr >> 8) & 0xff) + ((adr >> 16) & 0xff) + ((adr >> 24) & 0xff);
    key &= (NHASHITEM - 1);
    line = linehash[key];
    while ((line) && line->adr != adr) {
        line = line->next;
    }

    return line;
}
#endif

void
asmaddval(struct val *val)
{
    unsigned long   key = 0;
    uint8_t        *ptr;

    ptr = val->name;
    while (*ptr) {
        key += *ptr++;
    }
    key &= (NHASHITEM - 1);
    val->next = valhash[key];
    valhash[key] = val;

    return;
}

struct val *
asmfindval(uint8_t *str, asmword_t *valptr, uint8_t **retptr)
{
    unsigned long  key = 0;
    struct val    *val = NULL;
    uint8_t       *ptr;
    long           len = 0;

    if ((*str) && (isalpha(*str) || *str == '_')) {
        ptr = str;
        key += *str;
        str++;
        len++;
        while ((*str) && (isalpha(*str) || *str == '_')) {
            key += *str;
            str++;
            len++;
        }
        key &= (NHASHITEM - 1);
        val = valhash[key];
        while ((val) && strncmp((char *)val->name, (char *)ptr, len)) {
            val = val->next;
        }
        if (val) {
            *valptr = val->val;
            *retptr = str;
        }
    }

    return val;
}

static void
asmqueuesym(struct asmsym *sym)
{
    sym->next = NULL;
    if (!symqueue) {
        symqueue = sym;
    } else {
        sym->next = symqueue;
        symqueue = sym;
    }

    return;
}

static void
asmaddsym(struct asmsym *sym)
{
    uint8_t       *str = sym->name;
    unsigned long  key = 0;

    while (*str) {
        key += *str++;
    }
    key &= (NHASHITEM - 1);
    sym->next = symhash[key];
    symhash[key] = sym;

    return;
}

struct asmsym *
asmfindsym(uint8_t *name)
{
    struct asmsym *sym = NULL;
    uint8_t       *str = name;
    unsigned long  key = 0;

    while (*str) {
        key += *str++;
    }
    key &= (NHASHITEM - 1);
    sym = symhash[key];
    while ((sym) && strcmp((char *)sym->name, (char *)name)) {
        sym = sym->next;
    }

    return sym;
}

void
asmremovesyms(void)
{
    struct asmsym *sym1;
    struct asmsym *sym2;
    long           l;

    for (l = 0 ; l < NHASHITEM ; l++) {
        sym1 = symhash[l];
        while (sym1) {
            sym2 = sym1;
            sym1 = sym1->next;
            free(sym2);
        }
        symhash[l] = NULL;
    }

    return;
}

static void
asmaddglob(struct label *label)
{
    uint8_t       *str = label->name;
    unsigned long  key = 0;

    while (*str) {
        key += *str++;
    }
    key &= (NHASHITEM - 1);
    label->next = globhash[key];
    globhash[key] = label;

    return;
}

struct label *
asmfindglob(uint8_t *name)
{
    struct label  *label = NULL;
    uint8_t       *str = name;
    unsigned long  key = 0;

    while (*str) {
        key += *str++;
    }
    key &= (NHASHITEM - 1);
    label = globhash[key];
    while ((label) && strcmp((char *)label->name, (char *)name)) {
        label = label->next;
    }

    return label;
}

#if (ASMPREPROC)
void
asminitexpr(void)
{
    exproptab['~'] = EXPRNOT;
    exproptab['&'] = EXPRAND;
    exproptab['|'] = EXPROR;
    exproptab['^'] = EXPRXOR;
    exproptab['<'] = EXPRSHL;
    exproptab['>'] = EXPRSHR;
    exproptab['+'] = EXPRADD;
    exproptab['-'] = EXPRSUB;
    exproptab['*'] = EXPRMUL;
    exproptab['/'] = EXPRDIV;
    exproptab['%'] = EXPRMOD;
}

long
exprisop(uint8_t *str)
{
    int  ch = *str;
    long retval = exproptab[ch];

    if (retval == EXPRSHL && str[1] != '<') {
        retval = 0;
    } else if (retval == EXPRSHR && str[1] != '>') {
        retval = 0;
    }

    return retval;
}
#endif /* ASMPREPROC */

void
asminitop(void)
{
    struct op *op;
    long       l;

    for (l = 1 ; (opnametab[l]) ; l++) {
        op = malloc(sizeof(struct op));
        op->name = (uint8_t *)opnametab[l];
        op->code = (uint8_t)l;
        op->narg = opnargtab[l];
        asmaddop(op);
    }

    return;
}

void
asminitbuf(void)
{
#if (ASMBUF)
    long l;
#endif

    linebuf = malloc(LINELEN);
    strbuf = malloc(LINELEN);
#if (ASMBUF)
    readbuftab = malloc(nreadbuf * sizeof(struct readbuf));
    for (l = 0 ; l < nreadbuf ; l++) {
        readbuftab[l].data = malloc(READBUFSIZE);
    }
#endif

    return;
}

static asmuword_t
asmgetreg(uint8_t *str, uint8_t **retptr)
{
    asmuword_t reg = 0;

#if (ASMDEBUG)
    fprintf(stderr, "getreg: %s\n", str);
#endif
    if (*str == 'r') {
        str++;
        while ((*str) && isdigit(*str)) {
            reg *= 10;
            reg += *str - '0';
            str++;
        }
        while (*str == ')' || *str == ',') {
            str++;
        }
        *retptr = str;
    } else {
        fprintf(stderr, "invalid register name %s\n", str);
        
        exit(1);
    }

    return reg;
}

static uint8_t *
asmgetlabel(uint8_t *str, uint8_t **retptr)
{
    uint8_t *ptr = str;
    uint8_t *name = NULL;

#if (ASMDEBUG)
    fprintf(stderr, "getlabel: %s\n", str);
#endif
    while ((*str) && (isalpha(*str) || *str == '_')) {
        str++;
    }
    if (*str == ':') {
        *str++ = '\0';
        name = (uint8_t *)strdup((char *)ptr);
        *retptr = str;
    }

    return name;
}

static struct op *
asmgetinst(uint8_t *str, uint8_t **retptr)
{
    struct op *op;

    op = asmfindop(str);
#if (ASMDEBUG)
    fprintf(stderr, "getinst: %s\n", str);
#endif
    if (op) {
        str += op->len;
        *retptr = str;
    }

    return op;
}

static uint8_t *
asmgetsym(uint8_t *str, uint8_t **retptr)
{
    uint8_t *ptr = str;
    uint8_t *name = NULL;

#if (ASMDEBUG)
    fprintf(stderr, "getsym: %s\n", str);
#endif
    if (isalpha(*str) || *str == '_') {
        str++;
    }
    while (isalpha(*str) || *str == '_' || isdigit(*str)) {
        str++;
    }
    while ((*str) && (isspace(*str))) {
        str++;
    }
    if (*str == ',') {
        *str++ = '\0';
    }
    name = (uint8_t *)strdup((char *)ptr);
    *retptr = str;

    return name;
}

static uint8_t *
asmgetadr(uint8_t *str, uint8_t **retptr)
{
    uint8_t *ptr = str;
    uint8_t *name = NULL;
    
#if (ASMDEBUG)
    fprintf(stderr, "getadr: %s\n", str);
#endif
    while (isalpha(*str) || *str == '_') {
        str++;
    }
    if (*str == ',') {
        *str++ = '\0';
    }
    name = (uint8_t *)strdup((char *)ptr);
    *retptr = str;

    return name;
}

static asmuword_t
asmgetvalue(uint8_t *str, asmword_t *retval, uint8_t **retptr)
{
    long      found = 0;
    asmword_t val = 0;
    long      neg = 0;

#if (ASMDEBUG)
    fprintf(stderr, "getvalue: %s\n", str);
#endif
    if (*str == '-') {
        neg = 1;
        str++;
    }
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
        while ((*str) && isxdigit(*str)) {
            val <<= 4;
            val += (isdigit(*str)
                    ? *str - '0'
                    : (islower(*str)
                       ? *str - 'a' + 10
                       : *str - 'A' + 10));
            str++;
        }
        found = 1;
    } else if (str[0] == '0' && (str[1] == 'b' || str[1] == 'B')) {
        str += 2;
        while ((*str) && (*str == '0' || *str == '1')) {
            val <<= 1;
            val += *str - '0';
            str++;
        }
        found = 1;
    } else if (*str == '0') {
        str++;
        while ((*str) && isdigit(*str)) {
            if (*str > '7') {
                fprintf(stderr, "invalid number in octal constant: %s\n", str);

                exit(1);
            }
            val <<= 3;
            val += *str - '0';
            str++;
        }
        found = 1;
    } else if (isdigit(*str)) {
        while ((*str) && isdigit(*str)) {
            val *= 10;
            val += *str - '0';
            str++;
        }
        found = 1;
    }
    if (found) {
        if (neg) {
            val = -val;
        }
        *retval = val;
        while (*str == ',' || *str == '\'') {
            str++;
        }
        *retptr = str;
    }

    return found;
}

static asmuword_t
asmgetindex(uint8_t *str, asmword_t *retndx, uint8_t **retptr)
{
    asmuword_t reg = 0xff;
    asmword_t  val = 0;
    asmword_t  ndx = 0;
    long       neg = 0;

#if (ASMDEBUG)
    fprintf(stderr, "getindex: %s\n", str);
#endif
    if (*str == '-') {
        neg = 1;
        str++;
    }
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
        while ((*str) && isxdigit(*str)) {
            val <<= 4;
            val += (isdigit(*str)
                    ? *str - '0'
                    : (islower(*str)
                       ? *str - 'a' + 10
                       : *str - 'A' + 10));
            str++;
        }
    } else if (str[0] == '0' && (str[1] == 'b' || str[1] == 'B')) {
        str += 2;
        while ((*str) && (*str == '0' || *str == '1')) {
            val <<= 1;
            val += *str - '0';
            str++;
        }
    } else if (*str == '0') {
        str++;
        while ((*str) && isdigit(*str)) {
            if (*str > '7') {
                fprintf(stderr, "invalid number in octal constant: %s\n", str);

                exit(1);
            }
            val <<= 3;
            val += *str - '0';
            str++;
        }
    } else if (isdigit(*str)) {
        while ((*str) && isdigit(*str)) {
            val *= 10;
            val += *str - '0';
            str++;
        }
    }
    if (*str == '(') {
        ndx = 1;
        str++;
    }
    if (ndx) {
        if (*str == '%') {
            str++;
        }
#if (ZPC)
        reg = zpcgetreg(str, &str);
        if ((reg & 0x0f) >= ZPCNREG) {
            fprintf(stderr, "invalid register name %s\n", str);

            exit(1);
        }
#elif (WPM)
        reg = asmgetreg(str, &str);
        if (reg >= NREG) {
            fprintf(stderr, "invalid register name %s\n", str);

            exit(1);
        }
#endif
        *retptr = str;
    }
    if (neg) {
        val = -val;
    }
    *retndx = val;

    return reg;
}

static uint8_t
asmgetchar(uint8_t *str, uint8_t **retptr)
{
    uint8_t   *name = str;
    asmword_t  val = 0;

#if (ASMDEBUG)
    fprintf(stderr,"getchar: %s\n", str);
#endif
    str++;
    if (*str == '\\') {
        str++;
        if (isalpha(*str)) {
            switch (*str) {
                case 'n':
                    val = '\n';
                    str++;

                    break;
                case 't':
                    val = '\t';
                    str++;

                    break;
                case 'r':
                    val = '\r';
                    str++;

                    break;
                default:
                    fprintf(stderr, "invalid character literal: %s\n", name);
            }
        } else {
            if (*str == '0') {
                str++;
            }
            while ((*str) && isdigit(*str)) {
                if (*str > '7') {
                    fprintf(stderr, "invalid number in octal constant: %s\n",
                            name);
                    
                    exit(1);
                }
                val <<= 3;
                val += *str - '0';
                str++;
            }
        }
    } else if (isalpha(*str) || isspace(*str)) {
        val = *str;
        str++;
    } else {
        while ((*str) && isdigit(*str)) {
            val *= 10;
            val += *str - '0';
            str++;
        }
    }
    if (val > 0xff) {
        fprintf(stderr, "oversize character literal %s (%x)\n", name, val);

        exit(1);
    }
    if (*str == '\'') {
        str++;
    }
    if (*str == ',') {
        str++;
    }
    *retptr = str;

    return (uint8_t)val;
}

#if (ASMPREPROC)
void
printexprstk(void)
{
    long         l;
    long         nl;
    struct expr *expr;

    for (l = 0 ; l < NEXPRSTK ; l++) {
        expr = exprstk[l];
        if (expr) {
            fprintf(stderr, "%ld: ", l);
            nl = 1;
        }
        while (expr) {
            if (!expr->type) {
                fprintf(stderr, "%d, ", expr->val);
            } else {
                fprintf(stderr, "op == %lu, ", expr->type);
            }
            expr = expr->next;
        }
        if (nl) {
            fprintf(stderr, "\n");
        }
        nl = 0;
    }                                             
}

static asmword_t
asmeval(uint8_t *str, asmword_t *valptr, uint8_t **retptr)
{
    struct expr *expr1;
    struct expr *expr2;
    uint8_t     *ptr = str;
    long         lvl = 0;
    long         maxlvl = 0;
    long         loop = 1;
    asmword_t    retval = 0;
    asmword_t    res = 0;
    asmword_t    val;
    long         cur;
    long         op;
    exprfunc_t  *func;

    memset(exprstk, 0, sizeof(exprstk));
    while (loop) {
        while ((*ptr) && isspace(*ptr)) {
            ptr++;
        }
        while ((*ptr) && *ptr == '(') {
            lvl++;
            ptr++;
        }
        maxlvl = max(lvl, maxlvl);
        while ((*ptr) && isspace(*ptr)) {
            ptr++;
        }
        expr1 = malloc(sizeof(struct expr));
        if (asmfindval(ptr, &val, &ptr)) {
            expr1->type = 0;
            expr1->val = val;
            expr1->next = exprstk[lvl];
            exprstk[lvl] = expr1;
        } else {
            op = exprisop(ptr);
            if (op) {
                ptr++;
                if (op == EXPRSHL || op == EXPRSHR) {
                    ptr++;
                } 
                expr1->type = op;
                expr1->next = exprstk[lvl];
                exprstk[lvl] = expr1;
            } else if ((*ptr) && isdigit(*ptr)) {
#if (ZPC)
                zpctoken = zpcgettoken(ptr, &ptr);
#elif (WPM)
                asmgetvalue(ptr, &val, &ptr);
#endif
                expr1->type = 0;
                expr1->val = val;
                expr1->next = exprstk[lvl];
                exprstk[lvl] = expr1;
            } else if (isalpha(*ptr) || *ptr == '_') {
                asmfindval(ptr, &val, &ptr);
                expr1->type = 0;
                expr1->val = val;
                expr1->next = exprstk[lvl];
                exprstk[lvl] = expr1;
            } else if ((*ptr) && *ptr == ',') {
                lvl = 0;
            } else if ((*ptr) && *ptr == ')') {
                do {
                    lvl--;
                } while (*++ptr == ')');
            }
        }
        if (lvl < 0) {
            fprintf(stderr, "mismatched closing parentheses: %s\n", str);
            
            exit(1);
        }
        if (!lvl) {
            loop = 0;
        }
    }
    op = 0;
    cur = 0;
    while (maxlvl) {
        loop = 1;
        expr1 = exprstk[maxlvl];
        while (expr1) {
            if (!expr1->type) {
                if (!cur) {
                    cur++;
                    res = expr1->val;
                } else {
                    cur++;
                    val = expr1->val;
                }
            } else {
                op = expr1->type;
            }
            if ((op) && cur == 2) {
                func = exprfunctab[op];
                if (func) {
                    res = func(res, val);
                    retval = 1;
                }
                op = 0;
                cur = 1;
            }
            expr2 = expr1->next;
            free(expr1);
            expr1 = expr2;
        }
        maxlvl--;
    }
    if (retval) {
        if (*ptr == ',') {
            ptr++;
        }
        *valptr = res;
        *retptr = ptr;
    }

    return retval;
}
#endif

static struct asmtoken *
asmgettoken(uint8_t *str, uint8_t **retptr)
{
    long             buflen = LINELEN;
    long             len;
    uint8_t         *buf = strbuf;
    struct asmtoken *token1 = malloc(sizeof(struct asmtoken));
    struct asmtoken *token2;
#if (ZPC)
    struct zpctoken *zpctoken;
#endif
    struct op       *op = NULL;
    uint8_t         *name = str;
    asmword_t        val = RESOLVE;
    static long      size = 0;
    asmword_t        ndx;
    int              ch;
#if (ASMDB)
    uint8_t         *ptr;
#endif

    while (*str && isspace(*str)) {
        str++;
    }
    if (*str == ',') {
        str++;
    }
    while (*str && isspace(*str)) {
        str++;
    }
#if (ASMDEBUG)
    fprintf(stderr, "gettoken: %s\n", str);
#endif
    if ((*str) && (isdigit(*str) || *str == '-')) {
        val = asmgetindex(str, &ndx, &str);
#if (ZPC)
        if (val < ZPCNREG) {
            token1->type = TOKENINDEX;
            token1->data.ndx.reg = REGINDEX | val;
            token1->data.ndx.val = ndx;
        } else {
            zpctoken = zpcgettoken(str, &str);
            if (zpctoken) {
                token1->type = TOKENVALUE;
                token1->data.token = zpctoken;
#if 0
                token1->data.value.val = val;
                token1->data.value.size = size;
#endif
            } else {
                fprintf(stderr, "invalid token %s\n", linebuf);
                
                exit(1);
            }
        }
#elif (WPM)
        if (val < NREG) {
            token1->type = TOKENINDEX;
            token1->data.ndx.reg = REGINDEX | val;
            token1->data.ndx.val = ndx;
        } else if (asmgetvalue(str, &val, &str)) {
            token1->type = TOKENVALUE;
            token1->data.value.val = val;
            token1->data.value.size = size;
        } else {
            fprintf(stderr, "invalid token %s\n", linebuf);
                
            exit(1);
        }
#endif
    } else if ((*str) && *str == '"') {
        str++;
        len = 0;
        while (*str) {
            *buf++ = *str++;
            len++;
            if (len == buflen) {
                fprintf(stderr, "overlong line\n");

                exit(1);
            }
        }
        while (*str != '"') {
            ch = *str++;
            if (ch == '\\') {
                switch (*str) {
                    case 'n':
                        *buf++ = (uint8_t)'\n';
                        str++;

                        break;
                    case 'r':
                        *buf++ = (uint8_t)'\r';
                        str++;
                        
                        break;
                    case 't':
                        *buf++ = (uint8_t)'\t';
                        str++;
                        
                        break;
                }
            } else {
                *buf++ = (uint8_t)ch;
            }
            len--;
        }
        *buf++ = '\0';
        if (*str == '"') {
            str++;
            token1->type = TOKENSTRING;
            token1->data.str = (uint8_t *)strdup((char *)strbuf);
        }
    } else if ((*str) && *str == '%') {
        str++;
#if (ZPC)
        val = zpcgetreg(str, &str);
        if (val & ZPCREGSTKBIT) {
            token1->data.token->param = ZPCCALC;
            val &= ~ZPCREGSTKBIT;
        }
#elif (WPM)
        val = asmgetreg(str, &str);
#endif
        token1->type = TOKENREG;
        token1->data.reg = val;
    } else if ((*str) && (isalpha(*str) || *str == '_')) {
        name = asmgetlabel(str, &str);
        if (name) {
            token1->type = TOKENLABEL;
            token1->data.label.name = name;
            token1->data.label.adr = RESOLVE;
        } else {
#if (ASMDB)
            ptr = str;
#endif
            op = asmgetinst(str, &str);
            if (op) {
                token1->type = TOKENINST;
                token1->data.inst.name = op->name;
                token1->data.inst.op = op->code;
                token1->data.inst.narg = op->narg;
#if (ASMDB)
                token1->data.inst.data = (uint8_t *)strdup((char *)ptr);
#endif
            } else {
                name = asmgetsym(str, &str);
                if (name) {
                    token1->type = TOKENSYM;
                    token1->data.sym.name = name;
                    token1->data.sym.adr = RESOLVE;
                }
                if (!name) {
                    name = asmgetadr(str, &str);
                    if (name) {
                        token1->type = TOKENSYM;
                        token1->data.sym.name = name;
                        token1->data.sym.adr = RESOLVE;
                    } else {
                        fprintf(stderr, "invalid token %s\n", linebuf);
                        
                        exit(1);
                    }
                }
            }
        }
    } else if ((*str) && *str == '$') {
        str++;
        if ((*str) && (isalpha(*str) || *str == '_' || *str == '-')) {
            if (asmfindval(str, &val, &str)) {
                token1->type = TOKENIMMED;
                token1->val = val;
            } else if (asmgetvalue(str, &val, &str)) {
                token1->type = TOKENIMMED;
                token1->val = val;
            } else {
                name = asmgetsym(str, &str);
                if (name) {
                    token1->type = TOKENADR;
                    token1->data.adr.name = name;
                    token1->data.adr.val = RESOLVE;
                } else {
                    fprintf(stderr, "invalid token %s\n", linebuf);
                    
                    exit(1);
                }
            }
        } else if ((*str) && isdigit(*str)) {
            if (asmgetvalue(str, &val, &str)) {
                token1->type = TOKENIMMED;
                token1->val = val;
            } else {
                fprintf(stderr, "invalid immediate %s\n", str);

                exit(1);
            }
#if (ASMPREPROC)
        } else if ((*str) && *str == '(') {
            if (asmeval(str, &val, &str)) {
                token1->type = TOKENIMMED;
                token1->val = val;
            }
#endif
        }
    } else if (*str == '\'') {
        val = asmgetchar(str, &str);
        token1->type = TOKENCHAR;
        token1->data.ch = val;
    } else if (*str == '.') {
        str++;
        size = 0;
        if (!strncmp((char *)str, "long", 4)) {
            str += 4;
            token1->type = TOKENDATA;
            size = token1->data.size = 4;
        } else if (!strncmp((char *)str, "byte", 4)) {
            str += 4;
            token1->type = TOKENDATA;
            size = token1->data.size = 1;
        } else if (!strncmp((char *)str, "short", 5)) {
            str += 5;
            token1->type = TOKENDATA;
            size = token1->data.size = 2;
        } else if (!strncmp((char *)str, "globl", 5)) {
            str += 5;
            token1->type = TOKENGLOBL;
        } else if (!strncmp((char *)str, "space", 5)) {
            str += 5;
            token1->type = TOKENSPACE;
        } else if (!strncmp((char *)str, "org", 3)) {
            str += 3;
            token1->type = TOKENORG;
        } else if (!strncmp((char *)str, "align", 5)) {
            str += 5;
            token1->type = TOKENALIGN;
        } else if (!strncmp((char *)str, "asciz", 5)) {
            str += 5;
            token1->type = TOKENASCIZ;
        }
    } else if (*str == '*' || *str == '(') {
        str++;
        token1->type = TOKENINDIR;
        token2 = malloc(sizeof(struct asmtoken));
        if (*str == '%') {
            str++;
            val = asmgetreg(str, &str);
            token2->type = TOKENREG;
            token2->data.reg = REGINDIR | val;
            asmqueuetoken(token1);
            token1 = token2;
        } else if (isalpha(*str) || *str == '_') {
            name = asmgetsym(str, &str);
            if (name) {
                token2->type = TOKENSYM;
                token2->data.sym.name = name;
                asmqueuetoken(token1);
                token1 = token2;
            } else {
                fprintf(stderr, "invalid token %s\n", linebuf);

                exit(1);
            }
        } else {
            fprintf(stderr, "invalid token %s\n", linebuf);
            
            exit(1);
        }
    }
    *retptr = str;

    return token1;
}

static struct asmtoken *
asmprocvalue(struct asmtoken *token, asmadr_t adr,
             asmadr_t *retadr)
{
    uint8_t         *valptr = &physmem[adr];
    struct asmtoken *retval;

#if (ZPC)
    switch (token->data.token->type) {
        case ZPCINT64:
            *((int64_t *)valptr) = token->data.token->data.ui64.i64;
            *retadr = adr + sizeof(int64_t);

            break;
        case ZPCUINT64:
            *((uint64_t *)valptr) = token->data.token->data.ui64.u64;
            *retadr = adr + sizeof(uint64_t);

            break;
        case ZPCFLOAT:
            *((float *)valptr) = token->data.token->data.f32;
            *retadr = adr + sizeof(float);

            break;
        case ZPCDOUBLE:
            *((double *)valptr) = token->data.token->data.f64;
            *retadr = adr + sizeof(double);

            break;
    }
#elif (WPM)
    switch (token->data.value.size) {
        case 1:
            *valptr = token->data.value.val;

            break;
        case 2:
            *((uint16_t *)valptr) = token->data.value.val;

            break;
        case 4:
            *((uint32_t *)valptr) = token->data.value.val;

            break;
        case 8:
            *((uint64_t *)valptr) = token->data.value.val;

            break;
    }
    *retadr = adr + token->data.value.size;
#endif
    retval = token->next;
    asmfreetoken(token);

    return retval;
}

static struct asmtoken *
asmproclabel(struct asmtoken *token, asmadr_t adr,
             asmadr_t *retadr)
{
    struct asmsym    *sym;
    struct asmtoken  *retval;

    if (!_startset && !strncmp((char *)token->data.label.name, "_start", 6)) {
#if (WPMTRACE)
        fprintf(stderr, "_start == 0x%08x\n", adr);
#endif
        _start = adr;
        _startset = 1;
    }
    sym = asmfindsym(token->data.label.name);
    if (sym) {
        sym->adr = adr;
    } else {
        sym = malloc(sizeof(struct asmsym));
        sym->name = token->data.label.name;
        sym->adr = adr;
        asmaddsym(sym);
    }
    *retadr = adr;
    retval = token->next;
    asmfreetoken(token);

    return retval;
}

static struct asmtoken *
asmprocinst(struct asmtoken *token, asmadr_t adr,
            asmadr_t *retadr)
{
#if (ZPC)
    struct zpcopcode *op;
#elif (WPM)
    struct wpmopcode *op;
#endif
    asmadr_t          opadr = roundup2(adr, 4);
    struct asmtoken  *token1 = NULL;
    struct asmtoken  *token2 = NULL;
    struct asmtoken  *retval = NULL;
    struct asmsym    *sym;
    uint8_t           narg = token->data.inst.narg;
#if (ZPC)
    uint8_t           len = 8;
#elif (WPM)
    uint8_t           len = token->data.inst.op == OPNOP ? 1 : 4;
#endif

#if (ZPC)
    adr = opadr;
#elif (WPM)
    while (adr < opadr) {
        physmem[adr] = OPNOP;
        adr++;
    }
#endif
//    adr = opadr;
#if (ASMDB)
    asmaddline(adr, token->data.inst.data, token->file, token->line);
#endif
#if (ZPC)
    op = (struct zpcopcode *)&physmem[adr];
    op->inst = token->data.inst.op;
#elif (WPM)
    op = (struct wpmopcode *)&physmem[adr];
    op->inst = token->data.inst.op;
    if (op->inst == OPNOP) {
        retval = token->next;
        adr++;
    } else
#endif
    if (!narg) {
        op->arg1t = ARGNONE;
        op->arg2t = ARGNONE;
        op->reg1 = 0;
        op->reg2 = 0;
        retval = token->next;
    } else {
        token1 = token->next;
        asmfreetoken(token);
        if (token1) {
            switch(token1->type) {
                case TOKENVALUE:
                    op->arg1t = ARGIMMED;
#if (ZPC)
                    op->args[0] = token1->data.token->data.ui64.u64;
#elif (WPM)
                    op->args[0] = token1->data.value.val;
#endif
                    len += sizeof(asmword_t);
                    
                    break;
                case TOKENREG:
                    op->arg1t = ARGREG;
                    op->reg1 = token1->data.reg;
                    
                    break;
                case TOKENSYM:
                    op->arg1t = ARGADR;
                    sym = malloc(sizeof(struct sym));
                    sym->name = (uint8_t *)strdup((char *)token1->data.sym.name);
                    sym->adr = (asmadr_t)&op->args[0];
                    asmqueuesym(sym);
                    len += 4;
                    
                    break;
                case TOKENINDIR:
                    token1 = token1->next;
                    if (token1->type == TOKENREG) {
                        op->arg1t = ARGREG;
                        op->reg1 = token1->data.reg;
                    } else {
                        fprintf(stderr, "indirect addressing requires a register\n");

                        exit(1);
                    }
                    
                    break;
                case TOKENIMMED:
                    op->arg1t = ARGIMMED;
                    op->args[0] = token1->val;
                    len += 4;
                    
                    break;
                case TOKENADR:
                    op->arg1t = ARGIMMED;
                    sym = malloc(sizeof(struct sym));
                    sym->name = (uint8_t *)strdup((char *)token1->data.sym.name);
                    sym->adr = (asmadr_t)&op->args[0];
                    asmqueuesym(sym);
                    len += 4;

                    break;
                case TOKENINDEX:
                    op->arg1t = ARGREG;
                    op->reg1 = token1->data.ndx.reg;
                    op->args[0] = token1->data.ndx.val;
                    len += 4;

                    break;
                default:
                    fprintf(stderr, "invalid argument 1 of type %lx\n", token1->type);
                    printtoken(token1);

                    exit(1);

                    break;
            }
            token2 = token1->next;
            asmfreetoken(token1);
            retval = token2;
        }
        if (narg == 1) {
            op->arg2t = ARGNONE;
#if 0
#if (ZPC)
            op->reg2 = ZPCNREG;
#elif (WPM)
            op->reg2 = NREG;
#endif
#endif
        } else if (narg == 2 && (token2)) {
            switch(token2->type) {
                case TOKENVALUE:
                    op->arg2t = ARGIMMED;
#if (ZPC)
                    if (op->arg1t == ARGREG) {
                        op->args[0] = token2->data.token->data.ui64.i64;
                    } else {
                        op->args[1] = token2->data.token->data.ui64.i64;
                    }
#elif (WPM)
                    if (op->arg1t == ARGREG) {
                        op->args[0] = token2->data.value.val;
                    } else {
                        op->args[1] = token2->data.value.val;
                    }
#endif
                    len += sizeof(asmword_t);

                    break;
                case TOKENREG:
                    op->arg2t = ARGREG;
                    op->reg2 = token2->data.reg;
                    
                    break;
                case TOKENSYM:
                    op->arg2t = ARGADR;
                    sym = malloc(sizeof(struct sym));
                    sym->name = (uint8_t *)strdup((char *)token2->data.sym.name);
                    if (op->arg1t == ARGREG) {
                        sym->adr = (asmadr_t)&op->args[0];
                    } else {
                        sym->adr = (asmadr_t)&op->args[1];
                    }
                    asmqueuesym(sym);
                    len += 4;
                    
                    break;
                case TOKENINDIR:
                    token2 = token2->next;
                    if (token2->type == TOKENREG) {
                        op->arg2t = ARGREG;
                        op->reg2 = token2->data.reg;
                    } else {
                        fprintf(stderr, "indirect addressing requires a register\n");

                        exit(1);
                    }
                    
                    break;
                case TOKENIMMED:
                    op->arg2t = ARGIMMED;
                    if (op->arg1t == ARGREG) {
                        op->args[0] = token2->val;
                    } else {
                        op->args[1] = token2->val;
                    }
                    len += 4;
                    
                        break;
                case TOKENADR:
                    op->arg2t = ARGIMMED;
                    sym = malloc(sizeof(struct sym));
                    sym->name = (uint8_t *)strdup((char *)token2->data.sym.name);
                    if (op->arg1t == ARGREG) {
                        sym->adr = (asmadr_t)&op->args[0];
                    } else {
                        sym->adr = (asmadr_t)&op->args[1];
                    }
                    asmqueuesym(sym);
                    len += 4;

                    break;
                case TOKENINDEX:
                    op->arg2t = ARGREG;
                    op->reg2 = token2->data.ndx.reg;
                    if (op->arg1t == ARGREG) {
                        op->args[0] = token2->data.ndx.val;
                    } else {
                        op->args[1] = token2->data.ndx.val;
                    }
                    len += 4;

                    break;
                default:
                    fprintf(stderr, "invalid argument 2 of type %lx\n", token2->type);
                    printtoken(token2);

                    exit(1);

                    break;
            }
            retval = token2->next;
            asmfreetoken(token2);
        }
    }
    op->size = len >> 2;
    *retadr = adr + len;

    return retval;
}

static struct asmtoken *
asmprocchar(struct asmtoken *token, asmadr_t adr,
            asmadr_t *retadr)
{
    uint8_t         *valptr = &physmem[adr];
    struct asmtoken *retval;
    
    *valptr = token->data.ch;
    adr++;
    *retadr = adr;
    retval = token->next;
    asmfreetoken(token);

    return retval;
}

static struct asmtoken *
asmprocdata(struct asmtoken *token, asmadr_t adr,
            asmadr_t *retadr)
{
    struct asmtoken *token1 = token->next;
    asmadr_t         valadr = adr;

    while ((token1) && token1->type == TOKENVALUE) {
        token1 = asmprocvalue(token1, valadr, &valadr);
    }
    *retadr = valadr;

    return token1;
}

static struct asmtoken *
asmprocglobl(struct asmtoken *token, asmadr_t adr,
             asmadr_t *retadr)
{
    struct asmtoken *token1;
    struct label    *label;

    token1 = token->next;
    while ((token1) && token1->type == TOKENSYM) {
        token1 = asmproclabel(token1, adr, &adr);
        label = malloc(sizeof(label));
        label->name = (uint8_t *)strdup((char *)token1->data.label.name);
        label->adr = adr;
        asmaddglob(label);
    }
    *retadr = adr;

    return token1;
}

static struct asmtoken *
asmprocspace(struct asmtoken *token, asmadr_t adr,
             asmadr_t *retadr)
{
    struct asmtoken *token1;
    struct asmtoken *token2;
    asmadr_t         spcadr;
    uint8_t         *ptr;
    uint8_t          val;

    token1 = token->next;
    if ((token1) && token1->type == TOKENVALUE) {
#if (ZPC)
        spcadr = token1->data.token->data.ui64.u64;
#elif (WPM)
        spcadr = token1->data.value.val;
#endif
        token2 = token1->next;
        if ((token2) && token2->type == TOKENVALUE) {
            ptr = &physmem[spcadr];
#if (ZPC)
            val = token2->data.token->data.ui64.u64;
#elif (WPM)
            val = token2->data.value.val;
#endif
            while (adr < spcadr) {
                ptr[0] = val;
                adr++;
                ptr++;
            }
            token1 = token2->next;
        } else {
            adr = spcadr;
        }
    } else {
        fprintf(stderr, "invalid .space attribute token type %lx\n",
                token1->type);
        
        exit(1);
    }
    *retadr = adr;
    
    return token1;
}

static struct asmtoken *
asmprocorg(struct asmtoken *token, asmadr_t adr,
           asmadr_t *retadr)
{
    struct asmtoken *token1;
    asmadr_t         orgadr;
    uint8_t         *ptr;
    uint8_t          val;
    
    token1 = token->next;
    if ((token1) && token1->type == TOKENVALUE) {
        ptr = &physmem[adr];
#if (ZPC)
        orgadr = token1->data.token->data.ui64.u64;
        val = orgadr;
#elif (WPM)
        orgadr = token1->data.value.val;
        val = token1->data.value.val;
#endif
        while (adr < orgadr) {
            *ptr++ = val;
            adr++;
        }
        *retadr = adr;
    }
    
    return token1;
}

static struct asmtoken *
asmprocalign(struct asmtoken *token, asmadr_t adr,
             asmadr_t *retadr)
{
    struct asmtoken *token1;

    token1 = token->next;
    if ((token1) && token1->type == TOKENVALUE) {
#if (ZPC)
        adr = roundup2(adr, token1->data.token->data.ui64.u64);
#elif (WPM)
        adr = roundup2(adr, token1->data.value.val);
#endif
    } else {
        fprintf(stderr, "invalid .align attribute token type %lx\n",
                token1->type);

        exit(1);
     }
    *retadr = adr;

    return token1;
}

static struct asmtoken *
asmprocasciz(struct asmtoken *token, asmadr_t adr,
             asmadr_t *retadr)
{
    struct asmtoken *token1;
    struct asmtoken *token2;
    long             len = 0;
    uint8_t         *ptr;
    uint8_t         *str;

    token1 = token->next;
    while ((token1) && token1->type == TOKENSTRING) {
        ptr = &physmem[adr];
        str = token1->data.str;
        while ((*str) && *str != '\"') {
            if (*str == '\\') {
                str++;
                switch (*str) {
                    case 'n':
                        *ptr++ = '\n';
                        str++;
                        
                        break;
                    case 't':
                        *ptr++ = '\t';
                        str++;
                    
                        break;
                    case 'r':
                        *ptr++ = '\r';
                        str++;
                        
                        break;
                    default:
                        fprintf(stderr, "invalid character literal: \'%s\n", str);

                        exit(1);
                }
            } else {
                *ptr++ = *str++;
            }
            len++;
        }
        if (*str == '\"') {
            *ptr = '\0';
            len++;
        }
        adr += len;
        token2 = token1;
        token1 = token1->next;
        asmfreetoken(token2);
     }
    *retadr = adr;

    return token1;
}

void
asminit(void)
{
#if (ASMPREPROC)
    asminitexpr();
#endif
    asminitop();
    asminitbuf();
}

asmadr_t
asmtranslate(asmadr_t base)
{
    asmadr_t         adr = base;
    struct asmtoken *token = tokenqueue;
    tokfunc_t       *func;

    if (tokenqueue) {
        inputread = 1;
    }
    while (token) {
        func = tokfunctab[token->type];
        if (func) {
            token = func(token, adr, &adr);
            if (!token) {

                break;
            }
        } else {
            fprintf(stderr, "stray token of type %lx\n", token->type);

            exit(1);
        }
    }
    tokenqueue = NULL;

    return adr;
}

void
asmresolve(asmadr_t base)
{
    struct asmsym *sym = symqueue;
    struct asmsym *sym1;
    struct asmsym *item;
    struct label  *label;

    while (sym) {
        if (sym->adr == RESOLVE) {
            fprintf(stderr, "unresolved symbol %s\n", sym->name);

            exit(1);
        }
        item = asmfindsym(sym->name);
        if (item) {
            if (item->adr == RESOLVE) {
                fprintf(stderr, "invalid symbol %s\n", item->name);

                exit(1);
            }
            *((asmadr_t *)sym->adr) = item->adr;
        } else {
            label = asmfindglob(sym->name);
            if (label) {
                *((asmadr_t *)sym->adr) = label->adr;
            } else {
                fprintf(stderr, "unresolved symbol %s\n", sym->name);
                
                exit(1);
            }
        }
        sym1 = sym;
        sym = sym->next;
        free(sym1);
    }
    symqueue = NULL;

    return;
}

#if (ASMBUF)
void
asmreadfile(char *name, asmadr_t adr, int bufid)
#else
void
asmreadfile(char *name, asmadr_t adr)
#endif
{
    long             buflen = LINELEN;
#if (ASMBUF)
    int              fd = open((const char *)name, O_RDONLY);
#else
    FILE            *fp = fopen((char *)name, "r");
#endif
    long             eof = 0;
    struct asmtoken *token = NULL;
    struct val      *def;
    uint8_t         *fname;
    uint8_t         *ptr;
    uint8_t         *str = linebuf;
    uint8_t         *lim = NULL;
    long             loop = 1;
    int              ch;
    long             comm = 0;
    long             done = 1;
    long             len = 0;
#if (ASMDB)
    unsigned long    line = 0;
#endif
    asmword_t        val = 0;

    while (loop) {
        if (done) {
            if (eof) {
                loop = 0;
//                done = 0;

                break;
            }
            str = linebuf;
            done = 0;
            len = 0;
#if (ASMBUF)
            ch = asmgetc(fd, bufid);
#else
            ch = fgetc(fp);
#endif
            if (ch == EOF) {
                loop = 0;

                break;
            } else {
#if (ASMDB)
                line++;
#endif
                while (ch != EOF && ch != '\n') {
                    *str++ = (uint8_t)ch;
                    len++;
                    if (len == buflen) {
                        fprintf(stderr, "overlong line\n");
                        
                        exit(1);
                    }
#if (ASMBUF)
                    ch = asmgetc(fd, bufid);
#else
                    ch = fgetc(fp);
#endif
                    eof = (ch == EOF);
                }
                *str = '\0';
                str = linebuf;
                lim = str + len;
                while ((*str) && isspace(*str)) {
                    str++;
                }
                if (str > lim) {
                    done = 1;
                }
//                fprintf(stderr, "BUF: %s\n", str);
            }
        } else if (!strncmp((char *)str, ".define", 7)) {
            str += 7;
            while ((*str) && isspace(*str)) {
                str++;
            }
            if ((*str) && (isalpha(*str) || *str == '_')) {
                ptr = str;
                str++;
                while ((*str)
                       && (isalpha(*str) || *str == '_' || isdigit(*str))) {
                    str++;
                }
                *str++ = '\0';
                while ((*str) && isspace(*str)) {
                    str++;
                }
                if (asmgetvalue(str, &val, &str)) {
                    def = malloc(sizeof(struct val));
                    def->name = (uint8_t *)strdup((char *)ptr);
                    def->val = val;
                    asmaddval(def);
                } else {
                    fprintf(stderr, "invalid .define directive %s\n", ptr);
                    
                    exit(1);
                }
            }
        } else if (!strncmp((char *)str, ".include", 8)) {
            str += 8;
            while ((*str) && isspace(*str)) {
                str++;
            }
            if (*str == '<') {
                str++;
                fname = str;
                while ((*str) && *str != '>') {
                    str++;
                }
                if (*str == '>') {
                    *str = '\0';
#if (ASMBUF)
                    asmreadfile((char *)fname, adr, ++bufid);
                    bufid--;
#else
                    asmreadfile((char *)fname, adr);
#endif
                    asmresolve(adr);
                    asmremovesyms();
                } else {
                    fprintf(stderr, "invalid .include directive %s\n",
                            str);
                    
                    exit(1);
                }
            }
            done = 1;
        } else if (!strncmp((char *)str, ".import", 7)) {
            str += 7;
            while ((*str) && isspace(*str)) {
                str++;
            }
            if (*str == '<') {
                str++;
                fname = str;
                while ((*str) && *str != '>') {
                    str++;
                }
                if (*str == '>') {
                    *str = '\0';
#if (ASMBUF)
                    asmreadfile((char *)fname, adr, ++bufid);
                    bufid--;
#else
                    asmreadfile((char *)fname, adr);
#endif
                    asmresolve(adr);
                    asmremovesyms();
                } else {
                    fprintf(stderr, "invalid .import directive %s\n",
                            strbuf);

                    exit(1);
                }
            }
            done = 1;
        } else if (str[0] == ';'
                   || (str[0] == '/' && str[1] == '/')) {
            done = 1;
        } else if (str[0] == '/' && str[1] == '*') {
            /* comment */
            comm = 1;
            while (comm) {
#if (ASMBUF)
                ch = asmgetc(fd, bufid);
#else
                ch = fgetc(fp);
#endif
                if (ch == EOF) {
                    loop = 0;

                    break;
#if (ASMDB)
                } else if (ch == '\n') {
                    line++;
#endif
                } else if (ch == '*') {
#if (ASMBUF)
                    ch = asmgetc(fd, bufid);
#else
                    ch = fgetc(fp);
#endif
                    if (ch == '/') {
                        
                        comm = 0;
                    } else if (ch == EOF) {
                        comm = 0;
                        loop = 0;
                        eof = 1;
                    }
                }
            }
            done = 1;
        } else {
            if (*str) {
                token = asmgettoken(str, &str);
                if (token) {
#if (ASMDB)
                    token->file = (uint8_t *)strdup((char *)name);
                    token->line = line;
#endif
                    asmqueuetoken(token);
                }
                while (isspace(*str)) {
                    str++;
                }
                if (str >= lim) {
                    done = 1;
                }
            } else {
                done = 1;
            }
        }
    }
#if (ASMBUF)
    close(fd);
#else
    fclose(fp);
#endif

    return;
}

#if (WPM)
int
main(int argc, char *argv[])
{
    long      l;
    asmadr_t  adr = WPMTEXTBASE;
#if (ASMPROF)
    PROFTICK(tick);
#endif

    if (argc < 2) {
        fprintf(stderr, "usage: asm <file1> ...\n");

        exit(1);
    }
    asminit();
#if (WPM)
    wpminitmem(MEMSIZE);
    wpminit();
    memset(physmem, 0, WPMTEXTBASE);
#elif (ZPC)
#endif
    for (l = 1 ; l < argc ; l++) {
#if (ASMPROF)
        profstarttick(tick);
#endif
#if (ASMBUF)
        asmreadfile(argv[l], adr, readbufcur);
#else
        asmreadfile(argv[l], adr);
#endif
        if (!tokenqueue) {
            fprintf(stderr, "WARNING: no input in %s\n", argv[l]);
        } else {
            adr = asmtranslate(adr);
#if (WPM)
            asmresolve(WPMTEXTBASE);
#elif (ZPC)
            asmresolve(ZPCTEXTBASE);
#endif
            asmremovesyms();
#if (ASMPROF)
            profstoptick(tick);
            fprintf(stderr, "%lld cycles to process %s\n",
                    proftickdiff(tick), argv[l]);
#endif        
        }
    }
    if (!inputread) {
        fprintf(stderr, "empty input\n");

        exit(1);
    }
    wpminitthr(_start);
    pause();

    /* NOTREACHED */
    exit(0);
}
#endif /* WPM */

