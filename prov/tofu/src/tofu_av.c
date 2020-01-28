/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include <string.h>	    /* for memset() */

extern int      tofu_impl_uri2name(const void *vuri, size_t index,  void *vnam);

/*
 * man fi_av(3)
 *   fi_close
 *     When closing the address vector, there must be no opened
 *     endpoints associated with the AV.
 *     If resources are still associated with the AV when attempting to
 *     close, the call will return -FI_EBUSY.
 */
static int tofu_av_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_av *av_priv;

    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    assert(fid != 0);
    av_priv = container_of(fid, struct tofu_av, av_fid.fid);
    if (ofi_atomic_get32( &av_priv->av_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    /* tab */
    if (av_priv->av_tab.tab != 0) {
        free(av_priv->av_tab.tab); av_priv->av_tab.tab = 0;
    }
    av_priv->av_tab.nct = 0;
    av_priv->av_tab.mct = 0;
    /**/
    fastlock_destroy(&av_priv->av_lck);
    free(av_priv);
bad:
    return fc;
}

static struct fi_ops tofu_av_fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_av_close,
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
    .ops_open	    = fi_no_ops_open,
};

static int  tofu_av_resize(struct tofu_av_tab *at, size_t count);

/*
 * fi_av_insert
 */
static int
tofu_av_insert(struct fid_av *fid_av_,  const void *addr,  size_t count,
               fi_addr_t *fi_addr,  uint64_t flags, void *context)
{
    int            fc = FI_SUCCESS;
    struct tofu_av *av_priv;
    size_t         ic;
    uint32_t       afmt;

    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    FI_INFO(&tofu_prov, FI_LOG_AV, "count %ld flags %"PRIx64"\n",
	count, flags);

    assert(fid_av_ != 0);
    av_priv = container_of(fid_av_, struct tofu_av, av_fid);

    afmt = av_priv->av_dom->dom_fmt;

    /* fastlock_acquire( &av_priv->av_lck ); */
    fc = tofu_av_resize(&av_priv->av_tab, count);
    /* fastlock_release( &av_priv->av_lck ); */
    if (fc != FI_SUCCESS) { goto bad; }

    for (ic = 0; ic < count; ic++) {
	size_t index;
        struct ulib_sep_name    vnam;

	/* index */
	/* fastlock_acquire( &av_priv->av_lck ); */
	index = av_priv->av_tab.nct++;
	/* fastlock_release( &av_priv->av_lck ); */
	if (afmt == FI_ADDR_STR) {
            fc = tofu_impl_uri2name(addr, ic, (void*) &vnam);
	} else {
            FI_INFO(&tofu_prov, FI_LOG_AV, "Should be FT_ADDR_STR\n");
	    fc = -1; goto bad;
	}
	if (fc != FI_SUCCESS) {
	    if (fi_addr != 0) {
		fi_addr[ic] = FI_ADDR_NOTAVAIL;
	    }
	    fc = FI_SUCCESS; /* XXX ignored */
	} else {
	    if (fi_addr != 0) {
		fi_addr[ic] = index;
	    }
	}
	vnam.vpid = index;
	{/* copy name */
	    void *src = (void*) &vnam;
	    void *dst = (char *)av_priv->av_tab.tab + (index * 16); /* XXX */
	    memcpy(dst, src, sizeof(struct ulib_sep_name));
	}
    }

bad:
    return fc;
}

static int tofu_av_remove(
    struct fid_av *fid_av_,
    fi_addr_t *fi_addr,
    size_t count,
    uint64_t flags
)
{
    int fc = FI_SUCCESS;
    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    return fc;
}

static int tofu_av_lookup(
    struct fid_av *fid_av_,
    fi_addr_t fi_addr,
    void *addr,
    size_t *addrlen
)
{
    int fc = FI_SUCCESS;
    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    /*
     * man fi_av(3)
     *   fi_av_lookup
     *     The returned address is the same format as those stored by the AV.
     */
    fc = -FI_EINVAL;
    return fc;
}

static const char * tofu_av_straddr(
    struct fid_av *fid_av_,
    const void *addr,
    char *buf,
    size_t *len
)
{
    size_t bsz = *len;
    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    /*
     * man fi_av(3)
     *   fi_av_straddr
     *     The specified address must be of the same format as
     *     those stored by the AV, though the address itself is
     *     not required to have been inserted.
     */
    snprintf(buf, bsz, "mpi-world-rank-0");
    len[0] = 16 + 1;
    return buf;
}

static int tofu_av_resize(struct tofu_av_tab *at, size_t count)
{
    int fc = FI_SUCCESS;
    size_t new_mct = at->mct;
    void *new_tab;

    if (new_mct == 0) { assert(at->nct == 0); new_mct = 1; }
    while (new_mct < (at->nct + count)) {
	new_mct = (new_mct * 2);
    }
    if (new_mct == at->mct) {
	goto bad; /* XXX - is not an error */
    }
    new_tab = realloc(at->tab, new_mct * 16);
    if (new_tab == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
    /* clear */
    {
	char *bp = (char *)new_tab + (at->mct * 16);
	size_t bz = (new_mct - at->mct) * 16;
	memset(bp, 0, bz);
    }
    at->mct = new_mct;
    at->tab = new_tab;
bad:
    return fc;
}

static struct fi_ops_av tofu_av_ops = {
    .size	    = sizeof (struct fi_ops_av),
    .insert	    = tofu_av_insert,
    .insertsvc	    = fi_no_av_insertsvc,
    .insertsym	    = fi_no_av_insertsym,
    .remove	    = tofu_av_remove,
    .lookup	    = tofu_av_lookup,
    .straddr	    = tofu_av_straddr,
};

int
tofu_av_open(struct fid_domain *fid_dom, struct fi_av_attr *attr,
             struct fid_av **fid_av_, void *context)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_av *av_priv = 0;

    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom_priv = container_of(fid_dom, struct tofu_domain, dom_fid );

    /* tofu_chck_av_attr */
    if (attr != 0) {
        fprintf(stderr,
                "%s():%d\tav_type %d bits %d count %ld e/n %ld name %p\n",
                __func__, __LINE__,
                attr->type, attr->rx_ctx_bits, attr->count,
                attr->ep_per_node, attr->name);
	fc = tofu_chck_av_attr(attr);
	if (fc != FI_SUCCESS) { goto bad; }
    }

    av_priv = calloc(1, sizeof (av_priv[0]));
    if (av_priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* initialize av_priv */
    {
	av_priv->av_dom = dom_priv;
	ofi_atomic_initialize32(&av_priv->av_ref, 0);
	fastlock_init(&av_priv->av_lck);
	av_priv->av_fid.fid.fclass    = FI_CLASS_AV;
	av_priv->av_fid.fid.context   = context;
	av_priv->av_fid.fid.ops       = &tofu_av_fi_ops;
	av_priv->av_fid.ops           = &tofu_av_ops;
	/* dlist_init( &av_priv->av_ent ); */
    }
    /* av_priv */
    {
	av_priv->av_rxb = (attr == 0)? 0: attr->rx_ctx_bits;
    }
    R_DBG("YI************ av_priv->av_rxb(%d)", av_priv->av_rxb);

    /* return fid_dom */
    fid_av_[0] = &av_priv->av_fid;
    av_priv = 0; /* ZZZ */

bad:
    R_DBG("YI fc(%d)", fc);
    if (av_priv != 0) {
	tofu_av_close(&av_priv->av_fid.fid);
    }
    FI_INFO(&tofu_prov, FI_LOG_AV, "fi_errno %d\n", fc);
    return fc;
}
