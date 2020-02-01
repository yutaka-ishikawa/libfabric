/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include <pmix.h>

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
    struct tofu_domain *dom = 0;
    struct tofu_fabric *fab;

    FI_INFO(&tofu_prov, FI_LOG_DOMAIN, "in %s\n", __FILE__);
    assert(fid_fab != 0);
    R_DBG("DOM OPEN fid_fab(%p)", fid_fab);
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
    dom->dom_fid.mr            = &tofu_mr__ops;

    /* dlist_init( &dom_priv->dom_ent ); */
    /* dom_priv */
    dom->dom_fmt = (info == 0)? FI_ADDR_STR: info->addr_format;

    /* Initialization of Tofu NIC */
    {
        utofu_tni_id_t *tnis = 0;
        size_t  ntni = 0;
        size_t  ni;
        int     uc;
        const size_t mtni = sizeof (dom->tnis) / sizeof (dom->tnis[0]);

        uc = utofu_get_onesided_tnis(&tnis, &ntni);
        R_DBG("rdbgf(%x) rdbgl(%x) uc(%d) ntni(%ld)", rdbgf, rdbgl, uc, ntni);
        R_DBG0(RDBG_LEVEL1, "uc(%d) ntni(%ld)", uc, ntni);
        if (uc != UTOFU_SUCCESS) { fc = -FI_EOTHER; goto bad; }
        if (ntni > mtni) {
            ntni = mtni;
        }
        /* copy tnis[] and ntni */
        for (ni = 0; ni < ntni; ni++) {
            struct utofu_onesided_caps *cap;
            dom->tnis[ni] = tnis[ni];
            utofu_query_onesided_caps(tnis[ni], &cap);
            R_DBG0(RDBG_LEVEL2, "tnid(%d) num_stags(%d)",
                   tnis[ni], cap->num_reserved_stags);
            dom->vcqh[ni] = 0;
            dom->max_mtu = cap->max_mtu;
            dom->max_piggyback_size = cap->max_piggyback_size;
            dom->max_edata_size = cap->max_edata_size;
        }
        dom->ntni = ntni;
        /* free tnis[] */
        if (tnis != 0) {
            free(tnis); tnis = 0;
        }
    }
    /* return fid_dom */
    fid_dom[0] = &dom->dom_fid;
    dom = 0; /* ZZZ */

bad:
    if (dom != 0) {
	tofu_domain_close(&dom->dom_fid.fid );
    }
    fprintf(stderr, "%s: YI*** 2 return fc(%d)\n", __func__, fc); fflush(stderr);
    return fc;
}
