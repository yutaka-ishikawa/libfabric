/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int tofu_cep_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_cep *cep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    cep_priv = container_of(fid, struct tofu_cep, cep_fid.fid);

#ifndef	NOTDEF
    if (ofi_atomic_get32( &cep_priv->cep_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
#endif	/* NOTDEF */
    if (cep_priv->cep_send_cq != 0) {
	tofu_cq_rem_cep_tx(cep_priv->cep_send_cq, cep_priv);
    }
    if (cep_priv->cep_recv_cq != 0) {
	tofu_cq_rem_cep_rx(cep_priv->cep_recv_cq, cep_priv);
    }
#ifdef	CONF_TOFU_SHEA
    {
	const size_t offs_ulib = sizeof (cep_priv[0]);
	tofu_imp_ulib_fini(cep_priv, offs_ulib);
    }
#endif	/* CONF_TOFU_SHEA */
#ifdef	CONF_TOFU_RECV	/* DONE */
    if (cep_priv->recv_fs != 0) {
	tofu_recv_fs_free(cep_priv->recv_fs); cep_priv->recv_fs = 0;
    }
#endif	/* CONF_TOFU_RECV */
    if ( ! dlist_empty( &cep_priv->cep_ent_sep ) ) {
	if (cep_priv->cep_fid.fid.fclass == FI_CLASS_TX_CTX) {
	    tofu_sep_rem_cep_tx( cep_priv->cep_sep, cep_priv );
	}
	else {
	    tofu_sep_rem_cep_rx( cep_priv->cep_sep, cep_priv );
	}
    }
    if (cep_priv->cep_trx != 0) {
	if (cep_priv->cep_trx->cep_trx != 0) {
	    assert(cep_priv->cep_trx->cep_trx == cep_priv);
	    cep_priv->cep_trx->cep_trx = 0;
	}
    }
#ifdef	NOTDEF
    if (ofi_atomic_get32( &cep_priv->cep_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
#endif	/* NOTDEF */
    fastlock_destroy( &cep_priv->cep_lck );

    free(cep_priv);

bad:
    return fc;
}

static int tofu_cep_bind(struct fid *fid, struct fid *bfid, uint64_t flags)
{
    int fc = FI_SUCCESS;
    struct tofu_cep *cep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    cep_priv = container_of(fid, struct tofu_cep, cep_fid.fid);

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
    /* if (cep_priv->cep_enb != 0) { fc = -FI_EOPBADSTATE; goto bad; } */

    assert(bfid != 0);
    switch (bfid->fclass) {
	struct tofu_cq *cq__priv;
	struct tofu_cntr *ctr_priv;
    case FI_CLASS_CQ:
	cq__priv = container_of(bfid, struct tofu_cq, cq__fid.fid);
	if (cep_priv->cep_sep->sep_dom != cq__priv->cq__dom) {
	    fc = -FI_EDOMAIN /* -FI_EINVAL */; goto bad;
	}
	switch (fid->fclass) {
	case FI_CLASS_TX_CTX:
	    if (flags & FI_SEND) {
		/*
		 * man fi_endpoint(3)
		 *   An endpoint may only be bound to a single CQ or
		 *   counter for a given type of operation.
		 *   For example, a EP may not bind to two counters both
		 *   using FI_WRITE.
		 */
		if (cep_priv->cep_send_cq != 0) {
		    fc = -FI_EBUSY; goto bad;
		}
		cep_priv->cep_send_cq = cq__priv;
		tofu_cq_ins_cep_tx(cq__priv, cep_priv);
	    }
	    break;
	case FI_CLASS_RX_CTX:
	    if (flags & FI_RECV) {
		if (cep_priv->cep_recv_cq != 0) {
		    fc = -FI_EBUSY; goto bad;
		}
		cep_priv->cep_recv_cq = cq__priv;
		tofu_cq_ins_cep_rx(cq__priv, cep_priv);
	    }
	    break;
	default:
	    fc = -FI_EINVAL; goto bad;
	}
        break;
    case FI_CLASS_CNTR:
	ctr_priv = container_of(bfid, struct tofu_cntr, ctr_fid.fid);
	if (cep_priv->cep_sep->sep_dom != ctr_priv->ctr_dom) {
	    fc = -FI_EDOMAIN /* -FI_EINVAL */; goto bad;
	}
	switch (fid->fclass) {
	case FI_CLASS_TX_CTX:
	    if (flags & FI_SEND) {
	    }
	    if (flags & FI_WRITE) {
		if (cep_priv->cep_wop_ctr != 0) {
		    fc = -FI_EBUSY; goto bad;
		}
		cep_priv->cep_wop_ctr = ctr_priv;
	    }
	    if (flags & FI_READ) {
		if (cep_priv->cep_rop_ctr != 0) {
		    fc = -FI_EBUSY; goto bad;
		}
		cep_priv->cep_rop_ctr = ctr_priv;
	    }
	    break;
	case FI_CLASS_RX_CTX:
	    if (flags & FI_RECV) {
	    }
	    if (flags & FI_REMOTE_WRITE) {
	    }
	    if (flags & FI_REMOTE_READ) {
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

/* static */ int tofu_cep_ctrl(struct fid *fid, int command, void *arg)
{
    int fc = FI_SUCCESS;
    struct tofu_cep *cep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    cep_priv = container_of(fid, struct tofu_cep, cep_fid.fid);
    if (cep_priv == 0) { }

    switch (command) {
    case FI_ENABLE:
	switch (fid->fclass) {
	case FI_CLASS_TX_CTX:
	    if (cep_priv->cep_enb != 0) {
		goto bad; /* XXX - is not an error */
	    }
	    if ( ! cep_priv->cep_send_cq ) {
	    }
#ifdef	CONF_TOFU_SHEA
	    {
		const size_t offs_ulib = sizeof (cep_priv[0]);
		tofu_imp_ulib_enab(cep_priv, offs_ulib);
	    }
#endif	/* CONF_TOFU_SHEA */
	    cep_priv->cep_enb = 1;
	    break;
	case FI_CLASS_RX_CTX:
	    if (cep_priv->cep_enb != 0) {
		goto bad; /* XXX - is not an error */
	    }
	    if ( ! cep_priv->cep_recv_cq ) {
	    }
#ifdef	CONF_TOFU_SHEA
	    {
		const size_t offs_ulib = sizeof (cep_priv[0]);
		tofu_imp_ulib_enab(cep_priv, offs_ulib);
	    }
#endif	/* CONF_TOFU_SHEA */
	    cep_priv->cep_enb = 1;
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

static struct fi_ops tofu_cep_fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_cep_close,
#ifdef	notdef
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
#else	/* notdef */
    .bind	    = tofu_cep_bind,
    .control	    = tofu_cep_ctrl,
#endif	/* notdef */
    .ops_open	    = fi_no_ops_open,
};


static int tofu_cep_getopt(
    fid_t fid,
    int level,
    int optname,
    void *optval,
    size_t *optlen
)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    fc = -FI_ENOSYS;
    return fc;
}

static int tofu_cep_setopt(
    fid_t fid,
    int level,
    int optname,
    const void *optval,
    size_t optlen
)
{
    int fc = FI_SUCCESS;
    struct tofu_cep *cep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    cep_priv = container_of(fid, struct tofu_cep, cep_fid.fid);
    if (cep_priv == 0) { }

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
	/* cep_priv->min_multi_recv = ((size_t *)optval)[0]; */
	break;
    default:
	fc = -FI_ENOPROTOOPT; goto bad;
    }

bad:
    return fc;
}

static struct fi_ops_ep tofu_cep_ops = {
    .size           = sizeof (struct fi_ops_ep),
    .cancel         = fi_no_cancel,
#ifdef	notdef
    .getopt         = fi_no_getopt,
    .setopt         = fi_no_setopt,
#else	/* notdef */
    .getopt         = tofu_cep_getopt,
    .setopt         = tofu_cep_setopt,
#endif	/* notdef */
    .tx_ctx         = fi_no_tx_ctx,
    .rx_ctx         = fi_no_rx_ctx,
    .rx_size_left   = fi_no_rx_size_left, /* deprecated */
    .tx_size_left   = fi_no_tx_size_left, /* deprecated */
};
#ifdef	NOTDEF_OPS_RMA

/* XXX to be moved to rma.c */
static struct fi_ops_rma tofu_cep_ops_rma = {
    .size	    = sizeof (struct fi_ops_rma),
    .read	    = fi_no_rma_read,
    .readv	    = fi_no_rma_readv,
    .readmsg	    = fi_no_rma_readmsg,
    .write	    = fi_no_rma_write,
    .writev	    = fi_no_rma_writev,
    .writemsg	    = fi_no_rma_writemsg,
    .inject	    = fi_no_rma_inject,
    .writedata	    = fi_no_rma_writedata,
    .injectdata	    = fi_no_rma_injectdata,
};
#endif	/* NOTDEF_OPS_RMA */
#ifdef	NOTDEF_OPS_ATM

/* XXX to be moved to atm.c */
static struct fi_ops_atomic tofu_cep_ops_atomic = {
    .size	    = sizeof (struct fi_ops_atomic),
    .write	    = fi_no_atomic_write,
    .writev	    = fi_no_atomic_writev,
    .writemsg	    = fi_no_atomic_writemsg,
    .inject	    = fi_no_atomic_inject,
    .readwrite	    = fi_no_atomic_readwrite,
    .readwritev	    = fi_no_atomic_readwritev,
    .readwritemsg   = fi_no_atomic_readwritemsg,
    .compwrite	    = fi_no_atomic_compwrite,
    .compwritev	    = fi_no_atomic_compwritev,
    .compwritemsg   = fi_no_atomic_compwritemsg,
    .writevalid	    = fi_no_atomic_writevalid,
    .readwritevalid = fi_no_atomic_readwritevalid,
    .compwritevalid = fi_no_atomic_compwritevalid,
};
#endif	/* NOTDEF_OPS_ATM */

int tofu_cep_tx_context(
    struct fid_ep *fid_sep,
    int index,
    struct fi_tx_attr *attr,
    struct fid_ep **fid_cep_tx,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep_priv;
    struct tofu_cep *cep_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid_sep != 0);
    if (fid_sep->fid.fclass != FI_CLASS_SEP) {
	fc = -FI_EINVAL; goto bad;
    }
    sep_priv = container_of(fid_sep, struct tofu_sep, sep_fid );
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "api_version %08x\n",
        sep_priv->sep_dom->dom_fab->fab_fid.api_version);

    if (attr != 0) {
	struct fi_tx_attr *prov_attr = 0; /* default */
	uint64_t user_info_mode = 0;

	fc = tofu_chck_cep_tx_attr( prov_attr, attr, user_info_mode);
	if (fc != 0) { goto bad; }
    }

#ifdef	notdef
    cep_priv = calloc(1, sizeof (cep_priv[0]));
    if (cep_priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
#else	/* notdef */
    {
	size_t msiz;
#ifdef	CONF_TOFU_SHEA
	size_t offs_ulib;
#endif	/* CONF_TOFU_SHEA */

	msiz = sizeof (cep_priv[0]);
#ifdef	CONF_TOFU_SHEA
	offs_ulib = msiz;
	msiz += tofu_imp_ulib_size();
#endif	/* CONF_TOFU_SHEA */

	cep_priv = calloc(1, msiz);
	if (cep_priv == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
#ifdef	CONF_TOFU_SHEA
	tofu_imp_ulib_init(cep_priv, offs_ulib, 0 /* rx */ , attr /* tx */ );
#endif	/* CONF_TOFU_SHEA */
    }
#endif	/* notdef */

    /* initialize cep_priv */
    {
	cep_priv->cep_sep = sep_priv;
	cep_priv->cep_idx = index;
	ofi_atomic_initialize32( &cep_priv->cep_ref, 0 );
	fastlock_init( &cep_priv->cep_lck );

	cep_priv->cep_fid.fid.fclass	= FI_CLASS_TX_CTX;
	cep_priv->cep_fid.fid.context	= context;
	cep_priv->cep_fid.fid.ops	= &tofu_cep_fi_ops;
	cep_priv->cep_fid.ops		= &tofu_cep_ops;
	cep_priv->cep_fid.cm		= &tofu_cep_ops_cm;
	cep_priv->cep_fid.msg           = &tofu_cep_ops_msg;
	cep_priv->cep_fid.rma   	= &tofu_cep_ops_rma;
	cep_priv->cep_fid.tagged	= &tofu_cep_ops_tag;
	cep_priv->cep_fid.atomic	= &tofu_cep_ops_atomic;

	/* dlist_init( &cep_priv->cep_ent ); */
	dlist_init( &cep_priv->cep_ent_sep );
	dlist_init( &cep_priv->cep_ent_cq );
	dlist_init( &cep_priv->cep_ent_ctr );
#ifdef	CONF_TOFU_RECV	/* DONE */
	dlist_init( &cep_priv->recv_tag_hd );
	dlist_init( &cep_priv->recv_msg_hd );
	dlist_init( &cep_priv->unexp_tag_hd );
	dlist_init( &cep_priv->unexp_msg_hd );
	/* cep_priv->recv_fs = 0; */
#endif	/* CONF_TOFU_RECV */
	cep_priv->cep_xop_flg = (attr == 0)? 0UL: attr->op_flags;
    }
    /* check index */
    {
	struct tofu_cep *cep_dup;
	fastlock_acquire( &sep_priv->sep_lck );
	cep_dup = tofu_sep_lup_cep_byi_unsafe(sep_priv,
		    FI_CLASS_TX_CTX, index);
	fastlock_release( &sep_priv->sep_lck );
	if (cep_dup != 0) {
	    fc = -FI_EBUSY; goto bad;
	}
    }
    tofu_sep_ins_cep_tx( cep_priv->cep_sep, cep_priv );

    /* return fid_cep */
    fid_cep_tx[0] = &cep_priv->cep_fid;
    cep_priv = 0; /* ZZZ */

bad:
    if (cep_priv != 0) {
	tofu_cep_close( &cep_priv->cep_fid.fid );
    }
    return fc;
}

int tofu_cep_rx_context(
    struct fid_ep *fid_sep,
    int index,
    struct fi_rx_attr *attr,
    struct fid_ep **fid_cep_rx,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep_priv;
    struct tofu_cep *cep_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid_sep != 0);
    if (fid_sep->fid.fclass != FI_CLASS_SEP) {
	fc = -FI_EINVAL; goto bad;
    }
    sep_priv = container_of(fid_sep, struct tofu_sep, sep_fid );
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "api_version %08x\n",
        sep_priv->sep_dom->dom_fab->fab_fid.api_version);

    if (attr != 0) {
	struct fi_info *prov_info = 0; /* default */
	uint64_t user_info_mode = 0;

	fc = tofu_chck_cep_rx_attr( prov_info, attr, user_info_mode);
	if (fc != 0) { goto bad; }
    }

#ifdef	notdef
    cep_priv = calloc(1, sizeof (cep_priv[0]));
    if (cep_priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
#else	/* notdef */
    {
	size_t msiz;
#ifdef	CONF_TOFU_SHEA
	size_t offs_ulib;
#endif	/* CONF_TOFU_SHEA */

	msiz = sizeof (cep_priv[0]);
#ifdef	CONF_TOFU_SHEA
	offs_ulib = msiz;
	msiz += tofu_imp_ulib_size();
#endif	/* CONF_TOFU_SHEA */

	cep_priv = calloc(1, msiz);
	if (cep_priv == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
#ifdef	CONF_TOFU_SHEA
	tofu_imp_ulib_init(cep_priv, offs_ulib, attr /* rx */ , 0 /* tx */ );
#endif	/* CONF_TOFU_SHEA */
    }
#endif	/* notdef */

    /* initialize cep_priv */
    {
	cep_priv->cep_sep = sep_priv;
	cep_priv->cep_idx = index;
	ofi_atomic_initialize32( &cep_priv->cep_ref, 0 );
	fastlock_init( &cep_priv->cep_lck );

	cep_priv->cep_fid.fid.fclass	= FI_CLASS_RX_CTX;
	cep_priv->cep_fid.fid.context	= context;
	cep_priv->cep_fid.fid.ops	= &tofu_cep_fi_ops;
	cep_priv->cep_fid.ops		= &tofu_cep_ops;
	cep_priv->cep_fid.cm		= &tofu_cep_ops_cm;
	cep_priv->cep_fid.msg           = &tofu_cep_ops_msg;
	cep_priv->cep_fid.rma   	= &tofu_cep_ops_rma;
	cep_priv->cep_fid.tagged	= &tofu_cep_ops_tag;
	cep_priv->cep_fid.atomic	= &tofu_cep_ops_atomic;

	/* dlist_init( &cep_priv->cep_ent ); */
	dlist_init( &cep_priv->cep_ent_sep );
	dlist_init( &cep_priv->cep_ent_cq );
	dlist_init( &cep_priv->cep_ent_ctr );
#ifdef	CONF_TOFU_RECV	/* DONE */
	dlist_init( &cep_priv->recv_tag_hd );
	dlist_init( &cep_priv->recv_msg_hd );
	dlist_init( &cep_priv->unexp_tag_hd );
	dlist_init( &cep_priv->unexp_msg_hd );
	/* cep_priv->recv_fs = 0; */
#endif	/* CONF_TOFU_RECV */
	cep_priv->cep_xop_flg = (attr == 0)? 0UL: attr->op_flags;
    }
#ifdef	CONF_TOFU_RECV	/* DONE */
    {
	/*
	 * man fi_endpoint(3)
	 *   fi_rx_attr . size
	 *     The size of the context.
	 *     The size is specified as the minimum number of receive
	 *     operations that may be posted to the endpoint
	 *     without the operation returning -FI_EAGAIN.
	 */
        FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "YI: Should be CHECK if it works!!!!\n");        
	cep_priv->recv_fs = tofu_recv_fs_create(64, 0, 0); /* XXX attr->size */
	if (cep_priv->recv_fs == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
    }
#endif	/* CONF_TOFU_RECV */
    /* check index */
    {
	struct tofu_cep *cep_dup;
	fastlock_acquire( &sep_priv->sep_lck );
	cep_dup = tofu_sep_lup_cep_byi_unsafe(sep_priv,
		    FI_CLASS_RX_CTX, index);
	fastlock_release( &sep_priv->sep_lck );
	if (cep_dup != 0) {
	    fc = -FI_EBUSY; goto bad;
	}
    }
    tofu_sep_ins_cep_rx( cep_priv->cep_sep, cep_priv );

    /* return fid_cep */
    fid_cep_rx[0] = &cep_priv->cep_fid;
    cep_priv = 0; /* ZZZ */

bad:
    if (cep_priv != 0) {
	tofu_cep_close( &cep_priv->cep_fid.fid );
    }
    return fc;
}
