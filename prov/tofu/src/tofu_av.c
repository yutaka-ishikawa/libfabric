/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include "utflib.h"
#include "tofu_addr.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */
#include <string.h>	    /* for memset() */

int tofu_av_named;

extern int
tofu_impl_uri2name(const void *vuri, size_t index, struct tofu_vname *vnam);

/*
 * man fi_av(3)
 *   fi_close
 *     When closing the address vector, there must be no opened
 *     endpoints associated with the AV.
 *     If resources are still associated with the AV when attempting to
 *     close, the call will return -FI_EBUSY.
 */
static int
tofu_av_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_av *av_priv;
    int i;

    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    assert(fid != 0);
    av_priv = container_of(fid, struct tofu_av, av_fid.fid);
    if (ofi_atomic_get32( &av_priv->av_ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    /* tab */
    for (i = 0; i < CONF_TOFU_ATTR_MAX_EP_TXRX_CTX; i++) {
        if (av_priv->av_tab[i].vnm != 0) {
            free(av_priv->av_tab[i].vnm); av_priv->av_tab[i].vnm = 0;
        }
        av_priv->av_tab[i].nct = 0;
        av_priv->av_tab[i].mct = 0;
    }
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
    struct tofu_av *av;
    struct tofu_av_tab *avtp;
    size_t         idx, ic;
    uint32_t       afmt;

    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    FI_INFO(&tofu_prov, FI_LOG_AV, "count %ld flags %"PRIx64"\n", count, flags);
    assert(fid_av_ != 0);
    av = container_of(fid_av_, struct tofu_av, av_fid);
    afmt = av->av_dom->dom_fmt;
    if (afmt != FI_ADDR_STR) {
        FI_INFO(&tofu_prov, FI_LOG_AV, "Should be FT_ADDR_STR\n");
        fc = -1; goto bad;
    }

    R_DBG("addr(%p) fi_addr(%p) count(%ld)", addr, fi_addr, count);
    /*
     * It is now assumed that all contexts use the same address vector.
     * We should change av_tab[XXX] to av_tab single entry.
     *  idx = ((uint64_t)fi_addr[0]) >> (64 - av->av_rxb);
     */
    idx = 0;
    avtp = &av->av_tab[idx];
    /* fastlock_acquire(&av->av_lck); */
    fc = tofu_av_resize(avtp, count);
    /* fastlock_release(&av->av_lck); */
    if (fc != FI_SUCCESS) { goto bad; }

    /* fastlock_acquire(&av->av_lck); */
    for (ic = 0; ic < count; ic++) {
        struct tofu_vname   *vnam;
        utofu_vcq_id_t  vcqid;
	size_t          index;

	/* index */
	index = avtp->nct++;
        vnam = &avtp->vnm[index];
        fc = tofu_impl_uri2name(addr, ic, vnam);
	if (fc != FI_SUCCESS) {
	    if (fi_addr != 0) {
		fi_addr[ic] = FI_ADDR_NOTAVAIL;
	    }
	    fc = FI_SUCCESS; /* XXX ignored */
	} else if (fi_addr != 0) {
            fi_addr[ic] = index;
	}
	vnam->vpid = index;
        VNAME_TO_VCQID(vnam, vcqid);
        vnam->vcqid = vcqid;
        R_DBG("fi_addr[%ld] = %ld ==> vcqid(%lx)", ic, fi_addr[ic], vcqid);
        // R_DBG("fi_addr[%ld] = %ld ==> vcqid(%lx)", ic, fi_addr[ic], vcqid);
    }
    /* My rank must be resolved here */
    av->av_sep->sep_myrank
        = tofu_av_lookup_rank_by_vcqid(av, av->av_sep->sep_myvcqid);
    myrank = av->av_sep->sep_myrank;
    nprocs = count;
    /* fastlock_release(&av->av_lck); */
    utf_init_2(av, av->av_sep->sep_myvcqh, avtp->nct);
bad:
    return fc;
}

static int
tofu_av_remove(struct fid_av *fid_av_,  fi_addr_t *fi_addr,
               size_t count, uint64_t flags)
{
    int fc = FI_SUCCESS;
    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    return fc;
}

static int
tofu_av_lookup(struct fid_av *fid_av_,  fi_addr_t fi_addr,
               void *addr,  size_t *addrlen)
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

static const char *
tofu_av_straddr(struct fid_av *fid_av_, const void *addr,
                char *buf, size_t *len)
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

static int
tofu_av_resize(struct tofu_av_tab *at, size_t count)
{
    int fc = FI_SUCCESS;
    size_t new_mct = at->mct;
    void *new_vnm;

    new_mct = at->mct + count;
    if (new_mct == at->mct) { goto bad; /* XXX - is not an error */  }
    new_vnm = realloc(at->vnm, sizeof(struct tofu_vname)*new_mct);
    if (new_vnm == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
    /* clear */
    {
	char *bp = (char *)new_vnm + sizeof(struct tofu_vname)*at->mct;
	size_t bz = (new_mct - at->mct) * sizeof(struct tofu_vname);
	memset(bp, 0, bz);
    }
    at->mct = new_mct;
    at->vnm = new_vnm;
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
    struct tofu_domain *dom;
    struct tofu_av *av = 0;
    int     rank, np, ppn;

    FI_INFO(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom = container_of(fid_dom, struct tofu_domain, dom_fid);

    /* tofu_chck_av_attr */
    if (attr != 0) {
        /* av_type: 1 FI_AV_TABLE
         * name: FI_NAMED_AV_0 */
        { /* av_type(1) bits(8) count(0) e/n(0) name={"FI_NAMED_AV_0\n" or NULL} */
            char *cp;
            if (attr->name != NULL
                && (cp = index(attr->name, '\n')) != NULL) {
                *cp = 0;
            }
            R_DBG("av_type(%d) bits(%d) count(%ld) e/n(%ld) name=%s",
                  attr->type, attr->rx_ctx_bits, attr->count,
                  attr->ep_per_node, attr->name == 0 ? "NULL": attr->name);
        }
	fc = tofu_chck_av_attr(attr);
	if (fc != FI_SUCCESS) goto bad;
        if (attr->name && tofu_av_named == 0) {
            fc = -FI_ENOSYS;  goto bad;
        }
    }

    av = calloc(1, sizeof (av[0]));
    if (av == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
    /* initialize av */
    av->av_dom = dom;
    ofi_atomic_initialize32(&av->av_ref, 0);
    fastlock_init(&av->av_lck);
    av->av_fid.fid.fclass    = FI_CLASS_AV;
    av->av_fid.fid.context   = context;
    av->av_fid.fid.ops       = &tofu_av_fi_ops;
    av->av_fid.ops           = &tofu_av_ops;
    /* dlist_init( &av->av_ent ); */
    /* av */
    av->av_rxb = (attr == 0)? 0: attr->rx_ctx_bits;
    if (attr->name && tofu_av_named)  {
        extern struct tofu_vname *utf_get_peers(uint64_t **fi_addr, int *npp, int *ppnp, int *rnkp);
        struct tofu_vname *vnam;

        R_DBGMSG("address vector is now being registered");
        /* at this time av->av_tab cannot be allocated */
        vnam = utf_get_peers((uint64_t**) &attr->map_addr, &np, &ppn, &rank);
        if (vnam) {
            av->av_tab[0].vnm = vnam;
            av->av_tab[0].mct = np;
            av->av_tab[0].nct = np;
        }
        R_DBG("attr->map_addr=%p, av->av_tab[0].vnm=%p, nprocs=%d ppn=%d",
              attr->map_addr, vnam, np, ppn);
        {
            int i;
            uint64_t    *addr = (uint64_t*) attr->map_addr;
            char        buf[128];
            for (i = 0; i < np; i++) {
                fprintf(stderr, "\t: %ld -> %lx (%s)\n", *(addr + i), vnam[i].vcqid,
                        vcqid2string(buf, 128, vnam[i].vcqid));

            }
        }
        /* My rank and nprocs are set here */
        myrank = rank;
        // myrank = av->av_sep->sep_myrank; must be set in av_sep field
        nprocs = np;
        R_DBG("myrank(%d) nprocs(%d)", myrank, nprocs);
        /* fastlock_release(&av->av_lck); */
    }
    /* return fid_dom */
    fid_av_[0] = &av->av_fid;
    av = 0; /* clear for success */
bad:
    // R_DBG("YI fc(%d)", fc);
    if (av != 0) {
	tofu_av_close(&av->av_fid.fid);
    }
    FI_INFO(&tofu_prov, FI_LOG_AV, "fi_errno %d\n", fc);
    return fc;
}
