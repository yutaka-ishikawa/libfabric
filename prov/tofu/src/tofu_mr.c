/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int tofu_mr_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_mr *mr__priv;

    FI_INFO( &tofu_prov, FI_LOG_MR, "in %s\n", __FILE__);
    assert(fid != 0);
    mr__priv = container_of(fid, struct tofu_mr, mr__fid.fid);

    if (ofi_atomic_get32( &mr__priv->mr__ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    fastlock_destroy( &mr__priv->mr__lck );

    free(mr__priv);

bad:
    return fc;
}

static struct fi_ops tofu_mr__fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_mr_close,
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
    .ops_open	    = fi_no_ops_open,
};

static int tofu_mr_reg(
    struct fid *fid,
    const void *buf,
    size_t len,
    uint64_t access,
    uint64_t offset,
    uint64_t requested_key,
    uint64_t flags,
    struct fid_mr **fid_mr_,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_mr *mr__priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_MR, "in %s\n", __FILE__);
    assert(fid != 0);
    if (fid->fclass != FI_CLASS_DOMAIN) {
	fc = -FI_EINVAL; goto bad;
    }
    dom_priv = container_of(fid, struct tofu_domain, dom_fid.fid );

    mr__priv = calloc(1, sizeof (mr__priv[0]));
    if (mr__priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* initialize mr__priv */
    {
	mr__priv->mr__dom = dom_priv;
	ofi_atomic_initialize32( &mr__priv->mr__ref, 0 );
	fastlock_init( &mr__priv->mr__lck );

	mr__priv->mr__fid.fid.fclass    = FI_CLASS_MR;
	mr__priv->mr__fid.fid.context   = context;
	mr__priv->mr__fid.fid.ops       = &tofu_mr__fi_ops;
	mr__priv->mr__fid.mem_desc      = mr__priv; /* YYY */
	mr__priv->mr__fid.key           = (uintptr_t)mr__priv; /* YYY */

	/* dlist_init( &mr__priv->mr__ent ); */
    }
    /* fi_mr_attr */
    {
	struct fi_mr_attr *attr = &mr__priv->mr__att;
	struct iovec *vecs = &mr__priv->mr__iov;

	vecs->iov_base = (void *)buf;
	vecs->iov_len = len;

	attr->mr_iov = vecs;
	attr->iov_count = 1;
	attr->access = access;
	attr->offset = offset;
	attr->requested_key = requested_key;
	attr->context = context;
	attr->auth_key_size = 0;
	attr->auth_key = 0;

	mr__priv->mr__flg = flags;
    }

    /* return fid_dom */
    fid_mr_[0] = &mr__priv->mr__fid;
    mr__priv = 0; /* ZZZ */

bad:
    if (mr__priv != 0) {
	tofu_mr_close( &mr__priv->mr__fid.fid );
    }

    return fc;
}

struct fi_ops_mr tofu_mr__ops = {
    .size	    = sizeof (struct fi_ops_mr),
#ifdef	notdef
    .reg	    = fi_no_mr_reg,
#else	/* notdef */
    .reg	    = tofu_mr_reg,
#endif	/* notdef */
    .regv	    = fi_no_mr_regv,
    .regattr	    = fi_no_mr_regattr,
};
