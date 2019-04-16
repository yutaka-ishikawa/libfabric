/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include <string.h>	    /* for memset() */


static int tofu_av_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_av *av__priv;

    FI_INFO( &tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    assert(fid != 0);
    av__priv = container_of(fid, struct tofu_av, av__fid.fid);

    /*
     * man fi_av(3)
     *   fi_close
     *     When closing the address vector, there must be no opened
     *     endpoints associated with the AV.
     *     If resources are still associated with the AV when attempting to
     *     close, the call will return -FI_EBUSY.
     */
    if (ofi_atomic_get32( &av__priv->av__ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    /* tab */
    {
	if (av__priv->av__tab.tab != 0) {
	    free(av__priv->av__tab.tab); av__priv->av__tab.tab = 0;
	}
	av__priv->av__tab.nct = 0;
	av__priv->av__tab.mct = 0;
    }
    fastlock_destroy( &av__priv->av__lck );

    free(av__priv);

bad:
    return fc;
}

static struct fi_ops tofu_av__fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_av_close,
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
    .ops_open	    = fi_no_ops_open,
};

static int  tofu_av_resize(struct tofu_av_tab *at, size_t count);

static int tofu_av_insert(
    struct fid_av *fid_av_,
    const void *addr,
    size_t count,
    fi_addr_t *fi_addr,
    uint64_t flags,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_av *av__priv;
    size_t ic;
    uint32_t afmt;

    FI_INFO( &tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    FI_INFO( &tofu_prov, FI_LOG_AV, "count %ld flags %"PRIx64"\n",
	count, flags);

    assert(fid_av_ != 0);
    av__priv = container_of(fid_av_, struct tofu_av, av__fid);

    afmt = av__priv->av__dom->dom_fmt;

    /* fastlock_acquire( &av__priv->av__lck ); */
    fc = tofu_av_resize(&av__priv->av__tab, count);
    /* fastlock_release( &av__priv->av__lck ); */
    if (fc != FI_SUCCESS) { goto bad; }

    for (ic = 0; ic < count; ic++) {
	size_t index;
	uint64_t vnam[2]; /* XXX 16 */

	/* index */
	/* fastlock_acquire( &av__priv->av__lck ); */
	index = av__priv->av__tab.nct++;
	/* fastlock_release( &av__priv->av__lck ); */
	if (afmt == FI_ADDR_STR) {
            fprintf(stderr, "YI*****addr = %s\n", (char*)addr); fflush(stderr);
            fc = tofu_imp_str_uri_to_name(addr, ic, vnam);
	} else {
            FI_INFO(&tofu_prov, FI_LOG_AV, "Should be FT_ADDR_STR\n");
	    fc = -1; goto bad;
	}
	if (fc != FI_SUCCESS) {
	    vnam[0] = vnam[1] = 0;
	    if (fi_addr != 0) {
		fi_addr[ic] = FI_ADDR_NOTAVAIL;
	    }
	    fc = FI_SUCCESS; /* XXX ignored */
	}
	else {
	    if (fi_addr != 0) {
		fi_addr[ic] = index;
	    }
	}
	/* copy name */
	{
	    void *src = vnam;
	    void *dst = (char *)av__priv->av__tab.tab + (index * 16); /* XXX */
	    memcpy(dst, src, 16);
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
    FI_INFO( &tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
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
    FI_INFO( &tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
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
    FI_INFO( &tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
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
if (0) {
printf("\t%3ld : mct %6ld %c= %6ld\n", count,
at->mct, (at->mct == new_mct)? '=': '!', new_mct);
}

    if (new_mct == at->mct) {
	goto bad; /* XXX - is not an error */
    }

    new_tab = realloc(at->tab, new_mct * 16);
    if (new_tab == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* clear */
    /* if (new_mct - at->mct) */ {
	char *bp = (char *)new_tab + (at->mct * 16);
	size_t bz = (new_mct - at->mct) * 16;
	memset(bp, 0, bz);
    }
if (0) {
printf("nct %6ld mct %6ld %6ld tab %16p %c= %16p\n", at->nct, at->mct, new_mct,
at->tab, (at->tab == new_tab)? '=': '!', new_tab);
}
    at->mct = new_mct;
    at->tab = new_tab;

bad:
    return fc;
}

static struct fi_ops_av tofu_av__ops = {
    .size	    = sizeof (struct fi_ops_av),
    .insert	    = tofu_av_insert,
    .insertsvc	    = fi_no_av_insertsvc,
    .insertsym	    = fi_no_av_insertsym,
    .remove	    = tofu_av_remove,
    .lookup	    = tofu_av_lookup,
    .straddr	    = tofu_av_straddr,
};

int tofu_av_open(
    struct fid_domain *fid_dom,
    struct fi_av_attr *attr,
    struct fid_av **fid_av_,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_av *av__priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
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

    av__priv = calloc(1, sizeof (av__priv[0]));
    if (av__priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }

    /* initialize av__priv */
    {
	av__priv->av__dom = dom_priv;
	ofi_atomic_initialize32( &av__priv->av__ref, 0 );
	fastlock_init( &av__priv->av__lck );

	av__priv->av__fid.fid.fclass    = FI_CLASS_AV;
	av__priv->av__fid.fid.context   = context;
	av__priv->av__fid.fid.ops       = &tofu_av__fi_ops;
	av__priv->av__fid.ops           = &tofu_av__ops;

	/* dlist_init( &av__priv->av__ent ); */
    }
    /* av__priv */
    {
	av__priv->av__rxb = (attr == 0)? 0: attr->rx_ctx_bits;
    }

    /* return fid_dom */
    fid_av_[0] = &av__priv->av__fid;
    av__priv = 0; /* ZZZ */

bad:
    if (av__priv != 0) {
	tofu_av_close( &av__priv->av__fid.fid );
    }
    FI_INFO( &tofu_prov, FI_LOG_AV, "fi_errno %d\n", fc);
    return fc;
}
