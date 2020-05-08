/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */

extern utofu_stadd_t    utf_mem_reg(utofu_vcq_hdl_t vcqh, void *buf, size_t size);

static int
tofu_mr_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_mr *mr_priv;

    FI_INFO(&tofu_prov, FI_LOG_MR, "in %s\n", __FILE__);
    assert(fid != 0);
    mr_priv = container_of(fid, struct tofu_mr, mr_fid.fid);

    if (ofi_atomic_get32( &mr_priv->mr_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    fastlock_destroy( &mr_priv->mr_lck );

    free(mr_priv);

bad:
    return fc;
}

static struct fi_ops tofu_mr_fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_mr_close,
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
    .ops_open	    = fi_no_ops_open,
};

/*
 * fi_mr_reg:
 *  The key in fid_mr has a stadd value in utofu.
 *  The mr is FI_MR_BASIC mode, not FI_MR_SCALABLE mode.
 *  This means that the key of a memory area is managed by Tofu provider.
 *  An application obtains the key using fi_mr_key().
 *  Unknown: what are puposes of a descriptor obtained by fi_mr_desc() ?
 *                                                      2019/04/29
 *  access : FI_REMOTE_READ, FI_REMOTE_READ|FI_REMOTE_WRITE
 */
static int
tofu_mr_reg(struct fid *fid, const void *buf,  size_t len,
                       uint64_t access, uint64_t offset,
                       uint64_t requested_key,  uint64_t flags,
                       struct fid_mr **fid_mr_, void *context)
{
    int fc = FI_SUCCESS;
    int uc = 0;
    struct tofu_domain *dom_priv;
    struct tofu_mr     *mr_priv = 0;
    utofu_stadd_t      stadd;

    FI_INFO(&tofu_prov, FI_LOG_MR, " buf(%p) len(%ld) offset(0x%lx) key(0x%lx) flags(0x%lx) context(%p) in %s\n", buf, len, offset, requested_key, flags, context, __FILE__);

    //R_DBG("buf(%p) len(%ld) offset(0x%lx) key(0x%lx) flags(0x%lx) context(%p)", buf, len, offset, requested_key, flags, context);
    assert(fid != 0);
    if (fid->fclass != FI_CLASS_DOMAIN) {
	fc = -FI_EINVAL; goto bad;
    }
    dom_priv = container_of(fid, struct tofu_domain, dom_fid.fid );

    mr_priv = calloc(1, sizeof (mr_priv[0]));
    if (mr_priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* 
     * Registering memory region.
     * For all vcqhs, we need to register the same memory region so that
     * the area are accessed by any vcqh.
     * The current implementation, only one vcqh and thus it is much easy.
     */
    stadd = utf_mem_reg((utofu_vcq_hdl_t) dom_priv->vcqh[0], (void*) buf, len);
#if 0
    uint64_t           utofu_flg = 0;
    uc = utofu_reg_mem((utofu_vcq_hdl_t )dom_priv->vcqh[0],
                       (void*) buf, len, utofu_flg, &stadd);
#endif
    if (uc != UTOFU_SUCCESS) {
        fprintf(stderr, "%s: utofu_reg_mem error uc(%d)\n", __func__, uc);
        fc = -FI_ENOMEM; goto bad;
    }
#if 0
    for (i = 0; i < dom_priv->dom_nvcq; i++) {
        unsigned int    stag = 20; /* temporal hack */
        fprintf(stderr, "YIRMA***: %s:%d vcqh(%p)\n");
        uc = utofu_reg_mem_with_stag(
                (utofu_vcq_hdl_t )dom_priv->vcqh[i],
                buf, len, stag, utofu_flg, &stadd);
        if (uc != 0) {
            fc = -FI_ENOMEM; goto bad;
        }
    }
#endif /* 0 */
    
    /* initialize mr_priv */
    {
	mr_priv->mr_dom = dom_priv;
	ofi_atomic_initialize32( &mr_priv->mr_ref, 0 );
	fastlock_init( &mr_priv->mr_lck );
	mr_priv->mr_fid.fid.fclass  = FI_CLASS_MR;
	mr_priv->mr_fid.fid.context = context;
	mr_priv->mr_fid.fid.ops     = &tofu_mr_fi_ops;
	mr_priv->mr_fid.mem_desc    = mr_priv; /* YYY */
	mr_priv->mr_fid.key         = (uintptr_t) stadd;
	/* dlist_init( &mr_priv->mr_ent ); */
    }
    /* fi_mr_attr */
    {
	struct fi_mr_attr *attr = &mr_priv->mr_att;
	struct iovec *vecs = &mr_priv->mr_iov;

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

	mr_priv->mr_flg = flags;
    }
    FI_DBG(&tofu_prov, FI_LOG_MR, " buf(%p) len(%ld) registered key(0x%lx)\n",
           buf, len, mr_priv->mr_fid.key);
    //R_DBG("YIMR_REG: buf(%p) len(%ld) registered key(lcl_stadd)(0x%lx)\n", buf, len, mr_priv->mr_fid.key);

    /* return fid_dom */
    fid_mr_[0] = &mr_priv->mr_fid;
    mr_priv = 0; /* ZZZ */
bad:
    if (mr_priv != 0) {
	tofu_mr_close( &mr_priv->mr_fid.fid );
    }
    return fc;
}

struct fi_ops_mr tofu_mr_ops = {
    .size	    = sizeof (struct fi_ops_mr),
    .reg	    = tofu_mr_reg,
    .regv	    = fi_no_mr_regv,
    .regattr	    = fi_no_mr_regattr,
};
