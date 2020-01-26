#include "tofu_impl.h"
#include "tofu_addr.h"

extern char *tank2string(char *buf, size_t sz, uint64_t ui64);

static int
tofu_imp_name_tniq_byi(const void *vnam, size_t index,  uint8_t tniq[2])
{
    int fc = FI_SUCCESS;
    const struct ulib_sep_name *name = vnam;
    uint8_t tni, tcq;

    /* check valid flag */
    if (name->v == 0) { fc = -FI_EINVAL; goto bad; }

    /* check tni */
    {
	size_t mx = sizeof (name->tniq) / sizeof (name->tniq[0]);

	if (index > mx) { fc = -FI_EINVAL; goto bad; }

	tni = name->tniq[index] >> 4;
	tcq = name->tniq[index] &  0x0f;
	if (tni >= 6) { fc = -FI_EINVAL; goto bad; }
    }

    tniq[0] = tni; tniq[1] = tcq;

bad:
    return fc;
}

static inline void
ulib_cast_epnt_to_tank(const struct ulib_epnt_info *einf,
                       struct ulib_tank *tank)
{
    assert(sizeof (tank[0]) == sizeof (uint64_t));
    assert(einf->xyz[0] < (1UL << ULIB_TANK_BITS_TUX));
    tank->tux = einf->xyz[0];
    assert(einf->xyz[1] < (1UL << ULIB_TANK_BITS_TUY));
    tank->tuy = einf->xyz[1];
    assert(einf->xyz[2] < (1UL << ULIB_TANK_BITS_TUZ));
    tank->tuz = einf->xyz[2];

    assert(einf->xyz[3] < (1UL << ULIB_TANK_BITS_TUA));
    tank->tua = einf->xyz[3];
    assert(einf->xyz[4] < (1UL << ULIB_TANK_BITS_TUB));
    tank->tub = einf->xyz[4];
    assert(einf->xyz[5] < (1UL << ULIB_TANK_BITS_TUC));
    tank->tuc = einf->xyz[5];

    assert(einf->tni[0] < (1UL << ULIB_TANK_BITS_TNI));
    tank->tni = einf->tni[0];
    assert(einf->tcq[0] < (1UL << ULIB_TANK_BITS_TCQ));
    tank->tcq = einf->tcq[0];

    assert(einf->cid[0] < (1UL << ULIB_TANK_BITS_CID));
    tank->cid = einf->cid[0];
    tank->vld = 1;
    tank->pid = einf->pid[0];
    return ;
}

static inline void
ulib_cast_epnt_to_tank_ui64(const struct ulib_epnt_info *einf,
                            uint64_t *tank_ui64)
{
    union ulib_tofa_u tank_u = { .ui64 = 0, };
    ulib_cast_epnt_to_tank( einf, &tank_u.tank );
    tank_ui64[0] = tank_u.ui64;
    return ;
}

static inline int
tofu_imp_namS_to_tank(const void *vnam,  size_t index, uint64_t *tank_ui64)
{
    int fc = FI_SUCCESS;
    const struct ulib_sep_name *name = vnam;
    uint8_t tniq[2];
    struct ulib_epnt_info info[1];

    fc = tofu_imp_name_tniq_byi(vnam, index, tniq);
    if (fc != FI_SUCCESS) { goto bad; }

    info->xyz[0] = name->txyz[0];
    info->xyz[1] = name->txyz[1];
    info->xyz[2] = name->txyz[2];
    info->xyz[3] = name->a;
    info->xyz[4] = name->b;
    info->xyz[5] = name->c;
    info->tni[0] = tniq[0];
    info->tcq[0] = tniq[1];
    info->cid[0] = name->p; /* component id */
    info->pid[0] = name->vpid; /* virtual processor id. (rank) */

    ulib_cast_epnt_to_tank_ui64(info, tank_ui64);

bad:
    return fc;
}

static inline int
tofu_av_lup_tank(struct tofu_av *av_priv,  fi_addr_t fi_a,  uint64_t *tank)
{
    int fc = FI_SUCCESS;
    size_t av_idx, rx_idx;
    void *vnam;

    if (fi_a == FI_ADDR_NOTAVAIL) {
	fc = -FI_EINVAL; goto bad;
    }
    assert(av_priv->av_rxb >= 0);
    /* assert(av_priv->av_rxb <= TOFU_RX_CTX_MAX_BITS); */
    if (av_priv->av_rxb == 0) {
	rx_idx = 0;
	av_idx = fi_a;
    }
    else {
	rx_idx = ((uint64_t)fi_a) >> (64 - av_priv->av_rxb);
	/* av_idx = fi_a & rx_ctx_mask */
	av_idx = (((uint64_t)fi_a) << av_priv->av_rxb) >> av_priv->av_rxb;
    }
    if (av_idx >= av_priv->av_tab.nct) {
	fc = -FI_EINVAL; goto bad;
    }
    assert(av_priv->av_tab.tab != 0);
    vnam = (char *)av_priv->av_tab.tab + (av_idx * 16); /* XXX */

    /* get the Tofu network Address + raNK */
    fc = tofu_imp_namS_to_tank(vnam, rx_idx, tank);
    if (fc != FI_SUCCESS) { goto bad; }

bad:
    return fc;
}

static inline char *
fi_addr2string(char *buf, ssize_t sz, fi_addr_t fi_addr, struct fid_ep *fid_ep)
{
    struct tofu_ctx *ctx_priv;
    struct tofu_av *av_priv;
    uint64_t ui64;

    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    av_priv = ctx_priv->ctx_sep->sep_av_;
    tofu_av_lup_tank(av_priv, fi_addr, &ui64);
    return tank2string(buf, sz, ui64);
}

static int
utf_post(struct tofu_ctx *ctx, const struct fi_msg_tagged *msg, uint64_t flags)
{
    fprintf(stderr, "%s: ctx(%p) msg(%p) flags(%lx)\n", __func__, ctx, msg, flags);
    return 0;
}

static ssize_t
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
    ret = utf_post(ctx, msg, flags);
    fastlock_release(&ctx->ctx_lck);
bad:
    return ret;
}


/*
 * fi_recv
 */
static ssize_t 
tofu_ctx_msg_recv(struct fid_ep *fid_ep, void *buf,  size_t len,
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
    return ret;

}

/*
 * fi_recvv
 */
static ssize_t
tofu_ctx_msg_recvv(struct fid_ep *fid_ep,
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
    return ret;
}

/*
 * fi_recvmsg
 */
static ssize_t
tofu_ctx_msg_recvmsg(struct fid_ep *fid_ep, const struct fi_msg *msg,
                     uint64_t flags)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    tmsg.msg_iov    = msg->msg_iov;
    tmsg.desc	    = msg->desc;
    tmsg.iov_count  = msg->iov_count;
    tmsg.addr	    = msg->addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = msg->context;
    tmsg.data	    = msg->data;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_recvmsg src(%s) len(%ld) buf(%p) flags(0x%lx)", 
           fi_addr2string(buf1, 128, msg->addr, fid_ep),
           msg->msg_iov[0].iov_len, msg->msg_iov[0].iov_base, flags);

    ret = tofu_ctx_msg_recv_common(fid_ep, &tmsg, flags);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s return %ld\n", __FILE__, ret);
    if (ret != 0) { goto bad; }

bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s return %ld\n", __FILE__, ret);
    return ret;
}


static ssize_t
tofu_ctx_msg_send_common(struct fid_ep *fid_ep,
                         const struct fi_msg_tagged *msg,
                         uint64_t flags)
{
    ssize_t          ret = FI_SUCCESS;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "\tdest(%ld) iovcount(%ld) size(%ld) in %s\n", msg->addr, msg->iov_count, msg->msg_iov[0].iov_len, __FILE__);

    if (fid_ep->fid.fclass != FI_CLASS_TX_CTX) {
	ret = -FI_EINVAL; goto bad;
    }
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "flags %016"PRIx64"\n", flags);
    if ((flags & FI_TRIGGER) != 0) {
	ret = -FI_ENOSYS; goto bad;
    }
#if 0
    int fc;
    struct tofu_ctx  *ctx = 0;
    union ulib_tofa_u   tank;
    fi_addr_t        fi_a = msg->addr;
    struct tofu_av   *av_priv;
    ctx = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    if (msg->msg_iov[0].iov_base) {
        yi_showcntrl(__func__, __LINE__, msg->msg_iov[0].iov_base);
    }
    /* convert fi_addr to tank */
    av_priv = ctx_priv->ctx_sep->sep_av_;
    fc = tofu_av_lup_tank(av_priv, fi_a, &tank.ui64);
    if (fc != FI_SUCCESS) { ret = fc; goto bad; }
    tank.tank.pid = 0; tank.tank.vld = 0;
    /*
     * My rank must be resolved here
     */
    if (ictx->myrank == -1U) {
	struct tofu_sep *sep_priv = ctx_priv->ctx_sep;
	struct tofu_av *av_priv = sep_priv->sep_av_;
	assert(av_priv != 0);
	tofu_av_lup_rank(av_priv, ictx->vcqh, ictx->index, &ictx->myrank);
        myrank = ictx->myrank;

	/* for FI_CLASS_RX_CTX ? (ictx->shadow) */
	if (ctx_priv->ctx_trx != 0) {
	    struct ulib_ictx *ictx_peer = (void *)(ctx_priv->ctx_trx + 1);
	    if (ictx_peer->myrank == -1U) {
		ictx_peer->myrank = ictx->myrank;
	    }
	}
    }
    if (ictx->tofa.ui64 == tank.ui64) {
	int fc;
        FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "***SELF SEND\n");
        fc = tofu_impl_ulib_sendmsg_self(ctx_priv, sizeof(struct tofu_ctx),
                                         msg, flags);
	if (fc != 0) {
	    ret = fc; goto bad;
	}
    } else {
        const size_t    offs_ulib = sizeof (ctx_priv[0]);
	/* post it */
	fastlock_acquire(&ctx_priv->ctx_lck);
	ret = tofu_imp_ulib_send_post(ctx_priv, offs_ulib, msg, flags,
                                      ctx_priv->ctx_send_cq);
	fastlock_release(&ctx_priv->ctx_lck);
	if (ret != 0) { goto bad; }
    }
#endif
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "fi_errno %ld\n", ret);
    return ret;
}

/*
 * fi_sendmsg
 */
static ssize_t
tofu_ctx_msg_sendmsg(struct fid_ep *fid_ep,const struct fi_msg *msg,
		     uint64_t flags)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    tmsg.msg_iov    = msg->msg_iov;
    tmsg.desc	    = msg->desc;
    tmsg.iov_count  = msg->iov_count;
    tmsg.addr	    = msg->addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = msg->context;
    tmsg.data	    = msg->data;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_senddata dest(%s) len(%ld) buf(%p) data(%ld) flags(0x%lx)",
          fi_addr2string(buf1, 128, msg->addr, fid_ep),
          msg->msg_iov[0].iov_len, msg->msg_iov[0].iov_base, msg->data, flags);

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);
    if (ret != 0) { goto bad; }
bad:
    return ret;
}

/*
 * fi_sendv
 */
static ssize_t
tofu_ctx_msg_sendv(struct fid_ep *fid_ep, const struct iovec *iov,
		   void **desc, size_t count, fi_addr_t dest_addr, void *context)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    tmsg.msg_iov    = iov;
    tmsg.desc	    = desc;
    tmsg.iov_count  = count;
    tmsg.addr	    = dest_addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_sendv dest(%s) len(%ld) buf(%p) FI_COMPLETION",
          fi_addr2string(buf1, 128, dest_addr, fid_ep),
          iov[0].iov_len, iov[0].iov_base);

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, FI_COMPLETION /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    return ret;
}

/*
 * fi_send
 */
static ssize_t
tofu_ctx_msg_send(struct fid_ep *fid_ep, const void *buf, size_t len,
		  void *desc, fi_addr_t dest_addr, void *context)
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
    return ret;
}

/*
 * fi_inject
 */
static ssize_t
tofu_ctx_msg_inject(struct fid_ep *fid_ep,  const void *buf, size_t len,
		    fi_addr_t dest_addr)
{
    ssize_t ret = -FI_ENOSYS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    uint64_t flags = FI_INJECT;
    void        *alocbuf;

    alocbuf = malloc(len);
    memcpy(alocbuf, buf, len); // NEEDDS to handle dynamic memory allocation!!
    iovs->iov_base  = alocbuf;
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
    R_DBG1(RDBG_LEVEL3, "fi_inject dest(%s) len(%ld) orgbuf(%p) copied(%p) FI_INJECT",
          fi_addr2string(buf1, 128, dest_addr, fid_ep),
          len, buf, alocbuf);

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);

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
static ssize_t
tofu_ctx_tag_recv(struct fid_ep *fid_ep,
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

    R_DBG1(RDBG_LEVEL3, "fi_trecv src(%s) len(%ld) buf(%p) tag(0x%lx) ignore(0x%lx) flags(0x%lx)",
           fi_addr2string(buf1, 128, src_addr, fid_ep),
           len, buf, tag, ignore, flags);
    ret = tofu_ctx_msg_recv_common(fid_ep, &tmsg, flags);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s return %ld\n", __FILE__, ret);
    return ret;
}

/*
 * fi_trecvv
 */
static ssize_t
tofu_ctx_tag_recvv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count,
                   fi_addr_t src_addr,
                   uint64_t tag, uint64_t ignore, void *context)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fprintf(stderr, "\tYIYI: NEEDS to implement\n");
    R_DBG0(RDBG_LEVEL3, "NEEDS TO IMPLEMENT: fi_recvv len(%ld) buf(%p) tag(0x%lx) ignore(0x%lx) flags(0)",
          iov[0].iov_len,iov[0].iov_base, tag, ignore);
    return ret;
}

static ssize_t
tofu_ctx_tag_recvmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_tagged *msg, uint64_t flags)
{
    ssize_t ret = -FI_ENOSYS;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);

    flags |= FI_TAGGED;
    R_DBG1(RDBG_LEVEL3, "fi_trecvmsg src(%s) len(%ld) buf(%p) flags(0x%lx)", 
           fi_addr2string(buf1, 128, msg->addr, fid_ep),
           msg->msg_iov ? msg->msg_iov[0].iov_len : 0,
           msg->msg_iov ? msg->msg_iov[0].iov_base : 0, flags);

    ret = tofu_ctx_msg_recv_common(fid_ep, msg, flags);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s return %ld\n", __FILE__, ret);
    return ret;
}

/*
 * fi_tsend
 */
static ssize_t
tofu_ctx_tag_send(struct fid_ep *fid_ep, const void *buf, size_t len,
                  void *desc, fi_addr_t dest_addr,  uint64_t tag, void *context)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fprintf(stderr, "YI***** %s needs to implement\n", __func__);
    R_DBG1(RDBG_LEVEL3, "NEEDS to implement: fi_tsend dest(%s) len(%ld) buf(%p) desc(%p) tag(%lx)",
          fi_addr2string(buf1, 128, dest_addr, fid_ep),
          len, buf, desc, tag);

    return ret;
}

/*
 * fi_tsendv
 */
static ssize_t
tofu_ctx_tag_sendv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count, fi_addr_t dest_addr,
                   uint64_t tag, void *context)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fprintf(stderr, "YI***** %s needs to implement\n", __func__);
    R_DBG1(RDBG_LEVEL3, "NEEDS to implement: fi_tsendv dest(%s) len(%ld) FI_COMPLETION",
          fi_addr2string(buf1, 128, dest_addr, fid_ep), iov[0].iov_len);

    return ret;
}

/*
 * fi_tsendmsg
 */
static ssize_t
tofu_ctx_tag_sendmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_tagged *msg, uint64_t flags)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fprintf(stderr, "YI***** %s needs to implement\n", __func__);
    R_DBG1(RDBG_LEVEL3, "Needs to implement: fi_senddata dest(%s) len(%ld) data(%ld) flags(0x%lx)",
           fi_addr2string(buf1, 128, msg->addr, fid_ep),
           msg->msg_iov[0].iov_len, msg->data, flags);

    return ret;
}

/*
 * fi_tinject
 */
static ssize_t
tofu_ctx_tag_inject(struct fid_ep *fid_ep,
                    const void *buf, size_t len,
                    fi_addr_t dest_addr, uint64_t tag)
{
    ssize_t ret = -FI_ENOSYS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fprintf(stderr, "YI***** %s needs to implement\n", __func__);
    R_DBG1(RDBG_LEVEL3, "Needs to implement: fi_tinject dest(%s) len(%ld) FI_INJECT",
          fi_addr2string(buf1, 128, dest_addr, fid_ep), len);

    return ret;
}

/*
 * fi_tsenddata
 */
static ssize_t
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

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s data(%ld)\n", __FILE__, data);

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
        R_DBG1(RDBG_LEVEL3, "fi_tsenddata dest(%s) len(%ld) data(%ld) "
               "buf(%d) tag(%lx) flags(%lx)",
               fi_addr2string(buf1, 128, dest_addr, fid_ep),
               len, data, *(int*)buf, tag, flags);
    } else {
        R_DBG1(RDBG_LEVEL3, "fi_tsenddata dest(%s) len(%ld) data(%ld) "
               "buf=nil tag(%lx) flags(%lx)",
               fi_addr2string(buf1, 128, dest_addr, fid_ep),
               len, data, tag, flags);
    }

    ret = tofu_ctx_msg_send_common(fid_ep, &tmsg, flags);
    return ret;
}

/*
 * fi_tinjectdata
 *      remote CQ data is included
 */
static ssize_t 
tofu_ctx_tag_injectdata(struct fid_ep *fid_ep,
                        const void *buf, size_t len, uint64_t data,
                        fi_addr_t dest_addr, uint64_t tag)
{
    ssize_t ret = -FI_ENOSYS;
    struct fi_msg_tagged tmsg;
    struct iovec iovs[1];
    uint64_t flags = FI_INJECT | FI_TAGGED | FI_REMOTE_CQ_DATA
		    /* | TOFU_USE_OP_FLAG */ /* YYY */
		    ;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_tinjectdata dest(%s) len(%ld) data(%ld) flags(%lx)",
          fi_addr2string(buf1, 128, dest_addr, fid_ep), len, data,flags);

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
