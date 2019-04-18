/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_MSG_H
#define _TOFU_MSG_H

#include "tofu_impl.h"

#ifdef	CONF_TOFU_RECV

/*
 * 2018/04/15
 *  The message request is copied to the send entry (struct tofu_recv_en*)
 *      entry -- send_entry
 *      cep_priv -- context endpoint (has scalabe endpoint)
 *      msg -- tagged message
 *      flag --
 */
static inline int tofu_cep_msg_recv_fill(
    struct tofu_recv_en *entry,
    struct tofu_cep *cep_priv,
    const struct fi_msg_tagged *msg,
    uint64_t flags
)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    dlist_init( &entry->entry );
    if (msg->iov_count > TOFU_IOV_LIMIT) {
	FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "iov_count %ld > 1\n",
	    msg->iov_count);
	fc = -FI_EINVAL; goto bad;
    }
    {
	size_t ii;
	for (ii = 0; ii < msg->iov_count; ii++) {
	    entry->iovs[ii] = msg->msg_iov[ii];
	}
    }
    /* info->domain_attr->mr_mode & FI_MR_LOCAL */
    if (msg->desc != 0) { /* XXX */
	size_t ii;
	for (ii = 0; ii < msg->iov_count; ii++) {
	    entry->dscs[ii] = msg->desc[ii];
	}
    }
    entry->tmsg.msg_iov   = entry->iovs;
    entry->tmsg.desc      = entry->dscs;
    entry->tmsg.iov_count = msg->iov_count;
    entry->tmsg.addr      = msg->addr;
    entry->tmsg.context   = msg->context;
    entry->tmsg.data      = msg->data;

    entry->tmsg.tag       = msg->tag; /* YI 2019/04/11 */
    entry->tmsg.ignore    = msg->ignore; /* YI 2019/04/11 */

    entry->fidp           = &cep_priv->cep_fid.fid;
    entry->flags          = flags;
    
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, " fc(%d)\n", fc);
    return fc;
}
#endif	/* CONF_TOFU_RECV */
#ifdef	NOTDEF_UTIL

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

	wlen = ofi_copy_to_iov(iov_dst, ioc_dst, off, sbuf, slen);
	off += wlen;
    }

    return off - iof_dst;
}
#endif	/* NOTDEF_UTIL */

#if 0 /**************** YI *******/
static inline int tofu_cep_msg_match_recv_en(
    struct dlist_entry *item,
    const void *farg
)
{
    int ret;
    const struct tofu_recv_en *send_en = farg;
    struct tofu_recv_en *recv_en;

    recv_en = container_of(item, struct tofu_recv_en, entry);

    ret =
	((recv_en->tmsg.tag | recv_en->tmsg.ignore)
	    == (send_en->tmsg.tag | recv_en->tmsg.ignore))
	;

    return ret;
}

static inline int
tofu_msg_copy_report(struct tofu_cep *cep_priv_rx,
                     struct tofu_recv_en *recv_entry,
                     struct tofu_recv_en *send_entry)
{
    int                 fc = FI_SUCCESS;
    struct iovec        *iov_dst, *iov_src;
    size_t              rlen, wlen, ioc_dst, ioc_src, iof_dst = 0;
    struct fi_cq_tagged_entry cq_e[1];

    iov_src = (struct iovec *)send_entry->tmsg.msg_iov;
    ioc_src = send_entry->tmsg.iov_count;

    /* requested length */
    rlen = ofi_total_iov_len( iov_src, ioc_src );

    iov_dst = (struct iovec *)recv_entry->tmsg.msg_iov;
    ioc_dst = recv_entry->tmsg.iov_count;

    wlen = tofu_copy_iovs(iov_dst, ioc_dst, iof_dst, iov_src, ioc_src);
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "wlen(%ld) rlen(%ld)\n", wlen, rlen);
    if (wlen < rlen) {
        FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "wlen %ld <= rlen %ld\n",
		wlen, rlen);
        assert(0);
    }
    cq_e->flags	    = FI_RECV;
    cq_e->flags	    |= FI_MULTI_RECV;
    cq_e->op_context    = recv_entry->tmsg.context;
    cq_e->len	    = wlen;
    cq_e->buf  = recv_entry->tmsg.msg_iov[0].iov_base;
    cq_e->data	    = 0;
    cq_e->tag	    = send_entry->tmsg.tag;

    if (cep_priv_rx->cep_recv_cq != 0) {
        fc = tofu_cq_comp_tagged(cep_priv_rx->cep_recv_cq, cq_e);
        /*
         * Ask Hatanaka-san why ...
         */
        if (fc != 0) {
            FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "rx cq %d\n", fc);
            fc = 0; /* XXX ignored */
        }
    }
    return fc;
}

static inline int
tofu_cep_msg_sendmsg_self(struct tofu_cep *cep_priv_tx,
                          const struct fi_msg_tagged *msg,
                          uint64_t flags)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep_priv;
    struct tofu_cep *cep_priv_rx = 0;
    const size_t offs_ulib = sizeof (struct tofu_cep);

    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    assert(cep_priv_tx != 0);
    assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
    assert(cep_priv_tx->cep_sep != 0);
    sep_priv = cep_priv_tx->cep_sep;

    /* send_entry is locally copied here 2019/04/15
     * Can we avoid this copy ?
     */
    fc = tofu_cep_msg_recv_fill(send_entry, cep_priv_tx, msg, flags);
    if (fc != 0) {
        goto bad;
    }

    /* cep_priv_rx: finding the corresponding receiver's context */
    {
	struct dlist_entry *cep_dent;

        /* sep_hrx -- list of receiver's context endpoint */
	dlist_foreach( &sep_priv->sep_hrx, cep_dent ) {
	    struct tofu_cep *cep_priv;

	    cep_priv = container_of(cep_dent, struct tofu_cep, cep_ent_sep);
	    assert(cep_priv->cep_fid.fid.fclass == FI_CLASS_RX_CTX);
	    assert(cep_priv->cep_sep == sep_priv);

            /* finding the same sender's context index 2018/04/15 */
	    if (cep_priv->cep_idx == cep_priv_tx->cep_idx) {
		cep_priv_rx = cep_priv; /* found */
		break;
	    }
	}
	if (cep_priv_rx == 0) {
	    fc = -FI_EOTHER; goto bad;
	}
    }
    /*
     * cep_priv_rx is the reciever's context 2019/04/15
     */
    tofu_impl_ulib_sendmsg_self(cep_priv_rx, offs_ulib, send_entry, flags);
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "fi_errno %d\n", fc);
    return fc;
}
#endif	/* 0 YI******************* */

#endif	/* _TOFU_MSG_H */
