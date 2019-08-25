/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_debug.h"
#include "tofu_impl.h"
#include "ulib_shea.h"
#include "ulib_conv.h"
#include "ulib_ofif.h"
#include "ulib_toqc.h" 
#include "ulib_notify.h"
#include <assert.h>	    /* for assert() */

static inline void tofu_ofif_liov_set(struct iovec *lma_iov,
	void *buf, size_t len)
{
    lma_iov->iov_base   = buf;
    lma_iov->iov_len    = len;
    return ;
}

static inline void tofu_ofif_riov_set(struct fi_rma_iov *rma_iov,
	uint64_t rof, size_t len, uint64_t key)
{
    rma_iov->addr       = rof;
    rma_iov->len        = len;
    rma_iov->key        = key;
    return ;
}

static inline void tofu_ofif_desc_set(void *dscs[1], void *desc)
{
    dscs[0] = desc;
    return ;
}

static inline void tofu_ofif_mrma_set_lma(struct fi_msg_rma *rma,
	struct iovec *lma_iov, void **lma_dsc, size_t lma_cnt)
{
    rma->msg_iov        = lma_iov;
    rma->desc           = lma_dsc;
    rma->iov_count      = lma_cnt;
    return ;
}

static inline void tofu_ofif_mrma_set_rma(struct fi_msg_rma *rma,
	fi_addr_t rma_fia, struct fi_rma_iov *rma_iov, size_t rma_cnt)
{
    rma->addr           = rma_fia;
    rma->rma_iov        = rma_iov;
    rma->rma_iov_count  = rma_cnt;
    return ;
}

static inline size_t tofu_ofif_desc_get_len(void *desc)
{
    size_t ret = 0;
    struct tofu_mr *mr__priv = desc;
    struct fi_mr_attr *attr;
    size_t ii, ni;

    assert(mr__priv != 0);
    assert(mr__priv->mr__fid.fid.fclass == FI_CLASS_MR);
    attr = &mr__priv->mr__att;
    ni = attr->iov_count;
    for (ii = 0; ii < ni; ii++) {
	const struct iovec *ivec = &attr->mr_iov[ii];
	ret += ivec->iov_len;
    }
    return ret;
}

static inline uint64_t tofu_ofif_desc_get_key(void *desc)
{
    uint64_t ret;
    struct tofu_mr *mr__priv = desc;

    assert(mr__priv != 0);
    assert(mr__priv->mr__fid.fid.fclass == FI_CLASS_MR);
    ret = mr__priv->mr__fid.key;

    return ret;
}

static inline int
tofu_ofif_find_utof_cash(struct ulib_icep *icep,
	fi_addr_t rfia,
	struct ulib_utof_cash utof_cashes[2])
{
    int ret = FI_SUCCESS;
    extern int ulib_icep_find_desc(struct ulib_icep *icep,
	fi_addr_t dfia, struct ulib_toqc_cash **pp_cash_tmpl);
    struct ulib_toqc_cash *cash_tmpl = 0;
    int uc;

    uc = ulib_icep_find_desc(icep, rfia, &cash_tmpl);
    if (uc != UTOFU_SUCCESS) {
	assert(uc == UTOFU_SUCCESS);
	ret = -FI_ENOENT; goto bad;
    }
    assert(cash_tmpl != 0);

    utof_cashes[0] = cash_tmpl->addr[ 0 /* remote */ ];
    utof_cashes[1] = cash_tmpl->addr[ 1 /* local  */ ];

bad:
    return ret;
}

static inline int
tofu_ofif_regm_reg_iov(struct ulib_icep *icep,
	const struct iovec *iov, utofu_stadd_t *p_stad)
{
    int ret = FI_SUCCESS;
    utofu_stadd_t stad = -1UL;
    int uc;

    assert(iov != 0);
    uc = utofu_reg_mem(icep->vcqh,
	    iov->iov_base, iov->iov_len, 0 /* uflg */, &stad);
    if (uc != UTOFU_SUCCESS) {
	ret = -FI_EINVAL; goto bad;
    }
    p_stad[0] = stad;

bad:
    return ret;
}

static inline int
tofu_ofif_regm_rel(struct ulib_icep *icep, utofu_stadd_t stad)
{
    int ret = FI_SUCCESS;
    int uc;

    uc = utofu_dereg_mem(icep->vcqh, stad, 0 /* uflg */ );
    if (uc != UTOFU_SUCCESS) {
	ret = -FI_EINVAL; goto bad;
    }

bad:
    return ret;
}

static inline int
tofu_ofif_todq_make_read(struct ulib_utof_cash utof_cashes[2],
	const struct fi_msg_rma *rma,
	struct ulib_toqd_cash *p_toqd)
{
    int ret = FI_SUCCESS;
    uint64_t loff = 0;
    uint64_t roff = 0 /* YYY rma->rma_iov[0].addr */;
    uint64_t leng = rma->msg_iov[0].iov_len;
    uint64_t edat = rma->data; /* XXX hack: NEEDS FIXED 2019/08/15 */
    unsigned long uflg = 0
			| UTOFU_ONESIDED_FLAG_TCQ_NOTICE
			| UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
			| UTOFU_ONESIDED_FLAG_STRONG_ORDER
			;
    int uc;

    uc = ulib_desc_get1_from_cash(&utof_cashes[0],
	    &utof_cashes[1], roff, loff, leng, edat,
	    uflg, p_toqd);
    if (uc != UTOFU_SUCCESS) {
	assert(uc == UTOFU_SUCCESS);
	ret = -FI_EINVAL; goto bad;
    }

bad:
    return ret;
}


static inline ssize_t
tofu_icep_rma_read_common(struct fid_ep *fid_ep,
			  const struct fi_msg_rma *rma,
			  uint64_t flags)
{
    ssize_t fc = FI_SUCCESS;
    struct tofu_cep         *cep_priv;
    struct tofu_av          *av__priv;
    struct ulib_icep        *icep_ctxt, *icep;
    struct ulib_utof_cash   utof_cashes[2];
    utofu_stadd_t           lsta = -1UL;
    struct ulib_toqd_cash   toqd;
    struct ulib_toqc        *toqc;
    int                     uc;
    uint64_t                r_no = UINT64_MAX;

    assert(fid_ep->fid.fclass == FI_CLASS_TX_CTX);
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);

    assert(cep_priv->cep_sep != 0);
    av__priv = cep_priv->cep_sep->sep_av_;

    icep_ctxt = (struct ulib_icep *)(cep_priv + 1);
    icep = icep_ctxt->shadow;
    assert(icep != 0);

    toqc = icep->toqc;
    assert(toqc != 0);

    /* copy template cash to utof_cashes[] */
    fc = tofu_ofif_find_utof_cash(icep, rma->addr, utof_cashes);
    if (fc != FI_SUCCESS) { goto bad; }

    /* modify utof_cash (remote) to make real cash */
    assert(rma->rma_iov_count == 1);
    utof_cashes[ 0 /* remote */ ].stad = rma->rma_iov[0].key;

    /* modify utof_cash (local)  to make real cash */
    assert(rma->iov_count == 1);
    if (rma->desc == 0) {
	fc = tofu_ofif_regm_reg_iov(icep, &rma->msg_iov[0], &lsta);
	if (fc != FI_SUCCESS) { goto bad; }
	assert(lsta != -1UL);
	utof_cashes[ 1 /* local  */ ].stad = lsta;
    } else {
	utofu_stadd_t stad;
	/* desc = fi_mr_desc(fid_mr) in fi_domain.h */
	stad = tofu_ofif_desc_get_key(rma->desc[0]);
	utof_cashes[ 1 /* local  */ ].stad = stad;
    }

    /* make an RDMA-GET (fi_read) TOQ descriptor */
    fc = tofu_ofif_todq_make_read(utof_cashes, rma, &toqd);
    if (fc != FI_SUCCESS) { goto bad; }

    /* post it */
    uc = ulib_toqc_post(toqc, toqd.desc, toqd.size, 0 /* retp */,
		toqd.ackd, &r_no);
    if (uc != UTOFU_SUCCESS) {
	assert(uc == UTOFU_SUCCESS);
	if (uc == UTOFU_ERR_FULL) {
	    fc = -FI_EAGAIN; goto bad;
	}
	fc = -FI_EINVAL; goto bad;
    }

    struct ulib_rma_cmpl *rma_cmpl;
    uint64_t edat = rma->data; /* YYY XXX icep->nrma + 1 */

    assert((edat > 0) && (edat <= ULIB_RMA_NUM)); /* YYY */
    rma_cmpl = &toqc->rma_cmpl[edat - 1]; /* idx is (edata - 1) */

    rma_cmpl->magic = 1;
    rma_cmpl->cep_priv = cep_priv;
    /* YYY to use cep_read_cq for FI_READ instead of cep_send_cq (FI_SEND) */
    rma_cmpl->cq__priv = cep_priv->cep_send_cq;
    printf("%d: %s cq__priv = %p (cep_send_cq)\n", mypid, __func__, rma_cmpl->cq__priv); fflush(stderr);
    rma_cmpl->stadd = lsta;
    rma_cmpl->op_context = rma->context;
    rma_cmpl->flags = FI_READ | FI_RMA | flags;
    rma_cmpl->len = rma->msg_iov[0].iov_len;
    rma_cmpl->buf = rma->msg_iov[0].iov_base;
    rma_cmpl->data = 0;
    rma_cmpl->tag = 0;

    lsta = -1UL; /* ZZZ */

    if (0) {
	int ii;

	for (ii = 0; ii < 16; ii++) {
	    uc = ulib_toqc_prog(toqc);
	    if (uc != UTOFU_SUCCESS) { break; }
	}
    }

bad:
    if (lsta != -1UL) {
	(void) tofu_ofif_regm_rel(icep, lsta);
    }
    return fc;
}

static inline int
ulib_conv_paid(utofu_vcq_id_t vcqid, utofu_path_id_t *p_paid)
{
    int uc;
    union ulib_tofa_u utofa;
    uint8_t abc[3];

    utofa.ui64 = vcqid;
#if 0
    abc[0] = utofa.tank.tua;
    abc[1] = utofa.tank.tub;
    abc[2] = utofa.tank.tuc;
#else
    abc[0] = abc[1] = abc[2] = 0; /* all zero */
#endif
    uc = utofu_get_path_id(vcqid, abc, p_paid);
    printf("%d:PATH paid(0x%x) abc(%d:%d:%d)\n", mypid, *p_paid, abc[0], abc[1], abc[2]); fflush(stdout);
    return uc;
}


/*
 * MUST BE CRITIAL REGION 2019/04/29
 */
void
ulib_notify_rmacmpl_cq(struct ulib_toqc *toqc, int idx)
{
    struct ulib_rma_cmpl      *rma_cmpl;
    struct tofu_cq            *cq_priv;
    struct fi_cq_tagged_entry cq_e[1];
    struct tofu_cep           *cep_priv;
    struct ulib_icep          *icep_ctxt, *icep;

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "idx(%d)\n", idx);
    rma_cmpl = &toqc->rma_cmpl[idx - 1];
    cq_priv = rma_cmpl->cq__priv;
    cep_priv = rma_cmpl->cep_priv;
    icep_ctxt = (struct ulib_icep *)(rma_cmpl->cep_priv + 1);
    icep = icep_ctxt->shadow;
    if (rma_cmpl->stadd != -1UL) {
	int uc;
	uc = utofu_dereg_mem(icep->vcqh, rma_cmpl->stadd, 0 /* uflg */ );
	if (uc != UTOFU_SUCCESS) {
	    /* YYY */
	}
	rma_cmpl->stadd = -1UL;
    }
    assert(icep->nrma > 0);
    --icep->nrma;     /* decrement # of rma ops on the fly */

    /* completion notification */
    ulib_notify_sndcmpl_cntr(cep_priv->cep_send_ctr, 0);
    ulib_init_cqe(cq_e, rma_cmpl->op_context, rma_cmpl->flags, 
                  rma_cmpl->len, rma_cmpl->buf, rma_cmpl->data, rma_cmpl->tag);
    ulib_notify_sndcmpl_cq(cq_priv, NULL, cq_e);
}


/* should be moved to somewhere */
static inline char *
fi_addr2string(char *buf, ssize_t sz, fi_addr_t fi_addr, struct fid_ep *fid_ep)
{
    struct tofu_cep *cep_priv;
    struct tofu_av *av__priv;
    uint64_t ui64;

    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    av__priv = cep_priv->cep_sep->sep_av_;
    tofu_av_lup_tank(av__priv, fi_addr, &ui64);
    return tank2string(buf, sz, ui64);
}

static inline int tofu_cep_rma_rmsg_self(struct tofu_cep *cep_priv_tx,
                                         const struct fi_msg_rma *msg_rma,
                                         uint64_t flags)
{
    int fc = FI_SUCCESS;
    /* struct tofu_sep *sep_priv; */
    size_t rlen, wlen = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(cep_priv_tx != 0);
    assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
    /* assert(cep_priv_tx->cep_sep != 0); */
    /* sep_priv = cep_priv_tx->cep_sep; */

    if ((msg_rma->iov_count != msg_rma->rma_iov_count)
	|| (msg_rma->iov_count > 1)) {
	fc = -FI_EINVAL; goto bad;
    }

    /* copy: rlen and wlen */
    if (msg_rma->iov_count == 1) { /* likely() */
	struct iovec *iov_dst, *iov_src, iovs[1];
	size_t ioc_dst, ioc_src, iof_dst = 0;

	iov_dst = (struct iovec *)msg_rma->msg_iov;
	ioc_dst = msg_rma->iov_count;
	assert(iov_dst != 0);

	/* requested length */
	rlen = ofi_total_iov_len( iov_dst, ioc_dst );

	{
	    const struct fi_rma_iov *rma_iov = msg_rma->rma_iov;
	    struct fi_mr_attr *attr;
	    const struct iovec *mr_iov;

	    /* attr */
	    {
		struct tofu_mr *mr__priv;
		assert(rma_iov != 0);
		mr__priv = (void *)(uintptr_t)rma_iov->key; /* XXX */
		assert(mr__priv->mr__fid.fid.fclass == FI_CLASS_MR);
		attr = &mr__priv->mr__att;
	    }
	    /* mr_iov */
	    if ((attr->iov_count < 1)
		|| (attr->mr_iov == 0)
		|| (attr->mr_iov->iov_len < rma_iov->len)
		|| (attr->mr_iov->iov_base == 0)) {
		fc = -FI_EINVAL; goto bad;
	    }
	    mr_iov = attr->mr_iov;

	    /* FI_MR_BASIC */
	    iovs->iov_len = rma_iov->len;
	    iovs->iov_base = (void *)(uintptr_t)rma_iov->addr;
	    if ((iovs->iov_base < mr_iov->iov_base)
                || ((char*)iovs->iov_base >= (char*) ((char*)mr_iov->iov_base + mr_iov->iov_len))) {
		/* FI_MR_SCALABLE */
		assert(rma_iov->addr /* offset */ < mr_iov->iov_len);
		iovs->iov_base = (void*) ((char*)mr_iov->iov_base + rma_iov->addr);  /* offset */
		FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff %"PRIu64" o\n",
		    rma_iov->addr);
	    } else {
		FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff %ld V\n",
                         (char*) iovs->iov_base - (char*) mr_iov->iov_base);
	    }
	}

	iov_src = iovs;
	ioc_src = 1 /* msg_rma->rma_iov_count */;

	wlen = tofu_copy_iovs(iov_dst, ioc_dst, iof_dst, iov_src, ioc_src);
	FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "wlen %ld\n", wlen);
        if (wlen <= rlen) {
            FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "wlen <= rlenn");
            assert(0);
        }
    }
    {
	struct fi_cq_tagged_entry cq_e[1];
        /* The rma completion queue/counter is the sender side */
        ulib_notify_sndcmpl_cntr(cep_priv_tx->cep_send_ctr, 0);
        ulib_init_cqe(cq_e, msg_rma->context,
                      FI_RMA | FI_READ | FI_DELIVERY_COMPLETE,
                      wlen, 0, msg_rma->data, 0);
        ulib_notify_sndcmpl_cq(cep_priv_tx->cep_send_cq, NULL, cq_e);
    }
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}

static inline int 
tofu_cep_rma_wmsg_self(struct tofu_cep *cep_priv_tx,
                       const struct fi_msg_rma *msg_rma,
                       uint64_t flags)
{
    int fc = FI_SUCCESS;
    /* struct tofu_sep *sep_priv; */ /* AV */
    size_t rlen, wlen = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(cep_priv_tx != 0);
    assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
    /* assert(cep_priv_tx->cep_sep != 0); */
    /* sep_priv = cep_priv_tx->cep_sep; */

    if ((msg_rma->iov_count != msg_rma->rma_iov_count)
	|| (msg_rma->iov_count > 1)) {
	fc = -FI_EINVAL; goto bad;
    }

    /* copy: rlen and wlen */
    if (msg_rma->iov_count == 1) { /* likely() */
	struct iovec *iov_dst, *iov_src, iovs[1];
	size_t ioc_dst, ioc_src, iof_dst = 0;

	iov_src = (struct iovec *)msg_rma->msg_iov;
	ioc_src = msg_rma->iov_count;
	assert(iov_src != 0);

	/* requested length */
	rlen = ofi_total_iov_len( iov_src, ioc_src );

	{
	    const struct fi_rma_iov *rma_iov = msg_rma->rma_iov;
	    struct fi_mr_attr *attr;
	    const struct iovec *mr_iov;

	    /* attr */
	    {
		struct tofu_mr *mr__priv;
		assert(rma_iov != 0);
		mr__priv = (void *)(uintptr_t)rma_iov->key; /* XXX */
		assert(mr__priv->mr__fid.fid.fclass == FI_CLASS_MR);
		attr = &mr__priv->mr__att;
	    }
	    /* mr_iov */
	    if ( 0
		|| (attr->iov_count < 1)
		|| (attr->mr_iov == 0)
		|| (attr->mr_iov->iov_len < rma_iov->len)
		|| (attr->mr_iov->iov_base == 0)
	    ) {
		fc = -FI_EINVAL; goto bad;
	    }
	    mr_iov = attr->mr_iov;

	    /* FI_MR_BASIC */
	    iovs->iov_len = rma_iov->len;
	    iovs->iov_base = (void *)(uintptr_t)rma_iov->addr;
	    if ( 0
		|| (iovs->iov_base < mr_iov->iov_base)
                 || (iovs->iov_base >= (void*) ((char*)mr_iov->iov_base + mr_iov->iov_len))
	    ) {
		/* FI_MR_SCALABLE */
		assert(rma_iov->addr /* offset */ < mr_iov->iov_len);
		iovs->iov_base = (void*)((char*)mr_iov->iov_base + rma_iov->addr); /* offset */;
		FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff %"PRIu64" o\n",
		    rma_iov->addr);
	    }
	    else {
		FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff %ld V\n",
                         (char*) iovs->iov_base - (char*) mr_iov->iov_base);
	    }
	}

	iov_dst = iovs;
	ioc_dst = 1 /* msg_rma->rma_iov_count */;

	wlen = tofu_copy_iovs(iov_dst, ioc_dst, iof_dst, iov_src, ioc_src);
	FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "wlen %ld\n", wlen);
        if (wlen <= rlen) {
            FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "wlen <= rlenn");
            assert(0);
        }
    }
    {
	struct fi_cq_tagged_entry cq_e[1];
        /* The rma completion queue is the sender side */
        ulib_notify_sndcmpl_cntr(cep_priv_tx->cep_send_ctr, 0);
        ulib_init_cqe(cq_e, msg_rma->context,
                      FI_RMA | FI_WRITE | FI_DELIVERY_COMPLETE,
                      wlen, 0, msg_rma->data, 0);
        ulib_notify_sndcmpl_cq(cep_priv_tx->cep_send_cq, NULL, cq_e);
    }
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}


/*
 * fi_read:
 */
static ssize_t
tofu_cep_rma_read(struct fid_ep *fid_ep, void *buf, size_t len, void *desc,
                  fi_addr_t src_addr, uint64_t addr, uint64_t key,
                  void *context)
{
    ssize_t             ret = FI_SUCCESS;
    uint64_t            flags = FI_COMPLETION; /* must be set 2019/08/15 */
    struct tofu_cep     *cep_priv;
    struct ulib_icep    *icep_ctxt, *icep;
    struct fi_msg_rma   rma;
    struct iovec        lma_iov;
    struct fi_rma_iov   rma_iov;
    void                **lma_dsc = 0, *lma_dscs[1];

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "buf(%p) len(%ld) desc(%p) addr(0x%lx) key(%lx) context(%p) in %s\n", buf, len, desc, addr, key, context, __FILE__);
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);

    icep_ctxt = (struct ulib_icep*) (cep_priv + 1);
    icep = icep_ctxt->shadow;
    fprintf(stderr, "\ticep_ctxt(%p) icep_ctxt->shadow(%p)\n", icep_ctxt, icep); fflush(stderr);

    /* rma.msg_iov[] */
    tofu_ofif_liov_set(&lma_iov, buf, len);
    /* rma.rma_iov[] */
    tofu_ofif_riov_set(&rma_iov, addr /* roff */, len, key);
    /* rma.desc */
    if (desc != 0) {
	tofu_ofif_desc_set(&lma_dscs[0], desc);
	lma_dsc = lma_dscs;
    }

    tofu_ofif_mrma_set_lma(&rma, &lma_iov, lma_dsc, 1);
    tofu_ofif_mrma_set_rma(&rma, src_addr, &rma_iov, 1);
    rma.context = context;
    /* XXX flags &= ~FI_REMOTE_CQ_DATA */
    rma.data    = icep->nrma + 1; /* YYY XXX edat MUST BE CHANGED 2019/08/15 */

    ret = tofu_icep_rma_read_common(fid_ep, &rma, flags);
    if (ret != FI_SUCCESS) {
	assert(ret == FI_SUCCESS);
	goto bad;
    }

    icep->nrma++; /* YYY XXX edat */

bad:
    FI_DBG(&tofu_prov, FI_LOG_EP_DATA, "return %ld\n", ret);
    return ret;
}

/*
 * fi_readmsg
 */
static ssize_t
tofu_cep_rma_readmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_rma *msg,
                     uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_cep *cep_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    fprintf(stderr, "YI**** fi_readmsg is only implemented for self\n"); fflush(stderr);
    printf("YI**** fi_readmsg is only implemented for self\n"); fflush(stdout);

    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    /* if (cep_priv->cep_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "flags %"PRIx64" %c%c%c%c\n",
	flags,
	(flags & FI_COMPLETION)? 'C': '-',
	(flags & FI_INJECT_COMPLETE)? 'I': '-',
	(flags & FI_TRANSMIT_COMPLETE)? 'T': '-',
	(flags & FI_DELIVERY_COMPLETE)? 'D': '-');

    if ((msg == 0)
	|| ((msg->iov_count > 0) && (msg->msg_iov == 0))
	|| ((msg->rma_iov_count > 0) && (msg->rma_iov == 0))) {
        ret = -FI_EINVAL; goto bad;
    }
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "ioc %ld %ld\n",
	msg->iov_count, msg->rma_iov_count);
    {
	size_t msg_len, rma_len;
	msg_len = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	rma_len = tofu_total_rma_iov_len(msg->rma_iov, msg->rma_iov_count);
	FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "totlen %ld %ld\n",
	    msg_len, rma_len);

	if ((msg_len != rma_len)
	    /* || (msg_len > ep_attr->max_msg_size) */
	    /* || (msg->iov_count > tx_attr->iov_limit) */
	    /* || (msg->rma_iov_count > tx_attr->rma_iov_limit) */) {
	    ret = -FI_EINVAL; goto bad;
	}
	if ((flags & FI_INJECT) != 0) {
	}
	if (msg->rma_iov_count > 0) {
	    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "key %016"PRIx64" desc %p\n",
		msg->rma_iov[0].key, msg->desc);
	}
    }

    {
	int fc;
	fc = tofu_cep_rma_rmsg_self(cep_priv, msg, flags);
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    }

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %ld\n", ret);
    return ret;
}
#ifdef	NOTDEF_UTIL

static inline ssize_t tofu_total_rma_iov_len(
    const struct fi_rma_iov *rma_iov,
    size_t rma_iov_count
)
{
    size_t ii;
    ssize_t len = 0;
    for (ii = 0; ii < rma_iov_count; ii++) {
#ifndef	NDEBUG
	if ((rma_iov[ii].len > 0) && (rma_iov[ii].addr == 0)) {
	    return -FI_EINVAL;
	}
#endif
	len += rma_iov[ii].len;
    }
    return len;
}
#endif	/* NOTDEF_UTIL */

/*
 * fi_writemsg
 */
static ssize_t 
tofu_cep_rma_writemsg(struct fid_ep *fid_ep,
                      const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_cep *cep_priv = 0;

    fprintf(stderr, "YI**** fi_writemsg is only implemented for self\n"); fflush(stderr);
    printf("YI**** fi_writemsg is only implemented for self\n"); fflush(stdout);
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    /* if (cep_priv->cep_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "flags %"PRIx64" %c%c%c%c\n",
	flags,
	(flags & FI_COMPLETION)? 'C': '-',
	(flags & FI_INJECT_COMPLETE)? 'I': '-',
	(flags & FI_TRANSMIT_COMPLETE)? 'T': '-',
	(flags & FI_DELIVERY_COMPLETE)? 'D': '-');

    if (
	(msg == 0)
	|| ((msg->iov_count > 0) && (msg->msg_iov == 0))
	|| ((msg->rma_iov_count > 0) && (msg->rma_iov == 0))
    ) {
        ret = -FI_EINVAL; goto bad;
    }
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "ioc %ld %ld\n",
	msg->iov_count, msg->rma_iov_count);
    {
	size_t msg_len, rma_len;
	msg_len = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	rma_len = tofu_total_rma_iov_len(msg->rma_iov, msg->rma_iov_count);
	FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "totlen %ld %ld\n",
	    msg_len, rma_len);

	if ( 0
	    || (msg_len != rma_len)
	    /* || (msg_len > ep_attr->max_msg_size) */
	    /* || (msg->iov_count > tx_attr->iov_limit) */
	    /* || (msg->rma_iov_count > tx_attr->rma_iov_limit) */
	) {
	    ret = -FI_EINVAL; goto bad;
	}
	if ((flags & FI_INJECT) != 0) {
	    if ( 0
		/* || (msg_len > ep_attr->inject_size) */
	    ) {
		ret = -FI_EINVAL; goto bad;
	    }
	}
	if (msg->rma_iov_count > 0) {
	    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "key %016"PRIx64" desc %p\n",
		msg->rma_iov[0].key, msg->desc);
	}
    }

    {
	int fc;
	fc = tofu_cep_rma_wmsg_self( cep_priv, msg, flags );
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    }

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %ld\n", ret);
    return ret;
}

struct fi_ops_rma tofu_cep_ops_rma = {
    .size	    = sizeof (struct fi_ops_rma),
    .read	    = tofu_cep_rma_read,
    .readv	    = fi_no_rma_readv,
    .readmsg	    = tofu_cep_rma_readmsg,
    .write	    = fi_no_rma_write,
    .writev	    = fi_no_rma_writev,
    .writemsg	    = tofu_cep_rma_writemsg,
    .inject	    = fi_no_rma_inject,
    .writedata	    = fi_no_rma_writedata,
    .injectdata	    = fi_no_rma_injectdata,
};
