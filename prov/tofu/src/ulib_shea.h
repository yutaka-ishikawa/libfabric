/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef _ULIB_SHEA_H
#define _ULIB_SHEA_H

#include "ulib_conf.h"
#include "ulib_list.h"	    /* for DLST_+() */
#include "ulib_desc.h"	    /* for struct ulib_toqc_cash */

#include <stdint.h>
#include <assert.h>	    /* for assert() */

/* definitions */

#define ULIB_SHEA_NIL8	    (-1UL)

/* structures */

/* virtual (network + memory) address */
union ulib_shea_va_u {
    uint64_t		    va64;
    uint32_t		    va32[2];
    struct utlib_shea_va_s {
	int32_t		    rank;
	uint32_t	    e_id;
    }			    va_s;
};

union ulib_shea_ct_u {
    uint64_t		    ct64;
    uint32_t		    ct32[2];
    struct utlib_shea_ct_s {
	int32_t		    pcnt;       /* producer count */
	uint32_t	    ccnt;       /* consumer count */
    }			    ct_s;
};

struct ulib_shea_data {
    uint32_t nblk;
    uint32_t boff;
    uint32_t llen; /* last length */
    uint32_t rank;
    uint64_t utag;
    uint64_t flag;
    void *ctxt;
    void *toqc;
#ifdef	notdef_perf_0001
    void *toqc_cash;
#else	/* notdef_perf_0001 */
    void *toqc_cash_tmpl;
    void *toqc_cash_real;
    struct ulib_toqc_cash real;
#endif	/* notdef_perf_0001 */
    int (*func)(void *farg, int r_uc, const void *vctx);
    void *farg;
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tims[16];
#endif	/* CONF_ULIB_PERF_SHEA */
};

#define ULIB_SHEA_DATA_TFLG	(1ULL << 0)
#define ULIB_SHEA_DATA_ZFLG	(1ULL << 1)

struct ulib_shea_full {
#ifdef	notdef_shea_fix4
    union ulib_shea_va_u    addr;
    union ulib_shea_ct_u    cntr;
#else	/* notdef_shea_fix4 */
    union ulib_shea_ct_u    cntr;
    union ulib_shea_va_u    addr;
#endif	/* notdef_shea_fix4 */
};

struct ulib_shea_esnd {
    union ulib_shea_va_u    pred[3];
    union ulib_shea_ct_u    cntr[3];
    union ulib_shea_va_u    next;
    uint64_t		    r_no;
    uint8_t		    l_st; /* local state */
    uint8_t		    d_st; /* data state */
    uint32_t		    wait; /* flag */
    struct ulib_shea_full   self;
    struct ulib_shea_data * data;
    DLST_DECE(ulib_shea_esnd)	list;
};

typedef int (*ulib_shea_ercv_cbak_f)(void *farg, int r_uc, const void *vctx);

struct ulib_shea_ercv {
    union ulib_shea_va_u    tail;
    union ulib_shea_ct_u    cntr;
    struct ulib_shea_full   full; /* waiting esnd */
    uint64_t		    r_no;
    void *		    phdr;
    ulib_shea_ercv_cbak_f   func; /* recv call back */
    void *farg;
};

union ulib_shea_epnt {
    struct ulib_shea_esnd   esnd;
    struct ulib_shea_ercv   ercv;
    uint64_t ui64[32]; /* 256 CACHE_LINE */
};

enum {
    INSQ_TAIL = 1,
    INSQ_CHCK = 2, /* is a head ? */
    WAIT_HEAD = 3,
    INCR_CNTR = 4,
    HAVE_ROOM = 5, /* is full ? */
    REMQ_HEAD = 6,
    REMQ_CHCK = 7, /* still a head ? */
    WAIT_NEXT = 8,
    SEND_NORM = 9,
    SEND_EXCL = 10,
    SEND_DONE = 11,
};

/* sub-state in SEND_NORM or SEND_EXCL */
enum {
    DATA_GOAH = 20, /* go ahead */
    DATA_FULL = 21,
};

/* function prototypes */
struct ulib_toqc;	/* #include "ulib_toqc.h" */
struct ulib_utof_cash;	/* #include "ulib_desc.h" */
struct ulib_toqc_cash;	/* #include "ulib_desc.h" */
struct ulib_toqd_cash;	/* #include "ulib_desc.h" */

extern int	ulib_shea_foo0(struct ulib_shea_esnd *esnd);
extern int	ulib_shea_foo1(
		    struct ulib_toqc_cash *cash,
		    const struct ulib_utof_cash *rcsh,
		    const struct ulib_utof_cash *lcsh
		);
extern int	ulib_shea_foo2(
		    struct ulib_toqc *toqc,
		    struct ulib_toqd_cash *toqd,
		    struct ulib_shea_esnd *esnd
		);
extern int	ulib_shea_foo3(
		    struct ulib_toqc *toqc,
		    struct ulib_toqd_cash *toqd,
		    struct ulib_shea_esnd *esnd
		);
extern int	ulib_shea_foo4(
		    struct ulib_toqc *toqc,
		    struct ulib_toqd_cash *toqd,
		    struct ulib_shea_esnd *esnd
		);
extern int	ulib_shea_foo5(
		    struct ulib_toqc *toqc,
		    uint64_t raui,
		    struct ulib_shea_esnd *esnd
		);
extern int	ulib_shea_foo6(
		    struct ulib_toqc *toqc,
		    uint64_t raui,
		    struct ulib_shea_esnd *esnd
		);
extern int	ulib_shea_foo7(
		    struct ulib_toqc *toqc,
		    struct ulib_toqd_cash *toqd,
		    struct ulib_shea_esnd *esnd
		);
extern int	ulib_shea_foo8(
		    struct ulib_toqc *toqc,
		    uint64_t raui,
		    struct ulib_shea_ercv *ercv
		);
extern int	ulib_shea_foo9(
		    struct ulib_toqc *toqc,
#ifdef	notdef_perf_0001
		    struct ulib_toqc_cash *cash,
#else	/* notdef_perf_0001 */
		    struct ulib_toqc_cash *cash_tmpl,
		    struct ulib_toqc_cash *cash_real,
#endif	/* notdef_perf_0001 */
		    struct ulib_shea_esnd *esnd
		);
extern int	ulib_shea_foo10(
		    struct ulib_toqc *toqc,
		    struct ulib_toqd_cash *toqd,
		    struct ulib_shea_esnd *esnd,
		    uint64_t nsnd
		);
extern int	ulib_shea_foo11(
		    struct ulib_toqc *toqc,
		    struct ulib_toqd_cash *toqd,
		    struct ulib_shea_esnd *esnd,
		    uint64_t nsnd
		);
extern int	ulib_shea_foo12(
		    struct ulib_toqc *toqc,
		    struct ulib_toqd_cash *toqd,
		    struct ulib_shea_esnd *esnd,
		    uint64_t nsnd
		);
extern void	ulib_shea_recv_hndr_seqn_init(
		    struct ulib_shea_ercv *ercv
		);
extern int	ulib_shea_recv_hndr_prog(
		    struct ulib_toqc *toqc,
		    struct ulib_shea_ercv *ercv
		);

/* inline functions */

static inline void ulib_shea_chst(struct ulib_shea_esnd *esnd, uint8_t newst)
{
#ifndef	NDEBUG
    const char *o_st, *n_st;
    static const char *st_names[] = {
	[0]         = "(unknown)",
	[INSQ_TAIL] = "insq_tail",
	[INSQ_CHCK] = "insq_chck",
	[WAIT_HEAD] = "wait_head",
	[INCR_CNTR] = "incr_cntr",
	[HAVE_ROOM] = "have_room",
	[REMQ_HEAD] = "remq_head",
	[REMQ_CHCK] = "remq_chck",
	[WAIT_NEXT] = "wait_next",
	[SEND_NORM] = "send_norm",
	[SEND_EXCL] = "send_excl",
	[SEND_DONE] = "send_done",
    };

    if (
	0 /* (esnd->l_st < 0) */
	|| (esnd->l_st >= (sizeof (st_names) / sizeof (st_names[0])))
    ) {
	o_st = st_names[0]; /* unknown */
    }
    else {
	o_st = st_names[esnd->l_st];
    }
    if (
	0 /* (newst < 0) */
	|| (newst >= (sizeof (st_names) / sizeof (st_names[0])))
    ) {
	n_st = st_names[0]; /* unknown */
    }
    else {
	n_st = st_names[newst];
    }
if (1) {
printf("%p: %s --> %s\n", esnd, o_st, n_st);
fflush(stdout);
}
#endif	/* NDEBUG */

    esnd->l_st = newst;

    return ;
}

static inline uint64_t ulib_shea_data_nblk(const struct ulib_shea_data *data)
{
    uint64_t rv;
    assert(data != 0);
    assert(data->nblk > 0);
    rv = data->nblk;
    return rv;
}

static inline uint64_t ulib_shea_data_boff(const struct ulib_shea_data *data)
{
    uint64_t rv;
    assert(data != 0);
    assert(data->boff <= data->nblk);
    rv = data->boff;
    return rv;
}

static inline uint64_t ulib_shea_data_rblk(const struct ulib_shea_data *data)
{
    uint64_t rv;
    assert(data != 0);
    assert(data->boff <= data->nblk);
    rv = data->nblk - data->boff;
    return rv;
}

static inline uint64_t ulib_shea_data_incr_boff(
    struct ulib_shea_data *data,
    uint32_t incr
)
{
    uint64_t rv;
    assert(data != 0);
    assert(incr > 0);
    data->boff += incr;
    rv = ulib_shea_data_rblk(data);
    return rv;
}

static inline uint32_t ulib_shea_data_llen(const struct ulib_shea_data *data)
{
    uint32_t rv;
    assert(data != 0);
    /* assert(data->llen < ULIB_SHEA_DBLK); */
    rv = data->llen;
    return rv;
}

static inline uint32_t ulib_shea_data_rank(const struct ulib_shea_data *data)
{
    uint32_t rv;
    assert(data != 0);
    rv = data->rank;
    return rv;
}

static inline uint64_t ulib_shea_data_utag(const struct ulib_shea_data *data)
{
    uint64_t rv;
    assert(data != 0);
    rv = data->utag;
    return rv;
}

static inline uint64_t ulib_shea_data_flag(const struct ulib_shea_data *data)
{
    uint64_t rv;
    assert(data != 0);
    rv = data->flag;
    return rv;
}

static inline void * ulib_shea_data_ctxt(const struct ulib_shea_data *data)
{
    void * rv;
    assert(data != 0);
    rv = data->ctxt;
    return rv;
}

static inline void * ulib_shea_data_toqc(const struct ulib_shea_data *data)
{
    void * rv;
    assert(data != 0);
    assert(data->toqc != 0);
    rv = data->toqc;
    return rv;
}
#ifdef	notdef_perf_0001

static inline void * ulib_shea_data_toqc_cash(const struct ulib_shea_data *data)
{
    void * rv;
    assert(data != 0);
    assert(data->toqc_cash != 0);
    rv = data->toqc_cash;
    return rv;
}
#else	/* notdef_perf_0001 */

static inline void * ulib_shea_data_toqc_cash_tmpl(const struct ulib_shea_data *data)
{
    void * rv;
    assert(data != 0);
    assert(data->toqc_cash_tmpl != 0);
    rv = data->toqc_cash_tmpl;
    return rv;
}

static inline void * ulib_shea_data_toqc_cash_real(const struct ulib_shea_data *data)
{
    void * rv;
    assert(data != 0);
    assert(data->toqc_cash_real != 0);
    rv = data->toqc_cash_real;
    return rv;
}
#endif	/* notdef_perf_0001 */

static inline int ulib_shea_data_cbak(
    struct ulib_shea_data *data,
    int r_uc,
    const void *vctx
)
{
    int uc = 0;
    if (data->func != 0) {
	uc = (*data->func)(data->farg, r_uc, vctx);
    }
    return uc;
}

/* ======================================================================== */

static inline void ulib_shea_esnd_init(struct ulib_shea_esnd *esnd)
{
    esnd->pred[0].va64 = ULIB_SHEA_NIL8;
    esnd->cntr[0].ct64 = 0;
    esnd->next.va64 = ULIB_SHEA_NIL8;
    esnd->wait = 0;

    esnd->r_no = UINT64_MAX;
    esnd->l_st = INSQ_TAIL;
    esnd->d_st = DATA_GOAH;

    esnd->self.cntr.ct64 = 0;
    /* do NOT change esnd->self.addr.va64 */
    assert(esnd->self.addr.va64 != ULIB_SHEA_NIL8);
    assert(esnd->data != 0);

    return ;
}

/* ======================================================================== */

#ifdef	CONF_ULIB_SHEA_PAGE
#define ULIB_SHEA_PAGE			CONF_ULIB_SHEA_PAGE
#else	/* CONF_ULIB_SHEA_PAGE */
#define ULIB_SHEA_PAGE			(256 * 32)
#endif	/* CONF_ULIB_SHEA_PAGE */

#define ULIB_SHEA_CASH_LINE		256 /* cache line size */

#define ULIB_SHEA_DBLK			ULIB_SHEA_CASH_LINE
#define ULIB_SHEA_MBLK			(ULIB_SHEA_PAGE / ULIB_SHEA_CASH_LINE)

#define ULIB_SHEA_CNTR_SHIFT		32
#define ULIB_SHEA_CNTR_SIZE		(1UL << ULIB_SHEA_CNTR_SHIFT)
#define ULIB_SHEA_CNTR_MASK		(ULIB_SHEA_CNTR_SIZE - 1)
#define ULIB_SHEA_CNTR_DIFF(A_,B_)	( (uint32_t) ( \
					( (uint64_t)(A_) \
					+ ULIB_SHEA_CNTR_SIZE \
					- (uint64_t)(B_) ) \
					& ULIB_SHEA_CNTR_MASK \
					) )

#define ULIB_ROUNDUP(x,y)		((((x) + ((y) - 1)) / (y)) * (y))


static inline uint64_t ulib_shea_cntr_diff(const union ulib_shea_ct_u *cntr)
{
    uint64_t rv = 0;
    uint32_t diff;

    diff = ULIB_SHEA_CNTR_DIFF(cntr->ct_s.pcnt, cntr->ct_s.ccnt);
    if (diff < ULIB_SHEA_MBLK) {
	rv = ULIB_SHEA_MBLK - diff;
    }

    return rv;
}

static inline void ulib_shea_data_init(
    struct ulib_shea_data *data,
    void *ctxt,
    size_t tlen,
    uint32_t vpid,
    uint64_t utag,
    uint64_t flag
)
{
    uint32_t nblk, llen;
    const uint32_t boff = 0;

    /* data->buff = buff; */
    data->rank = vpid;
    data->utag = utag;
    data->boff = boff;
    data->ctxt = ctxt;

    /* nblk */
    if (tlen != 0) {
	nblk = ULIB_ROUNDUP(tlen, ULIB_SHEA_DBLK) / ULIB_SHEA_DBLK;
    }
    else { /* zero length */
	nblk = ULIB_ROUNDUP(1, ULIB_SHEA_DBLK) / ULIB_SHEA_DBLK;
	assert(nblk == 1);
    }
    data->nblk = nblk;

    /* llen */
    assert((nblk * ULIB_SHEA_DBLK) >= tlen);
    llen = ULIB_SHEA_DBLK - ((nblk * ULIB_SHEA_DBLK) - tlen);
    data->llen = llen;

    if (tlen == 0) { /* zero length */
	flag |= ULIB_SHEA_DATA_ZFLG;
    }
    data->flag = flag;

    /* data->toqc = 0; */
#ifdef	notdef_perf_0001
    /* data->toqc_cash = 0; */
#else	/* notdef_perf_0001 */
    /* data->toqc_cash_tmpl = 0; */
    /* data->toqc_cash_real = 0; */
#endif	/* notdef_perf_0001 */
    /* data->func = 0; */
    /* data->farg = 0; */
#ifdef  CONF_ULIB_PERF_SHEA
    {
	size_t it, nt = sizeof (data->tims) / sizeof (data->tims[0]);
	for (it = 0; it < nt; it++) {
	    data->tims[it] = 0ULL;
	}
    }
#endif  /* CONF_ULIB_PERF_SHEA */

    return ;
}
#ifndef	notdef_perf_0001

extern void	ulib_shea_data_init_cash(
		    struct ulib_shea_data *data,
		    void *toqc_cash,
		    void *toqc_cash_tmpl,
		    void *toqc_cash_real
		);
#endif	/* notdef_perf_0001 */

static inline void ulib_shea_data_init_cbak(
    struct ulib_shea_data *data,
    int (*func)(void *farg, int r_uc, const void *vctx),
    void *farg
)
{
    data->func = func;
    data->farg = farg;

    return ;
}

/* ======================================================================== */

#include <sys/uio.h>	    /* for struct iovec */

struct ulib_shea_rbuf {
    struct iovec iovs[2];
    uint16_t niov;
    uint16_t alloced;  /* if memory is allocated */
    uint32_t leng;
};

struct ulib_shea_uexp {
    struct dlist_entry entry; /* used for dlist */
    uint64_t utag;                 /* tag */
    uint32_t mblk;                 /* # of blocks will be received */
    uint32_t nblk;      /* current # of blocks received */
    uint32_t boff;      /* current last pointer */
    uint32_t flag;      /* fi flag */
    uint32_t srci;      /* for debug perpose (source rank) */
    struct ulib_shea_rbuf rbuf; /* receive buffer */
    uint64_t tims[1];   /* used for statistics (time) */
};

#define ULIB_SHEA_UEXP_FLAG_MBLK    0x01
#define ULIB_SHEA_UEXP_FLAG_TFLG    0x02
#define ULIB_SHEA_UEXP_FLAG_ZFLG    0x04

/* ======================================================================== */

DLST_DEFH(ulib_head_esnd, ulib_shea_esnd);

struct ulib_shea_mema {
    unsigned int	    stag;
    uint64_t		    tofa;
    utofu_stadd_t	    stad;
    utofu_vcq_hdl_t	    vcqh;
};

struct ulib_shea_cbuf {
    void *		    cptr;
    void *		    dptr;
    struct ulib_shea_ercv * cptr_ercv;
    void *		    cptr_hdrs; /* union ulib_shea_ph_u */
    struct ulib_shea_esnd * cptr_esnd;
    DLST_DECH(ulib_head_esnd)	free_esnd;
    size_t		    csiz;
    size_t		    dsiz;
    size_t		    bsiz;
    uint32_t		    hcnt;
/*  uint32_t		    rcnt; */ /* = 1 XXX */
    uint32_t		    scnt;
    size_t		    hsiz;
    size_t		    rsiz;
    size_t		    ssiz;
    struct ulib_shea_mema   ctrl;
    struct ulib_shea_mema   data;
};

struct ulib_shea_cbuf;

extern int  ulib_shea_cbuf_init(struct ulib_shea_cbuf *cbuf);
extern int  ulib_shea_cbuf_fini(struct ulib_shea_cbuf *cbuf);
extern int  ulib_shea_cbuf_enab(struct ulib_shea_cbuf *cbuf,
		utofu_vcq_hdl_t vcqh, unsigned int ctag, unsigned int dtag,
		ulib_shea_ercv_cbak_f func, void *farg);

extern struct ulib_shea_esnd *	ulib_shea_cbuf_esnd_qget(
		struct ulib_shea_cbuf *cbuf, void *data);
extern void ulib_shea_cbuf_esnd_qput(struct ulib_shea_cbuf *cbuf,
		struct ulib_shea_esnd *esnd);

#endif	/* _ULIB_SHEA_H */

