/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_UTL_H
#define _TOFU_UTL_H

#include "tofu_impl.h"


/* utility functions */

static inline ssize_t tofu_total_rma_iov_len(
    const struct fi_rma_iov *rma_iov,
    size_t rma_iov_count
)
{
    size_t ii;
    ssize_t len = 0;
    for (ii = 0; ii < rma_iov_count; ii++) {
	len += rma_iov[ii].len;
    }
    return len;
}

static inline size_t tofu_copy_iovs(
    struct iovec *iov_dst,	    /* dst iov */
    size_t ioc_dst,		    /* dst iov_count */
    size_t iof_dst,		    /* dst iov_offset */
    const struct iovec *iov_src,    /* src iov */
    size_t ioc_src		    /* src iov_count */
)
{
    size_t ii, off = iof_dst;

    for (ii = 0; ii < ioc_src; ii++) {
	void *sbuf = iov_src[ii].iov_base;
	size_t wlen, slen = iov_src[ii].iov_len;

#ifndef	NDEBUG
	if ((slen > 0) && (sbuf == 0)) {
	    return -FI_EINVAL;
	}
#endif
	wlen = ofi_copy_to_iov(iov_dst, ioc_dst, off, sbuf, slen);
	off += wlen;
    }

    return off - iof_dst;
}

static inline int tofu_atm_ioc_from_rma(
    const struct fi_rma_ioc *rma_ioc,
    size_t dtsz,
    struct fi_ioc *iocp
)
{
    int fc = FI_SUCCESS;
    const struct fi_mr_attr *attr;
    const struct iovec *mr_iov;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(rma_ioc != 0);
    /* attr */
    {
	struct tofu_mr *mr__priv;
	mr__priv = (void *)(uintptr_t)rma_ioc->key; /* XXX */
	assert(mr__priv->mr__fid.fid.fclass == FI_CLASS_MR);
	attr = &mr__priv->mr__att;
    }
    /* mr_iov */
    {
	/* check mr attr */
	if ( 0
	    || (attr->iov_count < 1)
	    || (attr->mr_iov == 0)
	    || (attr->mr_iov->iov_len < (rma_ioc->count * dtsz))
	    || (attr->mr_iov->iov_base == 0)
	) {
	    fc = -FI_EINVAL; goto bad;
	}
	mr_iov = attr->mr_iov;
    }

    /* FI_MR_BASIC */
    iocp->count = rma_ioc->count;
    iocp->addr = (void *)(uintptr_t)rma_ioc->addr;

    if ( 0
	|| (iocp->addr < mr_iov->iov_base)
         || (iocp->addr >= (void*) ((char*)mr_iov->iov_base + mr_iov->iov_len))
    ) {
	/* FI_MR_SCALABLE */
	assert(rma_ioc->addr /* offset */ < mr_iov->iov_len);
	iocp->addr = (void*) ((char*)mr_iov->iov_base + rma_ioc->addr); /* offset */
	FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff %"PRIu64" o\n",
	    rma_ioc->addr);
    }
    else {
	FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff 0x%lx V\n",
                 (char*) iocp->addr - (char*) mr_iov->iov_base);
    }

bad:
    return fc;
}

#endif	/* _TOFU_UTL_H */
