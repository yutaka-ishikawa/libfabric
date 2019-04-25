/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef _ULIB_TOQC_H
#define _ULIB_TOQC_H

#include "ulib_conf.h"

#include "utofu.h"

#include <stdint.h>	    /* for uint32_t */
#include <assert.h>	    /* for assert() */

/* definitions */

/* extra struct utofu_mrq_notice . notice_type */
#define ULIB_MRQ_TYPE_LCL_NOP	30

#define ULIB_TOQC_SHIFT	    5 /* 32 for small test */
#define ULIB_TOQC_SIZE	    (1UL << ULIB_TOQC_SHIFT)
#define ULIB_TOQC_MASK	    (ULIB_TOQC_SIZE - 1)

#define ULIB_TOQC_DIFF(A_,B_)	( (ulib_toqc_cnt_t) \
	    ( ((uint64_t)(A_) + ULIB_TOQC_SIZE - (uint64_t)(B_)) \
		& ULIB_TOQC_MASK ) )

/* structures */

typedef uint32_t	    ulib_toqc_cnt_t;

struct ulib_toqe {
    uint32_t dsiz;
#ifndef	notdef_fix3
    uint32_t csiz;
#endif	/* notdef_fix3 */
    uint32_t flag;
#ifdef	notdef_retp
    void *retp;
#else	/* notdef_retp */
    uint64_t *retp;
#endif	/* notdef_retp */
    struct utofu_mrq_notice ackd[1];
};

/* flags for ulib_toqe . flag */
#define ULIB_TOQE_FLAG_USED 0x01
#define ULIB_TOQE_FLAG_TCQD 0x02
#define ULIB_TOQE_FLAG_ACKD 0x04
#define ULIB_TOQE_FLAG_DONE ( 0 \
			    | ULIB_TOQE_FLAG_USED \
			    | ULIB_TOQE_FLAG_TCQD \
			    | ULIB_TOQE_FLAG_ACKD \
			    )
#define ULIB_TOQE_COMPLETED(FLAG)   ( \
			    ((FLAG) & ULIB_TOQE_FLAG_DONE) \
				== ULIB_TOQE_FLAG_DONE \
			    )


struct ulib_toqc {
    utofu_vcq_hdl_t vcqh;
    int64_t lock; /* YYY */
    ulib_toqc_cnt_t pcnt; /* producer counter : write pointer */
    ulib_toqc_cnt_t ccnt; /* consumer counter : read  pointer */
    struct ulib_toqe *toqe;
};

typedef int (*ulib_toqc_match_ackd_f)(
    const struct utofu_mrq_notice *ackd,
    const struct utofu_mrq_notice *expected
);


/* function prototypes */

extern int	ulib_toqc_post(
		    struct ulib_toqc *toqc,
		    void *desc,
		    size_t desc_size,
#ifdef	notdef_retp
		    void *retp,
#else	/* notdef_retp */
		    uint64_t retp[3],
#endif	/* notdef_retp */
		    const struct utofu_mrq_notice *ackd,
		    uint64_t *r_no
		);
extern int	ulib_toqc_init(
		    utofu_vcq_hdl_t vcqh,
		    struct ulib_toqc **pp_toqc
		);
extern int	ulib_toqc_fini(
		    struct ulib_toqc *toqc
		);
extern int	ulib_toqc_prog_tcqd(struct ulib_toqc *toqc);
extern int	ulib_toqc_prog_ackd(struct ulib_toqc *toqc);
extern int	ulib_toqc_prog(struct ulib_toqc *toqc);
extern int	ulib_toqc_read(struct ulib_toqc *toqc, uint64_t *r_no);
extern int	ulib_toqc_chck_tcqd(struct ulib_toqc *toqc, size_t *num_tcqd);

/* inline functions */

static inline void ulib_toqc_lock(struct ulib_toqc *toqc)
{
    assert(toqc->lock == 0); /* YYY */
    toqc->lock++; /* YYY */
    return ;
}

static inline void ulib_toqc_unlock(struct ulib_toqc *toqc)
{
    assert(toqc->lock == 1); /* YYY */
    toqc->lock--; /* YYY */
    return ;
}

static inline void ulib_toqc_abort(struct ulib_toqc *toqc, int uc)
{
    assert(__LINE__ == 0); /* YYY */
    return ;
}

static inline int ulib_toqc_chck(struct ulib_toqc *toqc, size_t nadd)
{
    ulib_toqc_cnt_t r, w, d1, d2;

    if (nadd >= ULIB_TOQC_SIZE) {
	return UTOFU_ERR_INVALID_ARG;
    }
    r = toqc->ccnt;
    w = toqc->pcnt;
    d1 = ULIB_TOQC_DIFF(w, r);

    w += nadd;
    d2 = ULIB_TOQC_DIFF(w, r);
    if (d2 < d1) {
	return UTOFU_ERR_FULL;
    }

    return d2; /* # of used rooms */
}

static inline struct ulib_toqe *ulib_toqc_toqe_pcnt(struct ulib_toqc *toqc)
{
    struct ulib_toqe *toqe;
    toqe = &toqc->toqe[toqc->pcnt & ULIB_TOQC_MASK];
    return toqe;
}

static inline struct ulib_toqe *ulib_toqc_toqe_pcnt_incr(struct ulib_toqc *toqc)
{
    struct ulib_toqe *toqe;
    toqe = &toqc->toqe[toqc->pcnt++ & ULIB_TOQC_MASK];
    return toqe;
}

static inline struct ulib_toqe *ulib_toqc_toqe_ccnt_wloc(
    struct ulib_toqc *toqc,
    ulib_toqc_cnt_t ccnt
)
{
    struct ulib_toqe *toqe;
    toqe = &toqc->toqe[ccnt & ULIB_TOQC_MASK];
    return toqe;
}

static inline void ulib_toqc_toqe_ccnt_update(struct ulib_toqc *toqc)
{
    while (toqc->ccnt != toqc->pcnt) {
	struct ulib_toqe *curr;
	curr = ulib_toqc_toqe_ccnt_wloc(toqc, toqc->ccnt);
	if (curr->flag != 0) { break; }
	toqc->ccnt++;
    }
    return ;
}

static inline int ulib_toqd_dcmp_get(
    const struct utofu_mrq_notice *ackd, 
    const struct utofu_mrq_notice *expected
)
{
    int ret = 0;
#ifdef	CONF_ULIB_UTOF_FIX1
    utofu_vcq_id_t ackd_vcq_id;
#endif	/* CONF_ULIB_UTOF_FIX1 */

    assert(ackd->notice_type == UTOFU_MRQ_TYPE_LCL_GET);
#ifdef	CONF_ULIB_UTOF_FIX1
    ackd_vcq_id =   0
		    | (ackd->vcq_id >> 32)
		    | (((ackd->vcq_id << 32) >> (32+16)) << 32)
		    | (ackd->vcq_id << (64-6))
		    ;
#endif	/* CONF_ULIB_UTOF_FIX1 */

    if (ackd->notice_type != expected->notice_type) {
	ret = __LINE__;
    }
#ifndef	CONF_ULIB_UTOF_FIX1
    else if (ackd->vcq_id != expected->vcq_id) {
	ret = __LINE__;
    }
#else	/* CONF_ULIB_UTOF_FIX1 */
    else if (ackd_vcq_id != expected->vcq_id) {
	ret = __LINE__;
    }
#endif	/* CONF_ULIB_UTOF_FIX1 */
    else if (ackd->edata != expected->edata) {
	ret = __LINE__;
    }
    else if (ackd->lcl_stadd != expected->lcl_stadd) {
	ret = __LINE__;
    }

    return ret;
}

static inline int ulib_toqd_dcmp_put(
    const struct utofu_mrq_notice *ackd,
    const struct utofu_mrq_notice *expected
)
{
    int ret = 0;
#ifdef	CONF_ULIB_UTOF_FIX1
    utofu_vcq_id_t ackd_vcq_id;
#endif	/* CONF_ULIB_UTOF_FIX1 */

    assert(ackd->notice_type == UTOFU_MRQ_TYPE_LCL_PUT);
#ifdef	CONF_ULIB_UTOF_FIX1
    ackd_vcq_id =   0
		    | (ackd->vcq_id >> 32)
		    | (((ackd->vcq_id << 32) >> (32+16)) << 32)
		    | (ackd->vcq_id << (64-6))
		    ;
#endif	/* CONF_ULIB_UTOF_FIX1 */

    if (ackd->notice_type != expected->notice_type) {
	ret = __LINE__;
    }
#ifndef	CONF_ULIB_UTOF_FIX1
    else if (ackd->vcq_id != expected->vcq_id) {
	ret = __LINE__;
    }
#else	/* CONF_ULIB_UTOF_FIX1 */
    else if (ackd_vcq_id != expected->vcq_id) {
	ret = __LINE__;
    }
#endif	/* CONF_ULIB_UTOF_FIX1 */
    else if (ackd->edata != expected->edata) {
	ret = __LINE__;
    }
    else if (ackd->rmt_stadd != expected->rmt_stadd) {
	ret = __LINE__;
    }

    return ret;
}

static inline int ulib_toqd_dcmp_armw(
    const struct utofu_mrq_notice *ackd,
    const struct utofu_mrq_notice *expected
)
{
    int ret = 0;
#ifdef	CONF_ULIB_UTOF_FIX1
    utofu_vcq_id_t ackd_vcq_id;
#endif	/* CONF_ULIB_UTOF_FIX1 */

    assert(ackd->notice_type == UTOFU_MRQ_TYPE_LCL_ARMW);
#ifdef	CONF_ULIB_UTOF_FIX1
    ackd_vcq_id =   0
		    | (ackd->vcq_id >> 32)
		    | (((ackd->vcq_id << 32) >> (32+16)) << 32)
		    | (ackd->vcq_id << (64-6))
		    ;
#endif	/* CONF_ULIB_UTOF_FIX1 */

    if (ackd->notice_type != expected->notice_type) {
	ret = __LINE__;
    }
#ifndef	CONF_ULIB_UTOF_FIX1
    else if (ackd->vcq_id != expected->vcq_id) {
	ret = __LINE__;
    }
#else	/* CONF_ULIB_UTOF_FIX1 */
    else if (ackd_vcq_id != expected->vcq_id) {
	ret = __LINE__;
    }
#endif	/* CONF_ULIB_UTOF_FIX1 */
    else if (ackd->edata != expected->edata) {
	ret = __LINE__;
    }
    else if (ackd->rmt_stadd != expected->rmt_stadd) {
	ret = __LINE__;
    }

    return ret;
}

static inline void ulib_toqc_match_ackd(
    struct ulib_toqc *toqc,
    const ulib_toqc_match_ackd_f func,
    const struct utofu_mrq_notice *ackd,
    int uc
)
{
    ulib_toqc_cnt_t ccnt, pcnt;

    ulib_toqc_lock(toqc);

    assert(toqc->ccnt != toqc->pcnt);
    ccnt = toqc->ccnt;
    pcnt = toqc->pcnt;
    while (ccnt != pcnt) {
	struct ulib_toqe *toqe;
	const struct utofu_mrq_notice *ackd_expected;
	int differ;

	toqe = ulib_toqc_toqe_ccnt_wloc(toqc, ccnt);
	if (
	    (toqe->flag == 0 /* all done */ )
	    || ((toqe->flag & ULIB_TOQE_FLAG_ACKD) != 0)
	) {
	    ccnt++; continue;
	}
	ackd_expected = toqe->ackd;

	differ = (*func)(ackd, ackd_expected);
	if (differ != 0) {
	    ccnt++;
	    continue;
	}
	/* assert(uc == 0); */ /* XXX */
	if (
	    (ackd->notice_type == UTOFU_MRQ_TYPE_LCL_ARMW)
	    && (toqe->retp != 0)
	) {
#ifdef	notdef_retp
	    ((uint64_t *)toqe->retp)[0] = ackd->rmt_value;
#else	/* notdef_retp */
	    toqe->retp[0] = ackd->rmt_value;
	    /* toqe->retp[1] = uc; */ /* YYY */
	    /* toqe->retp[2] = ackd->edata; */ /* YYY */
#endif	/* notdef_retp */
	}
// printf("ackd [%ld] flag %x\n", toqe - &toqc->toqe[0], toqe->flag);
// fflush(stdout);
	assert((toqe->flag & ULIB_TOQE_FLAG_USED) != 0);
	assert((toqe->flag & ULIB_TOQE_FLAG_ACKD) == 0);
	toqe->flag |= ULIB_TOQE_FLAG_ACKD;
	if (ULIB_TOQE_COMPLETED(toqe->flag)) { toqe->flag = 0; }

	/* update ccnt */
	if (
	    (ccnt == toqc->ccnt)
	    && (toqe->flag == 0)
	) {
	    toqc->ccnt++;
	    ulib_toqc_toqe_ccnt_update(toqc);
	    /* ccnt = toqc->ccnt; */
	}

	break;
    }
    assert(ccnt != pcnt); /* if not found */

    ulib_toqc_unlock(toqc);
    return ;
}

static inline void ulib_toqc_match(
    struct ulib_toqc *toqc, 
    const struct utofu_mrq_notice *tmrq,
    int uc
)
{
    if (tmrq->notice_type == UTOFU_MRQ_TYPE_LCL_PUT) {
	ulib_toqc_match_ackd(toqc, ulib_toqd_dcmp_put, tmrq, uc);
    }
    else if (tmrq->notice_type == UTOFU_MRQ_TYPE_LCL_GET) {
	ulib_toqc_match_ackd(toqc, ulib_toqd_dcmp_get, tmrq, uc);
    }
    else if (tmrq->notice_type == UTOFU_MRQ_TYPE_LCL_ARMW) {
	ulib_toqc_match_ackd(toqc, ulib_toqd_dcmp_armw, tmrq, uc);
if (0) {
printf("%s():%d rmt_value %"PRIu64"\n", __func__, __LINE__, tmrq->rmt_value);
}
    }
    else {
	/* UTOFU_MRQ_TYPE_RMT_{PUT,GET,ARMW} */
	/* #ifdef CONF_ULIB_UTOF_FIX6 */
	/* should be notice_type *_LCL_ARMW with error */
	if (tmrq->notice_type == UTOFU_MRQ_TYPE_RMT_ARMW) {
	    struct utofu_mrq_notice tmrq_copy;
	    tmrq_copy = tmrq[0];
	    tmrq_copy.notice_type = UTOFU_MRQ_TYPE_LCL_ARMW;
	    ulib_toqc_match_ackd(toqc, ulib_toqd_dcmp_armw, &tmrq_copy, uc);
	}
	/* #endif CONF_ULIB_UTOF_FIX6 */
    }

    return ;
}
#ifdef	CONF_ULIB_UTOF_FIX2

static inline void ulib_toqc_match_tcqd(
    struct ulib_toqc *toqc,
    void *cbdata,
    int r_uc
)
{
    ulib_toqc_cnt_t ccnt, pcnt;
    void *toqe_done = 0; /* XXX hack */

    ulib_toqc_lock(toqc);

    assert(toqc->ccnt != toqc->pcnt);
    ccnt = toqc->ccnt;
    pcnt = toqc->pcnt;
    while (ccnt != pcnt) {
	struct ulib_toqe *toqe;

	toqe = ulib_toqc_toqe_ccnt_wloc(toqc, ccnt);
	if (
	    (toqe->flag == 0 /* all done */ )
	    || ((toqe->flag & ULIB_TOQE_FLAG_TCQD) != 0)
	) {
	    if (toqe == cbdata) { toqe_done = toqe; } /* XXX hack */
	    ccnt++; continue;
	}

// printf("tcqd [%ld] flag %x\n", toqe - &toqc->toqe[0], toqe->flag);
// fflush(stdout);
	assert((toqe->flag & ULIB_TOQE_FLAG_USED) != 0);
	/* assert((toqe->flag & ULIB_TOQE_FLAG_TCQD) == 0); */
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
	if (r_uc != 0) { /* UTOFU_ISA_ERR_TCQ(uc) */
	    toqe->flag |= ULIB_TOQE_FLAG_ACKD; /* never ack */
	}
	if (ULIB_TOQE_COMPLETED(toqe->flag)) {
	    toqe->flag = 0;
	    ulib_toqc_toqe_ccnt_update(toqc);
	    /* ccnt = toqc->ccnt; */
	}

	break;
    }

    if (toqe_done == 0) { /* XXX hack */
	if (ccnt == pcnt) {
	    fprintf(stderr, "%s:%d\t%s: ccnt %u pcnt %u cc %d pc %d\n",
		__FILE__, __LINE__, __func__,
		ccnt, pcnt, toqc->ccnt, toqc->pcnt);
	    fflush(stdout);
	    fflush(stderr);
	}
	assert(ccnt != pcnt); /* if not found */
    }

    ulib_toqc_unlock(toqc);
    return ;
}
#endif	/* CONF_ULIB_UTOF_FIX2 */

#endif	/* _ULIB_TOQC_H */
