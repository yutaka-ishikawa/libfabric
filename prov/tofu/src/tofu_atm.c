/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

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
