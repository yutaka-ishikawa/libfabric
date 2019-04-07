/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_RMA_H
#define _TOFU_RMA_H

#include "tofu_impl.h"

#ifdef	CONF_TOFU_RMA

static inline int tofu_cep_rma_rmsg_self(
    struct tofu_cep *cep_priv_tx,
    const struct fi_msg_rma *msg_rma,
    uint64_t flags
)
{
    int fc = FI_SUCCESS;
    /* struct tofu_sep *sep_priv; */
    size_t rlen, wlen = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(cep_priv_tx != 0);
    assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
    /* assert(cep_priv_tx->cep_sep != 0); */
    /* sep_priv = cep_priv_tx->cep_sep; */

    if ( 0
	|| (msg_rma->iov_count != msg_rma->rma_iov_count)
	|| (msg_rma->iov_count > 1)
    ) {
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
                 || ((char*)iovs->iov_base >= (char*) ((char*)mr_iov->iov_base + mr_iov->iov_len))
	    ) {
		/* FI_MR_SCALABLE */
		assert(rma_iov->addr /* offset */ < mr_iov->iov_len);
		iovs->iov_base = (void*) ((char*)mr_iov->iov_base + rma_iov->addr);  /* offset */
		FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "woff %"PRIu64" o\n",
		    rma_iov->addr);
	    }
	    else {
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

    if (cep_priv_tx->cep_rop_ctr != 0) {
	struct tofu_cntr *ctr_priv = cep_priv_tx->cep_rop_ctr;
	assert(ctr_priv->ctr_fid.fid.fclass == FI_CLASS_CNTR);
	ofi_atomic_inc64( &ctr_priv->ctr_ctr );
    }
#ifdef	NOTYET

    /* report tx */
    {
	struct fi_cq_tagged_entry cq_e[1];

	cq_e->flags	    = FI_RMA | FI_WRITE | FI_DELIVERY_COMPLETE;
	cq_e->op_context    = msg_rma->context;
	cq_e->len	    = wlen;
	cq_e->buf	    = 0 /* msg_rma->msg_iov->iov_base */;
	cq_e->data	    = 0;
	cq_e->tag	    = 0 /* send_entry->tmsg.tag */;

	if (cep_priv_tx->cep_read_cq != 0) {
	    fc = tofu_cq_comp_tagged( cep_priv_tx->cep_read_cq, cq_e );
	    if (fc != 0) {
		/* may be EAGAIN */
		FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "tx cq %d\n", fc);
	    }
	}
    }
#endif	/* NOTYET */

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}
#endif	/* CONF_TOFU_RMA */
#ifdef	CONF_TOFU_RMA

static inline int tofu_cep_rma_wmsg_self(
    struct tofu_cep *cep_priv_tx,
    const struct fi_msg_rma *msg_rma,
    uint64_t flags
)
{
    int fc = FI_SUCCESS;
    /* struct tofu_sep *sep_priv; */ /* AV */
    size_t rlen, wlen = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(cep_priv_tx != 0);
    assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
    /* assert(cep_priv_tx->cep_sep != 0); */
    /* sep_priv = cep_priv_tx->cep_sep; */

    if ( 0
	|| (msg_rma->iov_count != msg_rma->rma_iov_count)
	|| (msg_rma->iov_count > 1)
    ) {
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

    if (cep_priv_tx->cep_wop_ctr != 0) {
	struct tofu_cntr *ctr_priv = cep_priv_tx->cep_wop_ctr;
	assert(ctr_priv->ctr_fid.fid.fclass == FI_CLASS_CNTR);
	ofi_atomic_inc64( &ctr_priv->ctr_ctr );
    }
#ifdef	NOTYET

    /* report tx */
    {
	struct fi_cq_tagged_entry cq_e[1];

	cq_e->flags	    = FI_RMA | FI_WRITE | FI_DELIVERY_COMPLETE;
	cq_e->op_context    = msg_rma->context;
	cq_e->len	    = wlen;
	cq_e->buf	    = 0 /* msg_rma->msg_iov->iov_base */;
	cq_e->data	    = 0;
	cq_e->tag	    = 0 /* send_entry->tmsg.tag */;

	if (cep_priv_tx->cep_write_cq != 0) {
	    fc = tofu_cq_comp_tagged( cep_priv_tx->cep_write_cq, cq_e );
	    if (fc != 0) {
		/* may be EAGAIN */
		FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "tx cq %d\n", fc);
	    }
	}
    }
#endif	/* NOTYET */

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}
#endif	/* CONF_TOFU_RMA */

#endif	/* _TOFU_RMA_H */

