/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int tofu_sep_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    sep_priv = container_of(fid, struct tofu_sep, sep_fid.fid);

    /*
     * man fi_endpoint(3)
     *   When closing a scalable endpoint, there must be no opened
     *   transmit contexts, or receive contexts associated with the
     *   scalable endpoint.
     *   If resources are still associated with the scalable endpoint
     *   when attempting to close, the call will return -FI_EBUSY.
     */
    if (ofi_atomic_get32( &sep_priv->sep_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    {
	tofu_imp_ulib_isep_clos(sep_priv);
    }
    if (sep_priv->sep_av_ != 0) {
	tofu_av_rem_sep(sep_priv->sep_av_, sep_priv);
    }
    fastlock_destroy( &sep_priv->sep_lck );

    free(sep_priv);

bad:
    return fc;
}

static int tofu_sep_bind(struct fid *fid, struct fid *bfid, uint64_t flags)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    sep_priv = container_of(fid, struct tofu_sep, sep_fid.fid);

    assert(bfid != 0);
    switch (bfid->fclass) {
	struct tofu_av *av__priv;
    case FI_CLASS_AV:
	av__priv = container_of(bfid, struct tofu_av, av__fid.fid);
	if (sep_priv->sep_dom != av__priv->av__dom) {
	    fc = -FI_EDOMAIN /* -FI_EINVAL */; goto bad;
	}
	/*
	 * man fi_endpoint(3)
	 *   fi_scalable_ep_bind
	 *     fi_scalable_ep_bind is used to associate a scalable
	 *     endpoint with an address vector.
	 */
	if (sep_priv->sep_av_ != 0) {
	    fc = -FI_EBUSY; goto bad;
	}
	sep_priv->sep_av_ = av__priv;
	tofu_av_ins_sep(av__priv, sep_priv);
	break;
    default:
	fc = -FI_ENOSYS; goto bad;
    }

bad:
    return fc;
}

static int tofu_sep_ctrl(struct fid *fid, int command, void *arg)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    sep_priv = container_of(fid, struct tofu_sep, sep_fid.fid);
    if (sep_priv == 0) { }

    switch (command) {
    case FI_ENABLE:
	if (sep_priv->sep_av_ == 0) {
	    fc = -FI_ENOAV; goto bad;
	}
	{
	    struct dlist_entry *head, *curr;

	    /* fastlock_acquire( &sep_priv->sep_lck ); */ /* YYY */
	    head = &sep_priv->sep_htx;

            /*
             * Sender's and Receiver's CEPs are linked
             */
	    dlist_foreach(head, curr) {
		struct tofu_cep *cep_priv_tx, *cep_priv_rx;

		cep_priv_tx = container_of(curr, struct tofu_cep, cep_ent_sep);
		assert(cep_priv_tx->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
		assert(cep_priv_tx->cep_sep == sep_priv);

		cep_priv_rx = tofu_sep_lup_cep_byi_unsafe(sep_priv,
				FI_CLASS_RX_CTX, cep_priv_tx->cep_idx);
		if (cep_priv_tx->cep_trx == 0) {
		    cep_priv_tx->cep_trx = cep_priv_rx;
		}
		if ((cep_priv_rx != 0) && (cep_priv_rx->cep_trx == 0)) {
		    cep_priv_rx->cep_trx = cep_priv_tx;
		}

		fc = tofu_cep_ctrl(&cep_priv_tx->cep_fid.fid, FI_ENABLE, 0);
		if (fc != FI_SUCCESS) {
		    /* fastlock_release( &sep_priv->sep_lck ); */ /* YYY */
		    goto bad;
		}
	    }

	    head = &sep_priv->sep_hrx;

	    dlist_foreach(head, curr) {
		struct tofu_cep *cep_priv_tx, *cep_priv_rx;

		cep_priv_rx = container_of(curr, struct tofu_cep, cep_ent_sep);
		assert(cep_priv_rx->cep_fid.fid.fclass == FI_CLASS_RX_CTX);
		assert(cep_priv_rx->cep_sep == sep_priv);

		cep_priv_tx = tofu_sep_lup_cep_byi_unsafe(sep_priv,
				FI_CLASS_TX_CTX, cep_priv_rx->cep_idx);
		if (cep_priv_rx->cep_trx == 0) {
		    cep_priv_rx->cep_trx = cep_priv_tx;
		}
		if ((cep_priv_tx != 0) && (cep_priv_tx->cep_trx == 0)) {
		    cep_priv_tx->cep_trx = cep_priv_rx;
		}

		fc = tofu_cep_ctrl(&cep_priv_rx->cep_fid.fid, FI_ENABLE, 0);
		if (fc != FI_SUCCESS) {
		    /* fastlock_release( &sep_priv->sep_lck ); */ /* YYY */
		    goto bad;
		}
	    }
	    /* fastlock_release( &sep_priv->sep_lck ); */ /* YYY */
	}
	break;
    default:
	fc = -FI_ENOSYS; goto bad;
    }

bad:
    return fc;
}

static struct fi_ops tofu_sep_fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_sep_close,
#ifdef	notdef
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
#else	/* notdef */
    .bind	    = tofu_sep_bind,
    .control	    = tofu_sep_ctrl,
#endif	/* notdef */
    .ops_open	    = fi_no_ops_open,
};

static struct fi_ops_ep tofu_sep_ops = {
    .size	    = sizeof (struct fi_ops_ep),
    .cancel	    = fi_no_cancel,
    .getopt	    = fi_no_getopt,
    .setopt	    = fi_no_setopt,
#ifdef	notdef
    .tx_ctx	    = fi_no_tx_ctx,
    .rx_ctx	    = fi_no_rx_ctx,
#else	/* notdef */
    .tx_ctx	    = tofu_cep_tx_context,
    .rx_ctx	    = tofu_cep_rx_context,
#endif	/* notdef */
    .rx_size_left   = fi_no_rx_size_left, /* deprecated */
    .tx_size_left   = fi_no_tx_size_left, /* deprecated */
};

int tofu_sep_open(
    struct fid_domain *fid_dom,
    struct fi_info *info,
    struct fid_ep **fid_sep,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_sep *sep_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom_priv = container_of(fid_dom, struct tofu_domain, dom_fid );
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "api_version %08x\n",
	dom_priv->dom_fab->fab_fid.api_version);

    if (info != 0) {
	struct fi_info *prov_info = 0; /* default */
	fc = tofu_chck_ep_attr(prov_info, info);
	if (fc != 0) {
	    goto bad;
	}
    }

#ifdef	notdef
    sep_priv = calloc(1, sizeof (sep_priv[0]));
    if (sep_priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
#else	/* notdef */
    {
	size_t msiz;

	msiz = sizeof (sep_priv[0]);
	msiz += tofu_imp_ulib_isep_size();

	sep_priv = calloc(1, msiz);
	if (sep_priv == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
	/*
	 * YYY attr (fi_ep_attr) . max_msg_size
	 * YYY attr (fi_ep_attr) . msg_prefix_size
	 * YYY attr (fi_ep_attr) . tx_ctx_cnt
	 * YYY attr (fi_ep_attr) . rx_ctx_cnt
	 */
	tofu_imp_ulib_isep_open(sep_priv); /* YYY attr */
    }
#endif  /* notdef */

    /* initialize sep_priv */
    {
	sep_priv->sep_dom = dom_priv;
	ofi_atomic_initialize32( &sep_priv->sep_ref, 0 );
	fastlock_init( &sep_priv->sep_lck );

	sep_priv->sep_fid.fid.fclass    = FI_CLASS_SEP;
	sep_priv->sep_fid.fid.context   = context;
	sep_priv->sep_fid.fid.ops       = &tofu_sep_fi_ops;
	sep_priv->sep_fid.ops           = &tofu_sep_ops;

	sep_priv->sep_fid.cm            = &tofu_sep_ops_cm;
	sep_priv->sep_fid.msg           = 0; /* fi_ops_msg */
	sep_priv->sep_fid.rma           = 0; /* fi_ops_rma */
	sep_priv->sep_fid.tagged        = 0; /* fi_ops_tagged */
	sep_priv->sep_fid.atomic        = 0; /* fi_ops_atomic */

	/* dlist_init( &sep_priv->sep_ent ); */
	dlist_init( &sep_priv->sep_htx );
	dlist_init( &sep_priv->sep_hrx );
    }

    /* return fid_sep */
    fid_sep[0] = &sep_priv->sep_fid;
    sep_priv = 0; /* ZZZ */

bad:
    if (sep_priv != 0) {
	tofu_sep_close( &sep_priv->sep_fid.fid );
    }
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "return %d in %s\n", fc, __FILE__);
    return fc;
}

