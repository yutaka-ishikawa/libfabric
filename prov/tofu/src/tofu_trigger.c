/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdint.h>	/* for uint64_t */
#include <assert.h>	/* for assert() */

#define TOFU_PCOL_INIT	    (UINT64_MAX - 0)
#define TOFU_PCOL_START	    (UINT64_MAX - 1)
#define TOFU_PCOL_FREE	    (UINT64_MAX - 2)

#define TOFU_PCOL_MAX_WORKS 2048

static int  tofu_pcol_work(struct tofu_domain *domain,
			   struct fi_deferred_work *dw);
static int  tofu_pcol_fidw2toof(struct fi_deferred_work *dw, size_t nw,
				fi_addr_t my_rxa);
	    

int tofu_queue_work(struct tofu_domain *domain, void *vp_dw)
{
    int rc = FI_SUCCESS, el = 0;
    struct fi_deferred_work *dw = vp_dw;

    if ((dw->op_type == FI_OP_CNTR_ADD)
	&& (dw->threshold == UINT64_MAX)
	&& (dw->completion_cntr == 0)
	&& (dw->op.cntr != 0)
	&& (dw->op.cntr->value == 0)) {
	rc = tofu_pcol_work(domain, dw);
	if (rc != 0) { el = __LINE__; goto bad; }
    } else {
	rc = -FI_ENOSYS;
	el = __LINE__; goto bad;
    }

bad:
    if (el != 0) {
	int rc_ = (rc < 0)? -rc: rc;
	const char *ep_ = fi_strerror(rc_);
	if (ep_ == 0) { ep_ = "(unknown)"; }
	fprintf(stderr, "OFI-ERROR %s (%d)\t%s:%s:%d\n",
	    ep_, rc_, __func__, __FILE__, el);
	fflush(stderr);
    }
    return rc;
}

static int tofu_ep_check_offloadable(struct tofu_domain *domain,
				     struct fid_ep *ep,
				     struct fid_cntr *fx_cntr,
				     struct fid_cntr *rx_cntr,
				     fi_addr_t *p_rx_addr)
{
    int rc = FI_SUCCESS, el = 0;

    switch (ep->fid.fclass) {
    case FI_CLASS_TX_CTX:
	break;
    case FI_CLASS_EP:
	rc = -FI_ENOSYS;
	el = __LINE__; goto bad;
    case FI_CLASS_RX_CTX:
    case FI_CLASS_SEP:
	rc = -FI_EINVAL;
	el = __LINE__; goto bad;
    }

    /* XXX */
    p_rx_addr[0] = FI_ADDR_NOTAVAIL;

bad:
    if (el != 0) { }
    return rc;
}

static int
tofu_pcol_work(struct tofu_domain *domain,
	       struct fi_deferred_work *dw)
{
    int rc = FI_SUCCESS, el = 0;
    uintptr_t subcommand = (uintptr_t)dw->context.internal[0];

    /*
     * TOFU_PCOL_INIT
     *   context.internal[1] IN     # of persistent works - 1
     *   context.internal[2] IN     initial threshold
     *   context.internal[7] OUT    internal control structure
     * TOFU_PCOL_START
     *   context.internal[3] IN/OUT pointer to fi_deferred_work
     *   context.internal[7] IN     internal control structure
     * TOFU_PCOL_FREE
     *   context.internal[7] IN     internal control structure
     *
     * struct fi_deferred_work {
     *   struct fid_cntr *cntr_comp;
     *   uint64_t threshold_base;
     *   uint64_t threshold_comp;
     * };
     */
    switch (subcommand) {
	size_t iw, nw;
	struct fid_ep *ep;
	struct fid_cntr *tx_cntr, *rx_cntr;
	fi_addr_t my_rxa;
    case TOFU_PCOL_INIT:
	tx_cntr = dw->op.cntr->cntr; /* completion_cntr */
	rx_cntr = dw->triggering_cntr;
	if ((tx_cntr == 0) || (rx_cntr == 0)) {
	    rc = -FI_EINVAL; el = __LINE__; goto bad;
	}
	nw = (size_t)(uintptr_t)dw->context.internal[1];
	if (nw < 1) {
	    rc = -FI_ENOMSG; el = __LINE__; goto bad;
	}
	else if (nw > TOFU_PCOL_MAX_WORKS) {
	    /* -FI_ENOSPC ? */
	    rc = -FI_EOVERRUN; el = __LINE__; goto bad;
	}
	ep = 0;
	for (iw = 0; iw < nw; iw++) {
	    struct fi_deferred_work *pw = &dw[iw + 1]; /* persistent work */

	    switch (pw->op_type) {
	    case FI_OP_WRITE:
		if (pw->triggering_cntr != rx_cntr) {
		    rc = -FI_EPERM; el = __LINE__; goto bad;
		}
		if (pw->completion_cntr != tx_cntr) {
		    rc = -FI_EPERM; el = __LINE__; goto bad;
		}

		if ((pw->op.rma == 0)
		    || (pw->op.rma->ep == 0)
		    || (pw->op.rma->flags != 0)) {
		    rc = -FI_EINVAL; el = __LINE__; goto bad;
		}
		if (ep == 0) {
		    ep = pw->op.rma->ep;
		} else if (pw->op.rma->ep != ep) {
		    rc = -FI_EPERM; el = __LINE__; goto bad;
		}
		{
		    struct fi_msg_rma *rma = &pw->op.rma->msg;

		    if ( 0
			|| (rma->msg_iov == 0)
			|| (rma->iov_count == 0)
			|| (rma->rma_iov == 0)
			|| (rma->rma_iov_count == 0)
		    ) {
			rc = -FI_EINVAL; el = __LINE__; goto bad;
		    }
		    if ( 0
			/* || (rma->desc == 0) */
			/* || (rma->context != &pw->context) */
		    ) {
			rc = -FI_EINVAL; el = __LINE__; goto bad;
		    }
		}
		break;
	    case FI_OP_READ:
	    case FI_OP_ATOMIC:
	    default:
		rc = -FI_EOPNOTSUPP; el = __LINE__; goto bad;
	    }
	}
	rc = tofu_ep_check_offloadable(domain, ep,
		tx_cntr, rx_cntr, &my_rxa);
	if (rc != 0) { el = __LINE__; goto bad; }

	rc = tofu_pcol_fidw2toof( &dw[1], nw, my_rxa );
	if (rc != 0) { el = __LINE__; goto bad; }

	if (0) {
	    size_t initial_threshold;

	    initial_threshold = (size_t)(uintptr_t)dw->context.internal[2];
	    if (initial_threshold > TOFU_PCOL_MAX_WORKS) {
	    }
	}
	break;
    case TOFU_PCOL_START:
	break;
    case TOFU_PCOL_FREE:
	break;
    default:
	rc = -FI_EINVAL;
	el = __LINE__; goto bad;
    }

bad:
    if (el != 0) {
	int rc_ = (rc < 0)? -rc: rc;
	const char *ep_ = fi_strerror(rc_);
	if (ep_ == 0) { ep_ = "(unknown)"; }
	fprintf(stderr, "OFI-ERROR %s (%d)\t%s:%s:%d\n",
	    ep_, rc_, __func__, __FILE__, el);
	fflush(stderr);
    }
    return rc;
}

/* control message : FI_OP_WRITE with zero-length data */
#define TOFU_DW_IS_CMSG(W0)	    ( 1 \
	    && ((W0)->op_type == FI_OP_WRITE) \
	    && ((W0)->op.rma->msg.iov_count == 1) \
	    && ((W0)->op.rma->msg.msg_iov[0].iov_len == 0) \
	    && ((W0)->op.rma->msg.rma_iov_count == 1) \
	    && ((W0)->op.rma->msg.rma_iov[0].len == 0) \
	    && ((W0)->op.rma->flags == 0) \
	    )

/* same control message ? */
#define TOFU_DW_SAME_CMSG(W1,W2)    ( 1 \
	    && ((W1)->threshold == (W2)->threshold) \
	    && ((W1)->triggering_cntr == (W2)->triggering_cntr) \
	    && ((W1)->completion_cntr == (W2)->completion_cntr) \
	    && ((W1)->op_type == (W2)->op_type) \
	    \
	    && ((W1)->op.rma->ep == (W2)->op.rma->ep) \
	    \
	    && ((W1)->op.rma->msg.iov_count == 1) \
	    && ((W1)->op.rma->msg.msg_iov[0].iov_len == 0) \
	    && ((W1)->op.rma->msg.rma_iov_count == 1) \
	    && ((W1)->op.rma->msg.rma_iov[0].len == 0) \
	    \
	    && ((W2)->op.rma->msg.iov_count == 1) \
	    && ((W2)->op.rma->msg.msg_iov[0].iov_len == 0) \
	    && ((W2)->op.rma->msg.rma_iov_count == 1) \
	    && ((W2)->op.rma->msg.rma_iov[0].len == 0) \
	    \
	    && ((W1)->op.rma->msg.addr == (W2)->op.rma->msg.addr) \
	    /* && ((W1)->op.rma->msg.data == (W2)->op.rma->msg.data) */ \
	    && ((W1)->op.rma->flags == (W2)->op.rma->flags) \
	    )
#define TOFU_DW_VOID_STAR_INC(VPP)	\
	    do { \
		uintptr_t uip_ = (uintptr_t)((VPP)[0]) + 1; \
		(VPP)[0] = (void *)uip_; \
	    } while (0)

struct tofu_minmax {
    size_t min;
    size_t max;
#ifndef	notdef_list_0002
    size_t p_x;
#endif	/* notdef_list_0002 */
};

static int tofu_dw_sort_comp(const void *v1, const void *v2)
{
#ifdef	notdef_list_0001
    const size_t *e1 = v1, *e2 = v2;

    assert(e1[0] != e2[0]);
    // return (e1[0] == e2[0])? (e1 > e2): (e1[0] > e2[0]);
    return (e1[0] > e2[0]);
#else	/* notdef_list_0001 */
    const struct tofu_minmax *e1 = v1, *e2 = v2;

    assert(e1->min != e2->min);
    // return (e1->min == e2->min)? (e1 > e2): (e1->min > e2->min);
    return (e1->min > e2->min);
#endif	/* notdef_list_0001 */
}

static int tofu_pcol_fidw2toof(
    struct fi_deferred_work *dw,
    size_t nw,
    fi_addr_t my_rxa
)
{
    int rc = FI_SUCCESS;
    size_t iw;
#ifdef	notdef_list_0001
    size_t *list = 0;
#else	/* notdef_list_0001 */
    struct tofu_minmax *list = 0;
#endif	/* notdef_list_0001 */
    size_t nent, ment, ient;
    
    /* initialize list[] of thresholds */
    {
	ment = nw;
	list = calloc(ment, sizeof (list[0]));
	if (list == 0) { rc = -FI_ENOMEM; goto bad; }
	nent = 0;
    }

    /* make a list of thresholds */
    for (iw = 0; iw < nw; iw++) {
	struct fi_deferred_work *w = &dw[iw];
	uint64_t threshold;

	threshold = w->threshold;
	for (ient = 0; ient < nent; ient++) {
#ifdef	notdef_list_0001
	    if (list[ient] == threshold) { break; }
#else	/* notdef_list_0001 */
	    if (list[ient].min == threshold) { break; }
#endif	/* notdef_list_0001 */
	}
	if (ient == nent) {
	    assert(nent < ment);
#ifdef	notdef_list_0001
	    list[nent++] = threshold;
#else	/* notdef_list_0001 */
	    list[nent].min = list[nent].max = threshold;
#ifndef	notdef_list_0002
	    list[ient].p_x = -1UL;
#endif	/* notdef_list_0002 */
	    nent++;
#endif	/* notdef_list_0001 */
	}
    }

    /* sort list[] by threshold */
    qsort( list, nent, sizeof (list[0]), tofu_dw_sort_comp );

    /* initialize local variable context.internal[0|1|2] */
    for (iw = 0; iw < nw; iw++) {
	struct fi_deferred_work *w = &dw[iw];

	w->context.internal[0] = (void *)1; /* increment value */
	w->context.internal[1] = (void *)0; /* duplicate */
	if (
	    (my_rxa != FI_ADDR_NOTAVAIL)
	    && (w->op.rma->msg.addr == my_rxa)
	) {
	    w->context.internal[2] = (void *)1; /* self-loop op */
	}
	else {
	    w->context.internal[2] = (void *)0; /* self-loop op */
	}
    }

#ifdef	notyet
    /*
     * fi_trigger(3)
     *   if two triggered operations have the same threshold, they
     *   will be triggered in the order in which they were submitted to
     *   the endpoint.
     */
    /*
     * [syn1 syn2 syn1 syn2] put1 [syn1 syn2 syn1 syn2] put2
     *
     * GOOD
     * [syn1 syn2] put1 [syn1 syn2] put2  
     *    x2   x2          x2   x2
     * NG
     * [syn1 syn2] put1 put2  
     *    x4   x4           
     */
#endif	/* notyet */
    /* calculate context.internal[0] and [2] */
    for (iw = 0; iw < nw; iw++) {
	struct fi_deferred_work *w = &dw[iw];
	size_t iw2;

	if (w->context.internal[0] == 0) { continue; }

	if ( ! TOFU_DW_IS_CMSG(w) ) { continue; }

	for (iw2 = iw + 1; iw2 < nw; iw2++) {
	    struct fi_deferred_work *w2 = &dw[iw2];

	    if (w2->context.internal[0] == 0) { continue; }
	    if (w2->threshold != w->threshold) { continue; }

	    if ( ! TOFU_DW_SAME_CMSG(w, w2) ) { continue; }

	    assert(w != w2);
	    assert(w2->context.internal[0] == (void *)1);
	    TOFU_DW_VOID_STAR_INC( &w->context.internal[0] );
	    if (w2->context.internal[2] != (void *)0) { /* self-loop op */
		assert(w2->context.internal[2] == (void *)1);
		TOFU_DW_VOID_STAR_INC( &w->context.internal[2] );
	    }
	    w2->context.internal[0] = (void *)0;
	}
    }

    /* calculate context.internal[1] */
    for (ient = 0; ient < nent; ient++) {
#ifdef	notdef_list_0001
	size_t threshold = list[ient];
#else	/* notdef_list_0001 */
	size_t threshold = list[ient].min;
#endif	/* notdef_list_0001 */
	struct fi_deferred_work *wh = 0; /* head */

	for (iw = 0; iw < nw; iw++) {
	    struct fi_deferred_work *w = &dw[iw];

	    /* skip obsolated entries */
	    if (w->context.internal[0] == 0) { continue; }

	    /* count the number of works with same threshold */
	    if (w->threshold != threshold) { continue; }

	    if (wh == 0) { wh = w; }
	    TOFU_DW_VOID_STAR_INC( &wh->context.internal[1] );
#ifndef	notdef_list_0001

	    if (w->context.internal[2] != 0) { /* self-loop op */
		list[ient].max += (uintptr_t)w->context.internal[0];
	    }
#endif	/* notdef_list_0001 */
	}
	assert(wh != 0);
    }

#ifdef	notdef_list_0002
    /*
     * YYY INC  processing [min,max]
     */
#else	/* notdef_list_0002 */
    /* undup */
    for (ient = 0; ient < nent; ient++) {
	size_t jent;

	assert(list[ient].max >= list[ient].min);
	if (list[ient].p_x != -1UL) { continue; }

	for (jent = 0; jent < nent; jent++) {
	    assert(list[jent].max >= list[jent].min);

	    if (jent == ient) { continue; }

	    if (list[jent].min >= list[ient].min) {
		assert(list[jent].min != list[ient].min); /* min is unique */
		if (list[jent].min <= list[ient].max) {
		    if (list[jent].p_x == -1UL) { list[jent].p_x = ient; }
		    else { assert(list[jent].p_x == ient); }
		}
	    }
	    else if (list[jent].min < list[ient].min) {
		if (list[jent].max >= list[ient].min) {
		    if (list[ient].p_x == -1UL) { list[ient].p_x = jent; }
		    else { assert(list[ient].p_x == jent); }
		}
	    }
	}
    }
#endif	/* notdef_list_0002 */

bad:
    if (list != 0) {
	free(list); list = 0;
    }
    return rc;
}
