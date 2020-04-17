/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include <assert.h>	    /* for assert() */

/*
 * fi_ioc (in fi_domain.h)
 *   struct fi_ioc { void *addr; size_t count; };
 * fi_rma_ioc (in fi_rma.h)
 *   struct fi_rma_ioc { uint64_t addr, size_t count; uint64_t key; };
 *
 * lcl fi_ioc     (+ desc)  const
 * rem fi_rma_ioc           const
 * res fi_ioc     (+ desc) !const
 * cmp fi_ioc     (+ desc)  const
 */

struct tofu_atm_vec {
    union {
	const struct fi_ioc *	    lcl;
	const struct fi_rma_ioc *   rmt;
    } vec;
    void **			    dsc;
    size_t			    ioc;
};

struct tofu_atm_arg {
    const struct fi_msg_atomic *    msg;
    uint64_t			    flg;
    struct tofu_atm_vec		    lcl; /* local  */
    struct tofu_atm_vec		    rmt; /* remote */
    struct tofu_atm_vec		    res; /* result */
    struct tofu_atm_vec		    cmp; /* compare */
};

static char *str_atmop[] = {
    "FI_MIN", "FI_MAX", "FI_SUM", "FI_PROD", "FI_LOR", "FI_LAND", "FI_BOR",
    "FI_BAND", "FI_LXOR", "FI_BXOR", "FI_ATOMIC_READ", "FI_ATOMIC_WRITE",
    "FI_CSWAP",	"FI_CSWAP_NE", "FI_CSWAP_LE", "FI_CSWAP_LT", "FI_CSWAP_GE",
    "FI_CSWAP_GT", "FI_MSWAP", "FI_ATOMIC_OP_LAST"};

static char *str_dtype[] = {
    "FI_INT8", "FI_UINT8", "FI_INT16", "FI_UINT16", "FI_INT32",
    "FI_UINT32", "FI_INT64", "FI_UINT64", "FI_FLOAT", "FI_DOUBLE",
    "FI_FLOAT_COMPLEX",	"FI_DOUBLE_COMPLEX", "FI_LONG_DOUBLE",
    "FI_LONG_DOUBLE_COMPLEX", "FI_DATATYPE_LAST" };

/* fi_atomicvalid() */
static int
tofu_ctx_atm_writevalid(struct fid_ep *ep, enum fi_datatype datatype,
                        enum fi_op op, size_t *count)
{
    int fc = FI_SUCCESS;

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
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL,
            "in %s, op(%d)=%s dtype(%d)=%s fc(%d)\n",
            __FILE__, op, str_atmop[op], datatype, str_dtype[datatype], fc);
    return fc;
}

/* fi_fetch_atomicvalid() */
static int
tofu_ctx_atm_readwritevalid(struct fid_ep *ep, enum fi_datatype datatype,
                            enum fi_op op, size_t *count)
{
    int fc = FI_SUCCESS;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
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
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL,
            "in %s, op(%d)=%s dtype(%d)=%s fc(%d)\n",
            __FILE__, op, str_atmop[op], datatype, str_dtype[datatype], fc);
    return fc;
}

/* fi_compare_atomicvalid() */
static int
tofu_ctx_atm_compwritevalid(struct fid_ep *ep, enum fi_datatype datatype,
                            enum fi_op op, size_t *count)
{
    int fc = FI_SUCCESS;
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fc = -FI_EOPNOTSUPP;
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL,
            "in %s, op(%d)=%s dtype(%d)=%s fc(%d)\n",
            __FILE__, op, str_atmop[op], datatype, str_dtype[datatype], fc);
    return fc;
}

static inline void
tofu_ce_atm_notify_self(struct tofu_ctx *ctx_priv_tx,
                        uint64_t wlen,
                        const struct tofu_atm_arg *aarg)
{
    struct fi_cq_tagged_entry cqe[1];
    uint64_t    flags;

    R_DBGMSG("YI******\n");
    /* The atomic operation queue/counter is the sender side */
    // ulib_notify_sndcmpl_cntr(ctx_priv_tx->ctx_send_ctr, 0);
    /*
     * man fi_cq(3)
     *   FI_ATOMIC
     *     Indicates that an atomic operation completed.
     *     This flag may be combined with an FI_READ, FI_WRITE,
     *     FI_REMOTE_READ, or FI_REMOTE_WRITE flag.
     */
    flags = (aarg->msg->op == FI_ATOMIC_READ) ?
                        (FI_ATOMIC|FI_READ) : (FI_ATOMIC|FI_WRITE);
#if 0
    ulib_init_cqe(cqe, aarg->msg->context, flags,
                  wlen, 0, aarg->msg->data, 0);
    ulib_notify_sndcmpl_cq(ctx_priv_tx->ctx_send_cq, NULL, cqe);
#endif
}

static inline int
tofu_ctx_atm_wmsg_self(struct tofu_ctx *ctx_priv_tx,
                       const struct tofu_atm_arg *aarg)
{
    int fc = FI_SUCCESS;

    R_DBGMSG("YI******\n");
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(ctx_priv_tx != 0);
    assert(ctx_priv_tx->ctx_fid.fid.fclass == FI_CLASS_TX_CTX);

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

#if 0
	/* rmt from rmt_ioc */
        fc = tofu_atm_ioc_from_rma(rmt_ioc, dtsz, tmpioc);
        if (fc != 0) { goto bad; }
#endif
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
        tofu_ce_atm_notify_self(ctx_priv_tx, dtsz, aarg);
    }
bad:
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}

static inline int
tofu_ctx_atm_rmsg_self(struct tofu_ctx *ctx_priv_tx,
                       const struct tofu_atm_arg *aarg)
{
    int fc = FI_SUCCESS;

    R_DBGMSG("YI******\n");
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(ctx_priv_tx != 0);
    assert(ctx_priv_tx->ctx_fid.fid.fclass == FI_CLASS_TX_CTX);

    /* check iov_count for lcl, rmt, and res */
    if ((aarg == 0)
	|| (aarg->lcl.ioc != aarg->res.ioc)
	|| (aarg->rmt.ioc != aarg->res.ioc)
	|| (aarg->res.ioc > 1)) {
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
#if 0
        fc = tofu_atm_ioc_from_rma(rmt_ioc, dtsz, tmpioc);
        if (fc != 0) { goto bad; }
#endif
        rmt = tmpioc;

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
        tofu_ce_atm_notify_self(ctx_priv_tx, dtsz, aarg);
    }
bad:
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}

/* fi_atomicmsg() */
static ssize_t
tofu_ctx_atm_writemsg(struct fid_ep *fid_ep,
                      const struct fi_msg_atomic *msg,
                      uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_ctx *ctx_priv = 0;

    R_DBGMSG("YI******\n");
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    if (ctx_priv == 0) { }
    /* if (ctx_priv->ctx_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

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

	fc = tofu_ctx_atm_wmsg_self( ctx_priv, aarg );
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    }

bad:
    return ret;
}

/* fi_fetch_atomicmsg() */
static ssize_t
tofu_ctx_atm_readwritemsg(struct fid_ep *fid_ep,
                          const struct fi_msg_atomic *msg,
                          struct fi_ioc *resultv, void **result_desc,
                          size_t result_count, uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_ctx *ctx_priv = 0;

    R_DBGMSG("YI******\n");
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    if (ctx_priv == 0) { }
    /* if (ctx_priv->ctx_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "C lcl %ld rem %ld res %ld cmp %ld\n",
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
    } else {
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
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "c lcl %ld rem %ld res %ld cmp %ld\n",
            msg->msg_iov->count, msg->rma_iov->count, resultv->count, 0L);
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "d lcl %p rem %p res %p cmp %p\n",
            msg->desc, NULL, result_desc, NULL);
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "op %d dt %d flags %016"PRIx64"\n",
            msg->op, msg->datatype, flags);
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "comp %"PRIx64" %c%c%c%c\n",
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

	fc = tofu_ctx_atm_rmsg_self( ctx_priv, aarg );
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    }

bad:
    return ret;
}

/* fi_compare_atomicmsg() */
static ssize_t
tofu_ctx_atm_compwritemsg(struct fid_ep *fid_ep,
                          const struct fi_msg_atomic *msg,
                          const struct fi_ioc *comparev,
                          void **compare_desc, size_t compare_count,
                          struct fi_ioc *resultv, void **result_desc,
                          size_t result_count, uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_ctx *ctx_priv = 0;

    R_DBGMSG("YI******\n");
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    if (ctx_priv == 0) { }
    /* if (ctx_priv->ctx_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    ret = -FI_ENOSYS;

bad:
    return ret;
}

struct fi_ops_atomic tofu_ctx_ops_atomic = {
    .size	    = sizeof (struct fi_ops_atomic),
    .write	    = fi_no_atomic_write,
    .writev	    = fi_no_atomic_writev,
    .writemsg	    = tofu_ctx_atm_writemsg,
    .inject	    = fi_no_atomic_inject,
    .readwrite	    = fi_no_atomic_readwrite,
    .readwritev	    = fi_no_atomic_readwritev,
    .readwritemsg   = tofu_ctx_atm_readwritemsg,
    .compwrite	    = fi_no_atomic_compwrite,
    .compwritev	    = fi_no_atomic_compwritev,
    .compwritemsg   = tofu_ctx_atm_compwritemsg,
    .writevalid	    = tofu_ctx_atm_writevalid,
    .readwritevalid = tofu_ctx_atm_readwritevalid,
    .compwritevalid = tofu_ctx_atm_compwritevalid,
};
