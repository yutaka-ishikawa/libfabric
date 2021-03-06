/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "ulib_conf.h"
#include "ulib_dlog.h"

#include "utofu.h"

#include "ulib_toqc.h"

#include <stdint.h>	    /* for uint32_t */
#include <assert.h>	    /* for assert() */
#include <stdlib.h>	    /* for calloc(), free() */
#include <string.h>	    /* for memcpy() */

/* #include "ulib_util.h" */
#define UTOFU_ISA_ERR_TCQ(_UC)	( \
				((_UC) <= -128) \
				&& ((_UC) > -(128+64)) \
			    )
#define UTOFU_ISA_ERR_MRQ(_UC)	( \
				((_UC) <= -192) \
				&& ((_UC) > -(192+64)) \
			    )
#define ULIB_TOQC_NOTICES   ( 0 \
			    | UTOFU_ONESIDED_FLAG_TCQ_NOTICE \
			    | UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE \
			    )


int ulib_toqc_post(
    struct ulib_toqc *toqc,
    void *desc,
    size_t desc_size,
#ifdef	notdef_retp
    void *retp, /* fetch value */
#else	/* notdef_retp */
    uint64_t retp[3], /* [0] fetch value [1] uc [2] edata */
#endif	/* notdef_retp */
    const struct utofu_mrq_notice *ackd,
    uint64_t *r_no /* recipient number */
)
{
    int uc = UTOFU_SUCCESS;
    struct ulib_toqe *toqe = 0;
    unsigned long flags = 0;

    ENTER_RC_C(uc);

    ulib_toqc_lock(toqc);

    /* check queue full */
    {
	int rv = ulib_toqc_chck( toqc, 1 ); /* # of rooms */
	if (rv < 0) {
	    ulib_toqc_unlock(toqc);
	    uc = rv; RETURN_IGN_C(uc);
	}
    }

    if (
	(ackd != 0)
	&& (((flags = ackd->reserved[1]) & ULIB_TOQC_NOTICES) != 0)
    ) {
	toqe = ulib_toqc_toqe_pcnt(toqc);
    }

    uc = utofu_post_toq(toqc->vcqh, desc, desc_size, toqe);
    if (uc != UTOFU_SUCCESS) {
	ulib_toqc_unlock(toqc);
	RETURN_BAD_C(uc);
    }

    if (toqe != 0) {
	if (r_no != 0) {
	    r_no[0] = toqc->pcnt; /* XXX recipient number */
	}
	/* increment pcnt (producre counter) */
	{
	    struct ulib_toqe *e;
	    e = ulib_toqc_toqe_pcnt_incr(toqc);
	    if (e == 0) { }
	    assert(e == toqe);
	}
	assert(toqe->flag == 0);
	toqe->flag = ULIB_TOQE_FLAG_USED;
	if ((flags & UTOFU_ONESIDED_FLAG_TCQ_NOTICE) == 0) {
	    toqe->flag |= ULIB_TOQE_FLAG_TCQD;
	}
	if ((flags & UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE) == 0) {
	    toqe->flag |= ULIB_TOQE_FLAG_ACKD;
	}
	if (ackd->notice_type == ULIB_MRQ_TYPE_LCL_NOP) {
	    toqe->flag |= ULIB_TOQE_FLAG_ACKD;
	}

	toqe->dsiz = desc_size;
	toqe->csiz = 0; /* completed */
	toqe->retp = retp;
	assert(sizeof (toqe->ackd) >= sizeof (ackd[0]));
	memcpy(toqe->ackd, ackd, sizeof (ackd[0]));
// printf("post [%ld] flag %x\n", toqe - &toqc->toqe[0], toqe->flag);
// fflush(stdout);
    }
#ifndef	notdef_toqc_fix1
    else if (r_no != 0) {
	r_no[0] = toqc->pcnt - 1; /* XXX recipient number */
    }
#endif	/* notdef_toqc_fix1 */

    ulib_toqc_unlock(toqc);

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_toqc_init(
    utofu_vcq_hdl_t vcqh,
    struct ulib_toqc **pp_toqc
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    {
	struct ulib_toqc *toqc;
	size_t msiz;

	msiz  = sizeof (toqc[0]);
	msiz += (sizeof (toqc->toqe[0]) * ULIB_TOQC_SIZE);

	toqc = calloc(1, msiz);
	if (toqc == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}

	toqc->vcqh = vcqh;
	toqc->toqe = (struct ulib_toqe *)&toqc[1];;

	/* return */
	pp_toqc[0] = toqc;
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_toqc_fini(
    struct ulib_toqc *toqc
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    if (toqc != 0) {
	free(toqc);
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_toqc_prog_tcqd(struct ulib_toqc *toqc)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* tcq */
    while (1) {
	const unsigned long flags = 0UL; /* UTOFU_POLL_FLAG+ */
	void *cbdata = 0;

	uc = utofu_poll_tcq( toqc->vcqh, flags, &cbdata );
	if (uc == UTOFU_ERR_NOT_FOUND) {
	    uc = UTOFU_SUCCESS;
	    break;
	}
	else if (uc != UTOFU_SUCCESS) {
	    if ( ! UTOFU_ISA_ERR_TCQ(uc) ) { /* UTOFU_ERR_TCQ_+ */
		ulib_toqc_abort(toqc, uc);
		RETURN_BAD_C(uc);
	    }
	    /* fall thru */
	}

#ifndef	CONF_ULIB_UTOF_FIX2
	/* toqe */
	{
	    struct ulib_toqe *toqe = cbdata;

	    assert(toqe != 0);
	    assert(toqe >= &toqc->toqe[0]);
	    assert(toqe <  &toqc->toqe[ULIB_TOQC_SIZE]);

// printf("tcqd [%ld] flag %x\n", toqe - &toqc->toqe[0], toqe->flag);
// fflush(stdout);
	    assert((toqe->flag & ULIB_TOQE_FLAG_USED) != 0);
	    assert((toqe->flag & ULIB_TOQE_FLAG_TCQD) == 0);
#ifdef	notdef_fix3
	    toqe->flag |= ULIB_TOQE_FLAG_TCQD;
#else	/* notdef_fix3 */
	    assert(toqe->csiz < toqe->dsiz);
	    toqe->csiz += 32 /* XXX */;
	    if (toqe->csiz >= toqe->dsiz) {
		assert(toqe->csiz == toqe->dsiz);
		toqe->flag |= ULIB_TOQE_FLAG_TCQD;
	    }
#endif	/* notdef_fix3 */
	    if (uc != 0) { /* UTOFU_ISA_ERR_TCQ(uc) */
		toqe->flag |= ULIB_TOQE_FLAG_ACKD;
	    }
	    if (ULIB_TOQE_COMPLETED(toqe->flag)) {
		toqe->flag = 0;
		ulib_toqc_lock(toqc);
		ulib_toqc_toqe_ccnt_update(toqc);
		ulib_toqc_unlock(toqc);
	    }
	    uc = UTOFU_SUCCESS;
	}
#else	/* CONF_ULIB_UTOF_FIX2 */
	ulib_toqc_match_tcqd(toqc, cbdata, uc);
	uc = UTOFU_SUCCESS;
#endif	/* CONF_ULIB_UTOF_FIX2 */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_toqc_prog_ackd(struct ulib_toqc *toqc)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* mrq */
    while (1) {
	const unsigned long flags = 0UL; /* UTOFU_POLL_FLAG+ */
	struct utofu_mrq_notice tmrq[1];

	uc = utofu_poll_mrq( toqc->vcqh, flags, tmrq );
	if (uc == UTOFU_ERR_NOT_FOUND) {
	    uc = UTOFU_SUCCESS;
	    break;
	}
	else if (uc != UTOFU_SUCCESS) {
	    if (UTOFU_ISA_ERR_MRQ(uc)) { /* UTOFU_ERR_MRQ_+ */
		ulib_toqc_match(toqc, tmrq, uc);
		uc = UTOFU_SUCCESS;
		continue;
	    }
	    ulib_toqc_abort(toqc, uc);
	    RETURN_BAD_C(uc);
	}
	ulib_toqc_match(toqc, tmrq, 0 /* uc */ );
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_toqc_prog(struct ulib_toqc *toqc)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* tcqd : local completion */
    uc = ulib_toqc_prog_tcqd(toqc);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* ackd */
    uc = ulib_toqc_prog_ackd(toqc);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_toqc_read(struct ulib_toqc *toqc, uint64_t *r_no)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    r_no[0] = toqc->ccnt; /* XXX */

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_toqc_chck_tcqd(struct ulib_toqc *toqc, size_t *pend_tcqd)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* get the number of the outstanding tcqds on the vcqh */
    uc = utofu_query_num_unread_tcq(toqc->vcqh, pend_tcqd);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}
