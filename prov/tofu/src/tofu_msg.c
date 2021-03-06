/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include "ulib_conv.h"
#include "tofu_impl.h"
#include "tofu_impl_internal.h"

static ssize_t
tofu_cep_msg_recv_common(struct fid_ep *fid_ep,
                         const struct fi_msg_tagged *msg,
                         uint64_t flags)
{
    ssize_t ret = FI_SUCCESS;
    struct tofu_cep *cep_priv = 0;
    struct tofu_recv_en *recv_entry;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_RX_CTX) {
	ret = -FI_EINVAL; goto bad;
    }
    if ((flags & ( FI_PEEK | FI_CLAIM )) != 0) {
	ret = -FI_EBADFLAGS; goto bad;
    }
    else if ((flags & FI_TRIGGER) != 0) {
	ret = -FI_ENOSYS; goto bad;
    }
    /* YYY flags: FI_MULTI_RECV */
    /* YYY flags: FI_COMPLETION with FI_SELECTIVE_COMPLETION */
    /* YYY msg->addr : FI_DIRECTED_RECV */
    if (msg->iov_count > TOFU_IOV_LIMIT) {
	ret = -FI_EINVAL; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    /* if (cep_priv->cep_enb == 0) { fc = -FI_EOPBADSTATE; goto bad; } */
    fastlock_acquire( &cep_priv->cep_lck );
    if (freestack_isempty(cep_priv->recv_fs)) {
        fastlock_release( &cep_priv->cep_lck );
        return -FI_EAGAIN;
    }
    recv_entry = freestack_pop(cep_priv->recv_fs);
    fastlock_release( &cep_priv->cep_lck );
    ret = tofu_cep_msg_recv_fill(recv_entry, cep_priv, msg, flags);
    if (ret != 0) {
	fastlock_acquire( &cep_priv->cep_lck );
	freestack_push(cep_priv->recv_fs, recv_entry);
	fastlock_release( &cep_priv->cep_lck );
	goto bad;
    }
    {
        struct dlist_entry *dep;
	struct dlist_entry *match;
        struct tofu_recv_en *send_entry;

        fastlock_acquire(&cep_priv->cep_lck);
        dep = (flags & FI_TAGGED) ?
            &cep_priv->unexp_tag_hd : &cep_priv->unexp_msg_hd;
        if (!dlist_empty(dep)) {
            match = dlist_find_first_match(dep,
                                           tofu_cep_msg_match_recv_en,
                                           recv_entry);
            if (match) {
                dlist_remove(match);
                send_entry = container_of(match, struct tofu_recv_en, entry);
                FI_DBG(&tofu_prov, FI_LOG_EP_CTRL,
                       "send(%p) receive(%p) in %s\n",
                       send_entry, recv_entry, __func__);
                tofu_msg_copy_report(cep_priv, recv_entry, send_entry);
                freestack_push(cep_priv->recv_fs, send_entry);
                freestack_push(cep_priv->recv_fs, recv_entry);
                fastlock_release(&cep_priv->cep_lck);
                return ret;
            }
        }
        fastlock_release(&cep_priv->cep_lck);
    }
    /* Enqueue posted queue */
    fastlock_acquire( &cep_priv->cep_lck );
    if (flags & FI_TAGGED) {
        dlist_insert_tail(&recv_entry->entry, &cep_priv->recv_tag_hd);
    } else {
        dlist_insert_tail(&recv_entry->entry, &cep_priv->recv_msg_hd);
    }
    fastlock_release( &cep_priv->cep_lck );
    {
        const size_t offs_ulib = sizeof (cep_priv[0]);
	/* YI 2019/04/11
         * uint64_t iflg = flags & ~FI_TAGGED;
         */
	fastlock_acquire( &cep_priv->cep_lck );
	ret = tofu_imp_ulib_recv_post(cep_priv, offs_ulib, msg, flags,
		tofu_cq_comp_tagged, cep_priv->cep_recv_cq);
	fastlock_release( &cep_priv->cep_lck );
	if (ret != 0) { goto bad; }
    }
bad:
    return ret;
}

static ssize_t
tofu_cep_msg_recvmsg(struct fid_ep *fid_ep, const struct fi_msg *msg,
                     uint64_t flags)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    tmsg.msg_iov    = msg->msg_iov;
    tmsg.desc	    = msg->desc;
    tmsg.iov_count  = msg->iov_count;
    tmsg.addr	    = msg->addr;
    tmsg.tag	    = 0;	/* XXX */
    tmsg.ignore	    = -1ULL;	/* XXX */
    tmsg.context    = msg->context;
    tmsg.data	    = msg->data;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    ret = tofu_cep_msg_recv_common(fid_ep, &tmsg, flags);
    if (ret != 0) { goto bad; }

bad:
    return ret;
}

static ssize_t
tofu_cep_msg_recvv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count, fi_addr_t src_addr,
                   void *context)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    tmsg.msg_iov    = iov;
    tmsg.desc	    = desc;
    tmsg.iov_count  = count;
    tmsg.addr	    = src_addr;
    tmsg.tag	    = 0;	/* XXX */
    tmsg.ignore	    = -1ULL;	/* XXX */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    ret = tofu_cep_msg_recv_common(fid_ep, &tmsg, 0 /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    return ret;
}

static ssize_t 
tofu_cep_msg_recv(struct fid_ep *fid_ep, void *buf,  size_t len,
                  void *desc, fi_addr_t src_addr, void *context)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    void *dscs[1];

    iovs->iov_base  = buf;
    iovs->iov_len   = len;

    dscs[0]	    = desc;

    tmsg.msg_iov    = iovs;
    tmsg.desc	    = (desc == 0)? 0: dscs;
    tmsg.iov_count  = 1;
    tmsg.addr	    = src_addr;
    tmsg.tag	    = 0;	/* XXX */
    tmsg.ignore	    = -1ULL;	/* XXX */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    ret = tofu_cep_msg_recv_common(fid_ep, &tmsg, 0 /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    return ret;

}

static ssize_t
tofu_cep_msg_send_common(struct fid_ep *fid_ep,
                         const struct fi_msg_tagged *msg,
                         uint64_t flags)
{
    ssize_t          ret = FI_SUCCESS;
    struct tofu_cep  *cep_priv = 0;
    struct tofu_imp_cep_ulib *icep;
    union ulib_tofa_u   tank;
    struct tofu_av   *av__priv;
    fi_addr_t        fi_a = msg->addr;
    int fc;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
	ret = -FI_EINVAL; goto bad;
    }
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "flags %016"PRIx64"\n", flags);
    if ((flags & FI_TRIGGER) != 0) {
	ret = -FI_ENOSYS; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    /* convert fi_addr to tank */
    av__priv = cep_priv->cep_sep->sep_av_;
    fc = tofu_av_lup_tank(av__priv, fi_a, &tank.ui64);
    if (fc != FI_SUCCESS) { ret = fc; goto bad; }
    tank.tank.pid = 0; tank.tank.vld = 0; tank.tank.cid = 0;
    icep = (struct tofu_imp_cep_ulib*) (cep_priv + 1);
    fprintf(stderr, "YI****** dest tofa(0x%lx) my tofa(0x%lx) in %s\n",
            tank.ui64, icep->tofa.ui64, __func__);
    if (icep->tofa.ui64 == tank.ui64) {
	int fc;
        FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "***SELF SEND\n");
	fc = tofu_cep_msg_sendmsg_self(cep_priv, msg, flags);
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    } else {
        const size_t offs_ulib = sizeof (cep_priv[0]);
	uint64_t iflg = flags & ~FI_TAGGED;
	uint64_t tank = -1ULL;

        FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "***REMOTE SEND\n");
	/* convert fi_addr to tank */
	{
	    struct tofu_av *av__priv;
	    fi_addr_t fi_a = msg->addr;
	    int fc;

	    assert(cep_priv->cep_sep != 0);
	    av__priv = cep_priv->cep_sep->sep_av_;
	    assert(av__priv != 0);

	    fc = tofu_av_lup_tank(av__priv, fi_a, &tank);
	    if (fc != FI_SUCCESS) { ret = fc; goto bad; }
	}

	/* struct fi_tx_attr . iov_limit >= msg->iov_count */
	if (msg->iov_count > 1) {
	    ret = -FI_EINVAL; goto bad;
	}

	/*
	 * msg_iov.iov_base ... lsta (uint64_t : utofu_stadd_t)
	 * msg_iov.iov_len .... tlen (size_t)
	 * desc ...............      (--> lsta)
	 * addr ............... tank (uint64_t : struct ulib_tank)
	 * tag  ............... utag
	 * ignore .............      (--> unused)
	 * context ............ ctxt
	 * data ...............      (--> unused)
	 */
	if (1) {
	    void *ctxt = msg->context;
	    uint64_t utag = msg->tag;
	    size_t tlen = 0;
	    uint64_t lsta;
	    int fc;

	    if (msg->iov_count <= 0) {
		/* tlen = 0; */
		lsta = -1ULL;
	    }
	    else if (msg->msg_iov[0].iov_len == 0) {
		/* tlen = 0; */
		lsta = -1ULL;
	    }
	    else {
		const struct iovec *iov1 = &msg->msg_iov[0];

		if (msg->desc != 0) {
		    struct tofu_mr *desc = msg->desc[0];
		    void *icep = (void *)((uint8_t *)cep_priv + offs_ulib);

		    fc = tofu_imp_ulib_immr_stad(desc, icep, iov1, &lsta);
		    if (fc != FI_SUCCESS) { ret = fc; goto bad; }
		}
		else {
		    void *icep = (void *)((uint8_t *)cep_priv + offs_ulib);

		    fc = tofu_imp_ulib_immr_stad_temp(icep, iov1, &lsta);
		    if (fc != FI_SUCCESS) { ret = fc; goto bad; }
		    iflg |= FI_MULTICAST; /* XXX need to free it */
		}
		tlen = iov1->iov_len;
		assert(lsta != -1ULL);
	    }

	    fc = tofu_imp_ulib_send_post_fast(cep_priv, offs_ulib,
		    lsta, tlen, tank, utag, ctxt, iflg,
		    tofu_cq_comp_tagged, cep_priv->cep_send_cq);
	    if (fc != 0) { goto bad; }
	}

	/* post it */
	fastlock_acquire( &cep_priv->cep_lck );
	ret = tofu_imp_ulib_send_post(cep_priv, offs_ulib, msg, iflg,
		tofu_cq_comp_tagged, cep_priv->cep_send_cq);
	fastlock_release( &cep_priv->cep_lck );
	if (ret != 0) { goto bad; }
    }

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "fi_errno %ld\n", ret);
    return ret;
}

static ssize_t tofu_cep_msg_sendmsg(
    struct fid_ep *fid_ep,
    const struct fi_msg *msg,
    uint64_t flags
)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    tmsg.msg_iov    = msg->msg_iov;
    tmsg.desc	    = msg->desc;
    tmsg.iov_count  = msg->iov_count;
    tmsg.addr	    = msg->addr;
    tmsg.tag	    = 0;	/* XXX */
    tmsg.ignore	    = -1ULL;	/* XXX */
    tmsg.context    = msg->context;
    tmsg.data	    = msg->data;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    ret = tofu_cep_msg_send_common(fid_ep, &tmsg, flags);
    if (ret != 0) { goto bad; }

bad:
    return ret;
}

static ssize_t tofu_cep_msg_sendv(
    struct fid_ep *fid_ep,
    const struct iovec *iov,
    void **desc,
    size_t count,
    fi_addr_t dest_addr,
    void *context
)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    tmsg.msg_iov    = iov;
    tmsg.desc	    = desc;
    tmsg.iov_count  = count;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = 0;	/* XXX */
    tmsg.ignore	    = -1ULL;	/* XXX */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    ret = tofu_cep_msg_send_common(fid_ep, &tmsg, 0 /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    return ret;
}

static ssize_t tofu_cep_msg_send(
    struct fid_ep *fid_ep,
    const void *buf,
    size_t len,
    void *desc,
    fi_addr_t dest_addr,
    void *context
)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    void *dscs[1];

    iovs->iov_base  = (void *)buf;
    iovs->iov_len   = len;

    dscs[0]	    = desc;

    tmsg.msg_iov    = iovs;
    tmsg.desc	    = (desc == 0)? 0: dscs;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = 0;	/* XXX */
    tmsg.ignore	    = -1ULL;	/* XXX */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    ret = tofu_cep_msg_send_common(fid_ep, &tmsg, 0 /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    return ret;
}

static ssize_t tofu_cep_msg_inject(
    struct fid_ep *fid_ep,
    const void *buf,
    size_t len,
    fi_addr_t dest_addr
)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    return ret;
}

/* ==================================================================== */
/* === for struct fi_ops_tagged ======================================= */
/* ==================================================================== */

static ssize_t
tofu_cep_tag_recv(struct fid_ep *fid_ep,
                  void *buf, size_t len, void *desc, fi_addr_t src_addr,
                  uint64_t tag, uint64_t ignore,  void *context)
{
    struct fi_msg_tagged tmsg;
    struct iovec         iov;
    uint64_t             flags;
    ssize_t              ret;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    iov.iov_base = buf; iov.iov_len = len;
    tmsg.msg_iov    = &iov;
    tmsg.desc	    = desc;
    tmsg.iov_count  = 1;
    tmsg.addr	    = src_addr;
    tmsg.tag	    = tag;
    tmsg.ignore	    = ignore;
    tmsg.context    = context;
    tmsg.data	    = 0;
    flags = FI_TAGGED;
    ret = tofu_cep_msg_recv_common(fid_ep, &tmsg, flags);

    return ret;
}

static ssize_t
tofu_cep_tag_recvv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count,
                   fi_addr_t src_addr,
                   uint64_t tag, uint64_t ignore, void *context)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    return ret;
}

static ssize_t
tofu_cep_tag_recvmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_tagged *msg, uint64_t flags)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    return ret;
}

static ssize_t
tofu_cep_tag_send(struct fid_ep *fid_ep,
                  const void *buf, size_t len,
                  void *desc, fi_addr_t dest_addr,
                  uint64_t tag, void *context)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    return ret;
}

static ssize_t
tofu_cep_tag_sendv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count, fi_addr_t dest_addr,
                   uint64_t tag, void *context)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    return ret;
}

static ssize_t
tofu_cep_tag_sendmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_tagged *msg, uint64_t flags)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    return ret;
}

static ssize_t
tofu_cep_tag_inject(struct fid_ep *fid_ep,
                    const void *buf, size_t len,
                    fi_addr_t dest_addr, uint64_t tag)
{
    ssize_t ret = -FI_ENOSYS;
    fprintf(stderr, "YI***** %s needs to implement\n", __func__);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    return ret;
}

static ssize_t
tofu_cep_tag_senddata(struct fid_ep *fid_ep,
                      const void *buf,  size_t len,
                      void *desc,  uint64_t data,
                      fi_addr_t dest_addr, uint64_t tag, void *context)
{
    ssize_t              ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;
    struct iovec         iovs[1];
    void                 *dscs[1];
    int                  flags;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    iovs->iov_base  = (void *)buf;
    iovs->iov_len   = len;
    dscs[0]	    = desc;
    tmsg.msg_iov    = iovs;
    tmsg.desc	    = (desc == 0) ? 0 : dscs;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = tag;
    tmsg.ignore	    = -1ULL;	/* ???? */
    tmsg.context    = context;
    tmsg.data	    = 0;
    flags = FI_TAGGED;

    ret = tofu_cep_msg_send_common(fid_ep, &tmsg, flags);
    return ret;
}

static ssize_t 
tofu_cep_tag_injectdata(struct fid_ep *fid_ep,
                        const void *buf, size_t len, uint64_t data,
                        fi_addr_t dest_addr, uint64_t tag)
{
    ssize_t ret = -FI_ENOSYS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    uint64_t flags = 0
		    | FI_INJECT | FI_TAGGED | FI_REMOTE_CQ_DATA
		    /* FI_REMOTE_CQ_DATA */ /* XXX */
		    /* | TOFU_NO_COMPLETION */ /* YYY */
		    /* | TOFU_USE_OP_FLAG */ /* YYY */
		    ;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    iovs->iov_base  = (void *)buf;
    iovs->iov_len   = len;
    tmsg.msg_iov    = iovs;
    tmsg.desc	    = 0;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = tag;
    tmsg.ignore	    = -1ULL;	/* XXX */
    tmsg.context    = 0;	/* XXX */
    tmsg.data	    = data;
    ret = tofu_cep_msg_send_common(fid_ep, &tmsg, flags);

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "fi_errno %ld in %s\n", ret, __FILE__);
    return ret;
}

/*
 * tofu_cep_ops_msg is reffered to in tofu_cep.c
 */
struct fi_ops_msg tofu_cep_ops_msg = {
    .size	    = sizeof (struct fi_ops_msg),
    .recv	    = tofu_cep_msg_recv,
    .recvv	    = tofu_cep_msg_recvv,
    .recvmsg	    = tofu_cep_msg_recvmsg,
    .send	    = tofu_cep_msg_send,
    .sendv	    = tofu_cep_msg_sendv,
    .sendmsg	    = tofu_cep_msg_sendmsg,
    .inject	    = tofu_cep_msg_inject,
    .senddata	    = fi_no_msg_senddata,
    .injectdata	    = fi_no_msg_injectdata,
};

/*
 * tofu_cep_ops_tag is reffered to in tofu_cep.c
 */
struct fi_ops_tagged tofu_cep_ops_tag = {
    .size	    = sizeof (struct fi_ops_tagged),
    .recv	    = tofu_cep_tag_recv,
    .recvv	    = tofu_cep_tag_recvv,
    .recvmsg	    = tofu_cep_tag_recvmsg,
    .send	    = tofu_cep_tag_send,
    .sendv	    = tofu_cep_tag_sendv,
    .sendmsg	    = tofu_cep_tag_sendmsg,
    .inject	    = tofu_cep_tag_inject,
    .senddata	    = tofu_cep_tag_senddata,
    .injectdata	    = tofu_cep_tag_injectdata,
};
