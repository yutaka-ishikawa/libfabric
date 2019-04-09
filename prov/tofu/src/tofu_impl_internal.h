/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_INT_ULIB_H
#define _TOFU_INT_ULIB_H

#include "tofu_prv.h"
#include "ulib_shea.h"
#include "ulib_ofif.h"
#include "ulib_toqc.h"
#include "ulib_conv.h"
/* ======================================================================== */

#include <stdint.h>

struct ulib_sep_tniq {
    uint64_t vers : 3; /* version # */
    uint64_t cmpi : 3; /* component id */
    uint64_t nniq : 3; /* # of niqs */
    uint64_t vald : 1; /* valid flag */
    uint64_t tni0 : 3; /* niq[0] */
    uint64_t tcq0 : 6;
    uint64_t tni1 : 3; /* niq[1] */
    uint64_t tcq1 : 6;
    uint64_t tni2 : 3; /* niq[2] */
    uint64_t tcq2 : 6;
    uint64_t tni3 : 3; /* niq[3] */
    uint64_t tcq3 : 6;
    uint64_t tni4 : 3; /* niq[4] */
    uint64_t tcq4 : 6;
    uint64_t tni5 : 3; /* niq[5] */
    uint64_t tcq5 : 6;
};

struct ulib_sep_name {
    uint8_t  txyz[3];
    uint8_t  a : 1;
    uint8_t  b : 2;
    uint8_t  c : 1;
    uint8_t  p : 3;	/* component id */
    uint8_t  v : 1;	/* valid */
    uint32_t vpid;
    uint8_t  tniq[8];	/* (tni + tcq)[8] */
    /* struct ulib_sep_tniq tniq; */
};

/* ------------------------------------------------------------------------ */

/* #include "prv.h" */		/* for struct tofu_sep */
/* #include "utofu.h" */	/* YYY */
#include <stdint.h>		/* YYY */

// defined in tofu_impl_internal.h
//typedef	uint16_t		utofu_tni_id_t;	    /* YYY */

struct tofu_imp_sep_ulib {
    utofu_tni_id_t  tnis[8];
    size_t ntni;
};

struct tofu_imp_sep_ulib_s {
    struct tofu_sep		sep_prv;
    struct tofu_imp_sep_ulib	sep_lib;
};

/* ------------------------------------------------------------------------ */

#include <ofi_list.h>		/* for struct dlist_entry */
#include <rdma/fi_tagged.h>	/* for struct fi_msg_tagged */
#include <rdma/fi_eq.h>		/* for struct fi_cq_tagged_entry */

#include <stdint.h>		/* for uint32_t */
#include <sys/uio.h>		/* for struct iovec */

struct ulib_shea_expd {
    struct dlist_entry entry;
    struct fi_msg_tagged tmsg;
    uint64_t flgs;
    uint32_t mblk;
    uint32_t nblk;
    uint64_t rtag;
    uint32_t wlen;
    uint32_t olen;
    uint32_t niov;
    struct iovec iovs[2];
    int (*func)(void *farg, const struct fi_cq_tagged_entry *comp);
    void *farg;
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tims[16];
#endif	/* CONF_ULIB_PERF_SHEA */
};

/* ------------------------------------------------------------------------ */

#include <ofi_list.h>		/* for struct dlist_entry */
#include <ofi_atom.h>		/* for ofi_atomic32_t */

#if 0
struct ulib_utof_cash {
    uint64_t		    vcqi;
    uint64_t		    stad;
    uint64_t		    stad_data;
    uint8_t		    paid;
    uintptr_t		    vcqh;
};

struct ulib_toqd_cash {
    uint64_t		    desc[8];
    size_t		    size;
    uint64_t		    ackd[8];
};
#endif

struct ulib_desc_cash {
    struct ulib_utof_cash   addr[2];
    uint64_t		    akey;    /* XXX tank */
    uint32_t		    vpid;
    ofi_atomic32_t	    refc;
    struct dlist_entry	    list;
    struct ulib_toqd_cash   swap[1];
    struct ulib_toqd_cash   fadd[1];
    struct ulib_toqd_cash   cswp[1];
    struct ulib_toqd_cash   puti[1];
    struct ulib_toqd_cash   phdr[1];
    struct ulib_toqd_cash   putd[2];
};

/* ------------------------------------------------------------------------ */

#include <ofi.h>	/* for container_of () */
#include <ofi_mem.h>	/* for DECLARE_FREESTACK */
#include <ofi_list.h>	/* for struct dlist_entry */
#include <ofi_iov.h>	/* for ofi_copy_to_iov() */

#include "tofu_impl_shea.h"		/* for struct ulib_shea_uexp */

/*
 * buffer pool for unexpected entries; see also ofi_mem.h
 *   struct ulib_uexp_fs;
 *   void ulib_uexp_fs_init(struct ulib_uexp_fs *fs, size_t size);
 *   struct ulib_uexp_fs *ulib_uexp_fs_create(size_t size);
 *   void ulib_uexp_fs_free(struct ulib_uexp_fs *fs);
 *   int ulib_uexp_fs_index(struct ulib_uexp_fs *fs, struct ulib_shea_uexp *en);
 */
#if 0
DECLARE_FREESTACK(struct ulib_shea_uexp, ulib_uexp_fs);
#endif
/* ------------------------------------------------------------------------ */

/* #include <ulib_shea.h> */		/* for struct ulib_shea_expd */

/*
 * buffer pool for expected entries; see also ofi_mem.h
 *   struct ulib_expd_fs;
 *   void ulib_expd_fs_init(struct ulib_expd_fs *fs, size_t size);
 *   struct ulib_expd_fs *ulib_expd_fs_create(size_t size);
 *   void ulib_expd_fs_free(struct ulib_expd_fs *fs);
 *   int ulib_expd_fs_index(struct ulib_expd_fs *fs, struct ulib_shea_expd *en);
 */
#if 0
DECLARE_FREESTACK(struct ulib_shea_expd, ulib_expd_fs);
#endif

/* ------------------------------------------------------------------------ */

#if 0
/* #include <ulib_shea.h> */		/* for struct ulib_shea_data */

/*
 * buffer pool for user data (send) entries; see also ofi_mem.h
 *   struct ulib_udat_fs;
 *   void ulib_udat_fs_init(struct ulib_udat_fs *fs, size_t size);
 *   struct ulib_udat_fs *ulib_udat_fs_create(size_t size);
 *   void ulib_udat_fs_free(struct ulib_udat_fs *fs);
 *   int ulib_udat_fs_index(struct ulib_udat_fs *fs, struct ulib_shea_data *en);
 */
DECLARE_FREESTACK(struct ulib_shea_data, ulib_udat_fs);
#endif

/* ------------------------------------------------------------------------ */
#ifndef	NOTDEF

/* #include <ulib_shea.h> */		/* for struct ulib_desc_cash */
#endif	/* NOTDEF */

/*
 * buffer pool for desc cash entries; see also ofi_mem.h
 *   struct ulib_cash_fs;
 *   void ulib_cash_fs_init(struct ulib_cash_fs *fs, size_t size);
 *   struct ulib_cash_fs *ulib_cash_fs_create(size_t size);
 *   void ulib_cash_fs_free(struct ulib_cash_fs *fs);
 *   int ulib_cash_fs_index(struct ulib_cash_fs *fs, struct ulib_desc_cash *en);
 */
DECLARE_FREESTACK(struct ulib_desc_cash, ulib_cash_fs);

/* ------------------------------------------------------------------------ */
#ifdef	NOTYET

#include "rbtree.h"
#endif	/* NOTYET */

struct ulib_uexp_fs;
struct ulib_expd_fs;
struct ulib_udat_fs;
struct ulib_cash_fs;

struct tofu_imp_cep_ulib {
    /* recv: unexpected */
    size_t uexp_sz;
    struct ulib_uexp_fs *uexp_fs;
    struct dlist_entry uexp_head_trcv; /* unexpected queue for tagged msg. */
    struct dlist_entry uexp_head_mrcv; /* unexpected queue for msg. */
    /* recv: expected */
    size_t expd_sz;
    struct ulib_expd_fs *expd_fs;
    struct dlist_entry expd_head_trcv; /* unexpected queue for tagged msg. */
    struct dlist_entry expd_head_mrcv; /* unexpected queue for msg. */
    /* send: */
    size_t udat_sz;
    struct ulib_udat_fs *udat_fs;
    /* control: */
    utofu_vcq_hdl_t vcqh;
    struct ulib_toqc *toqc;
    struct ulib_shea_cbuf cbuf;
    /* send : cash */
    size_t cash_sz;
    struct ulib_cash_fs *cash_fs;
    struct dlist_entry cash_hd;
#ifdef	NOTYET
    RbtHandle cash_rb;
#endif	/* NOTYET */
};

struct tofu_imp_cep_ulib_s {
    struct tofu_cep		cep_prv;
    struct tofu_imp_cep_ulib	cep_lib;
};

/* ======================================================================== */

static inline void tofu_imp_ulib_expd_init(
    struct ulib_shea_expd *expd,
    const struct fi_msg_tagged *tmsg,
    uint64_t flags
)
{
    expd->mblk = 0;
    expd->nblk = 0;
    expd->rtag = 0;
    expd->wlen = 0;
    expd->olen = 0;
#ifndef	NDEBUG
    dlist_init(&expd->entry);
#endif	/* NDEBUG */
    expd->flgs = flags; /* | FI_TAGGED */
    expd->tmsg = tmsg[0]; /* structure copy */
    /* (sizeof (expd->iovs) / sizeof (expd->iovs[0]) */
    assert(tmsg->iov_count <= 2); /* XXX */
    {
	size_t iv, nv = tmsg->iov_count;
	for (iv = 0; iv < nv; iv++) {
	    expd->iovs[iv].iov_base = tmsg->msg_iov[iv].iov_base;
	    expd->iovs[iv].iov_len  = tmsg->msg_iov[iv].iov_len;
	}
	expd->niov = nv;
    }
    return ;
}

/* ------------------------------------------------------------------------ */

static inline int tofu_imp_ulib_expd_match_uexp(
    struct dlist_entry *item,
    const void *farg
)
{
    int ret;
    const struct ulib_shea_expd *expd = farg;
    struct ulib_shea_uexp *uexp;

    uexp = container_of(item, struct ulib_shea_uexp, vspc_list);
    ret =
        ((expd->tmsg.tag | expd->tmsg.ignore)
            == (uexp->utag | expd->tmsg.ignore))
        ;
    return ret;
}

static inline void * tofu_imp_ulib_icep_find_uexp(
    struct tofu_imp_cep_ulib *icep,
    const struct ulib_shea_expd *expd
)
{
    void *uexp = 0;
    int is_tagged_msg;
    struct dlist_entry *head;
    struct dlist_entry *match;

    is_tagged_msg = ((expd->flgs & FI_TAGGED) != 0);
    if (is_tagged_msg) {
	head = &icep->uexp_head_trcv;
    }
    else {
	head = &icep->uexp_head_mrcv;
    }
    match = dlist_remove_first_match(head, tofu_imp_ulib_expd_match_uexp, expd);
    if (match == 0) {
	goto bad; /* XXX - is not an error */
    }
#ifndef	NDEBUG
    dlist_init(match);
#endif	/* NDEBUG */

    uexp = container_of(match, struct ulib_shea_uexp, vspc_list);

bad:
    return uexp;
}

/* ------------------------------------------------------------------------ */

static inline int tofu_imp_ulib_uexp_match_expd(
    struct dlist_entry *item,
    const void *farg
)
{
    int ret;
    const struct ulib_shea_uexp *uexp = farg;
    struct ulib_shea_expd *expd;

    expd = container_of(item, struct ulib_shea_expd, entry);
    ret =
	((expd->tmsg.tag | expd->tmsg.ignore)
	    == (uexp->utag | expd->tmsg.ignore))
	;
    return ret;
}

static inline void *tofu_imp_ulib_icep_find_expd(
    struct tofu_imp_cep_ulib *icep,
    const struct ulib_shea_uexp *uexp
)
{
    void *expd = 0;
    int is_tagged_msg;
    struct dlist_entry *head;
    struct dlist_entry *match;

    is_tagged_msg = ((uexp->flag & ULIB_SHEA_RINF_FLAG_TFLG) != 0);
    if (is_tagged_msg) {
	head = &icep->expd_head_trcv;
    }
    else {
	head = &icep->expd_head_mrcv;
    }
    match = dlist_remove_first_match(head, tofu_imp_ulib_uexp_match_expd, uexp);
    if (match == 0) {
	goto bad; /* XXX - is not an error */
    }
#ifndef	NDEBUG
    dlist_init(match);
#endif	/* NDEBUG */

    expd = container_of(match, struct ulib_shea_expd, entry);

bad:
    return expd;
}

/* ------------------------------------------------------------------------ */

static inline size_t tofu_imp_ulib_copy_iovs(
    struct iovec *iov_dst,
    size_t iov_dct,
    size_t iov_dof,
    const struct iovec *iov_src,
    size_t iov_sct
)
{
    size_t idx, wlen, iov_off = iov_dof;

    for (idx = 0; idx < iov_sct; idx++) {
        /* see also ofi_copy_to_iov() in include/ofi_iov.h */
        wlen = ofi_copy_to_iov(iov_dst, iov_dct, iov_off,
                        iov_src[idx].iov_base, iov_src[idx].iov_len);
        assert(wlen != -1UL);
        iov_off += wlen;
    }
    return (iov_off - iov_dof);
}

/* ------------------------------------------------------------------------ */

static inline void tofu_imp_ulib_expd_recv(
    struct ulib_shea_expd *expd,
    const struct ulib_shea_uexp *uexp
)
{
    if ((uexp->flag & ULIB_SHEA_RINF_FLAG_MBLK) != 0) {
	assert(expd->nblk == 0);
	assert(expd->mblk == 0);
	expd->mblk  = uexp->mblk;
	expd->rtag  = uexp->utag;
    }
    else {
	assert(expd->nblk != 0);
	assert(expd->mblk != 0);
	assert(expd->rtag == uexp->utag);
    }
    assert(expd->nblk == uexp->boff);
#ifndef	NDEBUG
    if ((uexp->flag & ULIB_SHEA_RINF_FLAG_MBLK) != 0) {
	assert((uexp->nblk + uexp->boff) <= uexp->mblk);
    }
    else {
	assert((uexp->nblk + uexp->boff) <= expd->mblk);
    }
#endif	/* NDEBUG */
    /* copy to the user buffer */
    {
	size_t wlen;

	wlen = tofu_imp_ulib_copy_iovs(
		expd->iovs,
		expd->niov,
		expd->wlen,
		uexp->rbuf.iovs,
		uexp->rbuf.niov
		);
	expd->wlen += wlen;
	assert(wlen <= uexp->rbuf.leng);
	expd->olen += (uexp->rbuf.leng - wlen);
    }
    /* update nblk */
    if (uexp->rbuf.leng == 0) {
	assert((uexp->flag & ULIB_SHEA_RINF_FLAG_ZFLG) != 0);
	assert((uexp->flag & ULIB_SHEA_RINF_FLAG_MBLK) != 0);
	assert(uexp->mblk == 1);
	assert(uexp->boff == 0);
	assert(uexp->nblk == 0);
	expd->nblk += 1 /* uexp->nblk */;
    }
    else {
	expd->nblk += uexp->nblk;
    }

    return ;
}

/* ------------------------------------------------------------------------ */

static inline int tofu_imp_ulib_expd_cond_comp(
    struct ulib_shea_expd *expd
)
{
    int rc = 0;
    rc = ((expd->mblk != 0) && (expd->nblk >= expd->mblk));
#ifndef	NDEBUG
    if (rc != 0) {
	assert(expd->nblk == expd->mblk);
    }
#endif	/* NDEBUG */
    return rc;
}

/* ------------------------------------------------------------------------ */

static inline void tofu_imp_ulib_uexp_rbuf_free(
    struct ulib_shea_uexp *uexp
)
{
    {
	struct ulib_shea_rbuf *rbuf = &uexp->rbuf;
	uint32_t iiov;

	for (iiov = 0; iiov < rbuf->niov; iiov++) {
	    if (rbuf->iovs[iiov].iov_len == 0) { continue; }
	    if (rbuf->iovs[iiov].iov_base != 0) {
		free(rbuf->iovs[iiov].iov_base); /* YYY */
		/* rbuf->iovs[iiov].iov_base = 0; */
	    }
	    /* rbuf->iovs[iiov].iov_len = 0; */
	}
	rbuf->niov = 0;
    }
    return ;
}

/* ------------------------------------------------------------------------ */

static inline void tofu_imp_ulib_icep_recv_rbuf_base(
    struct tofu_imp_cep_ulib *icep,
    const struct ulib_shea_uexp *uexp,
    struct iovec iovs[2]
)
{
    uint8_t *real_base;
    const struct ulib_shea_rbuf *rbuf = &uexp->rbuf;

    /* assert(icep->cbuf.dptr != 0); */ /* YYY cbuf */
    real_base = 0 /* icep->cbuf.dptr */; /* YYY cbuf */

    /* iov_base : offs => base + offs */
    {
	uint32_t iiov;

	for (iiov = 0; iiov < rbuf->niov; iiov++) {
	    uintptr_t offs = (uintptr_t)rbuf->iovs[iiov].iov_base; /* XXX */

	    /* assert((offs + rbuf->iovs[iiov].iov_len) <= icep->cbuf.dsiz); */ /* YYY cbuf */
	    iovs[iiov].iov_base = real_base + offs;
	    iovs[iiov].iov_len  = rbuf->iovs[iiov].iov_len;
	}
    }

    return ;
}

/* ------------------------------------------------------------------------ */

static inline int tofu_imp_ulib_icep_recv_rbuf_copy(
    struct tofu_imp_cep_ulib *icep,
    struct ulib_shea_rbuf *rbuf_dst,
    const struct ulib_shea_uexp *uexp
)
{
    int fc = FI_SUCCESS;
    uint32_t leng = uexp->rbuf.leng;

    assert(rbuf_dst->leng == leng); /* copied */

    if (leng > 0) {
	const struct ulib_shea_rbuf *rbuf_src = &uexp->rbuf;
	size_t wlen;
	struct iovec iovs[2];

	rbuf_dst->iovs[0].iov_base = malloc(leng);
	if (rbuf_dst->iovs[0].iov_base == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
	rbuf_dst->iovs[0].iov_len = leng;
	rbuf_dst->niov = 1;

	assert(sizeof (iovs) == sizeof (rbuf_src->iovs));
	tofu_imp_ulib_icep_recv_rbuf_base(icep, uexp, iovs);

	wlen = tofu_imp_ulib_copy_iovs(
		rbuf_dst->iovs, /* dst */
		rbuf_dst->niov, /* dst */
		0, /* off */
		iovs, /* src */
		rbuf_src->niov /* src */
		);
	assert (wlen == leng);
	if  (wlen != leng) { /* YYY abort */ }
    }

bad:
    return fc;
}

/* ------------------------------------------------------------------------ */

static inline void tofu_imp_ulib_icep_evnt_expd(
    struct tofu_imp_cep_ulib *icep,
    const struct ulib_shea_expd *expd
)
{
    if ((expd->func != 0) && (expd->farg != 0)) {
	struct fi_cq_tagged_entry comp[1];
	int fc;

	comp->op_context	= expd->tmsg.context;
	comp->flags		=   FI_RECV
				    | FI_MULTI_RECV
				    | (expd->flgs & FI_TAGGED)
				    ;
	comp->len		= expd->wlen;
	comp->buf		= expd->tmsg.msg_iov[0].iov_base;
	comp->data		= 0;
	comp->tag		= expd->rtag;

	fc = (*expd->func)(expd->farg, comp);
	if (fc != FI_SUCCESS) {
	}
    }
    return ;
}

/* ------------------------------------------------------------------------ */

static inline void tofu_imp_ulib_icep_link_expd(
    struct tofu_imp_cep_ulib *icep,
    struct ulib_shea_expd *expd
)
{
    int is_tagged_msg;
    struct dlist_entry *head;

    is_tagged_msg = ((expd->flgs & FI_TAGGED) != 0);
    if (is_tagged_msg) {
	head = &icep->expd_head_trcv;
    }
    else {
	head = &icep->expd_head_mrcv;
    }
#ifndef	NDEBUG
    assert(dlist_empty(&expd->entry));
#endif	/* NDEBUG */
    dlist_insert_tail(&expd->entry, head);

    return ;
}

static inline void tofu_imp_ulib_icep_link_expd_head(
    struct tofu_imp_cep_ulib *icep,
    struct ulib_shea_expd *expd
)
{
    int is_tagged_msg;
    struct dlist_entry *head;

    is_tagged_msg = ((expd->flgs & FI_TAGGED) != 0);
    if (is_tagged_msg) {
	head = &icep->expd_head_trcv;
    }
    else {
	head = &icep->expd_head_mrcv;
    }
#ifndef	NDEBUG
    assert(dlist_empty(&expd->entry));
#endif	/* NDEBUG */
    dlist_insert_head(&expd->entry, head);

    return ;
}

/* ------------------------------------------------------------------------ */

static inline struct ulib_shea_data *tofu_imp_ulib_icep_shea_data_qget(
    struct tofu_imp_cep_ulib *icep
)
{
    struct ulib_shea_data *data = 0;

    if (freestack_isempty(icep->udat_fs)) {
	goto bad;
    }
    data = freestack_pop(icep->udat_fs);
    assert(data != 0);
#ifdef	CONF_ULIB_SHEA_DATA
    printf("YI !!!!!!!!!! tofu_imp_ulib_icep_shea_data_qget in tofu_impl_internal.h\n");
#if 0
    /* initialize vcqh and stad_data */
    {
	struct ulib_desc_cash *cash_real = &data->real;
	const uint64_t akey = -1ULL /* YYY FI_ADDR_NOTAVAIL */ ;

	ulib_desc_cash_init(cash_real, akey);
    }
#endif
#endif	/* CONF_ULIB_SHEA_DATA */

bad:
    return data;
}

static inline void tofu_imp_ulib_icep_shea_data_qput(
    struct tofu_imp_cep_ulib *icep,
    struct ulib_shea_data *data
)
{
    assert(data != 0);
#ifdef	CONF_ULIB_SHEA_DATA
    printf("YI !!!!!!!!!! tofu_imp_ulib_icep_shea_data_qput in tofu_impl_internal.h\n");
#if 0
    /* unregister stad_data */
    {
	struct ulib_desc_cash *cash_real = &data->real;
	struct ulib_utof_cash *lcsh;

	lcsh = &cash_real->addr[1];
	if (lcsh->stad_data != (utofu_stadd_t)-1ULL) {
	    const unsigned long int flag = 0
					/* | UTOFU_REG_MEM_FLAG_READ_ONLY */
					;
	    int uc;

	    uc = utofu_dereg_mem(lcsh->vcqh, lcsh->stad_data, flag);
	    if (uc != UTOFU_SUCCESS) {
		/* YYY abort */
	    }
	    assert(uc == UTOFU_SUCCESS);
	    lcsh->stad_data = -1ULL;
	}
    }
#endif /* 0 */
#endif	/* CONF_ULIB_SHEA_DATA */
    freestack_push(icep->udat_fs, data);

    return ;
}

#endif	/* _TOFU_INT_ULIB_H */
