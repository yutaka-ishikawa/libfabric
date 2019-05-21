/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */

extern int
tofu_queue_work(struct tofu_domain *domain, void *vp_dw);

static int tofu_domain_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;

    FI_INFO( &tofu_prov, FI_LOG_DOMAIN, "in %s\n", __FILE__);
    assert(fid != 0);
    dom_priv = container_of(fid, struct tofu_domain, dom_fid.fid);

    /*
     * man fi_domain(3)
     *   fi_close
     *     All objects associated with the opened domain must be released
     *     prior to calling fi_close,
     *     otherwise the call will return -FI_EBUSY.
     */
    if (ofi_atomic_get32( &dom_priv->dom_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    fastlock_destroy( &dom_priv->dom_lck );

    // ofi_mr_map_close( &dom_priv->mr_map );

    free(dom_priv);

bad:
    return fc;
}

/*
 * for deffered work: FI_QUEUE_WORK, FI_CANCEL_WORK, and FI_FLUSH_WORK
 */
static int
tofu_domain_ctrl(struct fid *fid, int command, void *arg)
{
    int rc = FI_SUCCESS;
    struct tofu_domain *dom_priv;

    if (fid == 0) { rc = -FI_EINVAL; goto bad; }
    dom_priv = container_of(fid, struct tofu_domain, dom_fid.fid);

    switch (command) {
    case FI_QUEUE_WORK:
	rc = tofu_queue_work(dom_priv, arg);
	if (rc != 0) { goto bad; }
	break;
    case FI_CANCEL_WORK:
    case FI_FLUSH_WORK:
    default:
	rc = -FI_ENOSYS;
	goto bad;
    }

bad:
    return rc;
}


static struct fi_ops tofu_dom_fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_domain_close,
    .bind	    = fi_no_bind,
    .control	    = tofu_domain_ctrl,
    .ops_open	    = fi_no_ops_open,
};

static struct fi_ops_domain tofu_dom_ops = {
    .size	    = sizeof (struct fi_ops_domain),
    .av_open	    = tofu_av_open,
    .cq_open	    = tofu_cq_open,
    .endpoint	    = fi_no_endpoint,
    .scalable_ep    = tofu_sep_open,
    .cntr_open	    = tofu_cntr_open,
    .poll_open	    = fi_no_poll_open,
    .stx_ctx	    = fi_no_stx_context,
    .srx_ctx	    = fi_no_srx_context,
    .query_atomic   = fi_no_query_atomic,
};

/*
 * fi_domain
 */
int tofu_domain_open(
    struct fid_fabric *fid_fab,
    struct fi_info *info,
    struct fid_domain **fid_dom,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv = 0;
    struct tofu_fabric *fab_priv;

    FI_INFO( &tofu_prov, FI_LOG_DOMAIN, "in %s\n", __FILE__);
    assert(fid_fab != 0);
    fab_priv = container_of(fid_fab, struct tofu_fabric, fab_fid );
    FI_INFO( &tofu_prov, FI_LOG_DOMAIN, "api_version %08x\n",
	fab_priv->fab_fid.api_version);

    if ((info != 0) && (info->domain_attr != 0)) {
	struct fi_domain_attr *prov_attr = 0; /* default */

	fc = tofu_chck_dom_attr( prov_attr, info /* user_info */ );
	if (fc != 0) { goto bad; }
    }

    dom_priv = calloc(1, sizeof (dom_priv[0]));
    if (dom_priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* initialize dom_priv */
    {
	dom_priv->dom_fab = fab_priv;
	ofi_atomic_initialize32( &dom_priv->dom_ref, 0 );
	fastlock_init( &dom_priv->dom_lck );

	dom_priv->dom_fid.fid.fclass    = FI_CLASS_DOMAIN;
	dom_priv->dom_fid.fid.context   = context;
	dom_priv->dom_fid.fid.ops       = &tofu_dom_fi_ops;
	dom_priv->dom_fid.ops           = &tofu_dom_ops;
	dom_priv->dom_fid.mr            = &tofu_mr__ops;

	/* dlist_init( &dom_priv->dom_ent ); */

	// fc = ofi_mr_map_init( &tofu_prov,
	//   info->domain_attr->mr_mode, dom_priv->mr_map);
	// if (fc != 0) { goto bad; }
    }
    /* dom_priv */
    {
	dom_priv->dom_fmt = (info == 0)? FI_ADDR_STR: info->addr_format;
    }

    /* return fid_dom */
    fid_dom[0] = &dom_priv->dom_fid;
    dom_priv = 0; /* ZZZ */

bad:
    if (dom_priv != 0) {
	tofu_domain_close( &dom_priv->dom_fid.fid );
    }
    return fc;
}
