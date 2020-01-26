/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include "tofu_debug.h"
#include "tofu_impl.h"
#include "tofu_macro.h"

static int tofu_ctx_close(struct fid *fid);
static int tofu_ctx_bind(struct fid *fid, struct fid *bfid, uint64_t flags);
static struct fi_ops tofu_ctx_fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_ctx_close,
    .bind	    = tofu_ctx_bind,
    .control	    = tofu_ctx_ctrl,
    .ops_open	    = fi_no_ops_open,
};

static int tofu_ctx_getopt(fid_t fid, int level, int optname,
                           void *optval, size_t *optlen);
static int tofu_ctx_setopt(fid_t fid, int level,  int optname,
                           const void *optval, size_t optlen);

static struct fi_ops_ep tofu_ctx_ops = {
    .size           = sizeof (struct fi_ops_ep),
    .cancel         = fi_no_cancel,
    .getopt         = tofu_ctx_getopt,
    .setopt         = tofu_ctx_setopt,
    .tx_ctx         = fi_no_tx_ctx,
    .rx_ctx         = fi_no_rx_ctx,
    .rx_size_left   = fi_no_rx_size_left, /* deprecated */
    .tx_size_left   = fi_no_tx_size_left, /* deprecated */
};

static inline int
tofu_ictx_init(int index, struct tofu_ctx *ctx_priv, struct tofu_sep *sep_priv,
               void *context, void *attr, int class)
{
    int fc = FI_SUCCESS;
    /* initialize internal members */
    ctx_priv->enabled = 0;
    ctx_priv->vcqh = 0;
    ctx_priv->index = 0;
    /* initialize fabric members */
    ctx_priv->ctx_fid.fid.fclass  = class;
    ctx_priv->ctx_fid.fid.context = context;
    ctx_priv->ctx_fid.fid.ops   = &tofu_ctx_fi_ops;
    ctx_priv->ctx_fid.ops	= &tofu_ctx_ops;
    ctx_priv->ctx_fid.cm	= &tofu_ctx_ops_cm;
    ctx_priv->ctx_fid.msg       = &tofu_ctx_ops_msg;
    ctx_priv->ctx_fid.rma   	= &tofu_ctx_ops_rma;
    ctx_priv->ctx_fid.tagged	= &tofu_ctx_ops_tag;
    ctx_priv->ctx_fid.atomic	= &tofu_ctx_ops_atomic;
    ctx_priv->ctx_sep = sep_priv;
    ofi_atomic_initialize32(&ctx_priv->ctx_ref, 0);
    ctx_priv->ctx_idx = index;

    fastlock_init(&ctx_priv->ctx_lck);
    ctx_priv->ctx_xop_flg = (attr == 0)? 0UL :
        ((class == FI_CLASS_TX_CTX) ? ((struct fi_tx_attr*)attr)->op_flags:
         ((struct fi_rx_attr*)attr)->op_flags);
    dlist_init( &ctx_priv->ctx_ent_sep );
    dlist_init( &ctx_priv->ctx_ent_cq );
    dlist_init( &ctx_priv->ctx_ent_ctr );
    /* check if CTX of corresponding index has been registered */
    {
	struct tofu_ctx *ctx_dup;
	fastlock_acquire(&sep_priv->sep_lck);
	ctx_dup = tofu_sep_lup_ctx_byi_unsafe(sep_priv,
                                              class, index);
	fastlock_release(&sep_priv->sep_lck);
	if (ctx_dup != 0) {
            /* index's CTX has been already registered */
	    fc = -FI_EBUSY;
	}
    }
    return fc;
}

static inline void
tofu_cq_ins_ctx_tx(struct tofu_cq *cq_priv, struct tofu_ctx *ctx_priv)
{
    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(ctx_priv->ctx_fid.fid.fclass == FI_CLASS_TX_CTX);
    assert( dlist_empty(&ctx_priv->ctx_ent_cq) != 0 );

    fastlock_acquire(&cq_priv->cq_lck);
    {
	dlist_insert_tail(&ctx_priv->ctx_ent_cq, &cq_priv->cq_htx );
	ofi_atomic_inc32(&cq_priv->cq_ref );
    }
    fastlock_release(&cq_priv->cq_lck);
    return ;
}

static inline void
tofu_cq_rem_ctx_tx(struct tofu_cq *cq_priv, struct tofu_ctx *ctx_priv)
{
    FI_INFO(&tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert( dlist_empty(&ctx_priv->ctx_ent_cq) == 0 );

    fastlock_acquire(&cq_priv->cq_lck);
    {
	dlist_remove(&ctx_priv->ctx_ent_cq);
	ofi_atomic_dec32(&cq_priv->cq_ref);
    }
    fastlock_release(&cq_priv->cq_lck);
    return ;
}

static inline void
tofu_cq_ins_ctx_rx(struct tofu_cq *cq_priv, struct tofu_ctx *ctx_priv)
{
    FI_INFO(&tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(ctx_priv->ctx_fid.fid.fclass == FI_CLASS_RX_CTX);
    assert( dlist_empty(&ctx_priv->ctx_ent_cq) != 0 );

    fastlock_acquire(&cq_priv->cq_lck);
    {
	dlist_insert_tail(&ctx_priv->ctx_ent_cq, &cq_priv->cq_hrx);
	ofi_atomic_inc32(&cq_priv->cq_ref);
    }
    fastlock_release(&cq_priv->cq_lck);
    return;
}

static inline void
tofu_cq_rem_ctx_rx(struct tofu_cq *cq_priv, struct tofu_ctx *ctx_priv)
{
    FI_INFO(&tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert( dlist_empty(&ctx_priv->ctx_ent_cq) == 0 );

    fastlock_acquire(&cq_priv->cq_lck);
    {
	dlist_remove(&ctx_priv->ctx_ent_cq);
	ofi_atomic_dec32(&cq_priv->cq_ref);
    }
    fastlock_release(&cq_priv->cq_lck);
    return;
}

static inline void
tofu_sep_ins_ctx_tx(struct tofu_sep *sep_priv, struct tofu_ctx *ctx_priv)
{
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(ctx_priv->ctx_fid.fid.fclass == FI_CLASS_TX_CTX);
    assert( dlist_empty(&ctx_priv->ctx_ent_sep) != 0 );

    fastlock_acquire(&sep_priv->sep_lck);
    {
	dlist_insert_tail( &ctx_priv->ctx_ent_sep, &sep_priv->sep_htx );
	ofi_atomic_inc32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

static inline void
tofu_sep_rem_ctx_tx(struct tofu_sep *sep_priv, struct tofu_ctx *ctx_priv)
{
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert( dlist_empty(&ctx_priv->ctx_ent_sep) == 0 );

    fastlock_acquire(&sep_priv->sep_lck);
    {
	dlist_remove(&ctx_priv->ctx_ent_sep);
	ofi_atomic_dec32(&sep_priv->sep_ref);
    }
    fastlock_release(&sep_priv->sep_lck);
    return ;
}

static inline void
tofu_sep_ins_ctx_rx(struct tofu_sep *sep_priv, struct tofu_ctx *ctx_priv)
{
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(ctx_priv->ctx_fid.fid.fclass == FI_CLASS_RX_CTX);
    assert( dlist_empty(&ctx_priv->ctx_ent_sep) != 0 );

    fastlock_acquire(&sep_priv->sep_lck);
    {
	dlist_insert_tail(&ctx_priv->ctx_ent_sep, &sep_priv->sep_hrx);
	ofi_atomic_inc32(&sep_priv->sep_ref);
    }
    fastlock_release(&sep_priv->sep_lck);
    return ;
}

static inline void
tofu_sep_rem_ctx_rx(struct tofu_sep *sep_priv, struct tofu_ctx *ctx_priv)
{
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert( dlist_empty(&ctx_priv->ctx_ent_sep) == 0 );

    fastlock_acquire(&sep_priv->sep_lck);
    {
	dlist_remove(&ctx_priv->ctx_ent_sep);
	ofi_atomic_dec32(&sep_priv->sep_ref);
    }
    fastlock_release(&sep_priv->sep_lck);
    return;
}

static int
tofu_ictx_close(struct tofu_ctx *ctx)
{
    int uc = UTOFU_SUCCESS;

    if ((ctx == 0) || (ctx->ctx_sep == 0)) {
        fprintf(stderr, "YI*** ictx(%p) ctx->ctx_sep(%p)\n",
                ctx, ctx->ctx_sep); fflush(stderr);
	uc = UTOFU_ERR_INVALID_ARG; goto bad;
    }
    if (ctx->enabled != 0) {
	assert(ctx->vcqh != 0); /* XXX : UTOFU_VCQ_HDL_NULL */
	uc = utofu_free_vcq(ctx->vcqh);
	if (uc != UTOFU_SUCCESS) { goto bad; }
	ctx->vcqh = 0; /* XXX */
	ctx->enabled = 0;
    }
    /* unexpected entries */
    /* expected entries */
    /* transmit entries */
    /* desc_cash */
bad:
    return uc;
}

/*
 * fi_ep_close
 */
static int tofu_ctx_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_ctx *ctx_priv;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    ctx_priv = container_of(fid, struct tofu_ctx, ctx_fid.fid);

    if (ofi_atomic_get32(&ctx_priv->ctx_ref) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    if (ctx_priv->ctx_send_cq != 0) { /* remove ceq_send_cq */
	tofu_cq_rem_ctx_tx(ctx_priv->ctx_send_cq, ctx_priv);
    }
    if (ctx_priv->ctx_recv_cq != 0) {/* remove ceq_recv_cq */
	tofu_cq_rem_ctx_rx(ctx_priv->ctx_recv_cq, ctx_priv);
    }
    tofu_ictx_close(ctx_priv);
    if ( ! dlist_empty( &ctx_priv->ctx_ent_sep ) ) {
	if (ctx_priv->ctx_fid.fid.fclass == FI_CLASS_TX_CTX) {
	    tofu_sep_rem_ctx_tx(ctx_priv->ctx_sep, ctx_priv);
	}
	else {
	    tofu_sep_rem_ctx_rx( ctx_priv->ctx_sep, ctx_priv );
	}
    }
    if (ctx_priv->ctx_trx != 0) {
	if (ctx_priv->ctx_trx->ctx_trx != 0) {
	    assert(ctx_priv->ctx_trx->ctx_trx == ctx_priv);
	    ctx_priv->ctx_trx->ctx_trx = 0;
	}
    }
    if (ofi_atomic_get32(&ctx_priv->ctx_ref) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    fastlock_destroy(&ctx_priv->ctx_lck);

    free(ctx_priv);

bad:
    return fc;
}

/*
 * int
 *  fi_ep_bind(struct fid_ep *ep, struct fid *fid, uint64_t flags)
 *      ep: FI_CLASS_TX_CTX or FI_CLASS_RX_CTX
 *      fid: resources FI_CLASS_CQ or FI_CLASS_CNTR
 *      flags: FI_RECV(0x400), FI_SEND (0x800),
 *             FI_SELECTIVE_COMPLETION (0x800000000000000)
 *                Indicates that a completion queue entry should be
 *                written for data transfer operations. This flag only
 *                applies to operations issued on an endpoint that was
 *                bound to a completion queue with the
 *                FI_SELECTIVE_COMPLETION flag set, otherwise, it is
 *                ignored. fi_endpoint(3)
 *              Other flags are currently not yet implemented.
 *             
 */
static int tofu_ctx_bind(struct fid *fid, struct fid *bfid, uint64_t flags)
{
    int fc = FI_SUCCESS;
    struct tofu_ctx *ctx_priv;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s flags(%lx)\n", __FILE__, flags);
    assert(fid != 0);
    ctx_priv = container_of(fid, struct tofu_ctx, ctx_fid.fid);

    fc = ofi_ep_bind_valid( &tofu_prov, bfid, flags);
    if (fc != 0) {
	goto bad;
    }
    /*
     * man fi_endpoint(3)
     *   Endpoints making use of completion queues, counters,
     *   event queues, and/or address vectors must be bound to them
     *   before being enabled.
     */
    /* if (ctx_priv->ctx_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    assert(bfid != 0);
    switch (bfid->fclass) {
	struct tofu_cq *cq_priv;
	struct tofu_cntr *ctr_priv;
    case FI_CLASS_CQ:
	cq_priv = container_of(bfid, struct tofu_cq, cq_fid.fid);
	if (ctx_priv->ctx_sep->sep_dom != cq_priv->cq_dom) {
	    fc = -FI_EDOMAIN /* -FI_EINVAL */; goto bad;
	}
	switch (fid->fclass) {
	case FI_CLASS_TX_CTX:
            //printf("YI******bind: TX cq_priv(%p) flags(%lx)\n", cq_priv, flags);
            R_DBG0(RDBG_LEVEL1, "fi_ep_bind: CQ(%p) TX_CTX(%p)", cq_priv, ctx_priv);
	    if (flags & FI_SEND) {
		/*
		 * man fi_endpoint(3)
		 *   An endpoint may only be bound to a single CQ or
		 *   counter for a given type of operation.
		 *   For example, an EP may not bind to two counters both
		 *   using FI_WRITE.
		 */
                cq_priv->cq_ssel = flags&FI_SELECTIVE_COMPLETION ? 1 : 0;
		if (ctx_priv->ctx_send_cq != 0) {
		    fc = -FI_EBUSY; goto bad;
		}
		ctx_priv->ctx_send_cq = cq_priv;
		tofu_cq_ins_ctx_tx(cq_priv, ctx_priv);
	    }
	    break;
	case FI_CLASS_RX_CTX:
            //printf("YI******bind: RX cq_priv(%p) flags(%lx)\n", cq_priv, flags);
            R_DBG0(RDBG_LEVEL1, "fi_ep_bind: CQ(%p) RX_CTX(%p)", cq_priv, ctx_priv);
	    if (flags & FI_RECV) {
		if (ctx_priv->ctx_recv_cq != 0) {
		    fc = -FI_EBUSY; goto bad;
		}
		ctx_priv->ctx_recv_cq = cq_priv;
		tofu_cq_ins_ctx_rx(cq_priv, ctx_priv);
                cq_priv->cq_rsel = flags&FI_SELECTIVE_COMPLETION ? 1 : 0;
	    }
	    break;
	default:
	    fc = -FI_EINVAL; goto bad;
	}
        break;
    case FI_CLASS_CNTR:
	ctr_priv = container_of(bfid, struct tofu_cntr, ctr_fid.fid);
        //printf("YI*****bind: CNTR cq_priv(%p) flags(%lx)\n", ctr_priv, flags);
	if (ctx_priv->ctx_sep->sep_dom != ctr_priv->ctr_dom) {
	    fc = -FI_EDOMAIN /* -FI_EINVAL */; goto bad;
	}
	switch (fid->fclass) {
	case FI_CLASS_TX_CTX:
            //printf("YI***** TRANSMIT CONTEXT\n");
	    if (flags & (FI_SEND | FI_WRITE | FI_TRANSMIT)) {
		if (ctx_priv->ctx_send_ctr != 0) {
		    fc = -FI_EBUSY; goto bad;
		}
		ctx_priv->ctx_send_ctr = ctr_priv;
		ctr_priv->ctr_tsl = flags&FI_SELECTIVE_COMPLETION ? 1 : 0;
	    } else if (flags) {
                FI_INFO(&tofu_prov, FI_LOG_EP_CTRL,
                        "fi_ep_bind: Unsupported flags(%lx) in %s\n",
                        flags, __FILE__);
                fc = -FI_EBUSY; goto bad;
            }
	    break;
	case FI_CLASS_RX_CTX:
            //printf("YI***** RECEIVE CONTEXT\n");
	    if (flags & FI_RECV) {
		if (ctx_priv->ctx_recv_ctr != 0) {
		    fc = -FI_EBUSY; goto bad;
		}
		ctx_priv->ctx_recv_ctr = ctr_priv;
		ctr_priv->ctr_rsl = flags&FI_SELECTIVE_COMPLETION ? 1 : 0;
	    } else if (flags) {
                FI_INFO(&tofu_prov, FI_LOG_EP_CTRL,
                        "fi_ep_bind: Unsupported flags(%lx) in %s\n",
                        flags, __FILE__);
                fc = -FI_EBUSY; goto bad;
            }
	    break;
	default:
	    fc = -FI_EINVAL; goto bad;
	}
	break;
    default:
        fc = -FI_ENOSYS; goto bad;
    }

bad:
    return fc;
}

/*
 * ictx_ctrl_enab() is called from tofu_ctx_ctrl() that is
 * implementation of FI_ENABLE on fi_control
 */
static int
ictx_ctrl_enab(int class, struct tofu_ctx *ctx)
{
    int uc = UTOFU_SUCCESS;
    struct tofu_sep *sep = ctx->ctx_sep;

    if (ctx->enabled != 0) {
	uc = UTOFU_ERR_BUSY; goto bad;
    }

    /* unexpected entries */
    /* expected entries */
    /* transmit entries */
    /* desc_cash */
    /* ictx_ctrl_enab */
    if (ctx->vcqh == 0) {
	utofu_vcq_hdl_t vcqh = 0;
	utofu_tni_id_t tni_id;
        struct tofu_domain  *dom = ctx->ctx_sep->sep_dom;
	const utofu_cmp_id_t c_id = CONF_TOFU_CMPID;
	const unsigned long flags =	0
				/* | UTOFU_VCQ_FLAG_THREAD_SAFE */
				/* | UTOFU_VCQ_FLAG_EXCLUSIVE */
				/* | UTOFU_VCQ_FLAG_SESSION_MODE */
				;
	if ((ctx->index < 0) || (ctx->index >= sep->ntni)) {
	    uc = UTOFU_ERR_INVALID_TNI_ID; goto bad;
	}
	tni_id = sep->tnis[ctx->index];
        R_DBG("create_vcq tni_id=%d c_id(%d) flags(%ld))", tni_id, c_id, flags);
	uc = utofu_create_vcq_with_cmp_id(tni_id, c_id, flags, &vcqh);
        R_DBG("return uc = %d vcqh = %lx", uc, vcqh);
	if (uc != UTOFU_SUCCESS) { goto bad; }
	dbg_show_utof_vcqh(vcqh);
	assert(vcqh != 0); /* XXX : UTOFU_VCQ_HDL_NULL */
	ctx->vcqh = vcqh;
        /*
         * vcqh is copied to domain
         *   It seems that vcqh should be created at domain creation time,
         *   but vcqh associated with TNI is only created at this time.
         */
        dom->dom_vcqh[dom->dom_nvcq] = ctx->vcqh;
        dom->dom_nvcq++;
    }
    //ictx->nrma = 0;
    ctx->enabled = 1;

bad:
    return uc;
}

/*
 * fi_control FI_ENABLE
 */
int
tofu_ctx_ctrl(struct fid *fid, int command, void *arg)
{
    int fc = FI_SUCCESS;
    struct tofu_ctx *ctx_priv;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    ctx_priv = container_of(fid, struct tofu_ctx, ctx_fid.fid);

    switch (command) {
    case FI_ENABLE:
	switch (fid->fclass) {
	    int uc;
	case FI_CLASS_TX_CTX:
	    if (ctx_priv->ctx_enb != 0) {
		goto bad; /* XXX - is not an error */
	    }
	    uc = ictx_ctrl_enab(FI_CLASS_TX_CTX, ctx_priv);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }
	    ctx_priv->ctx_enb = 1;
	    break;
	case FI_CLASS_RX_CTX:
	    if (ctx_priv->ctx_enb != 0) {
		goto bad; /* XXX - is not an error */
	    }
	    uc = ictx_ctrl_enab(FI_CLASS_RX_CTX, ctx_priv);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }
	    ctx_priv->ctx_enb = 1;
	    break;
	default:
	    fc = -FI_EINVAL; goto bad;
	}
	break;
    default:
	fc = -FI_ENOSYS; goto bad;
    }

bad:
    return fc;
}

static int
tofu_ctx_getopt(fid_t fid, int level, int optname,
                           void *optval, size_t *optlen)
{
    int fc = FI_SUCCESS;
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fc = -FI_ENOSYS;
    return fc;
}

static int
tofu_ctx_setopt(fid_t fid, int level,  int optname,
                const void *optval, size_t optlen)
{
    int fc = FI_SUCCESS;
    struct tofu_ctx *ctx_priv;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    ctx_priv = container_of(fid, struct tofu_ctx, ctx_fid.fid);
    if (ctx_priv == 0) { }

    if (optval == 0) {
	fc = -FI_EINVAL; goto bad;
    }
    if (level != FI_OPT_ENDPOINT) {
	fc = -FI_ENOPROTOOPT; goto bad;
    }

    switch (optname) {
    case FI_OPT_MIN_MULTI_RECV:
	if (optlen != sizeof (size_t)) {
	    fc = -FI_EINVAL; goto bad;
	}
	if (((size_t *)optval)[0] <= 0) {
	    fc = -FI_EINVAL; goto bad;
	}
	/* ctx_priv->min_multi_recv = ((size_t *)optval)[0]; */
	break;
    default:
	fc = -FI_ENOPROTOOPT; goto bad;
    }

bad:
    return fc;
}


/*
 * Body of fi_tx_context in Tofu
 */
int
tofu_ctx_tx_context(struct fid_ep *fid_sep,  int index,
                    struct fi_tx_attr *attr,
                    struct fid_ep **fid_ctx_tx,  void *context)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep_priv;
    struct tofu_ctx *ctx_priv = 0;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid_sep != 0);
    if (fid_sep->fid.fclass != FI_CLASS_SEP) {
	fc = -FI_EINVAL; goto bad;
    }
    sep_priv = container_of(fid_sep, struct tofu_sep, sep_fid);
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "api_version %08x\n",
            sep_priv->sep_dom->dom_fab->fab_fid.api_version);
#if 0
    if (index >= (int)sep_priv->attr->ep_attr.tx_ctx_cnt) {
        fc = -FI_EINVAL; goto bad;
    }
#endif
    if (attr != 0) {
	struct fi_tx_attr *prov_attr = 0; /* default */
	uint64_t user_info_mode = 0;

	fc = tofu_chck_ctx_tx_attr(prov_attr, attr, user_info_mode);
	if (fc != 0) { goto bad; }
    }
    ctx_priv = calloc(1, sizeof(struct tofu_ctx));
    if (ctx_priv == 0) {
        fc = -FI_ENOMEM; goto bad;
    }
    //ulib_ofif_ictx_init(ctx_priv, offs_ulib);
    /* initialize ctx_priv and register it into SEP */
    if ((fc = tofu_ictx_init(index, ctx_priv, sep_priv,
                             context, attr, FI_CLASS_TX_CTX)) != FI_SUCCESS) {
        goto bad;
    }
    tofu_sep_ins_ctx_tx(ctx_priv->ctx_sep, ctx_priv);

    /* return fid_ctx */
    fid_ctx_tx[0] = &ctx_priv->ctx_fid;
    goto ok;
bad:
    if (ctx_priv != 0) {
	tofu_ctx_close( &ctx_priv->ctx_fid.fid );
    }
ok:
    return fc;
}

/*
 * Body of fi_rx_context in Tofu
 */
int
tofu_ctx_rx_context(struct fid_ep *fid_sep, int index,
                    struct fi_rx_attr *attr,
                    struct fid_ep **fid_ctx_rx, void *context)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep_priv;
    struct tofu_ctx *ctx_priv = 0;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid_sep != 0);
    if (fid_sep->fid.fclass != FI_CLASS_SEP) {
	fc = -FI_EINVAL; goto bad;
    }
    sep_priv = container_of(fid_sep, struct tofu_sep, sep_fid );
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "api_version %08x\n",
        sep_priv->sep_dom->dom_fab->fab_fid.api_version);

    if (attr != 0) {
	struct fi_info *prov_info = 0; /* default */
	uint64_t user_info_mode = 0;

	fc = tofu_chck_ctx_rx_attr( prov_info, attr, user_info_mode);
	if (fc != 0) { goto bad; }
    }
    ctx_priv = calloc(1, sizeof(struct tofu_ctx));
    if (ctx_priv == 0) {
        fc = -FI_ENOMEM; goto bad;
    }
    /* initialize ctx_priv and register it into SEP */
    if ((fc = tofu_ictx_init(index, ctx_priv, sep_priv,
                             context, attr, FI_CLASS_RX_CTX)) != FI_SUCCESS) {
        goto bad;
    }
    tofu_sep_ins_ctx_rx( ctx_priv->ctx_sep, ctx_priv );

    /* return fid_ctx */
    fid_ctx_rx[0] = &ctx_priv->ctx_fid;
    ctx_priv = 0; /* ZZZ */

bad:
    if (ctx_priv != 0) {
	tofu_ctx_close( &ctx_priv->ctx_fid.fid );
    }
    return fc;
}
