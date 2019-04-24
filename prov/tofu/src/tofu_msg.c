/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include "ulib_shea.h"
#include "ulib_conv.h"
#include "tofu_impl.h"
#include "ulib_ofif.h"

static ssize_t
tofu_cep_msg_recv_common(struct fid_ep *fid_ep,
                         const struct fi_msg_tagged *msg,
                         uint64_t flags)
{
    ssize_t             ret = FI_SUCCESS;
    struct tofu_cep     *cep_priv = 0;
    struct ulib_icep    *icep;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "\n\tsrc(%ld) in %s\n", msg->addr, __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_RX_CTX) {
	ret = -FI_EINVAL; goto bad;
    }
    if ((flags & ( FI_PEEK | FI_CLAIM )) != 0) {
	ret = -FI_EBADFLAGS; goto bad;
    } else if ((flags & FI_TRIGGER) != 0) {
	ret = -FI_ENOSYS; goto bad;
    }
    if (msg->iov_count > TOFU_IOV_LIMIT) {
	ret = -FI_EINVAL; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    icep = (struct ulib_icep*)(cep_priv + 1);

    fastlock_acquire(&cep_priv->cep_lck);
    //fprintf(stderr, "YI***** Completion function must be considered ? in %s\n", __func__);
    ret = ulib_icep_shea_recv_post(icep, msg, flags);
    fastlock_release( &cep_priv->cep_lck );
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
    struct ulib_icep *icep;
    union ulib_tofa_u   tank;
    struct tofu_av   *av__priv;
    fi_addr_t        fi_a = msg->addr;
    int fc;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "\n\tdest(%ld) in %s\n", msg->addr, __FILE__);

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
    icep = (struct ulib_icep*) (cep_priv + 1);
    {
        char buf1[128], buf2[128];
        fprintf(stderr, "YI****** dest(%s) my tofa(%s) in %s\n",
                tank2string(buf1, 128, tank.ui64),
                tank2string(buf2, 128, icep->tofa.ui64), __func__);
    }
    if (icep->tofa.ui64 == tank.ui64) {
	int fc;
        FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "***SELF SEND\n");
        fc = tofu_impl_ulib_sendmsg_self(cep_priv, sizeof(struct tofu_cep),
                                         msg, flags);
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    } else {
        const size_t    offs_ulib = sizeof (cep_priv[0]);
	uint64_t        iflg = flags & ~FI_TAGGED;
	/* post it */
	fastlock_acquire(&cep_priv->cep_lck);
	ret = tofu_imp_ulib_send_post(cep_priv, offs_ulib, msg, iflg,
		tofu_cq_comp_tagged, cep_priv->cep_send_cq);
	fastlock_release(&cep_priv->cep_lck);
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
    int                  flags = FI_TAGGED | FI_REMOTE_CQ_DATA;

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
