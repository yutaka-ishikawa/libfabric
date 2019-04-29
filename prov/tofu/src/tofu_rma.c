/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include "ulib_conv.h"

#include <assert.h>	    /* for assert() */

struct ulib_rma_cmpl {
    struct tofu_cq              *cq__priv;
    struct fi_cq_tagged_entry   cmpl;
};

/*
 * MUST BE CRITIAL REGION 2019/04/29
 */
void
ulib_notify_rma_cmpl(void *ptr)
{
    struct ulib_rma_cmpl *rma_vmpl = (struct ulib_rma_cmpl *) ptr;
    struct fi_cq_tagged_entry *cmpl;

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "rma_cmpl(%p)\n", rma_cmpl);
    /* get an entry pointed by w.p. */
    cmpl = ofi_cirque_tail(cq__priv->cq__ccq);
    *cmpl = rma_cmpl->cmpl;
    /* advance w.p. by one  */
    ofi_cirque_commit(cirq);
    free(rma_cmpl);
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

/*
 * fi_read:
 */
static ssize_t
tofu_cep_rma_read(struct fid_ep *fid_ep, void *buf, size_t len, void *desc,
                  fi_addr_t src_addr, uint64_t addr, uint64_t key,
                  void *context)
{
    ssize_t     ret = 0;
    int         uc, fc;
    struct tofu_cep     *cep_priv = 0;
    struct tofu_sep     *sep_priv = 0;
    union ulib_tofa_u   tank;
    utofu_vcq_id_t      rmt_vcq_id;
    utofu_stadd_t       lcl_stadd;
    uint64_t            edata = 123UL; /* temporal value */
    int                 retry;
    uint64_t            flags = UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE;
    struct ulib_rma_cmpl *rma_cmpl;
    char        prbuf[128];

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "buf(%p) len(%ld) desc(%p) addr(0x%lx) key(%lx) context(%p) in %s\n", __FILE__, buf, len, desc, addr, key, context);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
        ret = -FI_EINVAL; goto bad;
    }
    fprintf(stderr, "YIRMA: %s:%d remote(%s)\n",
            __func__, __LINE__,  fi_addr2string(prbuf, 128));
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    av__priv = cep_priv->cep_sep->sep_av_;
    fc = tofu_av_lup_tank(av__priv, fi_src_addr, &tank.ui64);
    if (fc != 0) {
        fprintf(stderr, "YIRMA: %s:%d ERROR(%d)\n", __func__, __LINE__, fc);
        ret = -1;  goto bad;
    }
    rmt_vcq_id = tank.ui64;
    uc = utofu_reg_mem(cep_priv->vcqh, buf, len, 0, &lcl_stadd);
    if (uc != UTOFU_SUCCESS) {
        fprintf(stderr, "YIRMA: %s:%d ERROR(%d)\n", __func__, __LINE__, uc);
        ret = -1;  goto bad;
    }
    /* preparing completion entry */
    rma_cmpl = calloc(sizeof(struct ulib_rma_cmpl), 1);
    rma_cmpl.cq__priv = cep_priv->cep_recv_cq;
    rma_cmpl.cmpl->flags = flags;
    rma_cmpl.cmpl->len = len;
    rma_cmpl.cmpl->buf = buf;
    rma_cmpl.cmpl->data = 0;
    rma_cmpl.cmpl->tag = 0;
    fprintf(stderr, "YIRMA: %s:%d rma_cmpl(%p)\n", __func__, __LINE__, rma_cmpl); fflush(stderr);
    retry = 10;
    do {
        /*
         * The edata is used for the ID of the fi_read transaction.
         * When the utogu_get transaction is completed, i.e., the remote
         * date has been received, the local MRQ notice raises.
         * utofu_toqc_prog_ackd() handles this notice.
         * cbdata points to a completion entry.
         */
        uc = utofu_get(cep_priv->vcqh, rmt_vcq_id, lcl_stadd, key,
                       len, edata, flags, rma_cmpl);
    } while (uc == UTOFU_ERR_BUSY && --retry > 0);
    
    if (uc != UTOFU_SUCCCES) {
        fprintf(stderr, "YIRMA: %s:%d ERROR(%d)\n", __func__, __LINE__, uc);
        fflush(stderr);
        ret = -1;  goto bad;
    }

            
bad:
    return ret;
}

static ssize_t tofu_cep_rma_readmsg(
    struct fid_ep *fid_ep,
    const struct fi_msg_rma *msg,
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
	fc = tofu_cep_rma_rmsg_self( cep_priv, msg, flags );
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

static ssize_t tofu_cep_rma_writemsg(
    struct fid_ep *fid_ep,
    const struct fi_msg_rma *msg,
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

