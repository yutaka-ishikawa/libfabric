#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <ofi.h>
#include <ofi_mem.h>
#include "ulib_list.h"

#undef DLST_NEXT
#define DLST_NEXT(EP_,HP_,EN_,FN_) ( \
    ((EP_)->FN_.next == (HP_))? 0: \
	DLIST_X_ENTRY( (EP_)->FN_.next, EN_, FN_) \
    )


//#include "ulib_shea.h"

struct ulib_toqc_cash {
    char	dummy1[8];
    DLST_DECE(ulib_toqc_cash)   list;
    char	dummy2[8];
};

struct ulib_cash_fs {
    char	dummy1[8];
    DLST_DECE(ulib_toqc_cash)   list;
    char	dummy2[8];
};

DECLARE_FREESTACK(struct ulib_toqc_cash, ulib_desc_fs);
DLST_DECH(ulib_head_desc) myhead;

/*
 *  "cash_tmpl->list.next should be head ?? "
 */

void
sub() {
    DLST_DECH(ulib_head_desc) *head = &myhead;
    struct ulib_toqc_cash	*cash_tmpl;
    int	count = 3;
    
    cash_tmpl = DLST_PEEK(head, struct ulib_toqc_cash, list);
    while (cash_tmpl != 0 && count-- > 0) {
	printf("\tcash_tmpl(%p) cash_tmpl->list.next(%p)\n"
	       "\t  head(%p) head->next(%p)\n",
	       cash_tmpl, cash_tmpl->list.next,
	       head, head->next);
	cash_tmpl = DLST_NEXT(cash_tmpl, head, struct ulib_toqc_cash, list);
    }
    if(cash_tmpl == 0) {
	printf("OK\n");
    } else {
	printf("ERROR\n");
    }
}

int
main()
{
    DLST_DECH(ulib_head_desc)	*head = &myhead;
    struct ulib_toqc_cash	*cash_tmpl;
    struct ulib_desc_fs		*cash_fs;
    
    cash_fs = ulib_desc_fs_create(512, 0, 0);
    DLST_INIT(head);
    
    printf("No entry\n");
    sub();
    printf("DLST_INSH\n");
    cash_tmpl = freestack_pop(cash_fs);
    DLST_INSH(head, cash_tmpl, list);
    sub();

    printf("DLST_INSH->DLST_RMOV->DLST_INSH\n");
    DLST_RMOV(head, cash_tmpl, list);
    DLST_INSH(head, cash_tmpl, list);
    sub();
    
    printf("DLST_INSH->DLST_RMOV->DLST_INSH->DLST_INS\n");
    cash_tmpl = freestack_pop(cash_fs);
    DLST_INSH(head, cash_tmpl, list);
    sub();

    return 0;
}

/*
dlist_insert_after(struct dlist_entry *item, struct dlist_entry *head)
{
    item->next = head->next;
    item->prev = head;
    head->next->prev = item;
    head->next = item;
}

dlist_insert_after(&(cash_tmpl)->list,head);

cash_tmpl =
    (cash_tmpl->list.next == head)? 0
    : ((struct ulib_toqc_cash *) ((char *)&(cash_tmpl)->list.next -__builtin_offsetof (struct ulib_toqc_cash, list)));
*/
