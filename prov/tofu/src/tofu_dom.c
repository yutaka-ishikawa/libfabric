/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include <pmix.h>

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */

extern struct tofu_vname *
utf_get_peers(uint64_t **fi_addr, int *npp, int *ppnp, int *rnkp);

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
    tfi_utf_finalize(dom_priv->tinfo);
    if (ofi_atomic_get32( &dom_priv->dom_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    fastlock_destroy( &dom_priv->dom_lck );

    // ofi_mr_map_close( &dom_priv->mr_map );

    free(dom_priv);

bad:
    return fc;
    //ofi_atomic_add32(&dom_priv->dom_ref, 128);
    //ofi_atomic_sub32(&dom_priv->dom_ref, 128);
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

/*
 * fi_endpoint is not available
 *      MPICH_CH4_OPF_ENABLE_SCALABLE_ENDPOINTS must be set
 */
int
tofu_endpoint(struct fid_domain *domain, struct fi_info *info,
              struct fid_ep **ep, void *context)
{
    fprintf(stdout, "MPI error: MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1 must be set.\n");
    fprintf(stderr, "MPI error: MPICH_CH4_OFI_ENABLE_SCALABLE_ENDPOINTS=1 must be set.\n");
    fflush(NULL);
    return -FI_ENOSYS;
}

static struct fi_ops_domain tofu_dom_ops = {
    .size	    = sizeof (struct fi_ops_domain),
    .av_open	    = tofu_av_open,
    .cq_open	    = tofu_cq_open,
//    .endpoint	    = fi_no_endpoint,
    .endpoint	    = tofu_endpoint,
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
int
tofu_domain_open(struct fid_fabric *fid_fab, struct fi_info *info,
    struct fid_domain **fid_dom, void *context)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom = 0;
    struct tofu_fabric *fab;

    FI_INFO(&tofu_prov, FI_LOG_DOMAIN, "in %s\n", __FILE__);
    assert(fid_fab != 0);
    //R_DBG("DOM OPEN fid_fab(%p)", fid_fab);
    fab = container_of(fid_fab, struct tofu_fabric, fab_fid );
    FI_INFO(&tofu_prov, FI_LOG_DOMAIN, "api_version %08x\n",
	fab->fab_fid.api_version);

    if ((info != 0) && (info->domain_attr != 0)) {
	struct fi_domain_attr *prov_attr = 0; /* default */
	fc = tofu_chck_dom_attr( prov_attr, info /* user_info */ );
	if (fc != 0) { goto bad; }
    }
    dom = calloc(1, sizeof (dom[0]));
    if (dom == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* initialize dom */
    dom->dom_fab = fab;
    ofi_atomic_initialize32(&dom->dom_ref, 0);
    fastlock_init(&dom->dom_lck);

    dom->dom_fid.fid.fclass    = FI_CLASS_DOMAIN;
    dom->dom_fid.fid.context   = context;
    dom->dom_fid.fid.ops       = &tofu_dom_fi_ops;
    dom->dom_fid.ops           = &tofu_dom_ops;
    dom->dom_fid.mr            = &tofu_mr_ops;

    dom->dom_fmt = (info == 0)? FI_ADDR_STR: info->addr_format;
    dom->myvcqh  = utf_info.vcqh;
    dom->myvcqid = utf_info.vcqid;
    dom->tinfo   = utf_info.tinfo;
    dom->myrank  = utf_info.myrank;
    dom->mynrnk  = utf_info.mynrnk;
    dom->ntni    = utf_info.ntni;
    dom->max_mtu = utf_info.max_mtu;
    dom->max_piggyback_size = utf_info.max_piggyback_size;
    dom->max_edata_size     = utf_info.max_edata_size;

    fprintf(stderr, "\t dom->tinfo(%p)\n", dom->tinfo);
    utf_cqtab_show();
    /* return fid_dom */
    fid_dom[0] = &dom->dom_fid;
    dom = 0; /* ZZZ */
bad:
    if (dom != 0) {
	tofu_domain_close(&dom->dom_fid.fid);
    }
    //fprintf(stderr, "%s: YI*** 2 return fc(%d)\n", __func__, fc); fflush(stderr);
    return fc;
}
