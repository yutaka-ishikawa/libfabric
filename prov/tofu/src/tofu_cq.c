/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */


static int tofu_cq_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_cq *cq__priv;

    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid != 0);
    cq__priv = container_of(fid, struct tofu_cq, cq__fid.fid);

    if (ofi_atomic_get32( &cq__priv->cq__ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    if (cq__priv->cq__ccq != 0) {
	tofu_ccirq_free(cq__priv->cq__ccq); cq__priv->cq__ccq = 0;
    }
    fastlock_destroy( &cq__priv->cq__lck );

    free(cq__priv);

bad:
    return fc;
}

static struct fi_ops tofu_cq__fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_cq_close,
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
    .ops_open	    = fi_no_ops_open,
};

static ssize_t tofu_cq_read(
    struct fid_cq *fid_cq,
    void *buf,
    size_t count
)
{
    ssize_t ret = 0;
    struct tofu_cq *cq__priv;
    ssize_t ment = (ssize_t)count, ient;

    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid_cq != 0);
    cq__priv = container_of(fid_cq, struct tofu_cq, cq__fid);
    if (cq__priv == 0) { }
#ifdef	NOTDEF
    {
	static uint32_t called = 0;
	ret = ((called++ & 0x7) == 0x7) ? -FI_EOTHER: -FI_EAGAIN;
    }
#else	/* NOTDEF */

    fastlock_acquire( &cq__priv->cq__lck );

    if (ofi_cirque_isempty( cq__priv->cq__ccq )) {
	ret = -FI_EAGAIN; goto bad;
    }

    if (ment > ofi_cirque_usedcnt( cq__priv->cq__ccq )) {
	ment = ofi_cirque_usedcnt( cq__priv->cq__ccq );
    }

    for (ient = 0; ient < ment; ient++) {
	struct fi_cq_tagged_entry *comp;

	/* get an entry pointed by r.p. */
	comp = ofi_cirque_head( cq__priv->cq__ccq );
	assert(comp != 0);

	/* copy */
        fprintf(stderr, "YI********** %s in %s: comp[0].op_context(%p)\n", __func__, __FILE__, comp[0].op_context); fflush(stderr);
	((struct fi_cq_tagged_entry *)buf)[ient] = comp[0];

	/* advance r.p. by one  */
	ofi_cirque_discard( cq__priv->cq__ccq );
    }
    ret = ient;
    assert(ret > 0);

bad:
    fastlock_release( &cq__priv->cq__lck );
#endif	/* NOTDEF */
    return ret;
}

static struct fi_ops_cq tofu_cq__ops = {
    .size	    = sizeof (struct fi_ops_cq),
#ifdef	notdef
    .read	    = fi_no_cq_read,
#else	/* notdef */
    .read	    = tofu_cq_read,
#endif	/* notdef */
    .readfrom	    = fi_no_cq_readfrom,
    .readerr	    = fi_no_cq_readerr,
    .sread	    = fi_no_cq_sread,
    .sreadfrom	    = fi_no_cq_sreadfrom,
    .signal	    = fi_no_cq_signal,
    .strerror	    = fi_no_cq_strerror,
};

int tofu_cq_open(
    struct fid_domain *fid_dom,
    struct fi_cq_attr *attr,
    struct fid_cq **fid_cq_,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_cq *cq__priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom_priv = container_of(fid_dom, struct tofu_domain, dom_fid );

    if (attr != 0) {
	FI_INFO( &tofu_prov, FI_LOG_CQ, "Requested: FI_CQ_FORMAT_%s\n",
	    (attr->format == FI_CQ_FORMAT_UNSPEC)?  "UNSPEC":
	    (attr->format == FI_CQ_FORMAT_CONTEXT)? "CONTEXT":
	    (attr->format == FI_CQ_FORMAT_MSG)?     "MSG":
	    (attr->format == FI_CQ_FORMAT_DATA)?    "DATA":
	    (attr->format == FI_CQ_FORMAT_TAGGED)?  "TAGGED": "<unknown>");
	fc = tofu_chck_cq_attr(attr);
	if (fc != 0) {
	    goto bad;
	}
    }

    cq__priv = calloc(1, sizeof (cq__priv[0]));
    if (cq__priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
    /* initialize cq__priv */
    {
	cq__priv->cq__dom = dom_priv;
	ofi_atomic_initialize32( &cq__priv->cq__ref, 0 );
	fastlock_init( &cq__priv->cq__lck );

	cq__priv->cq__fid.fid.fclass    = FI_CLASS_CQ;
	cq__priv->cq__fid.fid.context   = context;
	cq__priv->cq__fid.fid.ops       = &tofu_cq__fi_ops;
	cq__priv->cq__fid.ops           = &tofu_cq__ops;

	/* dlist_init( &cq__priv->cq__ent ); */
	dlist_init( &cq__priv->cq__htx );
	dlist_init( &cq__priv->cq__hrx );
    }

    /* tofu_comp_cirq */
    {
	/* YYY fi_cq_attr . size */
	cq__priv->cq__ccq = tofu_ccirq_create( 256 /* YYY */ );
	if (cq__priv->cq__ccq == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
    }

    /* return fid_dom */
    fid_cq_[0] = &cq__priv->cq__fid;
    cq__priv = 0; /* ZZZ */

bad:
    if (cq__priv != 0) {
	tofu_cq_close( &cq__priv->cq__fid.fid );
    }

    return fc;
}
