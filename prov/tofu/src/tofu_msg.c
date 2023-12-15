#include "tofu_impl.h"
#include "tofu_addr.h"
#include <rdma/fabric.h>

extern int	tfi_utf_sendmsg_self(struct tofu_ctx *ctx,
				      const struct fi_msg_tagged *msg,
				      uint64_t flags);
extern int	tfi_utf_send_post(struct tofu_ctx *ctx,
				   const struct fi_msg_tagged *msg,
				   uint64_t flags);
extern int	tfi_utf_recv_post(struct tofu_ctx *ctx,
				   const struct fi_msg_tagged *msg,
				   uint64_t flags);

static inline char *
fi_addr2string(char *buf, ssize_t sz, fi_addr_t fi_addr, struct fid_ep *fid_ep)
{
    struct tofu_ctx *ctx_priv;
    struct tofu_av *av_priv;
    utofu_vcq_id_t vcqi = 0;
    uint8_t xyz[8];
    uint16_t tni[1], tcq[1], cid[1];
    int uc;

    if (fi_addr == -1) {
	snprintf(buf, sz, "fi_addr(-1)");
    } else {
	ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
	av_priv = ctx_priv->ctx_sep->sep_av_;
	tofu_av_lookup_vcqid_by_fia(av_priv, fi_addr, &vcqi, 0);
	uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid);
	if (uc == UTOFU_SUCCESS) {
	    snprintf(buf, sz, "xyzabc(%02x:%02x:%02x:%02x:%02x:%02x), "
		     "tni(%d), tcq(%d), cid(0x%x)",
		     xyz[0], xyz[1], xyz[2], xyz[3], xyz[4], xyz[5],
		     tni[0], tcq[0], cid[0]);
	} else {
	    snprintf(buf, sz, "fi_addr_unspec");
	}
    }
    return buf;
}


static inline ssize_t
tofu_ctx_msg_recv_common(struct fid_ep *fid_ep,
                         const struct fi_msg_tagged *msg,
                         uint64_t flags)
{
    ssize_t             ret = FI_SUCCESS;
    struct tofu_ctx     *ctx;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL,
            "\tsrc(%ld) iovcount(%ld) buf(%p) size(%ld) flags(0x%lx) cntxt(%p) in %s\n",
            msg->addr, msg->iov_count, 
            msg->msg_iov ? msg->msg_iov[0].iov_base : 0,
            msg->msg_iov ? msg->msg_iov[0].iov_len : 0, flags,
            msg->context, __FILE__);
#if 0
    utf_printf("%s: src(%ld) iovcount(%ld) buf(%p) size(%ld) flags(0x%lx) cntxt(%p)\n",
	       __func__, msg->addr, msg->iov_count, 
	       msg->msg_iov ? msg->msg_iov[0].iov_base : 0,
	       msg->msg_iov ? msg->msg_iov[0].iov_len : 0, flags,
	       msg->context);
#endif
    if (msg->iov_count > TOFU_IOV_LIMIT) {
	ret = -FI_E2BIG; goto bad;
    }
    if (fid_ep->fid.fclass != FI_CLASS_RX_CTX) {
	ret = -FI_EINVAL; goto bad;
    }
    if ((flags & FI_TRIGGER) != 0) {
	ret = -FI_ENOSYS; goto bad;
    }
    if (msg->iov_count > TOFU_IOV_LIMIT) {
	ret = -FI_EINVAL; goto bad;
    }
    ctx = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    fastlock_acquire(&ctx->ctx_lck);
    ret = tfi_utf_recv_post(ctx, msg, flags);
    fastlock_release(&ctx->ctx_lck);
bad:
    return ret;
}


/*
 * fi_recv
 */
ssize_t 
tofu_ctx_msg_recv(struct fid_ep *fid_ep, void *buf,  size_t len,
                  void *desc, fi_addr_t src_addr, void *context)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    void *dscs[1];

    utf_tmr_begin(TMR_FI_RECV);
    iovs->iov_base  = buf;
    iovs->iov_len   = len;

    dscs[0]	    = desc;

    tmsg.msg_iov    = iovs;
    tmsg.desc	    = (desc == 0)? 0: dscs;
    tmsg.iov_count  = 1;
    tmsg.addr	    = src_addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_recvmsg src(%s) len(%ld) buf(%p) flags(0)",
          fi_addr2string(buf1, 128, src_addr, fid_ep),
           len, buf);

    ret = tofu_ctx_msg_recv_common(fid_ep, &tmsg, 0 /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    utf_tmr_end(TMR_FI_RECV);
    return ret;

}

/*
 * fi_recvv
 */
ssize_t
tofu_ctx_msg_recvv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count, fi_addr_t src_addr,
                   void *context)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    utf_tmr_begin(TMR_FI_RECV);
    tmsg.msg_iov    = iov;
    tmsg.desc	    = desc;
    tmsg.iov_count  = count;
    tmsg.addr	    = src_addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_recvv src(%s) len(%ld) buf(%p) flags(0)",
          fi_addr2string(buf1, 128, src_addr, fid_ep),
           iov[0].iov_len, iov[0].iov_base);

    ret = tofu_ctx_msg_recv_common(fid_ep, &tmsg, 0 /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    utf_tmr_end(TMR_FI_RECV);
    return ret;
}

/*
 * fi_recvmsg
 */
ssize_t
tofu_ctx_msg_recvmsg(struct fid_ep *fid_ep, const struct fi_msg *msg,
                     uint64_t flags)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    utf_tmr_begin(TMR_FI_RECV);
    tmsg.msg_iov    = msg->msg_iov;
    tmsg.desc	    = msg->desc;
    tmsg.iov_count  = msg->iov_count;
    tmsg.addr	    = msg->addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = msg->context;
    tmsg.data	    = msg->data;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_recvmsg src(%s) len(%ld) buf(%p) flags(%s)", 
           fi_addr2string(buf1, 128, msg->addr, fid_ep),
           msg->msg_iov[0].iov_len, msg->msg_iov[0].iov_base, tofu_fi_flags_string(flags));

    ret = tofu_ctx_msg_recv_common(fid_ep, &tmsg, flags);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s return %ld\n", __FILE__, ret);
    if (ret != 0) { goto bad; }

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s return %ld\n", __FILE__, ret);
    utf_tmr_end(TMR_FI_RECV);
    return ret;
}


ssize_t
tofu_ctx_msg_send_common(struct fid_ep *fid_ep,
                         const struct fi_msg_tagged *msg,
                         uint64_t flags)
{
    ssize_t          ret = FI_SUCCESS;
    struct tofu_ctx  *ctx = 0;
    ctx = container_of(fid_ep, struct tofu_ctx, ctx_fid);

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "\tdest(%ld) iovcount(%ld) size(%ld) in %s\n", msg->addr, msg->iov_count, msg->msg_iov[0].iov_len, __FILE__);

    //R_DBG("YI********** SEND class(%ld)", fid_ep->fid.fclass);
    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
	ret = -FI_EINVAL; goto bad;
    }
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "flags %016"PRIx64"\n", flags);
    if ((flags & FI_TRIGGER) != 0) {
	ret = -FI_ENOSYS; goto bad;
    }
    if (ctx->ctx_sep->sep_myrank == msg->addr) {
        ret = tfi_utf_sendmsg_self(ctx, msg, flags);
	if (ret != 0) { goto bad; }
    } else {
	/* post it */
	fastlock_acquire(&ctx->ctx_lck);
	ret = tfi_utf_send_post(ctx, msg, flags);
	fastlock_release(&ctx->ctx_lck);
	if (ret != 0) { goto bad; }
    }
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "fi_errno %ld\n", ret);
    return ret;
}

/*
 * fi_sendmsg
 */
ssize_t
tofu_ctx_msg_sendmsg(struct fid_ep *fid_ep,const struct fi_msg *msg,
		     uint64_t flags)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    utf_tmr_begin(TMR_FI_SEND);
    tmsg.msg_iov    = msg->msg_iov;
    tmsg.desc	    = msg->desc;
    tmsg.iov_count  = msg->iov_count;
    tmsg.addr	    = msg->addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = msg->context;
    tmsg.data	    = msg->data;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_senddata dest(%s) len(%ld) buf(%p) data(%ld) flags(%s)",
          fi_addr2string(buf1, 128, msg->addr, fid_ep),
          msg->msg_iov[0].iov_len, msg->msg_iov[0].iov_base, msg->data, tofu_fi_flags_string(flags));

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);
    if (ret != 0) { goto bad; }
bad:
    utf_tmr_end(TMR_FI_SEND);
    return ret;
}

/*
 * fi_sendv
 */
ssize_t
tofu_ctx_msg_sendv(struct fid_ep *fid_ep, const struct iovec *iov,
		   void **desc, size_t count, fi_addr_t dest_addr, void *context)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    utf_tmr_begin(TMR_FI_SEND);
    tmsg.msg_iov    = iov;
    tmsg.desc	    = desc;
    tmsg.iov_count  = count;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "count(%ld) iov[0].iov_len=%ld iov[0].iov_base=%p %s\n", count, iov[0].iov_len, iov[0].iov_base, __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_sendv dest(%s) len(%ld) buf(%p) FI_COMPLETION",
          fi_addr2string(buf1, 128, dest_addr, fid_ep),
          iov[0].iov_len, iov[0].iov_base);

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, FI_COMPLETION /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    utf_tmr_end(TMR_FI_SEND);
    return ret;
}

/*
 * fi_send
 */
ssize_t
tofu_ctx_msg_send(struct fid_ep *fid_ep, const void *buf, size_t len,
		  void *desc, fi_addr_t dest_addr, void *context)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    void *dscs[1];

    utf_tmr_begin(TMR_FI_SEND);
    iovs->iov_base  = (void *)buf;
    iovs->iov_len   = len;

    dscs[0]	    = desc;

    tmsg.msg_iov    = iovs;
    tmsg.desc	    = (desc == 0)? 0: dscs;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_send dest(%s) len(%ld) buf(%p) FI_COMPLETION",
          fi_addr2string(buf1, 128, dest_addr, fid_ep), len, buf);

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, FI_COMPLETION /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    utf_tmr_end(TMR_FI_SEND);
    return ret;
}

/*
 * fi_inject
 */
ssize_t
tofu_ctx_msg_inject(struct fid_ep *fid_ep,  const void *buf, size_t len,
		    fi_addr_t dest_addr)
{
    ssize_t ret = -FI_ENOSYS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    uint64_t flags = FI_INJECT;

    utf_tmr_begin(TMR_FI_SEND);
    if (len > CONF_TOFU_INJECTSIZE) {
	R_DBG("%s: Message size(%ld) is larger than the INJECT SIZE (%d)\n",
	      __func__, len, CONF_TOFU_INJECTSIZE);
	utf_tmr_end(TMR_FI_SEND);
	return -FI_ENOMEM;
    }
    iovs->iov_base  = (void*) buf;
    iovs->iov_len   = len;
    tmsg.msg_iov    = iovs;
    tmsg.desc	    = 0;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = 0;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "%s in %s  len(%ld)\n", __func__, __FILE__, len);
    R_DBG1(RDBG_LEVEL3, "fi_inject dest(%s) len(%ld) buf(%p) FI_INJECT",
          fi_addr2string(buf1, 128, dest_addr, fid_ep),
	   len, buf);

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);

    utf_tmr_end(TMR_FI_SEND);
    return ret;
}

/*
 * tofu_ctx_ops_msg is reffered to in tofu_ctx.c
 */
struct fi_ops_msg tofu_ctx_ops_msg = {
    .size	    = sizeof (struct fi_ops_msg),
    .recv	    = tofu_ctx_msg_recv,
    .recvv	    = tofu_ctx_msg_recvv,
    .recvmsg	    = tofu_ctx_msg_recvmsg,
    .send	    = tofu_ctx_msg_send,
    .sendv	    = tofu_ctx_msg_sendv,
    .sendmsg	    = tofu_ctx_msg_sendmsg,
    .inject	    = tofu_ctx_msg_inject,
    .senddata	    = fi_no_msg_senddata,
    .injectdata	    = fi_no_msg_injectdata,
};

/*
 * fi_trecv
 */
ssize_t
tofu_ctx_tag_recv(struct fid_ep *fid_ep,
                  void *buf, size_t len, void *desc, fi_addr_t src_addr,
                  uint64_t tag, uint64_t ignore,  void *context)
{
    struct fi_msg_tagged tmsg;
    struct iovec         iov;
    uint64_t             flags;
    ssize_t              ret;

    utf_tmr_begin(TMR_FI_TRECV);
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

    R_DBG1(RDBG_LEVEL3, "fi_trecv src(%ld: %s) len(%ld) buf(%p) tag(0x%lx) ignore(0x%lx) flags(%s)",
           src_addr, fi_addr2string(buf1, 128, src_addr, fid_ep),
           len, buf, tag, ignore, tofu_fi_flags_string(flags));
    ret = tofu_ctx_msg_recv_common(fid_ep, &tmsg, flags);

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s return %ld\n", __FILE__, ret);
    utf_tmr_end(TMR_FI_TRECV);
    return ret;
}

/*
 * fi_trecvv
 */
ssize_t
tofu_ctx_tag_recvv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count,
                   fi_addr_t src_addr,
                   uint64_t tag, uint64_t ignore, void *context)
{
    ssize_t ret = -FI_ENOSYS;

    utf_tmr_begin(TMR_FI_TRECV);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    utf_printf("%s/%s TOFU: ######## NEEDS to implement\n", __func__, __FILE__);
    R_DBG0(RDBG_LEVEL3, "NEEDS TO IMPLEMENT: fi_recvv len(%ld) buf(%p) tag(0x%lx) ignore(0x%lx) flags(0)",
          iov[0].iov_len,iov[0].iov_base, tag, ignore);

    utf_tmr_end(TMR_FI_TRECV);
    return ret;
}

ssize_t
tofu_ctx_tag_recvmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_tagged *msg, uint64_t flags)
{
    ssize_t ret = -FI_ENOSYS;

    utf_tmr_begin(TMR_FI_TRECV);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    flags |= FI_TAGGED;
    R_DBG1(RDBG_LEVEL3, "fi_trecvmsg src(%s) len(%ld) buf(%p) flags(%s)", 
           fi_addr2string(buf1, 128, msg->addr, fid_ep),
           msg->msg_iov ? msg->msg_iov[0].iov_len : 0,
           msg->msg_iov ? msg->msg_iov[0].iov_base : 0, tofu_fi_flags_string(flags));

    ret = tofu_ctx_msg_recv_common(fid_ep, msg, flags);

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s return %ld\n", __FILE__, ret);
    utf_tmr_end(TMR_FI_TRECV);
    return ret;
}

/*
 * fi_tsend
 */
ssize_t
tofu_ctx_tag_send(struct fid_ep *fid_ep, const void *buf, size_t len,
                  void *desc, fi_addr_t dest_addr,  uint64_t tag, void *context)
{
    ssize_t ret = -FI_ENOSYS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    uint64_t flags = FI_TAGGED | FI_COMPLETION | FI_SEND;

    utf_tmr_begin(TMR_FI_TSEND);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    utf_printf("YI****** MUST CHECK THIS FUNCTION buf(%p) len(%ld)\n", buf, len);

    iovs->iov_base  = (void*) buf;
    iovs->iov_len   = len;
    tmsg.msg_iov    = iovs;
    tmsg.desc	    = desc;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = tag;	/* tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = context;
    tmsg.data	    = 0;

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);
    utf_tmr_end(TMR_FI_TSEND);
    return ret;
}

/*
 * fi_tsendv
 */
ssize_t
tofu_ctx_tag_sendv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count, fi_addr_t dest_addr,
                   uint64_t tag, void *context)
{
    ssize_t ret = -FI_ENOSYS;

    utf_tmr_begin(TMR_FI_TSEND);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    utf_printf("%s/%s TOFU: ######## NEEDS to implement\n", __func__, __FILE__);
    R_DBG1(RDBG_LEVEL3, "NEEDS to implement: fi_tsendv dest(%s) len(%ld) FI_COMPLETION",
          fi_addr2string(buf1, 128, dest_addr, fid_ep), iov[0].iov_len);

    utf_tmr_end(TMR_FI_TSEND);
    return ret;
}

/*
 * fi_tsendmsg
 */
ssize_t
tofu_ctx_tag_sendmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_tagged *msg, uint64_t flags)
{
    ssize_t ret;
    uint64_t	myflgs;

    utf_tmr_begin(TMR_FI_TSEND);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    myflgs = flags | FI_TAGGED;
    ret = tofu_ctx_msg_send_common(fid_ep, msg, myflgs);

    utf_tmr_end(TMR_FI_TSEND);
    return ret;
}

/*
 * fi_tinject
 */
ssize_t
tofu_ctx_tag_inject(struct fid_ep *fid_ep,
                    const void *buf, size_t len,
                    fi_addr_t dest_addr, uint64_t tag)
{
    ssize_t ret = -FI_ENOSYS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    uint64_t flags = FI_INJECT;

    utf_tmr_begin(TMR_FI_TSEND);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "%s in %s  len(%ld)\n", __func__, __FILE__, len);
    R_DBG1(RDBG_LEVEL3, "fi_inject dest(%s) len(%ld) buf(%p) FI_INJECT",
          fi_addr2string(buf1, 128, dest_addr, fid_ep),
	   len, buf);

    if (len > CONF_TOFU_INJECTSIZE) {
	R_DBG("%s: Message size(%ld) is larger than the INJECT SIZE (%d)\n",
	      __func__, len, CONF_TOFU_INJECTSIZE);
	utf_tmr_end(TMR_FI_TSEND);
	return -FI_ENOMEM;
    }
    iovs->iov_base  = (void*) buf;
    iovs->iov_len   = len;
    tmsg.msg_iov    = iovs;
    tmsg.desc	    = 0;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = tag;	/* tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = 0;
    tmsg.data	    = 0;

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);

    utf_tmr_end(TMR_FI_TSEND);
    return ret;
}

/*
 * fi_tsenddata
 */
ssize_t
tofu_ctx_tag_senddata(struct fid_ep *fid_ep,
                      const void *buf,  size_t len,
                      void *desc,  uint64_t data,
                      fi_addr_t dest_addr, uint64_t tag, void *context)
{
    ssize_t              ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;
    struct iovec         iovs[1];
    void                 *dscs[1];
    uint64_t             flags = FI_TAGGED | FI_REMOTE_CQ_DATA | FI_COMPLETION;

    utf_tmr_begin(TMR_FI_TSEND);
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s data(%ld) tag(%lx) len(%ld)\n", __FILE__, data, tag, len);

    iovs->iov_base  = (void *)buf;
    iovs->iov_len   = len;
    dscs[0]	    = desc;
    tmsg.msg_iov    = iovs;
    tmsg.desc	    = (desc == 0) ? 0 : dscs;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = tag;
    tmsg.ignore	    = 0;	/* Compare all bits */
    tmsg.context    = context;
    tmsg.data	    = data;

    if (buf) {
        R_DBG1(RDBG_LEVEL3, "fi_tsenddata dest(%ld: %s) len(%ld) desc(%p) buf(%p) data(%ld) "
               "buf(%d) tag(%lx) flags(%s)",
	       dest_addr, fi_addr2string(buf1, 128, dest_addr, fid_ep),
               len, desc, buf, data, *(int*)buf, tag, tofu_fi_flags_string(flags));
    } else {
        R_DBG1(RDBG_LEVEL3, "fi_tsenddata dest(%ld: %s) len(%ld) desc(%p) buf(%p) data(%ld) "
               "buf=nil tag(%lx) flags(%s)",
	       dest_addr, fi_addr2string(buf1, 128, dest_addr, fid_ep),
               len, desc, buf, data, tag, tofu_fi_flags_string(flags));
    }

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);

    utf_tmr_end(TMR_FI_TSEND);
    return ret;
}

/*
 * fi_tinjectdata
 *      remote CQ data is included
 */
ssize_t 
tofu_ctx_tag_injectdata(struct fid_ep *fid_ep,
                        const void *buf, size_t len, uint64_t data,
                        fi_addr_t dest_addr, uint64_t tag)
{
    ssize_t ret = -FI_ENOSYS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];

    utf_tmr_begin(TMR_FI_TSEND);
    uint64_t flags = FI_INJECT | FI_TAGGED | FI_REMOTE_CQ_DATA
		    /* | TOFU_USE_OP_FLAG */ /* YYY */
		    ;
    //R_DBG("fi_tinjectdata dest(%ld) len(%ld) data(%ld) flags(%s)",
    //   dest_addr, len, data, tofu_fi_flags_string(flags));
    
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_tinjectdata dest(%ld: %s) len(%ld) data(%ld) buf(%p) tag(%lx) flags(%s)",
	   dest_addr, fi_addr2string(buf1, 128, dest_addr, fid_ep), len, data, buf, tag, tofu_fi_flags_string(flags));

    iovs->iov_base  = (void *)buf;
    iovs->iov_len   = len;
    tmsg.msg_iov    = iovs;
    tmsg.desc	    = 0;
    tmsg.iov_count  = 1;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = tag;
    tmsg.ignore	    = 0;	/* Compare all bits */
    tmsg.context    = 0;	/* XXX */
    tmsg.data	    = data;

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "fi_errno %ld in %s\n", ret, __FILE__);
    utf_tmr_end(TMR_FI_TSEND);
    return ret;
}

/*
 * tofu_ctx_ops_tag is reffered to in tofu_ctx.c
 */
struct fi_ops_tagged tofu_ctx_ops_tag = {
    .size	    = sizeof (struct fi_ops_tagged),
    .recv	    = tofu_ctx_tag_recv,
    .recvv	    = tofu_ctx_tag_recvv,
    .recvmsg	    = tofu_ctx_tag_recvmsg,
    .send	    = tofu_ctx_tag_send,
    .sendv	    = tofu_ctx_tag_sendv,
    .sendmsg	    = tofu_ctx_tag_sendmsg,
    .inject	    = tofu_ctx_tag_inject,
    .senddata	    = tofu_ctx_tag_senddata,
    .injectdata	    = tofu_ctx_tag_injectdata,
};
