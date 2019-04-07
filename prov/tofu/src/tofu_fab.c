/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <string.h>	    /* for strcasecmp() */
#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int tofu_fabric_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_fabric *fab_priv;

    FI_INFO( &tofu_prov, FI_LOG_FABRIC, "in %s\n", __FILE__);
    assert(fid != 0);
    fab_priv = container_of(fid, struct tofu_fabric, fab_fid.fid);

    if (ofi_atomic_get32( &fab_priv->fab_ref ) ) {
	fc = -FI_EBUSY; goto bad;
    }

    fastlock_destroy( &fab_priv->fab_lck );

    free(fab_priv);

bad:
    return fc;
}

static struct fi_ops tofu_fab_fi_ops = {
    .size	= sizeof (struct fi_ops),
    .close	= tofu_fabric_close,
    .bind	= fi_no_bind,
    .control	= fi_no_control,
    .ops_open	= fi_no_ops_open,
};

static struct fi_ops_fabric tofu_fab_ops = {
    .size       = sizeof (struct fi_ops_fabric),
    .domain     = tofu_domain_open,
    .passive_ep = fi_no_passive_ep,
    .eq_open    = fi_no_eq_open,
    .wait_open  = fi_no_wait_open,
    .trywait    = fi_no_trywait,
};

int tofu_fabric_open(
    struct fi_fabric_attr *attr,
    struct fid_fabric **fid,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_fabric *fab_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_FABRIC, "in %s ; version %08x\n",
	__FILE__, (attr == 0)? -1U: attr->api_version);
    if (attr != 0) {
	if ((attr->name != 0) && (strcasecmp(attr->name, "tofu") != 0)) {
	    fc = -FI_ENODATA; goto bad;
	}
	{
	    struct fi_fabric_attr *prov_attr = 0; /* default */
	    fc = tofu_chck_fab_attr( prov_attr, attr /* user_attr */ );
	    if (fc != 0) { goto bad; }
	}
    }

    fab_priv = calloc(1, sizeof (fab_priv[0]));
    if (fab_priv == 0) { fc = -FI_ENOMEM; goto bad; }

    /* initialize fab_priv */
    {
	fastlock_init( &fab_priv->fab_lck );
	ofi_atomic_initialize32( &fab_priv->fab_ref, 0 );

	fab_priv->fab_fid.fid.fclass    = FI_CLASS_FABRIC;
	fab_priv->fab_fid.fid.context   = context;
	fab_priv->fab_fid.fid.ops       = &tofu_fab_fi_ops;
	fab_priv->fab_fid.ops           = &tofu_fab_ops;
	/* fab_priv->fab_fid.api_version   = FI_VERSION(0,0); */

	/* dlist_init( &fab_priv->fab_ent ); */
    }

    /* return fid */
    fid[0] = &fab_priv->fab_fid;
    fab_priv = 0; /* ZZZ */

bad:
    if (fab_priv != 0) {
	tofu_fabric_close( &fab_priv->fab_fid.fid );
    }
    return fc;
}
