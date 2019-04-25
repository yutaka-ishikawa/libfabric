/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "ulib_conf.h"
#include "ulib_dlog.h"	    /* for ENTER_RC_C() */

#include "utofu.h"	    /* for UTOFU_SUCCESS */

#include "ulib_shea.h"
#include "ulib_desc.h"
#include "ulib_toqc.h"
#include "ulib_tick.h"

#include <stddef.h>	    /* for offsetof() */
#include <assert.h>	    /* for assert() */
#include <string.h>	    /* for memcpy() */

/* #include "ulib_conf.h" */
#define CONF_ULIB_UTOF_FIX4

static inline uint64_t ulib_shea_cntr_ui64(uint64_t pc, uint64_t cc)
{
#ifdef	NOTDEF_GCCX /* gcc extension */
    const uint64_t lval = ({
	union ulib_shea_ct_u cntr = { .ct_s.pcnt = pc, .ct_s.ccnt = cc, };
	cntr.ct64;
    });
#else	/* NOTDEF_GCCX */
    union ulib_shea_ct_u cntr = { .ct_s.pcnt = pc, .ct_s.ccnt = cc, };
#endif	/* NOTDEF_GCCX */
    return cntr.ct64;
}

static inline void ulib_shea_perf_l_st(int l_st, struct ulib_shea_data *data)
{
#ifdef	CONF_ULIB_PERF_SHEA
    assert(l_st >= 0);
    assert(l_st < (sizeof (data->tims) / sizeof (data->tims[0])));
    data->tims[l_st] = ulib_tick_time();
#endif	/* CONF_ULIB_PERF_SHEA */
    return ;
}

static inline void ulib_shea_cash_insq_tail( /* foo2 */
    struct ulib_toqd_cash *toqd,
    const struct ulib_toqd_cash *tmpl,
    struct ulib_shea_esnd *esnd
)
{
    /* swap */
    assert(sizeof (esnd->pred[0]) == sizeof (esnd->pred[0].va64));
    assert(tmpl->size == sizeof (uint64_t [4]));
    {
	uint64_t *desc = toqd->desc;
	const uint64_t lval = esnd->self.addr.va64;

	/* desc[] */
	{
	    if (tmpl->size == 32) { /* likely() */
		/* memcpy(desc, tmpl->desc, 32); */
		desc[0] = tmpl->desc[0]; desc[1] = tmpl->desc[1];
		desc[2] = tmpl->desc[2]; desc[3] = tmpl->desc[3];
	    }
	    else {
		memcpy(desc, tmpl->desc, tmpl->size);
	    }
#ifndef	CONF_ULIB_UTOF_FIX4
	    {
		int uc;
		uc = utofu_modify_armw8_op_value(desc, lval);
		if (uc != UTOFU_SUCCESS) {
		    /* YYY abort */
		}
	    }
#else	/* CONF_ULIB_UTOF_FIX4 */
	    desc[3] = lval;
#endif	/* CONF_ULIB_UTOF_FIX4 */
	}
	/* size */
	toqd->size = tmpl->size;
	/* ackd */
	memcpy(toqd->ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
    }

    return ;
}

static inline int ulib_shea_post_insq_tail( /* foo2 */
    struct ulib_toqc *toqc,
    /* const */ struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* swap */
    assert(toqd->size != 0);
    {
        uint64_t *retp = &esnd->pred[0].va64;

	esnd->pred[0].va64 = ULIB_SHEA_NIL8;
	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline void ulib_shea_cash_remq_head( /* foo3 */
    struct ulib_toqd_cash *toqd,
    const struct ulib_toqd_cash *tmpl,
    struct ulib_shea_esnd *esnd
)
{
    /* cswp */
    assert(sizeof (esnd->pred[0]) == sizeof (esnd->pred[0].va64));
    assert(esnd->pred[0].va64 == ULIB_SHEA_NIL8); /* XXX */
    assert(tmpl->size == sizeof (uint64_t [8]));
    {
	uint64_t *desc = toqd->desc;
	const uint64_t cmpv = esnd->self.addr.va64;
	const uint64_t swpv = ULIB_SHEA_NIL8;

	/* desc[] */
	{
	    if (tmpl->size == 64) { /* likely() */
		/* memcpy(desc, tmpl->desc, 64); */
		desc[0] = tmpl->desc[0]; desc[1] = tmpl->desc[1];
		desc[2] = tmpl->desc[2]; desc[3] = tmpl->desc[3];
		desc[4] = tmpl->desc[4]; desc[5] = tmpl->desc[5];
		desc[6] = tmpl->desc[6]; desc[7] = tmpl->desc[7];
	    }
	    else {
		memcpy(desc, tmpl->desc, tmpl->size);
	    }
#ifndef	CONF_ULIB_UTOF_FIX4
	    {
		int uc;
		uc = utofu_modify_cswap8_values(desc, cmpv, swpv);
		if (uc != UTOFU_SUCCESS) {
		    /* YYY abort */
		}
	    }
#else	/* CONF_ULIB_UTOF_FIX4 */
	    desc[1+0] = cmpv; /* write piggyback buffer */
	    desc[3+4] = swpv; /* atomic read modify write */
#endif	/* CONF_ULIB_UTOF_FIX4 */
	}
	/* size */
	toqd->size = tmpl->size;
	/* ackd */
	memcpy(toqd->ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
    }

    return ;
}

static inline int ulib_shea_post_remq_head( /* foo3 */
    struct ulib_toqc *toqc,
    /* const */ struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* swap */
    assert(toqd->size != 0);
    {
        uint64_t *retp = &esnd->pred[0].va64;

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline void ulib_shea_cash_incr_cntr( /* foo4 */
    struct ulib_toqd_cash *toqd,
    const struct ulib_toqd_cash *tmpl,
    struct ulib_shea_esnd *esnd
)
{
    /* fadd */
    assert(sizeof (esnd->cntr[0]) == sizeof (esnd->cntr[0].ct64));
    assert(tmpl->size == sizeof (uint64_t [4]));
    {
	uint64_t *desc = toqd->desc;
	const uint64_t lval = ulib_shea_cntr_ui64(
		ulib_shea_data_nblk(esnd->data) /* pcnt */, 0 /* ccnt */);

	/* desc[] */
	{
	    if (tmpl->size == 32) { /* likely() */
		/* memcpy(desc, tmpl->desc, 32); */
		desc[0] = tmpl->desc[0]; desc[1] = tmpl->desc[1];
		desc[2] = tmpl->desc[2]; desc[3] = tmpl->desc[3];
	    }
	    else {
		memcpy(desc, tmpl->desc, tmpl->size);
	    }
#ifndef	CONF_ULIB_UTOF_FIX4
	    {
		int uc;
		uc = utofu_modify_armw8_op_value(desc, lval);
		if (uc != UTOFU_SUCCESS) {
		    /* YYY abort */
		}
	    }
#else	/* CONF_ULIB_UTOF_FIX4 */
	    desc[3] = lval;
#endif	/* CONF_ULIB_UTOF_FIX4 */
	}
	/* size */
	toqd->size = tmpl->size;
	/* ackd */
	memcpy(toqd->ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
    }

    return ;
}

static inline int ulib_shea_post_incr_cntr( /* foo4 */
    struct ulib_toqc *toqc,
    /* const */ struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* fadd */
    assert(toqd->size != 0);
    {
        uint64_t *retp = &esnd->cntr[0].ct64;

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline void ulib_shea_cash_data_wait( /* foo7 */
    struct ulib_toqd_cash *toqd,
    const struct ulib_toqd_cash *tmpl,
    struct ulib_shea_esnd *esnd
)
{
    /* puti_le32 */
    assert(sizeof (esnd->self) == 16);
    assert(tmpl->size == sizeof (uint64_t [8]));
    {
	uint64_t *desc = toqd->desc;
	const void *ldat = &esnd->self;

	/* esnd->self (full) */
	{
	    uint32_t pcnt = esnd->cntr[0].ct_s.pcnt;
	    uint32_t nsnd = ulib_shea_data_rblk(esnd->data);

	    assert(nsnd > 0);
	    esnd->self.cntr.ct_s.pcnt = pcnt + nsnd;
	    esnd->self.cntr.ct_s.ccnt = pcnt;
	}
	/* desc[] */
	{
	    if (tmpl->size == 64) { /* likely() */
		/* memcpy(desc, tmpl->desc, 64); */
		desc[0] = tmpl->desc[0]; desc[1] = tmpl->desc[1];
		desc[2] = tmpl->desc[2]; desc[3] = tmpl->desc[3];
		desc[4] = tmpl->desc[4]; desc[5] = tmpl->desc[5];
		desc[6] = tmpl->desc[6]; desc[7] = tmpl->desc[7];
	    }
	    else {
		memcpy(desc, tmpl->desc, tmpl->size);
	    }
#ifndef	CONF_ULIB_UTOF_FIX4
	    {
		int uc;
		uc = utofu_modify_put_piggyback_ldat(desc, ldat);
		if (uc != UTOFU_SUCCESS) {
		    /* YYY abort */
		}
	    }
#else	/* CONF_ULIB_UTOF_FIX4 */
	    desc[1+0] = ((uint64_t *)ldat)[1]; /* write piggyback buffer */
	    desc[3+4] = ((uint64_t *)ldat)[0]; /* put piggyback */
#endif	/* CONF_ULIB_UTOF_FIX4 */
	}
	/* size */
	toqd->size = tmpl->size;
	/* ackd */
	memcpy(toqd->ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
    }

    return ;
}

static inline int ulib_shea_post_data_wait( /* foo7 */
    struct ulib_toqc *toqc,
    /* const */ struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* puti_le32 */
    assert(toqd->size != 0);
    {
        uint64_t * const retp = 0;

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}


/* ulib_shea_exec() */
int ulib_shea_foo0(
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;
    struct ulib_toqc *toqc;
    struct ulib_toqc_cash *cash_tmpl;
    struct ulib_toqc_cash *cash_real;

    ENTER_RC_C(uc);

    toqc = ulib_shea_data_toqc(esnd->data);
    cash_tmpl = ulib_shea_data_toqc_cash_tmpl(esnd->data);
    cash_real = ulib_shea_data_toqc_cash_real(esnd->data);

next_state:
    switch (esnd->l_st) { /* local state */
	struct ulib_toqd_cash *toqd;
	uint64_t cdif, nblk, rblk;
	uint64_t pred;
    case INSQ_TAIL:
	ulib_shea_perf_l_st(INSQ_TAIL, esnd->data);
	if (cash_real->swap[0].size == 0) {
	    ulib_shea_cash_insq_tail(cash_real->swap, cash_tmpl->swap, esnd);
	}
	toqd = cash_real->swap;
	uc = ulib_shea_post_insq_tail(toqc, toqd, esnd);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	/* INSQ_TAIL --> INSQ_CHCK */
	ulib_shea_chst(esnd, INSQ_CHCK);
	if (cash_real->fadd[0].size == 0) {
	    ulib_shea_cash_incr_cntr(cash_real->fadd, cash_tmpl->fadd, esnd);
	}
	if (cash_real->cswp[0].size == 0) {
	    ulib_shea_cash_remq_head(cash_real->cswp, cash_tmpl->cswp, esnd);
	}
	if (cash_real->puti[0].size == 0) {
	    ulib_shea_cash_data_wait(cash_real->puti, cash_tmpl->puti, esnd);
	}
	break;
    case INSQ_CHCK:
	ulib_shea_perf_l_st(INSQ_CHCK, esnd->data);
	if (esnd->pred[1].va64 != UTOFU_SUCCESS) { /* r_uc */
#ifdef	NOTYET
	    void *farg = esnd->farg;
	    int r_uc = (int64_t)esnd->pred[1].va64;
	    /* ulib_shea_free_esnd(esnd); */
	    ulib_shea_cbak(farg, r_uc, 0);
#endif	/* NOTYET */
	}
	if (esnd->pred[0].va64 == ULIB_SHEA_NIL8) { /* is a head of the q.? */
	    /* INSQ_CHCK --> INCR_CNTR */
	    ulib_shea_chst(esnd, INCR_CNTR);
	    goto next_state;
	}
	pred = esnd->pred[0].va64;
	/* uc = ulib_shea_pred_next(toqc, pred, esnd); */
	uc = ulib_shea_foo5(toqc, pred, esnd);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	/* INSQ_CHCK --> WAIT_HEAD */
	ulib_shea_chst(esnd, WAIT_HEAD);
	goto next_state; /* break; */
    case WAIT_HEAD:
	ulib_shea_perf_l_st(WAIT_HEAD, esnd->data);
	if (esnd->wait != 0) {
	    ulib_shea_chst(esnd, INCR_CNTR);
	    goto next_state;
	}
	/* YYY SCHEDULE_SELF() */
printf("#%d\twait_head YYY schd_self()\n", __LINE__);
	break;
    case INCR_CNTR:
	ulib_shea_perf_l_st(INCR_CNTR, esnd->data);
	if (cash_real->fadd[0].size == 0) {
	    ulib_shea_cash_incr_cntr(cash_real->fadd, cash_tmpl->fadd, esnd);
	}
	toqd = cash_real->fadd;
	uc = ulib_shea_post_incr_cntr(toqc, toqd, esnd);
	/* INCR_CNTR --> HAVE_ROOM */
	ulib_shea_chst(esnd, HAVE_ROOM);
	break;
    case HAVE_ROOM:
	ulib_shea_perf_l_st(HAVE_ROOM, esnd->data);
	if (esnd->cntr[1].ct64 != UTOFU_SUCCESS) { /* r_uc */
	}
	cdif = ulib_shea_cntr_diff(&esnd->cntr[0]);
	nblk = ulib_shea_data_nblk(esnd->data);
#ifndef	TEST_HAVE_ROOM
	if (cdif >= nblk) {
	    /* HAVE_ROOM --> REMQ_HEAD */
	    ulib_shea_chst(esnd, REMQ_HEAD);
	    goto next_state;
	}
#else	/* TEST_HAVE_ROOM */
	if (cdif > nblk) {
	    /* HAVE_ROOM --> REMQ_HEAD */
	    ulib_shea_chst(esnd, REMQ_HEAD);
	    goto next_state;
	}
#endif	/* TEST_HAVE_ROOM */
if (0) {
printf("#%d\thave_room cdif %"PRIu64" <= nblk %"PRIu64"\n", __LINE__, cdif, nblk);
fflush(stdout);
}
	/* HAVE_ROOM --> SEND_EXCL */
	ulib_shea_chst(esnd, SEND_EXCL);
	ulib_shea_perf_l_st(SEND_EXCL, esnd->data);
	goto next_state; /* break; */
    case REMQ_HEAD:
	ulib_shea_perf_l_st(REMQ_HEAD, esnd->data);
	if (esnd->next.va64 != ULIB_SHEA_NIL8) {
	    /* REMQ_HEAD --> WAIT_NEXT */
	    ulib_shea_chst(esnd, WAIT_NEXT);
	    goto next_state;
	}
	if (cash_real->cswp[0].size == 0) {
	    ulib_shea_cash_remq_head(cash_real->cswp, cash_tmpl->cswp, esnd);
	}
	toqd = cash_real->cswp;
	uc = ulib_shea_post_remq_head(toqc, toqd, esnd);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	/* REMQ_HEAD --> REMQ_CHCK */
	ulib_shea_chst(esnd, REMQ_CHCK);
	break;
    case REMQ_CHCK:
	ulib_shea_perf_l_st(REMQ_CHCK, esnd->data);
	if (esnd->pred[1].va64 != UTOFU_SUCCESS) { /* r_uc */
	}
	if (esnd->pred[0].va64 == esnd->self.addr.va64) {
	    /* REMQ_CHCK --> SEND_NORM */
	    ulib_shea_chst(esnd, SEND_NORM);
	    ulib_shea_perf_l_st(SEND_NORM, esnd->data);
	    goto next_state;
	}
	/* REMQ_CHCK --> WAIT_NEXT */
	ulib_shea_chst(esnd, WAIT_NEXT);
	goto next_state; /* break; */
    case WAIT_NEXT:
	ulib_shea_perf_l_st(WAIT_NEXT, esnd->data);
	if (esnd->next.va64 != ULIB_SHEA_NIL8) {
	    uint64_t next = esnd->next.va64;
	    /* uc = ulib_shea_wake_next(toqc, next, esnd); */
	    uc = ulib_shea_foo6(toqc, next, esnd);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	    /* WAIT_NEXT --> SEND_NORM */
	    ulib_shea_chst(esnd, SEND_NORM);
	    ulib_shea_perf_l_st(SEND_NORM, esnd->data);
	    goto next_state;
	}
	/* YYY SCHEDULE_SELF() */
printf("#%d\twait_next YYY schd_self()\n", __LINE__);
	break;
    case SEND_NORM:
	rblk = ulib_shea_data_rblk(esnd->data);
	if (rblk > 0) {
	    /* uc = ulib_shea_data(toqc, cash_tmpl, cash_real, esnd); */
	    uc = ulib_shea_foo9(toqc, cash_tmpl, cash_real, esnd);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#ifdef	CONF_ULIB_UTOF_FIX2
	    break; /* for progress */
#endif	/* CONF_ULIB_UTOF_FIX2 */
	}
	if (ulib_shea_data_rblk(esnd->data) == 0) {
	    /* SEND_NORM --> SEND_DONE */
	    ulib_shea_chst(esnd, SEND_DONE);
	    goto next_state;
	}
	/* YYY SCHEDULE_SELF() */
printf("#%d\tsend_norm YYY schd_self()\n", __LINE__);
	break;
    case SEND_EXCL:
	rblk = ulib_shea_data_rblk(esnd->data);
if (0) {
printf("#%d\tsend_excl rblk %"PRIu64" > 0, d_st %d\n", __LINE__,
rblk, esnd->d_st);
fflush(stdout);
}
	if (rblk > 0) {
	    /* uc = ulib_shea_data(toqc, cash_tmpl, cash_real, esnd); */
	    uc = ulib_shea_foo9(toqc, cash_tmpl, cash_real, esnd);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#ifdef	CONF_ULIB_UTOF_FIX2
	    break; /* for progress */
#endif	/* CONF_ULIB_UTOF_FIX2 */
	}
	if (ulib_shea_data_rblk(esnd->data) == 0) {
	    /* SEND_EXCL --> REMQ_HEAD */
	    ulib_shea_chst(esnd, REMQ_HEAD);
	    goto next_state;
	}
	/* YYY SCHEDULE_SELF() */
printf("#%d\tsend_excl YYY schd_self()\n", __LINE__);
	break;
    case SEND_DONE:
	ulib_shea_perf_l_st(SEND_DONE, esnd->data);
	break;
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_make_tmpl() */
int ulib_shea_foo1(
    struct ulib_toqc_cash *cash,
    const struct ulib_utof_cash *rcsh,
    const struct ulib_utof_cash *lcsh
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* swap */
    {
        const uint64_t roff = offsetof (struct ulib_shea_ercv, tail);
        const uint64_t leng = sizeof (((struct ulib_shea_ercv *)0)->tail);
        const uint64_t lval = ULIB_SHEA_NIL8; /* YYY */
        const uint64_t edat = 0;

        uc = ulib_desc_swap_from_cash(rcsh, lcsh, roff, lval, leng, edat,
		ULIB_DESC_UFLG_ARMW,
                cash->swap);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
    /* cswp */
    {
        const uint64_t roff = offsetof (struct ulib_shea_ercv, tail);
        const uint64_t leng = sizeof (((struct ulib_shea_ercv *)0)->tail);
        const uint64_t cmpv = 0; /* YYY */
        const uint64_t swpv = ULIB_SHEA_NIL8;
        const uint64_t edat = 0;

	uc = ulib_desc_cswp_from_cash(rcsh, lcsh, roff, swpv, cmpv, leng, edat,
		ULIB_DESC_UFLG_ARMW,
		cash->cswp);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
if (0) {
printf("size %ld cmd %08x %08x\n", cash->cswp[0].size,
((uint8_t *)cash->cswp[0].desc)[1],
((uint8_t *)cash->cswp[0].desc)[1+32]);
}
    }
    /* fadd */
    {
        const uint64_t roff = offsetof (struct ulib_shea_ercv, cntr);
        const uint64_t leng = sizeof (((struct ulib_shea_ercv *)0)->cntr);
        const uint64_t lval = ulib_shea_cntr_ui64(0, 0);
        const uint64_t edat = 0;

        uc = ulib_desc_fadd_from_cash(rcsh, lcsh, roff, lval, leng, edat,
		ULIB_DESC_UFLG_ARMW,
                cash->fadd);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
    /* puti */
    {
	const uint64_t roff = offsetof (struct ulib_shea_ercv, full);
	const uint64_t leng = sizeof (((struct ulib_shea_ercv *)0)->full);
	const uint64_t ldat[2] = { 0 };
	const uint64_t edat = 0;
	const unsigned long uflg =  0
				    | UTOFU_ONESIDED_FLAG_CACHE_INJECTION
				    | UTOFU_ONESIDED_FLAG_TCQ_NOTICE
				    ;

	assert(sizeof (((struct ulib_shea_ercv *)0)->full) == sizeof (ldat));
	uc = ulib_desc_puti_le32_from_cash(rcsh, lcsh, roff, ldat, leng, edat,
	    uflg,
	    cash->puti);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
    /* phdr (puti) */
    {
	const uint64_t roff = 1024; /* 2019/04/24 */
	const uint64_t leng = 32; /* YYY sizeof (union ulib_shea_ph_u); */
	const uint64_t ldat[4] = { 0 };
	const uint64_t edat = 0;
	const unsigned long uflg =  0
				    | UTOFU_ONESIDED_FLAG_STRONG_ORDER
				    | UTOFU_ONESIDED_FLAG_CACHE_INJECTION
#ifdef	CONF_ULIB_UTOF_FIX2
				    | UTOFU_ONESIDED_FLAG_TCQ_NOTICE
#endif	/* CONF_ULIB_UTOF_FIX2 */
				    ;

	assert(leng == sizeof (ldat));
	uc = ulib_desc_puti_le32_from_cash(rcsh, lcsh, roff, ldat, leng, edat,
	    uflg,
	    cash->phdr);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_insq_tail() */
int ulib_shea_foo2(
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* swap */
    {
	uint64_t desc[8];
        uint64_t *retp = &esnd->pred[0].va64;
	const uint64_t lval = esnd->self.addr.va64;

	/* desc[] */
	{
	    assert(toqd->size <= sizeof (desc));
	    if (toqd->size == 32) { /* likely() */
		/* memcpy(desc, toqd->desc, 32); */
		desc[0] = toqd->desc[0];
		desc[1] = toqd->desc[1];
		desc[2] = toqd->desc[2];
		desc[3] = toqd->desc[3];
	    }
	    else {
		memcpy(desc, toqd->desc, toqd->size);
	    }
	}
#ifndef	CONF_ULIB_UTOF_FIX4
	uc = utofu_modify_armw8_op_value(desc, lval);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#else	/* CONF_ULIB_UTOF_FIX4 */
	desc[3] = lval;
#endif	/* CONF_ULIB_UTOF_FIX4 */

	assert(sizeof (esnd->pred[0]) == sizeof (esnd->pred[0].va64));
	esnd->pred[0].va64 = ULIB_SHEA_NIL8;
	uc = ulib_toqc_post(toqc, desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_remq_head() */
int ulib_shea_foo3(
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* swap */
    {
	uint64_t desc[8];
        uint64_t *retp = &esnd->pred[0].va64;
	const uint64_t cmpv = esnd->self.addr.va64;
	const uint64_t swpv = ULIB_SHEA_NIL8;

	assert(sizeof (esnd->pred[0]) == sizeof (esnd->pred[0].va64));
	assert(esnd->pred[0].va64 == ULIB_SHEA_NIL8); /* XXX */
	/* desc[] */
	{
	    assert(toqd->size <= sizeof (desc));
	    if (toqd->size == 64) { /* likely() */
		/* memcpy(desc, toqd->desc, 64); */
		desc[0] = toqd->desc[0]; desc[1] = toqd->desc[1];
		desc[2] = toqd->desc[2]; desc[3] = toqd->desc[3];
		desc[4] = toqd->desc[4]; desc[5] = toqd->desc[5];
		desc[6] = toqd->desc[6]; desc[7] = toqd->desc[7];
	    }
	    else {
		memcpy(desc, toqd->desc, toqd->size);
	    }
	}
#ifndef	CONF_ULIB_UTOF_FIX4
	uc = utofu_modify_cswap8_values(desc, cmpv, swpv);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#else	/* CONF_ULIB_UTOF_FIX4 */
	assert(toqd->size == sizeof (uint64_t [8]));
	desc[1+0] = cmpv; /* write piggyback buffer */
	desc[3+4] = swpv; /* atomic read modify write */
#endif	/* CONF_ULIB_UTOF_FIX4 */

	uc = ulib_toqc_post(toqc, desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_incr_cntr() */
int ulib_shea_foo4(
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* fadd */
    {
	uint64_t desc[8];
        uint64_t *retp = &esnd->cntr[0].ct64;
        const uint64_t lval = ulib_shea_cntr_ui64(
		ulib_shea_data_nblk(esnd->data) /* pcnt */, 0 /* ccnt */);

	/* desc[] */
	{
	    assert(toqd->size <= sizeof (desc));
	    if (toqd->size == 32) { /* likely() */
		/* memcpy(desc, toqd->desc, 32); */
		desc[0] = toqd->desc[0];
		desc[1] = toqd->desc[1];
		desc[2] = toqd->desc[2];
		desc[3] = toqd->desc[3];
	    }
	    else {
		memcpy(desc, toqd->desc, toqd->size);
	    }
	}
#ifndef	CONF_ULIB_UTOF_FIX4
	uc = utofu_modify_armw8_op_value(desc, lval);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#else	/* CONF_ULIB_UTOF_FIX4 */
	desc[3] = lval;
#endif	/* CONF_ULIB_UTOF_FIX4 */

	uc = ulib_toqc_post(toqc, desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_pred_next() */
int ulib_shea_foo5(
    struct ulib_toqc *toqc,
    uint64_t raui,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    {
	const utofu_cmp_id_t c_id = CONF_ULIB_CMP_ID;
	void *epnt = 0;
	struct ulib_utof_cash rcsh[1], lcsh[1];

        const uint64_t roff = offsetof (struct ulib_shea_esnd, next);
        const uint64_t leng = sizeof (((struct ulib_shea_esnd *)0)->next);
	const uint64_t lval = esnd->self.addr.va64;
        const uint64_t edat = 0;
	const unsigned long uflg = UTOFU_ONESIDED_FLAG_CACHE_INJECTION;
	struct ulib_toqd_cash toqd[1];
        uint64_t *retp = 0;

	uc = ulib_utof_cash_remote(raui, c_id, epnt, rcsh);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	/* lcsh */
	{
	    lcsh->vcqi = 0;
	    lcsh->stad = 0;
	    lcsh->paid = 0;
	    lcsh->vcqh = toqc->vcqh;
	}

	uc = ulib_desc_puti_le08_from_cash(rcsh, lcsh,
                roff, lval, leng, edat, uflg, toqd);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
if (0) {
printf("\t" "dsiz %ld\n", toqd->size);
printf("\t" "%016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n",
toqd->desc[0], toqd->desc[1], toqd->desc[2], toqd->desc[3]);
}

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_wake_next() */
int ulib_shea_foo6(
    struct ulib_toqc *toqc,
    uint64_t raui,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    {
	const utofu_cmp_id_t c_id = CONF_ULIB_CMP_ID;
	void *epnt = 0;
	struct ulib_utof_cash rcsh[1], lcsh[1];

        const uint64_t roff = offsetof (struct ulib_shea_esnd, wait);
        const uint64_t leng = sizeof (((struct ulib_shea_esnd *)0)->wait);
        const uint64_t lval = 1;
        const uint64_t edat = 0;
	const unsigned long uflg = UTOFU_ONESIDED_FLAG_CACHE_INJECTION;
	struct ulib_toqd_cash toqd[1];
        uint64_t * const retp = 0;

	uc = ulib_utof_cash_remote(raui, c_id, epnt, rcsh);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	/* lcsh */
	{
	    lcsh->vcqi = 0;
	    lcsh->stad = 0;
	    lcsh->paid = 0;
	    lcsh->vcqh = toqc->vcqh;
	}

	uc = ulib_desc_puti_le08_from_cash(rcsh, lcsh,
                roff, lval, leng, edat, uflg, toqd);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
if (0) {
printf("\t" "dsiz %ld\n", toqd->size);
printf("\t" "%016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n",
toqd->desc[0], toqd->desc[1], toqd->desc[2], toqd->desc[3]);
}

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_data_wait() */
int ulib_shea_foo7(
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* puti_le32 */
    {
	uint64_t desc[8];
        uint64_t *retp = 0;
        const void *ldat = &esnd->self;

	/* esnd->self (full) */
	{
	    uint32_t pcnt = esnd->cntr[0].ct_s.pcnt;
	    uint32_t nsnd = ulib_shea_data_rblk(esnd->data);

	    assert(nsnd > 0);
	    esnd->self.cntr.ct_s.pcnt = pcnt + nsnd;
	    esnd->self.cntr.ct_s.ccnt = pcnt;
if (0) {
printf("%s(): ccnt %u\n", __func__, esnd->self.cntr.ct_s.ccnt);
}
	}
	/* desc[] */
	{
	    assert(toqd->size <= sizeof (desc));
	    if (toqd->size == 64) { /* likely() */
		memcpy(desc, toqd->desc, 64);
	    }
	    else {
		memcpy(desc, toqd->desc, toqd->size);
	    }
	}
#ifndef	CONF_ULIB_UTOF_FIX4
	uc = utofu_modify_put_piggyback_ldat(desc, ldat);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#else	/* CONF_ULIB_UTOF_FIX4 */
	desc[1+0] = ((uint64_t *)ldat)[1]; /* write piggyback buffer */
	desc[3+4] = ((uint64_t *)ldat)[0]; /* put piggyback */
#endif	/* CONF_ULIB_UTOF_FIX4 */

	uc = ulib_toqc_post(toqc, desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_data_wake() */
int ulib_shea_foo8(
    struct ulib_toqc *toqc,
    uint64_t raui,
    volatile struct ulib_shea_ercv *ercv
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    {
	const utofu_cmp_id_t c_id = CONF_ULIB_CMP_ID;
	void *epnt = 0;
	struct ulib_utof_cash rcsh[1], lcsh[1];

        const uint64_t roff = offsetof (struct ulib_shea_esnd, cntr[0].ct_s.ccnt);
        const uint64_t leng = sizeof (((struct ulib_shea_esnd *)0)->cntr[0].ct_s.ccnt);
	const uint64_t lval = ercv->cntr.ct_s.ccnt;
        const uint64_t edat = 0;
	const unsigned long uflg = UTOFU_ONESIDED_FLAG_CACHE_INJECTION;
	struct ulib_toqd_cash toqd[1];
        uint64_t * const retp = 0;

        fprintf(stderr, "\tYIPROTOCOL***: %s toqc(%p) raui(%ld) ercv(%p)\n", __func__, toqc, raui, ercv);
	uc = ulib_utof_cash_remote(raui, c_id, epnt, rcsh);
        //fprintf(stderr, "\tYIUTOFU***: %s uc(%d)\n", __func__, uc);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	/* lcsh */
	{
	    lcsh->vcqi = 0;
	    lcsh->stad = 0;
	    lcsh->paid = 0;
	    lcsh->vcqh = toqc->vcqh;
	}

	uc = ulib_desc_puti_le08_from_cash(rcsh, lcsh,
                roff, lval, leng, edat, uflg, toqd);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
if (0) {
printf("\t" "dsiz %ld\n", toqd->size);
printf("\t" "%016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n",
toqd->desc[0], toqd->desc[1], toqd->desc[2], toqd->desc[3]);
}
if (0) {
printf("\tW: R->S ccnt %"PRIu64"\n", lval);
}

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
                            (uint64_t*) &ercv->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline void  ulib_shea_cash_data_phdr( /* foo10 */
			struct ulib_toqd_cash *toqd,
			const struct ulib_toqd_cash *tmpl,
			struct ulib_shea_esnd *esnd,
			uint64_t nsnd
		    );
static inline int   ulib_shea_post_data_phdr( /* foo10 */
			struct ulib_toqc *toqc,
			struct ulib_toqd_cash *toqd,
			struct ulib_shea_esnd *esnd
		    );
static inline void  ulib_shea_cash_data_phdr_wait( /* foo11 */
			struct ulib_toqd_cash *toqd,
			const struct ulib_toqd_cash *tmpl,
			struct ulib_shea_esnd *esnd,
			uint64_t nsnd
		    );
static inline int   ulib_shea_post_data_phdr_wait( /* foo11 */
			struct ulib_toqc *toqc,
			struct ulib_toqd_cash *toqd,
			struct ulib_shea_esnd *esnd
		    );
static inline void  ulib_shea_cash_data_data( /* foo12 */
			struct ulib_toqd_cash toqd[2],
			const struct ulib_toqd_cash *tmpl,
			struct ulib_shea_esnd *esnd,
			uint64_t nsnd
		    );
static inline int   ulib_shea_post_data_data( /* foo12 */
			struct ulib_toqc *toqc,
			struct ulib_toqd_cash toqd[2],
			struct ulib_shea_esnd *esnd
		    );

/* ulib_shea_data() */
int ulib_shea_foo9(
    struct ulib_toqc *toqc,
    struct ulib_toqc_cash *cash_tmpl,
    struct ulib_toqc_cash *cash_real,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;
    uint64_t nava, nsnd;

    ENTER_RC_C(uc);

    nava = ulib_shea_cntr_diff(&esnd->cntr[0]);

    if (esnd->d_st == DATA_FULL) {
        if (0) {
            if (esnd->l_st == SEND_EXCL) {
                printf("#%d\tnava %"PRIu64" (pc %u cc %u)\n", __LINE__,
                       nava, esnd->cntr[0].ct_s.pcnt, esnd->cntr[0].ct_s.ccnt);
                fflush(stdout);
            }
        }
	if (nava <= 1) {
	    RETURN_OK_C(uc);
	}
	esnd->d_st = DATA_GOAH; /* go ahead */
    }
    if (esnd->d_st == DATA_GOAH) {
	int go_wait = 0;

	nsnd = ulib_shea_data_rblk(esnd->data);
	assert(nsnd > 0);

	if (nsnd > nava) {
	    /* if nava == 1 then nsnd = 0 */
	    if (nava > 0) {
		nsnd = nava - 1; go_wait = 1;
	    }
	    else {
		nsnd = 0; go_wait = 2;
	    }
	}
if (0) {
if (esnd->l_st == SEND_EXCL) {
printf("#%d\tnsnd %"PRIu64" nava %"PRIu64"\n", __LINE__, nsnd, nava);
fflush(stdout);
}
}
	if (nsnd > 0) {
if (0) {
printf("cash tmpl putd %2ld %2ld real %2ld %2ld\n",
cash_tmpl->putd[0].size, cash_tmpl->putd[1].size,
cash_real->putd[0].size, cash_real->putd[1].size);
}
	    if ( 1 /* cash_real->putd[0].size == 0 */ ) {
		const struct ulib_utof_cash *rcsh = &cash_tmpl->addr[0];
		const struct ulib_utof_cash *lcsh = &cash_real->addr[1];
		const uint64_t roff = 0, loff = 0, leng = 0;
		const uint64_t edat = 0;
		const unsigned long uflg =  0
				     /* | UTOFU_ONESIDED_FLAG_STRONG_ORDER */
#ifdef	CONF_ULIB_UTOF_FIX2
					| UTOFU_ONESIDED_FLAG_TCQ_NOTICE
#endif	/* CONF_ULIB_UTOF_FIX2 */
					;
		struct ulib_toqd_cash putd_tmpl[2];

		uc = ulib_desc_put1_from_cash_data(rcsh, lcsh,
			roff, loff, leng, edat, uflg, putd_tmpl);
		if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

		ulib_shea_cash_data_data(cash_real->putd,
		    putd_tmpl, esnd, nsnd);
	    }
	    uc =  ulib_shea_post_data_data(toqc, cash_real->putd, esnd);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	    if ( 1 /* cash_real->phdr[0].size == 0 */ ) {
		ulib_shea_cash_data_phdr(cash_real->phdr,
		    cash_tmpl->phdr, esnd, nsnd);
	    }
	    uc = ulib_shea_post_data_phdr(toqc, cash_real->phdr, esnd);
	    if (uc != UTOFU_SUCCESS) {
		/* cash_real->phdr[0].size = 0; */
		RETURN_BAD_C(uc);
	    }
	    /* cash_real->phdr[0].size = 0; */
	    /* update pcnt and boff */
	    {
		(void) ulib_shea_data_incr_boff(esnd->data, nsnd);
		esnd->cntr[0].ct_s.pcnt += nsnd;
	    }
	}
if (0) {
if (esnd->l_st == SEND_EXCL) {
if (go_wait == 0) {
printf("#%d\tgo_wait %d\n", __LINE__, go_wait);
}
else {
printf("#%d\tgo_wait %d (pc %u cc %u)\n", __LINE__,
go_wait, esnd->cntr[0].ct_s.pcnt, esnd->cntr[0].ct_s.ccnt);
}
fflush(stdout);
}
}
	if (go_wait != 0) {
	    if (go_wait == 1) {
		if ( 1 /* cash_real->phdr[0].size == 0 */ ) {
		    ulib_shea_cash_data_phdr_wait(cash_real->phdr,
			cash_tmpl->phdr, esnd, nsnd);
		}
		uc = ulib_shea_post_data_phdr_wait(toqc, cash_real->phdr, esnd);
		if (uc != UTOFU_SUCCESS) {
		    /* cash_real->phdr[0].size = 0; */
		    RETURN_BAD_C(uc);
		}
		/* cash_real->phdr[0].size = 0; */
	    }
	    else {
		assert(go_wait == 2);
		if ( 1 /* cash_real->puti[0].size == 0 */) {
		    ulib_shea_cash_data_wait(cash_real->puti,
			cash_tmpl->puti, esnd);
		}
		uc = ulib_shea_post_data_wait(toqc, cash_real->puti, esnd);
		if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	    }
	    esnd->d_st = DATA_FULL;
	}
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/*
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             srci                              | [0]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * | type|z|t| aoff|      llen     |      ///      |      ///      | [1]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             nblk                              | [2]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             mblk                              | [3]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             utag (1)                          | [4]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             utag (2)                          | [5]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             c_id                              | [6]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             seqn                              | [7]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 */

struct ulib_shea_phlh {
    uint32_t    srci;
    uint8_t     type : 3; /* ULIB_SHEA_PH_LARGE */
    uint8_t     zflg : 1;
    uint8_t     tflg : 1;
    uint8_t     aoff : 3;
    uint8_t     llen;
    uint8_t     rsv1;
    uint8_t     rsv2;
    uint32_t    nblk;
    uint32_t    mblk;
    uint64_t    utag;
    uint32_t    c_id;
    uint32_t    seqn;
};

struct ulib_shea_phlc {
    uint32_t    srci;
    uint8_t     type : 3; /* ULIB_SHEA_PH_LARGE_CONT */
    uint8_t     zflg : 1;
    uint8_t     tflg : 1;
    uint8_t     aoff : 3;
    uint8_t     llen;
    uint8_t     rsv1;
    uint8_t     rsv2;
    uint32_t    nblk;
    uint32_t    boff;
    uint64_t    utag;
    uint32_t    c_id;
    uint32_t    seqn;
};

/*
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             srci                              | [0]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * | type|z|t| /// |      ///      |      ///      |      ///      | [1]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             addr (1)                          | [2]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             addr (2)                          | [3]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             cntr (1)                          | [4]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             cntr (2)                          | [5]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             c_id                              | [6]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 * |                             seqn                              | [7]
 * |-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|-+-+-+-+-+-+-+-|
 */

struct ulib_shea_phwl {
    uint32_t    srci;     /* */
    uint8_t     type : 3; /* ULIB_SHEA_PH_WAIT[SA] */
    uint8_t     zflg : 1;
    uint8_t     tflg : 1;
    uint8_t     rsv4 : 3;
    uint8_t     rsv3;
    uint8_t     rsv1;
    uint8_t     rsv2;
    uint64_t    addr;   /* struct tofa for receiving wait completion */
    uint64_t    cntr;   /* pair of counters, pcnt, ccnt for wait ??? */
    uint32_t    c_id;   /* XXXX ??? */
    uint32_t    seqn;   /* producer's counter */
};

union ulib_shea_ph_u { /* protocol header */
    struct ulib_shea_phlh   phlh;       /* PH_LARGE */
    struct ulib_shea_phlc   phlc;       /* PH_LARGE_CONT */
    struct ulib_shea_phwl   phwl;       /* PH_WAITS, PH_WAITA */
    uint64_t                ui64[4];
    uint32_t                ui32[8];
};

enum {
    ULIB_SHEA_PH_UNDEF = 0,
    ULIB_SHEA_PH_LARGE = 1,
    ULIB_SHEA_PH_LARGE_CONT = 2,
    ULIB_SHEA_PH_WAITS = 3, /* sender wait */
    ULIB_SHEA_PH_WAITA = 4, /* receiver ack */
};

#define ULIB_SHEA_PH_MARKER_W	    (-1U)
#define ULIB_SHEA_PH_MARKER_L	    (0)

char *
phdr2string(union ulib_shea_ph_u *phdr, char *buf, ssize_t size)
{
    snprintf(buf, size, "srci(%d) type(%d) zflg(%d) tflg(%d) llen(%d) nblk(%d) mblk(%d) utag(%ld) seqn(%d)",
             phdr->phlh.srci, phdr->phlh.type, phdr->phlh.zflg,
             phdr->phlh.tflg, phdr->phlh.llen, phdr->phlh.nblk, 
             phdr->phlh.mblk, phdr->phlh.utag, phdr->phlh.seqn);
    return buf;
}

static inline void ulib_shea_make_phdr(
    struct ulib_shea_esnd *esnd,
    uint32_t nsnd,
    union ulib_shea_ph_u phdr[1]
)
{
    /* int uc = UTOFU_SUCCESS; */

    /* SHEA_PHDR_MAKE_L(PHDR_,SEQN_,MBLK_,NBLK_,BOFF_,LLEN_,RANK_,UTAG_) */
    {
	const uint32_t seqn = esnd->cntr[0].ct_s.pcnt;
	const uint32_t nblk = (uint32_t) ulib_shea_data_nblk(esnd->data);
	const uint32_t boff = (uint32_t) ulib_shea_data_boff(esnd->data);
	uint32_t llen = 0;
	const uint32_t rank = ulib_shea_data_rank(esnd->data); /* vpid */
	const uint64_t utag = ulib_shea_data_utag(esnd->data);
	const uint64_t flag = ulib_shea_data_flag(esnd->data);

	assert(nblk <= (1 << 24));
	assert((boff + nsnd) <= nblk);
	if ((boff + nsnd) == nblk) {
	    llen = ulib_shea_data_llen(esnd->data);
	}

        if (boff == 0) {
            phdr->ui64[0] = 0;
            phdr->phlh.seqn = seqn;
            phdr->phlh.type = ULIB_SHEA_PH_LARGE;
            phdr->phlh.tflg = ((flag & ULIB_SHEA_DATA_TFLG) != 0);
            phdr->phlh.zflg = ((flag & ULIB_SHEA_DATA_ZFLG) != 0);
            /* phdr->phlh.aoff = 0; */
            phdr->phlh.llen = llen;
            /* phdr->phlh.rsv1 = 0; */
            /* phdr->phlh.rsv2 = 0; */
            phdr->phlh.nblk = nsnd;
            phdr->phlh.mblk = nblk;
            phdr->phlh.srci = rank;
            phdr->phlh.c_id = 0; /* XXX ULIB_SHEA_PH_MARKER_L */
            phdr->phlh.utag = utag;
        } else {
            phdr->ui64[0] = 0;
            phdr->phlc.seqn = seqn;
            phdr->phlc.type = ULIB_SHEA_PH_LARGE_CONT;
            phdr->phlc.tflg = ((flag & ULIB_SHEA_DATA_TFLG) != 0);
            phdr->phlc.zflg = ((flag & ULIB_SHEA_DATA_ZFLG) != 0);
            /* phdr->phlc.aoff = 0; */
            phdr->phlc.llen = llen;
            /* phdr->phlc.rsv1 = 0; */
            /* phdr->phlc.rsv2 = 0; */
            phdr->phlc.nblk = nsnd;
            phdr->phlc.boff = boff;
            phdr->phlc.srci = rank;
            phdr->phlc.c_id = 0; /* XXX ULIB_SHEA_PH_MARKER_L */
            phdr->phlc.utag = utag;
        }
    }
    {
        char    buf[128];
        fprintf(stderr, "YIPROTOCOL: %s phdr[%s]\n", __func__,
                phdr2string(phdr, buf, 128));
    }

    return /* uc */;
}

static inline void ulib_shea_cash_data_phdr( /* foo10 */
    struct ulib_toqd_cash *toqd,
    const struct ulib_toqd_cash *tmpl,  /* template */
    struct ulib_shea_esnd *esnd,
    uint64_t nsnd
)
{
    union ulib_shea_ph_u phdr[1];
    uint64_t offs;

    /* phdr and offs */
    {
	uint32_t hloc; /* header location */

	ulib_shea_make_phdr(esnd, nsnd, phdr);
	assert(sizeof (phdr) == 32);

	/* powerof2(ULIB_SHEA_MBLK) */
	assert((((ULIB_SHEA_MBLK) - 1) & (ULIB_SHEA_MBLK)) == 0);
        /*
         * hloc is offset of uinon  ulib_shea_ph_u 2019/04/24
         */
	hloc = phdr->phlh.seqn & (ULIB_SHEA_MBLK-1);

	offs = hloc * sizeof (phdr);
        //fprintf(stderr, "YIPROTOCOL: %s hloc(%d) offs(0x%lx)\n", __func__, hloc, offs);
    }
    /* puti_le32 */
    {
	uint64_t *desc = toqd->desc;
	const void *ldat = phdr;

	/* desc[] */
	{
	    if (tmpl->size == 64) { /* likely() */
		/* memcpy(desc, tmpl->desc, 64); */
		desc[0] = tmpl->desc[0]; desc[1] = tmpl->desc[1];
		desc[2] = tmpl->desc[2]; desc[3] = tmpl->desc[3];
		desc[4] = tmpl->desc[4]; desc[5] = tmpl->desc[5];
		desc[6] = tmpl->desc[6]; desc[7] = tmpl->desc[7];
	    }
	    else {
		memcpy(desc, tmpl->desc, tmpl->size);
	    }
#ifndef	CONF_ULIB_UTOF_FIX4
	    {
		int uc;
		uc = utofu_modify_put_piggyback_lcl_data(desc, ldat);
		if (uc != UTOFU_SUCCESS) {
		    /* YYY abort */
		}
		uc = utofu_modify_put_piggyback_rmt_stadd_add(desc, offs);
		if (uc != UTOFU_SUCCESS) {
		    /* YYY abort */
		}
	    }
#else	/* CONF_ULIB_UTOF_FIX4 */
	    desc[1+0] = ((uint64_t *)ldat)[1]; /* write piggyback buffer */
	    desc[2+0] = ((uint64_t *)ldat)[2]; /* write piggyback buffer */
	    desc[3+0] = ((uint64_t *)ldat)[3]; /* write piggyback buffer */
	    desc[3+4] = ((uint64_t *)ldat)[0]; /* put piggyback */
	    desc[2+4] += offs;
#endif	/* CONF_ULIB_UTOF_FIX4 */
	}
	/* size */
	toqd->size = tmpl->size;
	/* ackd */
	memcpy(toqd->ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
	toqd->ackd[0].rmt_stadd += offs;
        //fprintf(stderr, "\t\t: rmt_stadd(0x%lx) ackd[0].rmt_stadd(0x%lx)\n", desc[2+4], toqd->ackd[0].rmt_stadd);
    }

    return ;
}

static inline int ulib_shea_post_data_phdr( /* foo10 */
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* puti_le32 */
    assert(toqd->size != 0);
    {
	uint64_t * const retp = 0;

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_data_phdr() */
int ulib_shea_foo10(
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd,
    uint64_t nsnd
)
{
    int uc = UTOFU_SUCCESS;
    union ulib_shea_ph_u phdr[1];
    uint64_t offs;

    ENTER_RC_C(uc);

    {
	uint32_t hloc; /* header location */
	ulib_shea_make_phdr(esnd, nsnd, phdr);
	assert(sizeof (phdr) <= 32);

	/* powerof2(ULIB_SHEA_MBLK) */
	assert((((ULIB_SHEA_MBLK) - 1) & (ULIB_SHEA_MBLK)) == 0);
        /*
         * hloc is offset of uinon  ulib_shea_ph_u 2019/04/24
         */
	hloc = phdr->phlh.seqn & (ULIB_SHEA_MBLK-1);

	offs = hloc * sizeof (phdr);
        //fprintf(stderr, "YIPROTOCOL: %s hloc(%d) offs(%ld)\n", __func__, hloc, offs);
    }
    /* puti */
    {
	uint64_t desc[8];
	uint64_t * const retp = 0;
	const void *ldat = phdr;

	/* desc[] */
	{
	    assert(toqd->size <= sizeof (desc));
	    if (toqd->size == 64) { /* likely() */
		/* memcpy(desc, toqd->desc, 64); */
		desc[0] = toqd->desc[0]; desc[1] = toqd->desc[1];
		desc[2] = toqd->desc[2]; desc[3] = toqd->desc[3];
		desc[4] = toqd->desc[4]; desc[5] = toqd->desc[5];
		desc[6] = toqd->desc[6]; desc[7] = toqd->desc[7];
	    }
	    else {
		memcpy(desc, toqd->desc, toqd->size);
	    }
	}
#ifndef	CONF_ULIB_UTOF_FIX4
	uc = utofu_modify_put_piggyback_lcl_data(desc, ldat);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	uc = utofu_modify_put_piggyback_rmt_stadd_add(desc, offs);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#else	/* CONF_ULIB_UTOF_FIX4 */
	desc[1+0] = ((uint64_t *)ldat)[1]; /* write piggyback buffer */
	desc[2+0] = ((uint64_t *)ldat)[2]; /* write piggyback buffer */
	desc[3+0] = ((uint64_t *)ldat)[3]; /* write piggyback buffer */
	desc[3+4] = ((uint64_t *)ldat)[0]; /* put piggyback */
	desc[2+4] += offs;
#endif	/* CONF_ULIB_UTOF_FIX4 */

	uc = ulib_toqc_post(toqc, desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline void ulib_shea_make_phdr_wait(
    struct ulib_shea_esnd *esnd,
    uint32_t nsnd,
    union ulib_shea_ph_u phdr[1]
)
{
    /* int uc = UTOFU_SUCCESS; */

    /* SHEA_PHDR_MAKE_L(PHDR_,SEQN_,MBLK_,NBLK_,BOFF_,LLEN_,RANK_,UTAG_) */
    {
	const uint32_t pcnt = esnd->cntr[0].ct_s.pcnt;
	const uint32_t rank = ulib_shea_data_rank(esnd->data); /* vpid */
	const uint64_t flag = ulib_shea_data_flag(esnd->data);

	phdr->ui64[0] = 0;
	phdr->phwl.srci = rank;
	phdr->phwl.type = ULIB_SHEA_PH_WAITS;
	phdr->phwl.tflg = ((flag & ULIB_SHEA_DATA_TFLG) != 0);
	phdr->phwl.zflg = ((flag & ULIB_SHEA_DATA_ZFLG) != 0);
	/* phdr->phwl.rsv4 = 0; */
	/* phdr->phwl.srv3 = 0; */
	/* phdr->phwl.rsv1 = 0; */
	/* phdr->phwl.rsv2 = 0; */
	phdr->phwl.addr = esnd->self.addr.va64;
	phdr->phwl.cntr = ulib_shea_cntr_ui64(pcnt + nsnd /*pc*/, pcnt /*cc*/);
	phdr->phwl.c_id = ULIB_SHEA_PH_MARKER_W;
	phdr->phwl.seqn = pcnt; /* seqn */
if (0) {
printf("\tW: S->R pcnt %u ccnt %u rblk %u\n",
pcnt + nsnd, pcnt, (uint32_t)ulib_shea_data_rblk(esnd->data));
}
    }

    return /* uc */;
}

static inline void ulib_shea_cash_data_phdr_wait( /* foo11 */
    struct ulib_toqd_cash *toqd,
    const struct ulib_toqd_cash *tmpl,
    struct ulib_shea_esnd *esnd,
    uint64_t nsnd
)
{
    union ulib_shea_ph_u phdr[1];
    uint64_t offs;

    /* phdr and offs */
    {
	uint32_t hloc; /* header location */

	ulib_shea_make_phdr_wait(esnd, nsnd, phdr);
	assert(sizeof (phdr) == 32);

	/* powerof2(ULIB_SHEA_MBLK) */
	assert((((ULIB_SHEA_MBLK) - 1) & (ULIB_SHEA_MBLK)) == 0);
	hloc = phdr->phlh.seqn & (ULIB_SHEA_MBLK-1);

	offs = hloc * sizeof (phdr);
    }
    /* puti_le32 */
    {
	uint64_t *desc = toqd->desc;
	const void *ldat = phdr;

	/* desc[] */
	{
	    if (tmpl->size == 64) { /* likely() */
		/* memcpy(desc, tmpl->desc, 64); */
		desc[0] = tmpl->desc[0]; desc[1] = tmpl->desc[1];
		desc[2] = tmpl->desc[2]; desc[3] = tmpl->desc[3];
		desc[4] = tmpl->desc[4]; desc[5] = tmpl->desc[5];
		desc[6] = tmpl->desc[6]; desc[7] = tmpl->desc[7];
	    }
	    else {
		memcpy(desc, tmpl->desc, tmpl->size);
	    }
#ifndef	CONF_ULIB_UTOF_FIX4
	    {
		int uc;
		uc = utofu_modify_put_piggyback_lcl_data(desc, ldat);
		if (uc != UTOFU_SUCCESS) {
		    /* YYY abort */
		}
		uc = utofu_modify_put_piggyback_rmt_stadd_add(desc, offs);
		if (uc != UTOFU_SUCCESS) {
		    /* YYY abort */
		}
	    }
#else	/* CONF_ULIB_UTOF_FIX4 */
	    desc[1+0] = ((uint64_t *)ldat)[1]; /* write piggyback buffer */
	    desc[2+0] = ((uint64_t *)ldat)[2]; /* write piggyback buffer */
	    desc[3+0] = ((uint64_t *)ldat)[3]; /* write piggyback buffer */
	    desc[3+4] = ((uint64_t *)ldat)[0]; /* put piggyback */
	    desc[2+4] += offs;
#endif	/* CONF_ULIB_UTOF_FIX4 */
	}
	/* size */
	toqd->size = tmpl->size;
	/* ackd */
	memcpy(toqd->ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
	toqd->ackd[0].rmt_stadd += offs;
    }

    return ;
}

static inline int ulib_shea_post_data_phdr_wait( /* foo11 */
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* puti_le32 */
    assert(toqd->size != 0);
    {
	uint64_t * const retp = 0;

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_data_phdr_wait() */
int ulib_shea_foo11(
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd,
    uint64_t nsnd
)
{
    int uc = UTOFU_SUCCESS;
    union ulib_shea_ph_u phdr[1];
    uint64_t offs;

    ENTER_RC_C(uc);

    {
	uint32_t hloc; /* header location */

	ulib_shea_make_phdr_wait(esnd, nsnd, phdr);
	assert(sizeof (phdr) <= 32);

	/* powerof2(ULIB_SHEA_MBLK) */
	assert((((ULIB_SHEA_MBLK) - 1) & (ULIB_SHEA_MBLK)) == 0);
	hloc = phdr->phlh.seqn & (ULIB_SHEA_MBLK-1);

	offs = hloc * sizeof (phdr);
    }
    /* puti */
    {
	uint64_t desc[8];
	uint64_t * const retp = 0;
	const void *ldat = phdr;

	/* desc[] */
	{
	    assert(toqd->size <= sizeof (desc));
	    if (toqd->size == 64) { /* likely() */
		/* memcpy(desc, toqd->desc, 64); */
		desc[0] = toqd->desc[0]; desc[1] = toqd->desc[1];
		desc[2] = toqd->desc[2]; desc[3] = toqd->desc[3];
		desc[4] = toqd->desc[4]; desc[5] = toqd->desc[5];
		desc[6] = toqd->desc[6]; desc[7] = toqd->desc[7];
	    }
	    else {
		memcpy(desc, toqd->desc, toqd->size);
	    }
	}
#ifndef	CONF_ULIB_UTOF_FIX4
	uc = utofu_modify_put_piggyback_lcl_data(desc, ldat);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	uc = utofu_modify_put_piggyback_rmt_stadd_add(desc, offs);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#else	/* CONF_ULIB_UTOF_FIX4 */
	desc[1+0] = ((uint64_t *)ldat)[1]; /* write piggyback buffer */
	desc[2+0] = ((uint64_t *)ldat)[2]; /* write piggyback buffer */
	desc[3+0] = ((uint64_t *)ldat)[3]; /* write piggyback buffer */
	desc[3+4] = ((uint64_t *)ldat)[0]; /* put piggyback */
	desc[2+4] += offs;
#endif	/* CONF_ULIB_UTOF_FIX4 */

	uc = ulib_toqc_post(toqc, desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline void ulib_shea_cash_data_data( /* foo12 */
    struct ulib_toqd_cash toqd[2],
    const struct ulib_toqd_cash *tmpl,
    struct ulib_shea_esnd *esnd,
    uint64_t nsnd
)
{
    /* roff, loff, leng */
    {
	uint32_t hloc; /* header location */
	uint32_t boff; /* block offset */
	uint32_t nblk; /* */

	/* powerof2(ULIB_SHEA_MBLK) */
	assert((((ULIB_SHEA_MBLK) - 1) & (ULIB_SHEA_MBLK)) == 0);
	hloc = esnd->cntr[0].ct_s.pcnt & (ULIB_SHEA_MBLK-1);

	boff = ulib_shea_data_boff(esnd->data);
	nblk = ulib_shea_data_nblk(esnd->data);

	if ((hloc + nsnd) <= ULIB_SHEA_MBLK /* dsiz / bsiz */ ) {
	    uint64_t roff = hloc * ULIB_SHEA_DBLK /* bsiz */;
	    uint64_t loff = boff * ULIB_SHEA_DBLK;
	    uint64_t leng = nsnd * ULIB_SHEA_DBLK;

	    /* last fragment */
	    /* (loff + leng) <= (nblk * ULIB_SHEA_DBLK) */
	    if ((boff + nsnd) >= nblk) {
		uint32_t llen = ulib_shea_data_llen(esnd->data);

		/* (loff + leng) == (nblk * ULIB_SHEA_DBLK) */
		assert((boff + nsnd) == nblk);
		assert(llen <= ULIB_SHEA_DBLK);
		leng -= (ULIB_SHEA_DBLK - llen);
	    }
	    /* desc */
	    {
		uint64_t *desc = toqd[0].desc;

		memcpy(desc, tmpl->desc, tmpl->size);
#ifndef	CONF_ULIB_UTOF_FIX4
		uc = utofu_modify_put_length(desc, leng);
		uc = utofu_modify_put_rmt_stadd_add(desc, roff);
		uc = utofu_modify_put_lcl_stadd_add(desc, loff);
#else	/* CONF_ULIB_UTOF_FIX4 */
		desc[1+0] += leng; /* XXX initial leng = 0 */
		desc[2+0] += roff;
		desc[3+0] += loff;
#endif	/* CONF_ULIB_UTOF_FIX4 */
	    }
	    /* size */
	    toqd[0].size = tmpl->size;
	    /* ackd */
	    memcpy(toqd[0].ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
	    toqd[0].ackd[0].rmt_stadd += (roff + leng); /* XXX leng = 0 */

	    toqd[1].size = 0;
	}
	else { /* wrap around */
	    uint32_t fblk = ULIB_SHEA_MBLK - hloc;
	    uint64_t roff = hloc * ULIB_SHEA_DBLK /* bsiz */;
	    uint64_t loff = boff * ULIB_SHEA_DBLK;
	    uint64_t leng = fblk * ULIB_SHEA_DBLK;

if (0) {
printf("\t[0] roff %4u leng %4u [1] roff %4u leng %4"PRIu64"\n",
hloc, fblk, 0, nsnd - fblk);
}
	    /* === toqd[0] === */
	    /* desc */
	    {
		uint64_t *desc = toqd[0].desc;

		memcpy(desc, tmpl->desc, tmpl->size);
#ifndef	CONF_ULIB_UTOF_FIX4
		uc = utofu_modify_put_length(desc, leng);
		uc = utofu_modify_put_rmt_stadd_add(desc, roff);
		uc = utofu_modify_put_lcl_stadd_add(desc, loff);
#else	/* CONF_ULIB_UTOF_FIX4 */
		desc[1+0] += leng; /* XXX initial leng = 0 */
		desc[2+0] += roff;
		desc[3+0] += loff;
#endif	/* CONF_ULIB_UTOF_FIX4 */
	    }
	    /* size */
	    toqd[0].size = tmpl->size;
	    /* ackd */
	    memcpy(toqd[0].ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
	    toqd[0].ackd[0].rmt_stadd += (roff + leng); /* XXX leng = 0 */

	    /* === toqd[1] === */
	    roff = 0;
	    loff = (boff + fblk) * ULIB_SHEA_DBLK;
	    leng = (nsnd - fblk) * ULIB_SHEA_DBLK;

	    /* last fragment */
	    /* (loff + leng) <= (nblk * ULIB_SHEA_DBLK) */
	    if ((boff + nsnd) >= nblk) {
		uint32_t llen = ulib_shea_data_llen(esnd->data);

		/* (loff + leng) == (nblk * ULIB_SHEA_DBLK) */
		assert((boff + nsnd) == nblk);
		assert(llen <= ULIB_SHEA_DBLK);
		leng -= (ULIB_SHEA_DBLK - llen);
	    }

	    /* desc */
	    {
		uint64_t *desc = toqd[1].desc;

		memcpy(desc, tmpl->desc, tmpl->size);
#ifndef	CONF_ULIB_UTOF_FIX4
		uc = utofu_modify_put_length(desc, leng);
		uc = utofu_modify_put_rmt_stadd_add(desc, roff);
		uc = utofu_modify_put_lcl_stadd_add(desc, loff);
#else	/* CONF_ULIB_UTOF_FIX4 */
		desc[1+0] += leng; /* XXX initial leng = 0 */
		desc[2+0] += roff;
		desc[3+0] += loff;
#endif	/* CONF_ULIB_UTOF_FIX4 */
	    }
	    /* size */
	    toqd[1].size = tmpl->size;
	    /* ackd */
	    memcpy(toqd[1].ackd, tmpl->ackd, sizeof (tmpl->ackd)); /* XXX */
	    toqd[1].ackd[0].rmt_stadd += (roff + leng); /* XXX leng = 0 */
	}
    }

    return ;
}

static inline int ulib_shea_post_data_data( /* foo12 */
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash toqd[2],
    struct ulib_shea_esnd *esnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* put1 */
    assert(toqd->size != 0);
    {
	uint64_t * const retp = 0;

	uc = ulib_toqc_post(toqc, toqd->desc, toqd->size, retp, toqd->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
    if (toqd[1].size != 0) {
	struct ulib_toqd_cash *toq1 = &toqd[1];
	uint64_t * const retp = 0;

	uc = ulib_toqc_post(toqc, toq1->desc, toq1->size, retp, toq1->ackd,
		&esnd->r_no);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ulib_shea_data_data() */
int ulib_shea_foo12(
    struct ulib_toqc *toqc,
    struct ulib_toqd_cash *toqd,
    struct ulib_shea_esnd *esnd,
    uint64_t nsnd
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline volatile union ulib_shea_ph_u *ulib_shea_recv_hdlr_phdr(
    volatile struct ulib_shea_ercv *ercv
)
{
    volatile union ulib_shea_ph_u *phdr = ercv->phdr;

    //fprintf(stderr, "\tYIUTOFU***: ercv->cntr.ct_s.ccnt(%d)\n", ercv->cntr.ct_s.ccnt);
    /* powerof2(ULIB_SHEA_MBLK) */
    assert((((ULIB_SHEA_MBLK) - 1) & (ULIB_SHEA_MBLK)) == 0);
    /* phdr */
    {
	uint32_t hloc = ercv->cntr.ct_s.ccnt & (ULIB_SHEA_MBLK-1);
	phdr += hloc;
    }

    return phdr;
}

static inline void /* int */ ulib_shea_recv_hndr_full(
    volatile struct ulib_shea_ercv *ercv,
    struct ulib_shea_full *full
)
{
    /* int uc = UTOFU_SUCCESS; */
    volatile union ulib_shea_ph_u *phdr;
    union ulib_shea_ph_u phwl;

    assert(ercv->cntr.ct_s.pcnt != ercv->cntr.ct_s.ccnt);
    full->addr.va64 = ULIB_SHEA_NIL8;

    /* phdr */
    phdr = ulib_shea_recv_hdlr_phdr(ercv);

    phwl.phwl.seqn = phdr->phwl.seqn;
    fprintf(stderr, "\t\t: phwl.phwl.seqn(%d) ercv->cntr.ct_s.ccn(%d)\n",
            phwl.phwl.seqn, ercv->cntr.ct_s.ccnt);
    if (phwl.phwl.seqn != ercv->cntr.ct_s.ccnt) {
	full->cntr.ct_s.pcnt = ULIB_SHEA_PH_UNDEF;
	goto bad; /* XXX - is not an error */
    }
    phwl.phwl.type = phdr->phwl.type;

    if (phwl.phwl.type == ULIB_SHEA_PH_WAITS) {
	/* copy phdr */
	phwl.ui64[0] = phdr->ui64[0];
	phwl.ui64[1] = phdr->ui64[1];
	phwl.ui64[2] = phdr->ui64[2];
	phwl.ui64[3] = phdr->ui64[3];
	assert(phwl.phwl.type == ULIB_SHEA_PH_WAITS);
	assert(phwl.phwl.c_id == ULIB_SHEA_PH_MARKER_W);

	/* set full */
	full->cntr.ct64 = phwl.phwl.cntr;
	full->addr.va64 = phwl.phwl.addr;
	assert(full->addr.va64 != ULIB_SHEA_NIL8);
	assert(full->cntr.ct_s.ccnt == phwl.phwl.seqn);
	assert(full->cntr.ct_s.ccnt == ercv->cntr.ct_s.ccnt);

#ifndef	DEBUG_SHEA_PHWL
	{
	    phdr->phwl.type = ULIB_SHEA_PH_WAITA;
	}
#else	/* DEBUG_SHEA_PHWL */
	{
	    union ulib_shea_ph_u umod;
	    uint64_t sval, fval; /* fetch and store value */

	    umod.ui64[0] = phwl.ui64[0];
	    umod.phwl.type = ULIB_SHEA_PH_WAITA;

	    sval = umod.ui64[0];
	    fval = atomic_exchange( &phdr->ui64[0], sval ); /* YYY */
	    if (fval != phwl.ui64[0]) {
		/* YYY abort  */
	    }
	}
#endif	/* DEBUG_SHEA_PHWL */
    }
    else if (phwl.phwl.type == ULIB_SHEA_PH_WAITA) {
	/* full->addr.va64 = ULIB_SHEA_NIL8; */
	full->cntr.ct_s.pcnt = ULIB_SHEA_PH_WAITA /* phwl.phwl.type */;
    }
    else {
	/* wait for overwriting last word in ULIB_SHEA_PH_WAITS */
	{
	    unsigned long loop = 1000; /* XXX */

	    do {
		if (phdr->phwl.c_id != ULIB_SHEA_PH_MARKER_W) {
		    break;
		}
	    } while (loop-- == 1);
	    if (loop == 0) {
printf("%s():%d\ttype %d loop %ld c_id %u\n", __func__, __LINE__,
phwl.phwl.type, loop, phdr->phwl.c_id);
		/* YYY abort  */
	    }
	}
	/* full->addr.va64 = ULIB_SHEA_NIL8; */
	full->cntr.ct_s.pcnt = phwl.phwl.type;
    }

bad:
    return /* uc */;
}

static inline void ulib_shea_recv_hndr_seqn_wrap(
    volatile struct ulib_shea_ercv *ercv,
    uint32_t nblk
)
{
    /* int uc = UTOFU_SUCCESS; */
    const uint32_t lcnt = UINT32_MAX - (ULIB_SHEA_MBLK-1);
    const uint32_t ccnt = ercv->cntr.ct_s.ccnt;

    assert(nblk <= ULIB_SHEA_MBLK);

    if (ccnt >= lcnt) {
	volatile union ulib_shea_ph_u *phdr = ercv->phdr;
	uint32_t ii, ni, iv, hloc;

	hloc = ccnt & (ULIB_SHEA_MBLK-1);

	iv = hloc;
	ni = hloc + nblk;
	if (ni > ULIB_SHEA_MBLK) { ni = ULIB_SHEA_MBLK; }

	for (ii = iv; ii < ni; ii++) {
	    phdr[ii].phlh.seqn = 0;
#ifndef	NDEBUG
	    phdr[ii].phlh.type = ULIB_SHEA_PH_UNDEF;
#endif	/* NDEBUG */
	}
	if (iv == 0) {
	    phdr[ 0 /*iv*/ ].phlh.seqn = UINT32_MAX;
#ifndef	NDEBUG
	    phdr[ 0 /*iv*/ ].phlh.type = ULIB_SHEA_PH_UNDEF;
#endif	/* NDEBUG */
	}
    }
    else if ((ccnt + nblk) > lcnt) {
	volatile union ulib_shea_ph_u *phdr = ercv->phdr;
	uint32_t ii, ni, iv, hloc;

	hloc = ccnt & (ULIB_SHEA_MBLK-1);

	iv = 0;
	ni = (hloc + nblk) % ULIB_SHEA_MBLK;

	for (ii = iv; ii < ni; ii++) {
	    phdr[ii].phlh.seqn = 0;
#ifndef	NDEBUG
	    phdr[ii].phlh.type = ULIB_SHEA_PH_UNDEF;
#endif	/* NDEBUG */
	}
	if (iv == 0) { /* ??? */
	    phdr[ 0 /*iv*/ ].phlh.seqn = UINT32_MAX;
#ifndef	NDEBUG
	    phdr[ 0 /*iv*/ ].phlh.type = ULIB_SHEA_PH_UNDEF;
#endif	/* NDEBUG */
	}
    }

    return /* uc */;
}

void ulib_shea_recv_hndr_seqn_init( /* obsolated */
    volatile struct ulib_shea_ercv *ercv
)
{
    /* int uc = UTOFU_SUCCESS; */
    {
	volatile union ulib_shea_ph_u *phdr = ercv->phdr;
	uint32_t ii, ni = ULIB_SHEA_MBLK;

	phdr[0].phlh.seqn = UINT32_MAX;
#ifndef	NDEBUG
	phdr[0].phlh.type = ULIB_SHEA_PH_UNDEF;
#endif	/* NDEBUG */
	for (ii = 1; ii < ni; ii++) {
	    phdr[ii].phlh.seqn = 0;
#ifndef	NDEBUG
	    phdr[ii].phlh.type = ULIB_SHEA_PH_UNDEF;
#endif	/* NDEBUG */
	}
    }
    return /* uc */;
}

static inline void ulib_shea_recv_info(
    volatile struct ulib_shea_ercv *ercv,
    const void *vp_rpkt,
    struct ulib_shea_uexp *rinf
)
{
    volatile union ulib_shea_ph_u *ph_u = (volatile union ulib_shea_ph_u *)vp_rpkt;

    assert(ph_u != 0);

    //fprintf(stderr, "\tYIUTOFU***: %s uexp(%p) ercv->dptr(%p)\n", __func__, rinf, ercv->dptr);
    if (ph_u->phlh.type == ULIB_SHEA_PH_LARGE) {
	rinf->utag = ph_u->phlh.utag;
	rinf->srci = ph_u->phlh.srci;
	rinf->mblk = ph_u->phlh.mblk;
	rinf->boff = 0;
	rinf->flag = ULIB_SHEA_UEXP_FLAG_MBLK;
	rinf->flag |= ((ph_u->phlh.tflg != 0)? ULIB_SHEA_UEXP_FLAG_TFLG: 0);
	rinf->flag |= ((ph_u->phlh.zflg != 0)? ULIB_SHEA_UEXP_FLAG_ZFLG: 0);
    } else if (ph_u->phlh.type == ULIB_SHEA_PH_LARGE_CONT) {
	rinf->utag = ph_u->phlc.utag;
	rinf->srci = ph_u->phlc.srci;
	rinf->mblk = 0;
	rinf->boff = ph_u->phlc.boff;
	rinf->flag  = 0;
	rinf->flag |= ((ph_u->phlc.tflg != 0)? ULIB_SHEA_UEXP_FLAG_TFLG: 0);
	rinf->flag |= ((ph_u->phlc.zflg != 0)? ULIB_SHEA_UEXP_FLAG_ZFLG: 0);
    } else {
	/* YYY abort */
	rinf->utag = UINT64_MAX;
	rinf->srci = UINT32_MAX;
	rinf->flag = 0;
    }

    /* recv buff */
    {
	const uint32_t type = ph_u->phlh.type;
	uint32_t nblk, llen;

	if (type == ULIB_SHEA_PH_LARGE) {
	    nblk = ph_u->phlh.nblk;
	    llen = ph_u->phlh.llen;
	}
	else if (type == ULIB_SHEA_PH_LARGE_CONT) {
	    nblk = ph_u->phlc.nblk;
	    llen = ph_u->phlc.llen;
	}
	else {
	    /* YYY abort */
	    nblk = 0;
	    llen = 0;
	}
	if (ph_u->phlh.zflg != 0) {
	    assert(nblk == 1);
	    assert(llen == 0);
#ifndef	NDEBUG
	    switch (type) {
	    case ULIB_SHEA_PH_LARGE:
		assert(ph_u->phlh.mblk == 1);
		break;
	    /* case ULIB_SHEA_PH_LARGE_CONT: */
	    default:
		assert(type == ULIB_SHEA_PH_LARGE);
		break;
	    }
	    llen = 0;
#endif	/* NDEBUG */
	    nblk = 0; /* XXX 1 --> 0 */
	}
	rinf->nblk = nblk;

	/* rbuf */
	{
	    const uint32_t hloc = ph_u->phlh.seqn % ULIB_SHEA_MBLK;
	    const uint32_t hcnt = ULIB_SHEA_MBLK;
	    const uint32_t bsiz = ULIB_SHEA_DBLK;
	    struct ulib_shea_rbuf *rbuf = &rinf->rbuf;
	    struct iovec *iovs = rbuf->iovs;

	    rbuf->leng = (nblk * bsiz);
	    if (llen > 0) {
		rbuf->leng -= (bsiz - llen);
	    }
	    if ((hloc + nblk) <= hcnt) {
		iovs[0].iov_base = (void *)(((char*)ercv->dptr) + hloc * bsiz);
		iovs[0].iov_len  = (nblk * bsiz);
		if (llen > 0) {
		    iovs[0].iov_len -= (bsiz - llen);
		}
		rbuf->niov = 1;
	    } else {
		iovs[0].iov_base = (void *)(((char*)ercv->dptr) + hloc * bsiz);
		iovs[0].iov_len  = ((hcnt - hloc) * bsiz);
		iovs[1].iov_base = 0;
		iovs[1].iov_len  = ((nblk - (hcnt - hloc)) * bsiz);
		if (llen > 0) {
		    iovs[1].iov_len -= (bsiz - llen);
		}
		rbuf->niov = 2;
	    }
            //fprintf(stderr, "\tYIPROTOCOL***: iovs[0].iov_base(%p) iovs[0].iov_len(%lx)\n", iovs[0].iov_base, iovs[0].iov_len); fflush(stderr);
	}
    }
#ifdef	CONF_ULIB_PERF_SHEA
    memset(rinf->tims, 0, sizeof (rinf->tims));
#endif	/* CONF_ULIB_PERF_SHEA */

    return ;
}

int ulib_shea_recv_hndr_prog(
    struct ulib_toqc *toqc,
    volatile struct ulib_shea_ercv *ercv
)
{
    int uc = UTOFU_SUCCESS;
    volatile union ulib_shea_ph_u *phdr;
    uint32_t nblk;
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tick[4];
#endif	/* CONF_ULIB_PERF_SHEA */

    ENTER_RC_C(uc);

    //fprintf(stderr, "\tYIUTOFU***: %s toqc(%p) ercv(%p)\n", __func__, toqc, ercv);
#ifdef	CONF_ULIB_PERF_SHEA
    tick[0] = ulib_tick_time();
#endif	/* CONF_ULIB_PERF_SHEA */
    //fprintf(stderr, "\tYIUTOFU***: pcnt(%d) ccnt(%d)\n", ercv->cntr.ct_s.pcnt, ercv->cntr.ct_s.ccnt);
    if (ercv->cntr.ct_s.pcnt == ercv->cntr.ct_s.ccnt) {
	goto chck_wait;
    }
#if 0
    {
        volatile union ulib_shea_ph_u   *phdr = (union ulib_shea_ph_u*) ercv->phdr;
        union ulib_shea_ph_u    tmp = *phdr;
        char    buf[128];
        fprintf(stderr, "\tYIUTOFU***: NOT EUQAL Going through...\n");
        fprintf(stderr, "\t\t phdr(%p)=[%s]\n", phdr, phdr2string(&tmp, buf, 128));
    }
#endif /* 0 */
    /* full */
    {
	struct ulib_shea_full full[1];

	ulib_shea_recv_hndr_full(ercv, full);

        //fprintf(stderr, "\tYIUTOFU***: full->addr.va64(%ld) full->ctr.ct_s.pcnt(%d)\n", full->addr.va64, full->cntr.ct_s.pcnt);
	if (full->addr.va64 != ULIB_SHEA_NIL8) { /* ULIB_SHEA_PH_WAITS */
	    /* uc = ulib_shea_data_wake(tocq, addr.va64, ercv) */
	    uc = ulib_shea_foo8(toqc, full->addr.va64, ercv);
	    if (uc != UTOFU_SUCCESS) {
		/* if (uc == EAGAIN) { } */
		ercv->full.addr.va64 = full->addr.va64;
		uc = 0; /* XXX */
	    }
	    goto chck_wait;
	}
	else if (full->cntr.ct_s.pcnt == ULIB_SHEA_PH_WAITA) {
	    goto chck_wait;
	}
	else if (full->cntr.ct_s.pcnt == ULIB_SHEA_PH_UNDEF) {
	    goto chck_wait;
	}
    }

    /* phdr */
    phdr = ulib_shea_recv_hdlr_phdr(ercv);
    {
	volatile union ulib_shea_ph_u *ph = phdr;

        //fprintf(stderr, "\tYIUTOFU***: phlh.seqn(%d) ct_s.ccnt(%d)\n", ph->phlh.seqn, ercv->cntr.ct_s.ccnt);
	if (ph->phlh.seqn != ercv->cntr.ct_s.ccnt) {
	    goto chck_wait;
	}
    }

    if (phdr->phlh.type == ULIB_SHEA_PH_LARGE) {
	assert(phdr->phlh.nblk > 0);
	assert(phdr->phlh.mblk > 0);
	assert(phdr->phlh.mblk <= (1<<24));
	assert(phdr->phlh.nblk <= phdr->phlh.mblk);
	nblk = phdr->phlh.nblk;
    }
    else if (phdr->phlh.type == ULIB_SHEA_PH_LARGE_CONT) {
	assert(phdr->phlc.nblk > 0);
	assert(phdr->phlc.boff > 0);
	nblk = phdr->phlc.nblk;
    }
    else {
	/* YYY abort  */
	assert(phdr->phlh.type == ULIB_SHEA_PH_LARGE);
	nblk = 0; /* YYY */
    }
    assert(nblk <= ULIB_SHEA_MBLK);

    /* callback function */ /* YYY */
    {
	struct ulib_shea_uexp rinf[1];

        /* phdr is now OK not for volatile 2019/04/25 */
	ulib_shea_recv_info(ercv, (union ulib_shea_ph_u *) phdr, rinf);
#ifdef	notdef
	if (rinf->nblk != nblk) {
printf("nblk %u %u\n", rinf->nblk, nblk);
	}
	assert(rinf->nblk == nblk); /* ZFLG */
#endif	/* notdef */
#ifdef	CONF_ULIB_PERF_SHEA
	{   uint64_t tnow = ulib_tick_time();
	    rinf->tims[ 0 /* phdr */ ] = (tnow - tick[0]);
	}
#endif	/* CONF_ULIB_PERF_SHEA */
        //fprintf(stderr, "\tYIUTOFU***: ercv->func(%p)\n", ercv->func);
	if (ercv->func != 0) { /* YYY ulib_icep_recv_call_back */
	    uc = (*ercv->func)(ercv->farg, 0, rinf);
	    if (uc != UTOFU_SUCCESS) { /* YYY EAGAIN */
		uc = 0; /* YYY */
		goto chck_wait;
	    }
	}
    }

    /* reset seqn in phdr */
    ulib_shea_recv_hndr_seqn_wrap(ercv, nblk);

    /* update ccnt */
    ercv->cntr.ct_s.ccnt += nblk;

chck_wait:
    {
	union ulib_shea_va_u addr;

	addr.va64 = ercv->full.addr.va64;
	if (
	    (addr.va64 != ULIB_SHEA_NIL8)
	    && (ercv->full.cntr.ct_s.ccnt <= ercv->cntr.ct_s.ccnt)
	) {
	    if ((ercv->full.cntr.ct_s.ccnt + 1) == ercv->cntr.ct_s.ccnt) {
		assert(ercv->cntr.ct_s.ccnt != ercv->cntr.ct_s.pcnt);
		addr.va64 = ULIB_SHEA_NIL8;
	    }
	    else {
		ercv->full.addr.va64 = ULIB_SHEA_NIL8;
	    }
	} else { /* no space yet */
	    addr.va64 = ULIB_SHEA_NIL8;
	}
        //fprintf(stderr, "\tYIUTOFU***: addr.va64(%ld)\n", addr.va64);
	if (addr.va64 != ULIB_SHEA_NIL8) {
	    /* uc = ulib_shea_data_wake(tocq, addr.va64, ercv) */
	    uc = ulib_shea_foo8(toqc, addr.va64, ercv);
	    if (uc != UTOFU_SUCCESS) {
		/* if (uc == EAGAIN) { } */
		ercv->full.addr.va64 = addr.va64;
		uc = 0; /* XXX */
	    }
	}
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

void ulib_shea_data_init_cash(
    struct ulib_shea_data *data,
    void *toqc_cash,
    void *toqc_cash_tmpl,
    void *toqc_cash_real
)
{
    data->toqc = toqc_cash;
    data->toqc_cash_tmpl = toqc_cash_tmpl;
    data->toqc_cash_real = toqc_cash_real;

    return ;
}

/* #define TEST_CBUF_ESND */

#include "ulib_conv.h"	    /* for union ulib_tofa_u */

#include <stdlib.h>	    /* for free() */
#include <unistd.h>	    /* for sysconf() */
#include <string.h>	    /* for memset() */


int ulib_shea_cbuf_init(struct ulib_shea_cbuf *cbuf)
{
    int uc = UTOFU_SUCCESS;

    /* sanity checks */
    {
	assert(sizeof (struct ulib_shea_esnd) <= 128);
	assert(sizeof (struct ulib_shea_ercv) <= 1024); /* XXX 256 x 4 */
	assert(sizeof (union ulib_shea_ph_u)  == 32);

	/* assert(powerof2(ULIB_SHEA_PAGE)); */
	/* assert(powerof2(ULIB_SHEA_CASH_LINE)); */
	assert((((ULIB_SHEA_PAGE) - 1) & (ULIB_SHEA_PAGE)) == 0);
	assert((((ULIB_SHEA_CASH_LINE) - 1) & (ULIB_SHEA_CASH_LINE)) == 0);
	assert(ULIB_SHEA_PAGE >= (ULIB_SHEA_CASH_LINE * 2));
    }
    /* initialize cbuf[0] */
    {
	memset(cbuf, 0, sizeof (cbuf[0]));

	cbuf->ctrl.stag = -1U;
	cbuf->data.stag = -1U;
	DLST_INIT(&cbuf->free_esnd);

	cbuf->csiz = ULIB_SHEA_PAGE;
	cbuf->dsiz = ULIB_SHEA_PAGE;
	cbuf->bsiz = ULIB_SHEA_CASH_LINE;

	cbuf->hcnt = ULIB_SHEA_MBLK;	/* header count: dsiz / bsiz */
	cbuf->hsiz = sizeof (union ulib_shea_ph_u) * cbuf->hcnt;

	/* cbuf->rcnt = 1; */		/* ercv count XXX */
	cbuf->rsiz = 1024;		/* ercv size */
	assert((cbuf->rsiz + cbuf->hsiz) <= ULIB_SHEA_PAGE);

	cbuf->ssiz = ULIB_SHEA_PAGE - (cbuf->rsiz + cbuf->hsiz);
	cbuf->scnt = cbuf->ssiz / sizeof (struct ulib_shea_esnd);
	assert(cbuf->scnt > 0);
    }
    /* offsets */
    {
	cbuf->cptr_ercv = 0; /* XXX offset from cbuf->cptr */
	cbuf->cptr_hdrs = (void *)((char *)cbuf->cptr_ercv + cbuf->rsiz);
	cbuf->cptr_esnd = (void *)((char *)cbuf->cptr_hdrs + cbuf->hsiz);
    }

    return uc;
}

int ulib_shea_cbuf_fini(struct ulib_shea_cbuf *cbuf)
{
    int uc = UTOFU_SUCCESS;

    if (cbuf->ctrl.stag != -1U) {
	const unsigned long int flag =  0;
        fprintf(stderr, "YI********* cbuf->ctrl.stad(%lx) %s\n", cbuf->ctrl.stad, __func__); fflush(stderr);
	uc = utofu_dereg_mem(cbuf->ctrl.vcqh, cbuf->ctrl.stad, flag);
	if (uc != UTOFU_SUCCESS) { }
	cbuf->ctrl.stag = -1U;
    }
    if (cbuf->data.stag != -1U) {
	const unsigned long int flag =  0;
        fprintf(stderr, "YI********* cbuf->ctrl.stad(%lx) %s\n", cbuf->ctrl.stad, __func__); fflush(stderr);
	uc = utofu_dereg_mem(cbuf->data.vcqh, cbuf->data.stad, flag);
	if (uc != UTOFU_SUCCESS) { }
	cbuf->data.stag = -1U;
    }
    if (cbuf->cptr != 0) {
	free(cbuf->cptr); cbuf->cptr = 0;
    }
    if (cbuf->dptr != 0) {
	free(cbuf->dptr); cbuf->dptr = 0;
    }

    return uc;
}

int ulib_shea_cbuf_enab_mema(
    utofu_vcq_hdl_t vcqh,
    void *bptr,
    size_t bsiz,
    unsigned int stag,
    struct ulib_shea_mema *mema
)
{
    int uc = UTOFU_SUCCESS;
    uint64_t offs;

    ENTER_RC_C(uc);

    if (stag == -1U) {
	uc = UTOFU_ERR_INVALID_STAG; RETURN_BAD_C(uc);
    }
    if ((bptr == 0) || (bsiz == 0)) {
	uc = UTOFU_ERR_INVALID_ARG; RETURN_BAD_C(uc);
    }

    if (mema->stag != -1U) {
	RETURN_OK_C(uc);
    }

    /* offs */
    offs = (uintptr_t)bptr % 256;
    if (offs != 0) {
	bptr = (uint8_t *)bptr - offs;
	bsiz += offs;
    }
    /* tofa */
    {
	utofu_vcq_id_t vcqi = -1UL;
	struct ulib_epnt_info i;
	union ulib_tofa_u tofa_u;

	uc = utofu_query_vcq_id(vcqh, &vcqi);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	uc = utofu_query_vcq_info(vcqi, i.xyz, i.tni, i.tcq, i.cid);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	tofa_u.ui64 = 0;

	uc = ulib_cast_epnt_to_tofa( &i, &tofa_u.tofa );
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	uc = ulib_cast_stad_to_tofa( stag, offs, &tofa_u.tofa );
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	mema->tofa = tofa_u.ui64;
    }
    /* stof */
    {
	const unsigned long int flag =  0
					/* | UTOFU_REG_MEM_FLAG_READ_ONLY */
					;
	utofu_stadd_t *stof = &mema->stad;

	uc = utofu_reg_mem_with_stag(vcqh, bptr, bsiz, stag, flag, stof);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	mema->vcqh = vcqh;
    }
    /* enabled */
    mema->stag = stag;

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

void ulib_shea_cbuf_ercv_init(
    struct ulib_shea_cbuf *cbuf,
    ulib_shea_ercv_cbak_f func,
    void *farg
)
{
    volatile struct ulib_shea_ercv *ercv = cbuf->cptr_ercv;

    assert(ercv != 0);
    /* tail */
    ercv->tail.va64 = ULIB_SHEA_NIL8;
    /* cntr: cntr.ct_s.pcnt = 0; cntr.ct_s.ccnt = 0; */
    ercv->cntr.ct64 = 0;
    /* full */
    ercv->full.addr.va64 = ULIB_SHEA_NIL8;
    /* r_no, phdr, dptr, func, farg */
    ercv->r_no = 0;
    ercv->phdr = cbuf->cptr_hdrs;
    ercv->dptr = cbuf->dptr;
    ercv->func = func;
    ercv->farg = farg;

    return ;
}

void ulib_shea_cbuf_ercv_init_func(
    struct ulib_shea_cbuf *cbuf,
    ulib_shea_ercv_cbak_f func,
    void *farg
)
{
    volatile struct ulib_shea_ercv *ercv = cbuf->cptr_ercv;

    /* func */
    ercv->func = func;
    /* farg */
    ercv->farg = farg;

    return ;
}

void ulib_shea_cbuf_hdrs_init(
    struct ulib_shea_cbuf *cbuf
)
{
    /* int uc = UTOFU_SUCCESS; */
    {
	union ulib_shea_ph_u *phdr = cbuf->cptr_hdrs;
	uint32_t ii, ni = cbuf->hcnt;

	assert(cbuf->hcnt > 0);
	phdr[0].phlh.seqn = UINT32_MAX;
#ifndef	NDEBUG
	phdr[0].phlh.type = ULIB_SHEA_PH_UNDEF;
#endif	/* NDEBUG */
	for (ii = 1; ii < ni; ii++) {
	    phdr[ii].phlh.seqn = 0;
#ifndef	NDEBUG
	    phdr[ii].phlh.type = ULIB_SHEA_PH_UNDEF;
#endif	/* NDEBUG */
	}
    }
    return /* uc */;
}

void ulib_shea_cbuf_esnd_init_addr(
    struct ulib_shea_cbuf *cbuf
)
{
    struct ulib_shea_esnd *esnd = cbuf->cptr_esnd;

    assert(esnd != 0);
    assert(cbuf->ctrl.stag != -1U);
    {
	uint32_t is, ns = cbuf->scnt;

	for (is = 0; is < ns; is++) {
	    union ulib_tofa_u toau = { .ui64 = cbuf->ctrl.tofa, };

	    toau.tofa.off += ((uint8_t *)&esnd[is] - (uint8_t *)cbuf->cptr);

	    esnd[is].self.addr.va64 = toau.ui64;
	}
    }

    return ;
}

int ulib_shea_cbuf_enab_buff(
    struct ulib_shea_cbuf *cbuf
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    {
	long algn = sysconf(_SC_PAGESIZE);

	/* ctrl */
	if (cbuf->cptr == 0) {
	    int rc = posix_memalign(&cbuf->cptr, algn, cbuf->csiz);
	    if (rc != 0) {
		uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	    }

	    /* offsets (update) */
	    assert(cbuf->cptr_ercv == 0);
	    cbuf->cptr_ercv = (void *)
		((uint8_t *)cbuf->cptr + (uintptr_t)cbuf->cptr_ercv);
	    cbuf->cptr_hdrs = (void *)
		((uint8_t *)cbuf->cptr + (uintptr_t)cbuf->cptr_hdrs);
	    cbuf->cptr_esnd = (void *)
		((uint8_t *)cbuf->cptr + (uintptr_t)cbuf->cptr_esnd);

            fprintf(stderr, "YIPROTOCOL: cptr(%p) cptr_ercv(%p) cptr_hdrs(%p)  cptr_esnd(%p)\n", cbuf->cptr, cbuf->cptr_ercv, cbuf->cptr_hdrs, cbuf->cptr_esnd);

	    memset(cbuf->cptr, 0, cbuf->csiz);
	    /* hdrs init */
	    ulib_shea_cbuf_hdrs_init(cbuf);
	    /* cptr_esnd */
	    {
		uint32_t ic, nc = cbuf->scnt;

		for (ic = 0; ic < nc; ic++) {
		    struct ulib_shea_esnd *esnd = &cbuf->cptr_esnd[ic];

		    /* insert tail */
		    DLST_INST(&cbuf->free_esnd, esnd, list);
		}
	    }
	}

	/* data */
	if (cbuf->dptr == 0) {
	    int rc = posix_memalign(&cbuf->dptr, algn, cbuf->dsiz);
	    if (rc != 0) {
		uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	    }
	    memset(cbuf->dptr, 0, cbuf->dsiz);
	}
    }
    /* ercv init */
    ulib_shea_cbuf_ercv_init(cbuf, 0 /* func */ , 0 /* farg */ ); /* YYY */

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_shea_cbuf_enab(
    struct ulib_shea_cbuf *cbuf,
    utofu_vcq_hdl_t vcqh,
    unsigned int ctag,
    unsigned int dtag,
    ulib_shea_ercv_cbak_f func,
    void *farg
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    if (vcqh == 0) {
	uc = UTOFU_ERR_INVALID_VCQ_ID; RETURN_BAD_C(uc);
    }
    if ((ctag == -1U) || (dtag == -1U)) {
	uc = UTOFU_ERR_INVALID_STAG; RETURN_BAD_C(uc);
    }

    /* allocate buffers */
    uc = ulib_shea_cbuf_enab_buff(cbuf);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* ctrl */
    uc = ulib_shea_cbuf_enab_mema(vcqh, cbuf->cptr, cbuf->csiz, ctag,
	    &cbuf->ctrl);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* esnd addr */
    ulib_shea_cbuf_esnd_init_addr(cbuf);

    /* data */
    uc = ulib_shea_cbuf_enab_mema(vcqh, cbuf->dptr, cbuf->dsiz, dtag,
	    &cbuf->data);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* cbuf -> cptr_ercv . func */
    ulib_shea_cbuf_ercv_init_func(cbuf, func, farg);

#ifdef	TEST_CBUF_ESND

    {
	extern void ulib_shea_cbuf_esnd_test(struct ulib_shea_cbuf *cbuf);

	ulib_shea_cbuf_esnd_test(cbuf);
    }
#endif	/* TEST_CBUF_ESND */

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

struct ulib_shea_esnd *ulib_shea_cbuf_esnd_qget(
    struct ulib_shea_cbuf *cbuf,
    void *data
)
{
    struct ulib_shea_esnd *esnd;

    esnd = DLST_PEEK(&cbuf->free_esnd, struct ulib_shea_esnd, list);
    if (esnd == 0) {
	goto bad;
    }
    DLST_RMOV(&cbuf->free_esnd, esnd, list);

    esnd->data = data;
    ulib_shea_esnd_init(esnd);

bad:
    return esnd;
}

void ulib_shea_cbuf_esnd_qput(
    struct ulib_shea_cbuf *cbuf,
    struct ulib_shea_esnd *esnd
)
{
    DLST_INST(&cbuf->free_esnd, esnd, list);

    return ;
}
#ifdef	TEST_CBUF_ESND

void ulib_shea_cbuf_esnd_test(struct ulib_shea_cbuf *cbuf)
{
    DLST_DECH(ulib_head_esnd) busy_esnd;
    size_t ii, ni = 2;

    DLST_INIT(&busy_esnd);

    for (ii = 0; ii < ni; ii++) {
	struct ulib_shea_esnd *esnd;
	void *data = (void *)0x01UL;
	uint32_t nget = 0, nput = 0;

	do {
	    esnd = ulib_shea_cbuf_esnd_qget(cbuf, data);
	    if (esnd != 0) {
		nget++;
		DLST_INST(&busy_esnd, esnd, list);
	    }
	} while (esnd != 0);

	do {
	    esnd = DLST_PEEK(&busy_esnd, struct ulib_shea_esnd, list);
	    if (esnd != 0) {
		DLST_RMOV(&busy_esnd, esnd, list);

		ulib_shea_cbuf_esnd_qput(cbuf, esnd);
		nput++;
	    }
	} while (esnd != 0);

printf("%s():%d\t%03ld nget %3u nput %3u scnt %3u\n",
__func__, __LINE__, ii, nget, nput, cbuf->scnt);
    }

}
#endif	/* TEST_CBUF_ESND */


#include <stdio.h>	/* for FILE */

long ulib_huge_page_size(void)
{
    FILE *fp;
    long hugepagesize;

    fp = fopen("/proc/meminfo", "r");
    if (fp == 0) {
	hugepagesize = 0;
    }
    else {
	long ls = 0;
	const char *fmt1 = "Hugepagesize: %ld kB \n";
	char buf[1024];

	while (fgets(buf, sizeof (buf), fp) != 0) {
	    int rc = sscanf(buf, fmt1, &ls);
	    if (rc == 1) { ls *= 1024; break; }
	}
	hugepagesize = ls;
	(void) fclose(fp); fp = 0;
    }

    return hugepagesize;
}
