/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_debug.h"
#include "tofu_impl.h"
#include "ulib_shea.h"
#include "ulib_ofif.h"
#include "ulib_notify.h"
#include "tofu_atm.h"
#include <assert.h>	    /* for assert() */


/* fi_atomicvalid() */
static int tofu_cep_atm_writevalid(
    struct fid_ep *ep,
    enum fi_datatype datatype,
    enum fi_op op,
    size_t *count
)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    switch (op) {
    case FI_SUM:
    case FI_BOR:
    case FI_BAND:
    case FI_BXOR:
    case FI_ATOMIC_READ:
    case FI_ATOMIC_WRITE:
    case FI_CSWAP:
	switch (datatype) {
	case FI_UINT32:
	case FI_UINT64:
	    break;
	case FI_INT32:
	case FI_INT64:
	    break;
	default:
	    fc = -FI_EOPNOTSUPP; goto bad;
	}
	break;
    /* case FI_MIN: */
    /* case FI_MAX: */
    /* case FI_PROD: */
    /* case FI_LOR: */
    /* case FI_LAND: */
    /* case FI_LXOR: */
    /* case FI_CSWAP_NE: */
    /* case FI_CSWAP_LE: */
    /* case FI_CSWAP_LT: */
    /* case FI_CSWAP_GE: */
    /* case FI_CSWAP_GT: */
    /* case FI_MSWAP: */
    default:
	fc = -FI_EOPNOTSUPP; goto bad;
    }
    count[0] = 1;
bad:
    return fc;
}

/* fi_fetch_atomicvalid() */
static int tofu_cep_atm_readwritevalid(
    struct fid_ep *ep,
    enum fi_datatype datatype,
    enum fi_op op,
    size_t *count
)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    switch (op) {
    case FI_SUM:
    case FI_BOR:
    case FI_BAND:
    case FI_BXOR:
    case FI_ATOMIC_READ:
    case FI_ATOMIC_WRITE:
    case FI_CSWAP:
	switch (datatype) {
	case FI_UINT32:
	case FI_UINT64:
	    break;
	case FI_INT32:
	case FI_INT64:
	    break;
	default:
	    fc = -FI_EOPNOTSUPP; goto bad;
	}
	break;
    /* case FI_MIN: */
    /* case FI_MAX: */
    /* case FI_PROD: */
    /* case FI_LOR: */
    /* case FI_LAND: */
    /* case FI_LXOR: */
    /* case FI_CSWAP_NE: */
    /* case FI_CSWAP_LE: */
    /* case FI_CSWAP_LT: */
    /* case FI_CSWAP_GE: */
    /* case FI_CSWAP_GT: */
    /* case FI_MSWAP: */
    default:
	fc = -FI_EOPNOTSUPP; goto bad;
    }
    count[0] = 1;
bad:
    return fc;
}

/* fi_compare_atomicvalid() */
static int tofu_cep_atm_compwritevalid(
    struct fid_ep *ep,
    enum fi_datatype datatype,
    enum fi_op op,
    size_t *count
)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fc = -FI_EOPNOTSUPP;
    return fc;
}

static inline void
tofu_ce_atm_notify_self(struct tofu_cep *cep_priv_tx,
                        uint64_t wlen,
                        const struct tofu_atm_arg *aarg)
{
    struct fi_cq_tagged_entry cqe[1];
    uint64_t    flags;

    /* The atomic operation queue/counter is the sender side */
    ulib_notify_sndcmpl_cntr(cep_priv_tx->cep_send_ctr, 0);

    /*
     * man fi_cq(3)
     *   FI_ATOMIC
     *     Indicates that an atomic operation completed.
     *     This flag may be combined with an FI_READ, FI_WRITE,
     *     FI_REMOTE_READ, or FI_REMOTE_WRITE flag.
     */
    flags = (aarg->msg->op == FI_ATOMIC_READ) ?
                        (FI_ATOMIC|FI_READ) : (FI_ATOMIC|FI_WRITE);
    ulib_init_cqe(cqe, aarg->msg->context, flags,
                  wlen, 0, aarg->msg->data, 0);
    ulib_notify_sndcmpl_cq(cep_priv_tx->cep_send_cq, NULL, cqe);
}

static inline int tofu_cep_atm_wmsg_self(struct tofu_cep *cep_priv_tx,
                                         const struct tofu_atm_arg *aarg)
{
    int fc = FI_SUCCESS;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(cep_priv_tx != 0);
    assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);

    /* check iov_count for lcl and rmt */
    if ((aarg == 0)
	|| (aarg->lcl.ioc != aarg->rmt.ioc)
	|| (aarg->rmt.ioc > 1)) {
	fc = -FI_EINVAL; goto bad;
    }

    /* remote op. */
    if (aarg->rmt.ioc == 1) { /* likely() */
	const struct fi_ioc *lcl, *rmt /* , *res, *cmp */;
	size_t dtsz;
	const struct fi_rma_ioc *rmt_ioc;
	struct fi_ioc tmpioc[1];

	dtsz = ofi_datatype_size(aarg->msg->datatype);
	if (dtsz == 0) {
	    fc = -FI_EINVAL; goto bad;
	}

	/*
	 * man fi_atomic(3)
	 *   For FI_ATOMIC_READ operations, the source buffer operand
	 *   (e.g. fi_fetch_atomic buf parameter) is ignored and may be NULL.
	 */
	if (aarg->msg->op == FI_ATOMIC_READ) {
	    lcl = 0;
	    if (((rmt_ioc = aarg->rmt.vec.rmt) == 0)) {
		fc = -FI_EINVAL; goto bad;
	    }
	    if ((rmt_ioc->count > 1) /* XXX */) {
		fc = -FI_EINVAL; goto bad;
	    }
	} else {
	    if (((lcl = aarg->lcl.vec.lcl) == 0)
		|| ((rmt_ioc = aarg->rmt.vec.rmt) == 0)) {
		fc = -FI_EINVAL; goto bad;
	    }
	    if ((lcl->count != rmt_ioc->count)
		|| (rmt_ioc->count > 1) /* XXX */) {
		fc = -FI_EINVAL; goto bad;
	    }
	}

	/* rmt from rmt_ioc */
        fc = tofu_atm_ioc_from_rma(rmt_ioc, dtsz, tmpioc);
        if (fc != 0) { goto bad; }
        rmt = tmpioc;

	switch (aarg->msg->op) {
	    size_t ic, nc;
	case FI_SUM:
	    switch (aarg->msg->datatype) {
	    case FI_INT32:
		if (dtsz != sizeof (int32_t)) {
		    fc = -FI_EINVAL; goto bad;
		}
		nc = rmt->count;
		for (ic = 0; ic < nc; ic++) {
		    int32_t addv = ((int32_t *)lcl->addr)[ic];
		    int32_t *dst = &((int32_t *)rmt->addr)[ic];

		    dst[0] = dst[0] + addv; /* op (add) */
		}
		break;
	    default:
		fc = -FI_EOPNOTSUPP; goto bad;
	    }
	    break;
	default:
	    fc = -FI_EOPNOTSUPP; goto bad;
	}
        tofu_ce_atm_notify_self(cep_priv_tx, dtsz, aarg);
    }
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}

static inline int
tofu_cep_atm_rmsg_self(struct tofu_cep *cep_priv_tx,
                       const struct tofu_atm_arg *aarg)
{
    int fc = FI_SUCCESS;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(cep_priv_tx != 0);
    assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);

    /* check iov_count for lcl, rmt, and res */
    if ((aarg == 0)
	|| (aarg->lcl.ioc != aarg->res.ioc)
	|| (aarg->rmt.ioc != aarg->res.ioc)
	|| (aarg->res.ioc > 1)
    ) {
	fc = -FI_EINVAL; goto bad;
    }

    /* remote op. */
    if (aarg->res.ioc == 1) { /* likely() */
	const struct fi_ioc *lcl, *rmt, *res /* , *cmp */;
	size_t dtsz;
	const struct fi_rma_ioc *rmt_ioc;
	struct fi_ioc tmpioc[1];

	dtsz = ofi_datatype_size(aarg->msg->datatype);
	if (dtsz == 0) {
	    fc = -FI_EINVAL; goto bad;
	}

	/*
	 * man fi_atomic(3)
	 *   For FI_ATOMIC_READ operations, the source buffer operand
	 *   (e.g. fi_fetch_atomic buf parameter) is ignored and may be NULL.
	 */
	if (aarg->msg->op == FI_ATOMIC_READ) {
	    lcl = 0;
	    if (((rmt_ioc = aarg->rmt.vec.rmt) == 0)
		|| ((res = aarg->res.vec.lcl) == 0)) {
		fc = -FI_EINVAL; goto bad;
	    }
	    if ((rmt_ioc->count != res->count)
		|| (res->count > 1) /* XXX */) {
		fc = -FI_EINVAL; goto bad;
	    }
	} else {
	    if (((lcl = aarg->lcl.vec.lcl) == 0)
		|| ((rmt_ioc = aarg->rmt.vec.rmt) == 0)
		|| ((res = aarg->res.vec.lcl) == 0)) {
		fc = -FI_EINVAL; goto bad;
	    }
	    if ((lcl->count != res->count)
		|| (rmt_ioc->count != res->count)
		|| (res->count > 1) /* XXX */) {
		fc = -FI_EINVAL; goto bad;
	    }
	}

	/* rmt from rmt_ioc */
	{
#ifdef	NOTDEF
	    const struct fi_mr_attr *attr;
	    const struct iovec *mr_iov;

	    /* attr */
	    {
		struct tofu_mr *mr__priv;
		assert(rmt_ioc != 0);
		mr__priv = (void *)(uintptr_t)rmt_ioc->key; /* XXX */
		assert(mr__priv->mr__fid.fid.fclass == FI_CLASS_MR);
		attr = &mr__priv->mr__att;
	    }
	    /* mr_iov */
	    {
		/* check mr attr */
		if ( 0
		    || (attr->iov_count < 1)
		    || (attr->mr_iov == 0)
		    /* || (attr->mr_iov->iov_len < rmt_ioc->count ) */
		    || (attr->mr_iov->iov_base == 0)
		) {
		    fc = -FI_EINVAL; goto bad;
		}
		mr_iov = attr->mr_iov;
	    }

	    /* FI_MR_BASIC */
	    tmpioc->count = rmt_ioc->count;
	    tmpioc->addr = (void *)(uintptr_t)rmt_ioc->addr;
	    if ( 0
		|| (tmpioc->addr < mr_iov->iov_base)
		|| (tmpioc->addr >= (mr_iov->iov_base + mr_iov->iov_len))
	    ) {
		/* FI_MR_SCALABLE */
		assert(rmt_ioc->addr /* offset */ < mr_iov->iov_len);
		tmpioc->addr = mr_iov->iov_base + rmt_ioc->addr /* offset */;
		FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff %"PRIu64" o\n",
		    rmt_ioc->addr);
	    } else {
		FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff %ld V\n",
		    tmpioc->addr - mr_iov->iov_base);
	    }

	    rmt = tmpioc;
#else	/* NOTDEF */
	    fc = tofu_atm_ioc_from_rma(rmt_ioc, dtsz, tmpioc);
	    if (fc != 0) { goto bad; }

	    rmt = tmpioc;
#endif	/* NOTDEF */
	}

	switch (aarg->msg->op) {
	    size_t ic, nc;
	case FI_SUM:
	    switch (aarg->msg->datatype) {
	    case FI_INT32:
		if (dtsz != sizeof (int32_t)) {
		    fc = -FI_EINVAL; goto bad;
		}
		nc = res->count;
		for (ic = 0; ic < nc; ic++) {
		    int32_t addv = ((int32_t *)lcl->addr)[ic];
		    int32_t *dst = &((int32_t *)rmt->addr)[ic];
		    int32_t *sav = &((int32_t *)res->addr)[ic];

		    sav[0] = dst[0]; /* fetch */
		    dst[0] = dst[0] + addv; /* op (add) */
		}
		break;
	    default:
		fc = -FI_EOPNOTSUPP; goto bad;
	    }
	    break;
	default:
	    fc = -FI_EOPNOTSUPP; goto bad;
	}
        tofu_ce_atm_notify_self(cep_priv_tx, dtsz, aarg);
    }
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}

/* fi_atomicmsg() */
static ssize_t tofu_cep_atm_writemsg(
    struct fid_ep *fid_ep,
    const struct fi_msg_atomic *msg,
    uint64_t flags
)
{
    ssize_t ret = 0;
    struct tofu_cep *cep_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    if (cep_priv == 0) { }
    /* if (cep_priv->cep_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    {
	struct tofu_atm_arg aarg[1];
	int fc;

	aarg->msg = msg;
	aarg->flg = flags;

	aarg->lcl.vec.lcl = msg->msg_iov;
	aarg->lcl.dsc = msg->desc;
	aarg->lcl.ioc = msg->iov_count;

	aarg->rmt.vec.rmt = msg->rma_iov;
	aarg->rmt.dsc = 0;
	aarg->rmt.ioc = msg->rma_iov_count;

	/* aarg->res.vec.lcl = 0; */
	/* aarg->res.dsc = 0; */
	/* aarg->res.ioc = 0; */

	/* aarg->cmp.vec.lcl = 0; */
	/* aarg->cmp.dsc = 0; */
	/* aarg->cmp.ioc = 0; */

	fc = tofu_cep_atm_wmsg_self( cep_priv, aarg );
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    }

bad:
    return ret;
}

/* fi_fetch_atomicmsg() */
static ssize_t tofu_cep_atm_readwritemsg(
    struct fid_ep *fid_ep,
    const struct fi_msg_atomic *msg,
    struct fi_ioc *resultv,
    void **result_desc,
    size_t result_count,
    uint64_t flags
)
{
    ssize_t ret = 0;
    struct tofu_cep *cep_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    if (cep_priv == 0) { }
    /* if (cep_priv->cep_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "C lcl %ld rem %ld res %ld cmp %ld\n",
	msg->iov_count, msg->rma_iov_count, result_count, 0L);
    /*
     * man fi_atomic(3)
     *   For FI_ATOMIC_READ operations, the source buffer operand
     *   (e.g. fi_fetch_atomic buf parameter) is ignored and may be NULL.
     */
    if (msg->op == FI_ATOMIC_READ) {
	if (msg->rma_iov_count != result_count) {
	    ret = -FI_EINVAL; goto bad;
	}
    }
    else {
	if ( 0
	    || (msg->iov_count != result_count)
	    || (msg->rma_iov_count != result_count)
	) {
	    ret = -FI_EINVAL; goto bad;
	}
    }
    if (result_count <= 0) {
	ret = -FI_EINVAL; goto bad;
    }
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "c lcl %ld rem %ld res %ld cmp %ld\n",
	msg->msg_iov->count, msg->rma_iov->count, resultv->count, 0L);
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "d lcl %p rem %p res %p cmp %p\n",
	msg->desc, NULL, result_desc, NULL);
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "op %d dt %d flags %016"PRIx64"\n",
	msg->op, msg->datatype, flags);
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "comp %"PRIx64" %c%c%c%c\n",
	flags,
	(flags & FI_COMPLETION)? 'C': '-',
	(flags & FI_INJECT_COMPLETE)? 'I': '-',
	(flags & FI_TRANSMIT_COMPLETE)? 'T': '-',
	(flags & FI_DELIVERY_COMPLETE)? 'D': '-');

    {
	struct tofu_atm_arg aarg[1];
	int fc;

	aarg->msg = msg;
	aarg->flg = flags;

	aarg->lcl.vec.lcl = msg->msg_iov;
	aarg->lcl.dsc = msg->desc;
	aarg->lcl.ioc = msg->iov_count;

	aarg->rmt.vec.rmt = msg->rma_iov;
	aarg->rmt.dsc = 0;
	aarg->rmt.ioc = msg->rma_iov_count;

	aarg->res.vec.lcl = resultv;
	aarg->res.dsc = result_desc;
	aarg->res.ioc = result_count;

	/* aarg->cmp.vec.lcl = 0; */
	/* aarg->cmp.dsc = 0; */
	/* aarg->cmp.ioc = 0; */

	fc = tofu_cep_atm_rmsg_self( cep_priv, aarg );
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    }

bad:
    return ret;
}

/* fi_compare_atomicmsg() */
static ssize_t tofu_cep_atm_compwritemsg(
    struct fid_ep *fid_ep,
    const struct fi_msg_atomic *msg,
    const struct fi_ioc *comparev,
    void **compare_desc,
    size_t compare_count,
    struct fi_ioc *resultv,
    void **result_desc,
    size_t result_count,
    uint64_t flags
)
{
    ssize_t ret = 0;
    struct tofu_cep *cep_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    if (cep_priv == 0) { }
    /* if (cep_priv->cep_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    ret = -FI_ENOSYS;

bad:
    return ret;
}

struct fi_ops_atomic tofu_cep_ops_atomic = {
    .size	    = sizeof (struct fi_ops_atomic),
    .write	    = fi_no_atomic_write,
    .writev	    = fi_no_atomic_writev,
#ifdef	notdef
    .writemsg	    = fi_no_atomic_writemsg,
#else	/* notdef */
    .writemsg	    = tofu_cep_atm_writemsg,
#endif	/* notdef */
    .inject	    = fi_no_atomic_inject,
    .readwrite	    = fi_no_atomic_readwrite,
    .readwritev	    = fi_no_atomic_readwritev,
#ifdef	notdef
    .readwritemsg   = fi_no_atomic_readwritemsg,
#else	/* notdef */
    .readwritemsg   = tofu_cep_atm_readwritemsg,
#endif	/* notdef */
    .compwrite	    = fi_no_atomic_compwrite,
    .compwritev	    = fi_no_atomic_compwritev,
#ifdef	notdef
    .compwritemsg   = fi_no_atomic_compwritemsg,
#else	/* notdef */
    .compwritemsg   = tofu_cep_atm_compwritemsg,
#endif	/* notdef */
#ifdef	notdef
    .writevalid	    = fi_no_atomic_writevalid,
    .readwritevalid = fi_no_atomic_readwritevalid,
    .compwritevalid = fi_no_atomic_compwritevalid,
#else	/* notdef */
    .writevalid	    = tofu_cep_atm_writevalid,
    .readwritevalid = tofu_cep_atm_readwritevalid,
    .compwritevalid = tofu_cep_atm_compwritevalid,
#endif	/* notdef */
};
