#ifndef __IA32_STDATOMIC_H__
#define __IA32_STDATOMIC_H__

#define ATOMIC_BOOL_LOCK_FREE     2
#define ATOMIC_CHAR_LOCK_FREE     2
#define ATOMIC_CHAR16_T_LOCK_FREE 2
#define ATOMIC_CHAR32_T_LOCK_FREE 2
#define ATOMIC_WCHAR_T_LOCK_FREE  2
#define ATOMIC_SHORT_LOCK_FREE    2
#define ATOMIC_INT_LOCK_FREE      2
#define ATOMIC_LONG_LOCK_FREE     2
#define ATOMIC_LLONG_LOCK_FREE    0 /* FIXME: should this be 1 (sometimes)? */
#define ATOMIC_POINTER_LOCK_FREE  2

#endif /* __IA32_STDATOMIC_H__ */

