/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_debug.h"
#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int
tofu_mr_close(struct fid *fid)
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

/*
 * fi_mr_reg:
 *  The key in fid_mr has a stadd value in utofu.
 *  The mr is FI_MR_BASIC mode, not FI_MR_SCALABLE mode.
 *  This means that the key of a memory area is managed by Tofu provider.
 *  An application obtains the key using fi_mr_key().
 *  Unknown: what are puposes of a descriptor obtained by fi_mr_desc() ?
 *  2019/04/29
 */
static int
tofu_mr_reg(struct fid *fid, const void *buf,  size_t len,
                       uint64_t access, uint64_t offset,
                       uint64_t requested_key,  uint64_t flags,
                       struct fid_mr **fid_mr_, void *context)
{
    R_DBG("tofu_mr_reg how we will implement. fid(%p)\n", fid);
    return -1;
#if 0
    int fc = FI_SUCCESS;
    int uc;
    struct tofu_domain *dom_priv;
    struct tofu_mr     *mr__priv = 0;
    utofu_stadd_t      stadd;
    uint64_t           utofu_flg = 0;

    FI_INFO( &tofu_prov, FI_LOG_MR, " buf(%p) len(%ld) offset(0x%lx) key(0x%lx) flags(0x%lx) context(%p) in %s\n", buf, len, offset, requested_key, flags, context, __FILE__);
    assert(fid != 0);
    if (fid->fclass != FI_CLASS_DOMAIN) {
	fc = -FI_EINVAL; goto bad;
    }
    dom_priv = container_of(fid, struct tofu_domain, dom_fid.fid );

    mr__priv = calloc(1, sizeof (mr__priv[0]));
    if (mr__priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* 
     * Registering memory region.
     * For all vcqhs, we need to register the same memory region so that
     * the area are accessed by any vcqh.
     * The current implementation, only one vcqh and thus it is much easy.
     */
    uc = utofu_reg_mem((utofu_vcq_hdl_t )dom_priv->dom_vcqh[0],
                       (void*) buf, len, utofu_flg, &stadd);
    if (uc != UTOFU_SUCCESS) {
        fprintf(stderr, "%s: utofu_reg_mem error uc(%d)\n", __func__, uc);
        fc = -FI_ENOMEM; goto bad;
    }
#if 0
    for (i = 0; i < dom_priv->dom_nvcq; i++) {
        unsigned int    stag = 20; /* temporal hack */
        fprintf(stderr, "YIRMA***: %s:%d vcqh(%p)\n");
        uc = utofu_reg_mem_with_stag(
                (utofu_vcq_hdl_t )dom_priv->dom_vcqh[i],
                buf, len, stag, utofu_flg, &stadd);
        if (uc != 0) {
            fc = -FI_ENOMEM; goto bad;
        }
    }
#endif /* 0 */
    
    /* initialize mr__priv */
    {
	mr__priv->mr__dom = dom_priv;
	ofi_atomic_initialize32( &mr__priv->mr__ref, 0 );
	fastlock_init( &mr__priv->mr__lck );

	mr__priv->mr__fid.fid.fclass    = FI_CLASS_MR;
	mr__priv->mr__fid.fid.context   = context;
	mr__priv->mr__fid.fid.ops       = &tofu_mr__fi_ops;
	mr__priv->mr__fid.mem_desc      = mr__priv; /* YYY */
	mr__priv->mr__fid.key           = (uintptr_t) stadd;
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
    FI_DBG(&tofu_prov, FI_LOG_MR, " buf(%p) len(%ld) registered key(0x%lx)\n",
           buf, len, mr__priv->mr__fid.key);
    /*printf("%d:YIMR_REG: buf(%p) len(%ld) registered key(lcl_stadd)(0x%lx)\n",
      mypid, buf, len, mr__priv->mr__fid.key); fflush(stdout);*/

    /* return fid_dom */
    fid_mr_[0] = &mr__priv->mr__fid;
    mr__priv = 0; /* ZZZ */
bad:
    if (mr__priv != 0) {
	tofu_mr_close( &mr__priv->mr__fid.fid );
    }

    return fc;
#endif
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
