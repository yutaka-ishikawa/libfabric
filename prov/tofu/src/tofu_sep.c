/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include "tofu_addr.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int tofu_sep_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
//    struct tofu_sep *sep;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
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

/*
 * fi_ep_bind
 */
static int tofu_sep_bind(struct fid *fid, struct fid *bfid, uint64_t flags)
{
    int fc = FI_SUCCESS;
    struct tofu_sep *sep;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    sep = container_of(fid, struct tofu_sep, sep_fid.fid);
    assert(bfid != 0);
    switch (bfid->fclass) {
	struct tofu_av *av;
    case FI_CLASS_AV:
	/*
	 * man fi_endpoint(3)
	 *   fi_scalable_ep_bind
	 *     fi_scalable_ep_bind is used to associate a scalable
	 *     endpoint with an address vector.
	 */
	av = container_of(bfid, struct tofu_av, av_fid.fid);
	if (sep->sep_dom != av->av_dom) {
            //R_DBG("sep->sep_dom(%p) != av->av_dom(%p)",
            //    sep->sep_dom, av->av_dom);
	    fc = -FI_EDOMAIN /* -FI_EINVAL */; goto bad;
	}
        if (av->av_sep) {
            R_DBG("%s: av_sep has been initialized", __func__);
            fc = -FI_EBUSY; goto bad;
        }
        av->av_sep = sep;
	if (sep->sep_av_ != 0) {
            /* Scalable endpoint aleady has an address vector */
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
    struct tofu_sep *sep;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid != 0);
    sep = container_of(fid, struct tofu_sep, sep_fid.fid);

    switch (command) {
    case FI_ENABLE:
        // R_DBG("YI***** FI_ENABLE arg(%p)", arg);
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
    .bind	    = tofu_sep_bind,
    .control	    = tofu_sep_ctrl,
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
    struct tofu_domain *dom;
    struct tofu_sep *sep = 0;

    //R_DBG("YI**** fid_dom(%p)", fid_dom);
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom = container_of(fid_dom, struct tofu_domain, dom_fid);
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "api_version %08x\n",
             dom->dom_fab->fab_fid.api_version);
    if (dom->dom_sep) {
        R_DBG("%s: More than one SEP is created\n", __func__);
        fc = -FI_EINVAL; goto bad;
    }
    sep = calloc(1, sizeof(struct tofu_sep));
    if (sep == 0) {
        fc = -FI_ENOMEM; goto bad;
    }
    /* Tofu Domain points to SEP */
    dom->dom_sep = sep;
    /*
     * YYY attr (fi_ep_attr) . max_msg_size
     * YYY attr (fi_ep_attr) . msg_prefix_size
     * YYY attr (fi_ep_attr) . tx_ctx_cnt
     * YYY attr (fi_ep_attr) . rx_ctx_cnt
     */
    //tofu_impl_isep_open(sep); /* YYY attr */

    /* initialize sep */
    sep->sep_dom = dom;
    ofi_atomic_initialize32( &sep->sep_ref, 0 );
    fastlock_init(&sep->sep_lck);

    sep->sep_fid.fid.fclass    = FI_CLASS_SEP;
    sep->sep_fid.fid.context   = context;
    sep->sep_fid.fid.ops       = &tofu_sep_fi_ops;
    sep->sep_fid.ops           = &tofu_sep_ops;

    sep->sep_fid.cm            = &tofu_sep_ops_cm;
    sep->sep_fid.msg           = 0; /* fi_ops_msg */
    sep->sep_fid.rma           = 0; /* fi_ops_rma */
    sep->sep_fid.tagged        = 0; /* fi_ops_tagged */
    sep->sep_fid.atomic        = 0; /* fi_ops_atomic */

    /* dlist_init( &sep->sep_ent ); */
    dlist_init(&sep->sep_htx);
    dlist_init(&sep->sep_hrx);
#if 0
    /* first initialization */
    {
        int     i;
        for (i = 0; i < CONF_TOFU_CTXC; i++) {
            sep->sep_sctx[i].index = -1;
            sep->sep_rctx[i].index = -1;
        }
    }
#endif
    sep->sep_myvcqidx = -1;
    sep->sep_myvcqid = 0;
    sep->sep_myrank = -1;
    /* return fid_sep */
    fid_sep[0] = &sep->sep_fid;
bad:
    //R_DBG("YI**** fc(%d)", fc);
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "return %d in %s\n", fc, __FILE__);
    return fc;
}
