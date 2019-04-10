/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef _ULIB_LIST_H
#define _ULIB_LIST_H

#include "ulib_conf.h"

#if	defined(CONF_ULIB_LIST_TAILQ)

#include <stddef.h>		    /* for NULL in sys/queue.h */
#include <sys/queue.h>

#define DLST_DECE(EN_)		    TAILQ_ENTRY(EN_)
#define DLST_DEFH(HN_,EN_)	    TAILQ_HEAD(HN_,EN_)
#define DLST_DECH(HN_)		    struct HN_
#define DLST_INIT(HP_)		    TAILQ_INIT(HP_)
#define DLST_EMPT(HP_)		    TAILQ_EMPTY(HP_)
#define DLST_INSH(HP_,EP_,FN_)	    TAILQ_INSERT_HEAD(HP_,EP_,FN_)
#define DLST_INST(HP_,EP_,FN_)	    TAILQ_INSERT_TAIL(HP_,EP_,FN_)
#define DLST_RMOV(HP_,EP_,FN_)	    TAILQ_REMOVE(HP_,EP_,FN_)
#define DLST_PEEK(HP_,EP_,FN_)	    TAILQ_FIRST(HP_)
#define DLST_LAST(HP_,HN_,EP_,FN_)  TAILQ_LAST(HP_,HN_)
#define DLST_NEXT(EP_,FP_,EN_,FN_)  TAILQ_NEXT(EP_, FN_)

#define DLST_ISCLR(EP_,FN_)	    ((EP_)->FN_.tqe_prev == 0)
#define DLST_FLCLR(EP_,FN_)	    (EP_)->FN_.tqe_prev = 0

#elif   defined(CONF_ULIB_LIST_DLIST)

#ifndef container_of
#include <stddef.h> /* for offsetof() */
#define container_of(ptr, type, field) \
	    ((type *) ((char *)ptr - offsetof(type, field)))
#endif

#include "ofi_list.h"

#define DLST_DECE(EN_)		    struct dlist_entry
#define DLST_DEFH(HN_,EN_)	    struct dlist_entry
#define DLST_DECH(HN_)		    struct dlist_entry
#define DLST_INIT(HP_)		    dlist_init(HP_)
#define DLST_EMPT(HP_)		    dlist_empty(HP_)
#define DLST_INSH(HP_,EP_,FN_)	    dlist_insert_head(&(EP_)->FN_,HP_)
#define DLST_INST(HP_,EP_,FN_)	    dlist_insert_tail(&(EP_)->FN_,HP_)
#define DLST_RMOV(HP_,EP_,FN_)	    dlist_remove(&(EP_)->FN_)

#define DLIST_X_FIRST(HP_)	    (dlist_empty(HP_)? 0: (HP_)->next)
#define DLIST_X_LAST(HP_)	    (dlist_empty(HP_)? 0: (HP_)->prev)
/* (field pointer, entry type, field name) */
/* container_of(list, type, field) */
#define DLIST_X_ENTRY(FP_,EN_,FN_)  container_of(FP_,EN_,FN_)

#define DLST_PEEK(HP_,EN_,FN_) ( \
    dlist_empty(HP_)? 0: \
	DLIST_X_ENTRY( DLIST_X_FIRST(HP_), EN_, FN_) \
    )
#define DLST_LAST(HP_,HN_,EN_,FN_) ( \
    dlist_empty(HP_)? 0: \
	DLIST_X_ENTRY( DLIST_X_LAST(HP_), EN_, FN_) \
    )
#define DLST_NEXT(EP_,HP_,EN_,FN_) ( \
    ((EP_)->FN_.next == (HP_))? 0: \
	DLIST_X_ENTRY( &(EP_)->FN_.next, EN_, FN_) \
    )

#define DLST_ISCLR(EP_,FN_)	    ((EP_)->FN_.prev == 0)
#define DLST_FLCLR(EP_,FN_)	    (EP_)->FN_.prev = 0

#else

#error  "use CONF_ULIB_LIST_{TAILQ,DLIST}"

#endif  /* defined(CONF_ULIB_LIST_TAILQ) */

#endif	/* _ULIB_LIST_H */
