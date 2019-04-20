/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_AV__H
#define _TOFU_AV__H

#include "tofu_impl.h"

static inline void tofu_av_ins_sep(
    struct tofu_av *av__priv,
    struct tofu_sep *sep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    assert(sep_priv->sep_fid.fid.fclass == FI_CLASS_SEP);

    fastlock_acquire( &av__priv->av__lck );
    {
#ifdef	NOTYET
	if (dlist_empty( &sep_priv->sep_ent_av )) {
	    dlist_insert_tail( &sep_priv->sep_ent_av, &av__priv->av__hse );
	}
#endif	/* NOTYET */
	ofi_atomic_inc32( &av__priv->av__ref );
    }
    fastlock_release( &av__priv->av__lck );
    return ;
}

static inline void tofu_av_rem_sep(
    struct tofu_av *av__priv,
    struct tofu_sep *sep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    assert(sep_priv->sep_fid.fid.fclass == FI_CLASS_SEP);

    fastlock_acquire( &av__priv->av__lck );
    {
#ifdef	NOTYET
	if ( ! dlist_empty( &sep_priv->sep_ent_av ) ) {
	    dlist_remove( &sep_priv->sep_ent_av );
	    dlist_init( &sep_priv->sep_ent_av );
	}
#endif	/* NOTYET */
	ofi_atomic_dec32( &av__priv->av__ref );
    }
    fastlock_release( &av__priv->av__lck );
    return ;
}

static inline int
tofu_av_lup_tank(struct tofu_av *av__priv,  fi_addr_t fi_a,  uint64_t *tank)
{
    int fc = FI_SUCCESS;
    size_t av_idx, rx_idx;
    void *vnam;

    if (fi_a == FI_ADDR_NOTAVAIL) {
	fc = -FI_EINVAL; goto bad;
    }
    assert(av__priv->av__rxb >= 0);
    /* assert(av__priv->av_rxb <= TOFU_RX_CTX_MAX_BITS); */
    if (av__priv->av__rxb == 0) {
	rx_idx = 0;
	av_idx = fi_a;
    }
    else {
	rx_idx = ((uint64_t)fi_a) >> (64 - av__priv->av__rxb);
	/* av_idx = fi_a & rx_ctx_mask */
	av_idx = (((uint64_t)fi_a) << av__priv->av__rxb) >> av__priv->av__rxb;
    }
    if (av_idx >= av__priv->av__tab.nct) {
	fc = -FI_EINVAL; goto bad;
    }
    assert(av__priv->av__tab.tab != 0);
    vnam = (char *)av__priv->av__tab.tab + (av_idx * 16); /* XXX */

    /* get the Tofu network Address + raNK */
    fc = tofu_imp_namS_to_tank(vnam, rx_idx, tank);
    if (fc != FI_SUCCESS) { goto bad; }

bad:
    return fc;
}

#endif	/* _TOFU_AV__H */
