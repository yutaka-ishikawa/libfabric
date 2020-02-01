#ifndef LIST_DEF
#define LIST_DEF

#define myoffsetof(t, m) ((size_t) &((t *)0)->m)
#define container_of(ptr, type, field) ((type *) ((char *)ptr - myoffsetof(type, field)))

typedef struct slist_entry {
    struct slist_entry	*next;
} slist_entry;

typedef struct slist {
    struct slist_entry	*head;
    struct slist_entry	*tail;
} slist;

static inline void
slist_init(slist *lst, slist_entry *ent)
{
    lst->head = lst->tail = ent;
}

static inline slist_entry *
slist_append(slist *lst, slist_entry *ent)
{
    slist_entry	*ohead;
    if ((ohead = lst->head)) {
	lst->tail->next = ent;
    } else { /* empty */
	lst->head = ent;
    }
    lst->tail = ent;
    ent->next = NULL;
    return ohead;
}

static inline void
slist_insert(slist *lst, slist_entry *ent)
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
slist_insert2(slist_entry *head, slist_entry *tail, slist_entry *ent)
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

static inline slist_entry *
slist_remove(slist *lst)
{
    slist_entry *ent = lst->head;
    if (lst->head == lst->tail) {
	slist_init(lst, 0);
    } else {
	lst->head = ent->next;
    }
    if (ent) ent->next = 0;
    return ent;
}

#define slistent_remove(prev, cur)   (prev)->next = (cur)->next
#define slistent_next(lst)	(lst)->next
#define slist_head(lst)		((lst)->head)
#define slist_isnull(lst)	((lst)->head == NULL)
#define slist_foreach(lst, cur)					\
	for ((cur) = (lst)->head; (cur) != NULL;		\
			(cur) = (cur)->next)
#define slist_foreach2(lst, cur, prev)				\
	for ((prev) = NULL, (cur) = (lst)->head; (cur) != NULL;	\
			(prev) = (cur), (cur) = (cur)->next)
#define slistent_foreach2(ini, cur, prev)			\
	    for ((cur) = (ini), (prev) = 0; (cur) != 0;		\
			(prev) = (cur), (cur) = (cur)->next)

typedef struct dlist {
    struct dlist	*next;
    struct dlist	*prev;
} dlist;

static inline void
dlist_init(dlist *lst)
{
    lst->next = lst->prev = lst;
}

static inline void
dlist_insert(dlist *head, dlist *ent)
{
    ent->next = head->next;
    ent->prev = head;
    head->next->prev = ent;
    head->next = ent;
}

static inline void
dlist_remove(dlist *ent)
{
    ent->prev->next = ent->next;
    ent->next->prev = ent->prev;
}


static inline dlist*
dlist_get(dlist *head)
{
    dlist	*dp;
    if (head == head->next) {
	return NULL;
    }
    dp = head->next;
    dlist_remove(dp);
    return dp;
}

#define dlist_foreach(head, ent) \
    for ((ent) = (head)->next; (ent) != (head); (ent) = (ent)->next)
#define dlist_foreach_rev(head, ent) \
    for ((ent) = (head)->prev; (ent) != (head); (ent) = (ent)->prev)

#endif /* ~LIST_DEF */
