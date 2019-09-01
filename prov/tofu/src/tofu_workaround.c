/*
 * Workaround for Tofu ulib on tlib bugs.
 * To use this workaround,
 *	the TOFU_SIM_BUG flag must be defined in tofu_conf.h
 */
#include "tofu_conf.h"
#include "tofu_debug.h"
#include "ulib_conf.h"
#include "utofu.h"
#include <ofi_list.h>

#define WA_EVENT_SIZE	128

struct wa_event {
    struct slist_entry	sent;
    void		*cbdata;
    size_t		desc_size;
};

struct wa_event	wa_event[WA_EVENT_SIZE];
struct slist	wa_evfreelst;
struct slist	wa_evlst;

/*
 * slist_insert_head(struct slist_entry *item, struct slist *list)
 * slist_insert_tail(struct slist_entry *item, struct slist *list)
 * slist_remove_head(struct slist *list)
 */

void
wa_init()
{
    int	i;

    slist_init(&wa_evfreelst);
    slist_init(&wa_evlst);
    for (i = 0; i < WA_EVENT_SIZE; i++) {
	slist_insert_tail(&wa_event[i].sent, &wa_evfreelst);
    }
}


int
wa_utofu_post_toq(utofu_vcq_hdl_t  vcqh, void *desc,
	       size_t desc_size, void *cbdata)
{
    int	rc;
    struct wa_event	*we;

    we = (struct wa_event*) slist_remove_head(&wa_evfreelst);
    if (we == 0) {
	fprintf(stderr, "No more event free list for workarround\n");
	fflush(stderr);
	exit(-1);
    }
    we->cbdata = cbdata;
    we->desc_size = desc_size;
    // printf("wa_utofu_post_toq: we(%p) cbdata(%p) desc_size(%ld)\n", we, we->cbdata, we->desc_size); fflush(stdout);
    slist_insert_tail((struct slist_entry*)we, &wa_evlst);
    rc = utofu_post_toq(vcqh, desc, desc_size, cbdata);
    return rc;
}

int
wa_utofu_poll_tcq(utofu_vcq_hdl_t vcqh, unsigned long int   flags,
		  void **cbdata)
{
    int	rc;
    struct wa_event	*we;

    /* extract request entry */
    we =  (struct wa_event*) slist_remove_head(&wa_evlst);
    if (we == NULL) {
	// printf("wa_utofu_poll_tqc: UTOFU_ERR_NOT_FOUND\n"); fflush(stdout);
	return UTOFU_ERR_NOT_FOUND;
    }
    rc = utofu_poll_tcq(vcqh, flags, cbdata);
    // printf("wa_utofu_poll_tcq: we(%p) cbdata(%p) utofu-return-cbdata(%p) desc_size(%ld)\n", we, we->cbdata, *cbdata, we->desc_size); fflush(stdout);
    if (we->desc_size > 32) {
	/* one more */
	rc = utofu_poll_tcq(vcqh, flags, cbdata);
	// printf("\t one more: cbdata(%p)\n", cbdata); fflush(stdout);
    }
    *cbdata = we->cbdata;
    // printf("wa_utofu_poll_tqc: return cbdata (%p)\n", *cbdata); fflush(stdout);
    /* push back the entry to free list */
    slist_insert_tail(&we->sent, &wa_evfreelst);
    return rc;
}
