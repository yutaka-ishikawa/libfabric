/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_SEP_H
#define _TOFU_SEP_H

#include "tofu_impl.h"

static inline void tofu_sep_ins_cep_tx(
    struct tofu_sep *sep_priv,
    struct tofu_cep *cep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(cep_priv->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
    assert( dlist_empty( &cep_priv->cep_ent_sep ) != 0 );

    fastlock_acquire( &sep_priv->sep_lck );
    {
	dlist_insert_tail( &cep_priv->cep_ent_sep, &sep_priv->sep_htx );
	ofi_atomic_inc32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

static inline void tofu_sep_rem_cep_tx(
    struct tofu_sep *sep_priv,
    struct tofu_cep *cep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert( dlist_empty( &cep_priv->cep_ent_sep ) == 0 );

    fastlock_acquire( &sep_priv->sep_lck );
    {
	dlist_remove( &cep_priv->cep_ent_sep );
	ofi_atomic_dec32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

static inline void tofu_sep_ins_cep_rx(
    struct tofu_sep *sep_priv,
    struct tofu_cep *cep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(cep_priv->cep_fid.fid.fclass == FI_CLASS_RX_CTX);
    assert( dlist_empty( &cep_priv->cep_ent_sep ) != 0 );

    fastlock_acquire( &sep_priv->sep_lck );
    {
	dlist_insert_tail( &cep_priv->cep_ent_sep, &sep_priv->sep_hrx );
	ofi_atomic_inc32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

static inline void tofu_sep_rem_cep_rx(
    struct tofu_sep *sep_priv,
    struct tofu_cep *cep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert( dlist_empty( &cep_priv->cep_ent_sep ) == 0 );

    fastlock_acquire( &sep_priv->sep_lck );
    {
	dlist_remove( &cep_priv->cep_ent_sep );
	ofi_atomic_dec32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

/* get cep_priv by index : must lock sep */
static inline struct tofu_cep *
tofu_sep_lup_cep_byi_unsafe(struct tofu_sep *sep_priv,
                            size_t fclass,
                            int index)
{
    struct tofu_cep *found = 0;
    struct dlist_entry *head, *curr;

    assert((fclass == FI_CLASS_TX_CTX) || (fclass == FI_CLASS_RX_CTX));
    if (fclass == FI_CLASS_TX_CTX) {
	head = &sep_priv->sep_htx;
    } else /* if (fclass == FI_CLASS_RX_CTX) */ {
	head = &sep_priv->sep_hrx;
    }
    dlist_foreach(head, curr) {
	struct tofu_cep *cep_priv;

	cep_priv = container_of(curr, struct tofu_cep, cep_ent_sep);
	assert(cep_priv->cep_sep == sep_priv);

	if (cep_priv->cep_idx == index) {
	    found = cep_priv;
	    break;
	}
    }
    return found;
}

#endif	/* _TOFU_SEP_H */

