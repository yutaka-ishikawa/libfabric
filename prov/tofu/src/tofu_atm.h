/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_ATM_H
#define _TOFU_ATM_H

#include "tofu_impl.h"

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

#ifdef	CONF_TOFU_RMA

static inline int tofu_cep_atm_wmsg_self(
    struct tofu_cep *cep_priv_tx,
    const struct tofu_atm_arg *aarg
)
{
    int fc = FI_SUCCESS;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(cep_priv_tx != 0);
    assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);

    /* check iov_count for lcl and rmt */
    if ((aarg == 0)
	|| (aarg->lcl.ioc != aarg->rmt.ioc)
	|| (aarg->rmt.ioc > 1)
    ) {
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
	    if ( 0
		|| ((rmt_ioc = aarg->rmt.vec.rmt) == 0)
	    ) {
		fc = -FI_EINVAL; goto bad;
	    }
	    if ( 0
		|| (rmt_ioc->count > 1) /* XXX */
	    ) {
		fc = -FI_EINVAL; goto bad;
	    }
	}
	else {
	    if ( 0
		|| ((lcl = aarg->lcl.vec.lcl) == 0)
		|| ((rmt_ioc = aarg->rmt.vec.rmt) == 0)
	    ) {
		fc = -FI_EINVAL; goto bad;
	    }
	    if ( 0
		|| (lcl->count != rmt_ioc->count)
		|| (rmt_ioc->count > 1) /* XXX */
	    ) {
		fc = -FI_EINVAL; goto bad;
	    }
	}

	/* rmt from rmt_ioc */
	{
	    fc = tofu_atm_ioc_from_rma(rmt_ioc, dtsz, tmpioc);
	    if (fc != 0) { goto bad; }

	    rmt = tmpioc;
	}

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

	if (cep_priv_tx->cep_wop_ctr != 0) {
	    struct tofu_cntr *ctr_priv = cep_priv_tx->cep_wop_ctr;
	    assert(ctr_priv->ctr_fid.fid.fclass == FI_CLASS_CNTR);
	    ofi_atomic_inc64( &ctr_priv->ctr_ctr );
	}

	/*
	 * man fi_cq(3)
	 *   FI_ATOMIC
	 *     Indicates that an atomic operation completed.
	 *     This flag may be combined with an FI_READ, FI_WRITE,
	 *     FI_REMOTE_READ, or FI_REMOTE_WRITE flag.
	 */
	/*
	 * aarg->msg->op == FI_ATOMIC_READ
	 *   cq_e->flags = FI_ATOMIC | FI_READ
	 * otherwise
	 *   cq_e->flags = FI_ATOMIC | FI_WRITE
	 */
    }

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}
#endif	/* CONF_TOFU_RMA */
#ifdef	CONF_TOFU_RMA

static inline int tofu_cep_atm_rmsg_self(
    struct tofu_cep *cep_priv_tx,
    const struct tofu_atm_arg *aarg
)
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
	    if ( 0
		|| ((rmt_ioc = aarg->rmt.vec.rmt) == 0)
		|| ((res = aarg->res.vec.lcl) == 0)
	    ) {
		fc = -FI_EINVAL; goto bad;
	    }
	    if ( 0
		|| (rmt_ioc->count != res->count)
		|| (res->count > 1) /* XXX */
	    ) {
		fc = -FI_EINVAL; goto bad;
	    }
	}
	else {
	    if ( 0
		|| ((lcl = aarg->lcl.vec.lcl) == 0)
		|| ((rmt_ioc = aarg->rmt.vec.rmt) == 0)
		|| ((res = aarg->res.vec.lcl) == 0)
	    ) {
		fc = -FI_EINVAL; goto bad;
	    }
	    if ( 0
		|| (lcl->count != res->count)
		|| (rmt_ioc->count != res->count)
		|| (res->count > 1) /* XXX */
	    ) {
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
	    }
	    else {
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

	if (cep_priv_tx->cep_wop_ctr != 0) {
	    struct tofu_cntr *ctr_priv = cep_priv_tx->cep_wop_ctr;
	    assert(ctr_priv->ctr_fid.fid.fclass == FI_CLASS_CNTR);
	    ofi_atomic_inc64( &ctr_priv->ctr_ctr );
	}

	/*
	 * man fi_cq(3)
	 *   FI_ATOMIC
	 *     Indicates that an atomic operation completed.
	 *     This flag may be combined with an FI_READ, FI_WRITE,
	 *     FI_REMOTE_READ, or FI_REMOTE_WRITE flag.
	 */
	/*
	 * aarg->msg->op == FI_ATOMIC_READ
	 *   cq_e->flags = FI_ATOMIC | FI_READ
	 * otherwise
	 *   cq_e->flags = FI_ATOMIC | FI_WRITE
	 */
    }

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}
#endif	/* CONF_TOFU_RMA */

#endif	/* _TOFU_ATM_H */
