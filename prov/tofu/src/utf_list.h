#ifndef LIST_DEF
#define LIST_DEF

#define myoffsetof(t, m) ((size_t) &((t *)0)->m)
#ifdef container_of
#undef container_of
#endif
#define container_of(ptr, type, field) ((type *) ((char *)ptr - myoffsetof(type, field)))

typedef struct utfslist_entry {
    struct utfslist_entry	*next;
} utfslist_entry;

typedef struct utfslist {
    struct utfslist_entry	*head;
    struct utfslist_entry	*tail;
} utfslist;

static inline void
utfslist_init(utfslist *lst, utfslist_entry *ent)
{
    lst->head = lst->tail = ent;
}

static inline utfslist_entry *
utfslist_append(utfslist *lst, utfslist_entry *ent)
{
    utfslist_entry	*ohead = lst->head;
    if (lst->tail) {
	lst->tail->next = ent;
    } else { /* empty */
	lst->head = ent;
    }
    lst->tail = ent;
    ent->next = NULL;
    /* ohead still points to the next entry */
    return ohead;
}

static inline void
utfslist_insert(utfslist *lst, utfslist_entry *ent)
{
    if (lst->head) {
	ent->next = lst->head;
    } else { /* empty */
	lst->tail = ent;
	ent->next = NULL;
    }
    lst->head = ent;
}

static inline void
utfslist_insert2(utfslist_entry *head, utfslist_entry *tail, utfslist_entry *ent)
{
    ent->next = NULL;
    if (head->next) {
	if (tail->next) {
	    tail->next->next = ent;
	} else {
	    tail->next = ent;
	}
    } else {
	head->next = tail->next = ent;
    }
}

static inline utfslist_entry *
utfslist_remove(utfslist *lst)
{
    utfslist_entry *ent = lst->head;
    if (lst->head == lst->tail) {
	utfslist_init(lst, 0);
    } else {
	lst->head = ent->next;
    }
    if (ent) ent->next = 0;
    return ent;
}

static inline utfslist_entry *
utfslist_remove2(utfslist *lst, utfslist_entry *cur, utfslist_entry *prev)
{
    //utfslist_entry *ent = lst->head;
    if (lst->head == cur) {
	lst->head = cur->next;
	if (lst->head == NULL) lst->tail = NULL;
    } else if (lst->tail == cur) {
	/* cur is the tail means no successor */
	lst->tail = prev;
    }
    if (prev) prev->next = cur->next;
    return cur->next;
}

#ifdef USE_UTFSLIST_NEXT
static utfslist_entry *
utfslist_next(utfslist *lst)
{
    if (lst->head) {
	return lst->head->next;
    } else {
	return NULL;
    }
}
#endif

#define utfslistent_remove(prev, cur)   (prev)->next = (cur)->next
#define utfslistent_next(lst)	(lst)->next
#define utfslist_head(lst)		((lst)->head)
#define utfslist_isnull(lst)	((lst)->head == NULL)
#define utfslist_foreach(lst, cur)					\
	for ((cur) = (lst)->head; (cur) != NULL;		\
			(cur) = (cur)->next)
#define utfslist_foreach2(lst, cur, prev)				\
	for ((prev) = NULL, (cur) = (lst)->head; (cur) != NULL;	\
			(prev) = (cur), (cur) = (cur)->next)
#define utfslistent_foreach2(ini, cur, prev)			\
	    for ((cur) = (ini), (prev) = 0; (cur) != 0;		\
			(prev) = (cur), (cur) = (cur)->next)

typedef struct utfdlist {
    struct utfdlist	*next;
    struct utfdlist	*prev;
} utfdlist;

static inline void
utfdlist_init(utfdlist *lst)
{
    lst->next = lst->prev = lst;
}

static inline void
utfdlist_insert(utfdlist *head, utfdlist *ent)
{
    ent->next = head->next;
    ent->prev = head;
    head->next->prev = ent;
    head->next = ent;
}

static inline void
utfdlist_remove(utfdlist *ent)
{
    ent->prev->next = ent->next;
    ent->next->prev = ent->prev;
}


static inline utfdlist*
utfdlist_get(utfdlist *head)
{
    utfdlist	*dp;
    if (head == head->next) {
	return NULL;
    }
    dp = head->next;
    utfdlist_remove(dp);
    return dp;
}

#define utfdlist_foreach(head, ent) \
    for ((ent) = (head)->next; (ent) != (head); (ent) = (ent)->next)
#define utfdlist_foreach_rev(head, ent) \
    for ((ent) = (head)->prev; (ent) != (head); (ent) = (ent)->prev)

#endif /* ~LIST_DEF */
