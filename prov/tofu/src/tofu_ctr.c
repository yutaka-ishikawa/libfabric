/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int tofu_cntr_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_cntr *ctr_priv;

    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    assert(fid != 0);
    ctr_priv = container_of(fid, struct tofu_cntr, ctr_fid.fid);

    if (ofi_atomic_get32( &ctr_priv->ctr_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    fastlock_destroy( &ctr_priv->ctr_lck );

    free(ctr_priv);

bad:
    return fc;
}

static struct fi_ops tofu_ctr_fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_cntr_close,
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
    .ops_open	    = fi_no_ops_open,
};

static uint64_t tofu_cntr_read(struct fid_cntr *cntr_fid)
{
    uint64_t ret;
    struct tofu_cntr *ctr_priv;

    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    assert(cntr_fid != 0);
    ctr_priv = container_of(cntr_fid, struct tofu_cntr, ctr_fid);
    if (ctr_priv == 0) { }

    ret = ofi_atomic_get64( &ctr_priv->ctr_ctr );
    return ret;
}

static uint64_t tofu_cntr_readerr(struct fid_cntr *cntr_fid)
{
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    return 0; /* YYY ofi_atomic_get64(&cntr->err) */
}

static int tofu_cntr_add(struct fid_cntr *cntr_fid, uint64_t value)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    return fc;
}

static int tofu_cntr_adderr(struct fid_cntr *cntr_fid, uint64_t value)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    return fc;
}

static int tofu_cntr_set(struct fid_cntr *cntr_fid, uint64_t value)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    return fc;
}

static int tofu_cntr_seterr(struct fid_cntr *cntr_fid, uint64_t value)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    return fc;
}

static int tofu_cntr_wait(struct fid_cntr *cntr_fid, uint64_t threshold, int timeout)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    fc = -FI_ETIMEDOUT;
    return fc;
}

static struct fi_ops_cntr tofu_ctr_ops = {
    .size	    = sizeof (struct fi_ops_cntr),
    .read	    = tofu_cntr_read,
    .readerr	    = tofu_cntr_readerr,
    .add	    = tofu_cntr_add,
    .set	    = tofu_cntr_set,
    .wait	    = tofu_cntr_wait,
    .adderr	    = tofu_cntr_adderr,
    .seterr	    = tofu_cntr_seterr,
};

static int tofu_cntr_verify_attr(
    const struct fi_cntr_attr *user_attr
)
{
    int fc = FI_SUCCESS;

    /* events: enum fi_cntr_events */
    switch (user_attr->events) {
    case FI_CNTR_EVENTS_COMP:
	break;
    default:
	fc = -FI_ENOSYS; goto bad;
    }

    /* wait_obj: enum fi_wait_obj */
    switch (user_attr->wait_obj) {
    case FI_WAIT_NONE:
    case FI_WAIT_UNSPEC:
	break;
    case FI_WAIT_SET:
    case FI_WAIT_FD:
    case FI_WAIT_MUTEX_COND:
    default:
	fc = -FI_ENOSYS; goto bad;
    }

    /* wait_set: struct fid_wait * */
	/* This field is ignored if wait_obj is not FI_WAIT_SET. */

    /* flags: uint64_t */
	/* Flags are reserved for future use, and must be set to 0. */
	if (user_attr->flags != 0) {
	    /* fc = -FI_EINVAL; goto bad; */
	}

bad:
    return fc;
}

int tofu_cntr_open(
    struct fid_domain *fid_dom,
    struct fi_cntr_attr *attr,
    struct fid_cntr **fid_ctr,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_cntr *ctr_priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom_priv = container_of(fid_dom, struct tofu_domain, dom_fid );

    if (attr != 0) {
	fc = tofu_cntr_verify_attr(attr);
	if (fc != 0) {
	    goto bad;
	}
    }

    ctr_priv = calloc(1, sizeof (ctr_priv[0]));
    if (ctr_priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* initialize ctr_priv */
    {
	ctr_priv->ctr_dom = dom_priv;
	ofi_atomic_initialize32( &ctr_priv->ctr_ref, 0 );
	fastlock_init( &ctr_priv->ctr_lck );

	ctr_priv->ctr_fid.fid.fclass    = FI_CLASS_CNTR;
	ctr_priv->ctr_fid.fid.context   = context;
	ctr_priv->ctr_fid.fid.ops       = &tofu_ctr_fi_ops;
	ctr_priv->ctr_fid.ops           = &tofu_ctr_ops;

	/* dlist_init( &ctr_priv->ctr_ent ); */
	dlist_init( &ctr_priv->ctr_htx );
	dlist_init( &ctr_priv->ctr_hrx );
	ofi_atomic_initialize64( &ctr_priv->ctr_ctr, 0 );
	ofi_atomic_initialize64( &ctr_priv->ctr_err, 0 );
    }

    /* return fid_dom */
    fid_ctr[0] = &ctr_priv->ctr_fid;
    ctr_priv = 0; /* ZZZ */

bad:
    if (ctr_priv != 0) {
	tofu_cntr_close( &ctr_priv->ctr_fid.fid );
    }

    return fc;
}
