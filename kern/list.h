#ifndef __KERN_LIST_H__
#define __KERN_LIST_H__

#include <stddef.h>
#include <kern/macros.h>

/* list stuff based on what I found in Linux */

/* REFERENCE: http://lxr.free-electrons.com/source/include/linux/list.h */

struct listhead {
    struct listhead *prev;
    struct listhead *next;
};

/* initialise list */
#define LISTHEAD(name) { &(name), &(name) }
#define listinit(item)                                                  \
    ((item)->next = (item), (item)->prev = (item))
/* add item between prev and next */
#define __listadd(item, prev, next)                                     \
    ((next)->prev = (item),                                             \
     (item)->next = next,                                               \
     (item)->prev = (prev),                                             \
     (prev)->next = (item))
/* add item1 after item2 */
#define listaddafter(item1, item2)                                      \
    __listadd(item1, item2, (item2)->next)
/* add item1 before item2 */
#define listaddbefore(item1, item2)                                     \
    __listadd(item1, (item2)->prev, item2)
/* remove item from between prev and next */
#define listdelbetween(prev, next)                                      \
    ((next)->prev = prev, (prev)->next = next)
/* remove item */
#define listdel(item)                                                   \
    listdelbetween((item)->prev, (item)->next)
/* replace item1 with item2 */
#define listreplace(item1, item2)                                       \
    ((item2)->next = (item1)->next,                                     \
     (item2)->next->prev = (item2),                                     \
     (item2)->prev = (item1)->prev,                                     \
     (item2)->prev->next = (item2))
/* replace item1 with item2, initialise new list with item1 */
#define listreplaceinit(item1, item2)                                   \
    (listreplace(item1, item2), listinit(item1))
/* remove item1 from list, initialise new list with it */
#define listdelinit(item1)                                              \
    (listdel(item1), listinit(item1))
/* remove item1 from list, add it after another list's item2 */
#define listmoveafter(item1, item2)                                     \
    (listdel(item1), listaddafter(item1, item2))
/* remove item1 from list, add it before another list's item2 */
#define listmovebefore(item, item2)                                     \
    (listdel(item), listaddbefore(item, item2))
/* check if item is the last one on list */
#define listislast(item, list)                                          \
    ((item)->next == list)
/* check if list is empty */
#define listisempty(item)                                               \
    ((item)->next == item)
/* rotate list to the left */
#define listrotleft(item)                                               \
    do {                                                                \
        struct listhead *_first;                                        \
                                                                        \
        if (listisempty(item)) {                                        \
            _first = (item)->next;                                      \
            listmovebefore(first, item);                                \
        }                                                               \
    } while (0)
/* check if list has just one item */
#define listissingular(item)                                            \
    (listisempty(item) && (item)->next == (item)->prev)
#define __listcutpos(list, item, entry)                                 \
    do {                                                                \
        struct listhead *_first = (entry)->next;                        \
                                                                        \
        (list)->next = (item)->next;                                    \
        (list)->next->prev = list;                                      \
        (list)->prev = entry;                                           \
        (entry)->next = list;                                           \
        (item)->next = _first;                                          \
        _first->prev = item;                                            \
    } while (0)
/* cut list in two if entry != item */
#define listcutpos(list, item, entry)                                   \
    do {                                                                \
        if (!listisempty(item)                                          \
            && !(listissingular(item)                                   \
                 && ((item)->next != (entry) && (item) != (entry)))) {  \
            if ((item) == (entry)) {                                    \
                listinit(list);                                         \
            } else {                                                    \
                __listcutpos(list, item, entry);                        \
            }                                                           \
        } while (0)
#define __listsplice(list, prev, next)                                  \
    do {                                                                \
        struct listhead *_first = (list)->next;                         \
        struct listhead *_last = (list)->prev;                          \
                                                                        \
        _first->prev = prev;                                            \
        (prev)->next = _first;                                          \
        _last->next = next;                                             \
        (next)->prev = _last;                                           \
        } while (0)
/* join two lists by adding list after item */
#define listsplice(list, item)                                          \
    do {                                                                \
        if (!listisempty(list)) {                                       \
            __listsplice(list, item, (item)->next);                     \
        }                                                               \
    } while (0)
/* join two lists by adding list before item */
#define listsplicetail(list, item)                                      \
    do {                                                                \
        if (!listisempty(list)) {                                       \
            __listsplice(list, (item)->prev, item);                     \
        }                                                               \
    } while (0)
/* join two lists and reinitialise the emptied one */
#define listspliceinit(list, item)                                      \
    do {                                                                \
        if (!listisempty(list)) {                                       \
            __listsplice(list, item, (item)->next);                     \
            listinit(list);                                             \
        }                                                               \
    } while (0)
#define listsplicetailinit(list, item)                                  \
    do {                                                                \
        if (!listisempty(list)) {                                       \
            __listsplice(list, (item)->prev, item);                     \
            listinit(list);                                             \
        }                                                               \
    } while (0)
/* get struct for list entry */
#define listentry(ptr, type, member)                                    \
    container(ptr, type, member)
/* get first element from list */
#define listfirstentry(ptr, type, member)                               \
    listentry((ptr)->next, type, member)
/* get last element from list */
#define listlastentry(ptr, type, member)                                \
    listentry((ptr)->prev, type, member)
/* get first element from list or NULL if list is empty */
#define listfirstentryornull(ptr, type, member)                         \
    (!listempty(ptr) ? listfirst(ptr, type, member) : NULL)
/* get next element in list */
#define listnextentry(pos, member)                                      \
    listentry((pos)->member.next, typeof(*(pos)), member)
/* get next previous in list */
#define listpreventry(pos, member)                                      \
    listentry((pos)->member.prev, typeof(*(pos)), member)
/* iterate over a list */
#define listiter(pos, item)                                             \
    for (pos = (item)->next ; (pos) != (item) ; pos = (pos)->next)
/* iterate over a list backwards */
#define listiterrev(pos, item)                                          \
    for (pos = (item)->prev ; (pos) != (item) ; pos = (pos)->prev)
/* iterate over a list; entries won't be removed */
#define listitersafe(pos, tmp, item)                                    \
    for (pos = (item)->next, tmp = (pos)->next ;                        \
         (pos) != (item) ;                                              \
         (pos) = (tmp), (tmp) = (pos)->next)
#define listiterrevsafe(pos, tmp, item)                                 \
    for (pos = (item)->prev, tmp = (pos)->prev ;                        \
         (pos) != (item) ;                                              \
         (pos) = (tmp), (tmp) = (pos)->prev)
#define listiterentry(pos, item, member)                                \
    for (pos = listfirst(item, typeof(*(pos)), member) ;                \
         &(pos)->member != (item) ;                                     \
         pos = listnextentry(pos, member))
#define listiterentryrev(pos, item, member)                             \
    for (pos = listlast(item, typeof(*(pos)), member) ;                 \
         &(pos)->member != (item) ;                                     \
         pos = listpreventry(pos, member))
/* prepare pos entry for listitercont */
#define listprepentry(pos, item, member)                                \
    ((pos) ? listentry(item, typeof(*(pos)), member))
#define listiterentrycont(pos, item, member)                            \
    for (pos = listnextentry(pos, member) ;                             \
         &(pos)->member != (item) ;                                     \
         pos = listnextentry(pos, member))
#define listiterentrycontrev(pos, item, member)                         \
    for (pos = listpreventry(pos, member) ;                             \
         &(pos)->member != (item) ;                                     \
         pos = listpreventry(pos, member))
#define listiterentryfrom(pos, item, member)                            \
    for ( ;                                                             \
          &(pos)->member != (item) ;                                    \
          pos = listnextentry(item))
/* entries won't be removed */
#define listiterentrysafe(pos, tmp, item, member)                       \
    for (pos = listfirstentry(item, typeof(*(pos)), member),            \
             tmp = listnextentry(pos, member) ;                         \
         &(pos)->member != (head) ;                                     \
         pos = tmp, tmp = listnextentry(tmp, member))
#define listiterentrysafecont(pos, tmp, item, member)                   \
    for (pos = listnexttentry(item, typeof(*(pos)), member),            \
             tmp = listnextentry(pos, member) ;                         \
         &(pos)->member != (head) ;                                     \
         pos = tmp, tmp = listnextentry(tmp, member))
#define listiterentrysafefrom(pos, tmp, head, member)                   \
    for (tmp = listnextentry(pos, member) ;                             \
         &(pos)->member != (head) ;\
         pos = tmp, tmp = listnextentry(tmp, member))
#define listiterentrysaferev(pos, tmp, item, member)                    \
    for (pos = listlasttentry(item, typeof(*(pos)), member),            \
             tmp = listpreventry(pos, member) ;                         \
         &(pos)->member != (head) ;                                     \
         pos = tmp, tmp = listpreventry(tmp, member))
/* reset stale list for safe loops */
#define listsaferesetnext(pos, tmp, member)                             \
    (tmp = listnextentry(pos, member))

#endif /* __KERN_LIST_H__ */

