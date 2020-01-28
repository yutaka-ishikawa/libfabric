/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int tofu_sep_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
//    struct tofu_sep *sep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    /*
     * man fi_endpoint(3)
     *   When closing a scalable endpoint, there must be no opened
     *   transmit contexts, or receive contexts associated with the
     *   scalable endpoint.
     *   If resources are still associated with the scalable endpoint
     *   when attempting to close, the call will return -FI_EBUSY.
     */
    return fc;
}

static int tofu_sep_bind(struct fid *fid, struct fid *bfid, uint64_t flags)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    sep = container_of(fid, struct tofu_sep, sep_fid.fid);
    assert(bfid != 0);
    switch (bfid->fclass) {
	struct tofu_av *av;
    case FI_CLASS_AV:
	av = container_of(bfid, struct tofu_av, av_fid.fid);
	if (sep->sep_dom != av->av_dom) {
            R_DBG("sep->sep_dom(%p) != av_priv->av_dom(%p)",
                  sep->sep_dom, av->av_dom);
	    fc = -FI_EDOMAIN /* -FI_EINVAL */; goto bad;
	}
	/*
	 * man fi_endpoint(3)
	 *   fi_scalable_ep_bind
	 *     fi_scalable_ep_bind is used to associate a scalable
	 *     endpoint with an address vector.
	 */
	if (sep->sep_av_ != 0) {
	    fc = -FI_EBUSY; goto bad;
	}
	sep->sep_av_ = av;
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
    //struct tofu_sep *sep_priv;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    //sep_priv = container_of(fid, struct tofu_sep, sep_fid.fid);
    //if (sep_priv == 0) { }

#if 0
    switch (command) {
    case FI_ENABLE:
	break;
    default:
	fc = -FI_ENOSYS; goto bad;
    }

bad:
#endif
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
    .tx_ctx	    = tofu_ctx_tx_context,
    .rx_ctx	    = tofu_ctx_rx_context,
    .rx_size_left   = fi_no_rx_size_left, /* deprecated */
    .tx_size_left   = fi_no_tx_size_left, /* deprecated */
};

int tofu_sep_open(struct fid_domain *fid_dom,  struct fi_info *info,
                  struct fid_ep **fid_sep,   void *context)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_sep *sep_priv = 0;

    R_DBG("YI**** fid_dom(%p)", fid_dom);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom_priv = container_of(fid_dom, struct tofu_domain, dom_fid);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "api_version %08x\n",
             dom_priv->dom_fab->fab_fid.api_version);
    if (dom_priv->dom_sep) {
        R_DBG("%s: More than one SEP is created\n", __func__);
        fc = -FI_EINVAL; goto bad;
    }
    sep_priv = calloc(1, sizeof(struct tofu_sep));
    if (sep_priv == 0) {
        fc = -FI_ENOMEM; goto bad;
    }
    /* Tofu Domain points to SEP */
    dom_priv->dom_sep = sep_priv;
    /*
     * YYY attr (fi_ep_attr) . max_msg_size
     * YYY attr (fi_ep_attr) . msg_prefix_size
     * YYY attr (fi_ep_attr) . tx_ctx_cnt
     * YYY attr (fi_ep_attr) . rx_ctx_cnt
     */
    //tofu_impl_isep_open(sep_priv); /* YYY attr */

    /* initialize sep_priv */
    sep_priv->sep_dom = dom_priv;
    ofi_atomic_initialize32( &sep_priv->sep_ref, 0 );
    fastlock_init(&sep_priv->sep_lck);

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
    dlist_init(&sep_priv->sep_htx);
    dlist_init(&sep_priv->sep_hrx);
    /* first initialization */
    {
        int     i;
        for (i = 0; i < CONF_TOFU_CTXC; i++) {
            sep_priv->sep_sctx[i].index = -1;
            sep_priv->sep_rctx[i].index = -1;
        }
    }
    sep_priv->sep_vcqidx = -1;
    /* return fid_sep */
    fid_sep[0] = &sep_priv->sep_fid;
bad:
    R_DBG("YI**** fc(%d)", fc);
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "return %d in %s\n", fc, __FILE__);
    return fc;
}
