/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_CTR_H
#define _TOFU_CTR_H

#include "tofu_impl.h"

static inline void tofu_cntr_ins_ctx_tx(
    struct tofu_cntr *ctr_priv,
    struct tofu_ctx *ctx_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    assert(ctx_priv->ctx_fid.fid.fclass == FI_CLASS_TX_CTX);

    fastlock_acquire( &ctr_priv->ctr_lck );
    {
	if (dlist_empty( &ctx_priv->ctx_ent_ctr )) {
	    dlist_insert_tail( &ctx_priv->ctx_ent_ctr, &ctr_priv->ctr_htx );
	}
	ofi_atomic_inc32( &ctr_priv->ctr_ref );
    }
    fastlock_release( &ctr_priv->ctr_lck );
    return ;
}

static inline void tofu_cntr_rem_ctx_tx(
    struct tofu_cntr *ctr_priv,
    struct tofu_ctx *ctx_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);

    fastlock_acquire( &ctr_priv->ctr_lck );
    {
	if ( ! dlist_empty( &ctx_priv->ctx_ent_ctr ) ) {
	    dlist_remove( &ctx_priv->ctx_ent_ctr );
	    dlist_init( &ctx_priv->ctx_ent_ctr );
	}
	ofi_atomic_dec32( &ctr_priv->ctr_ref );
    }
    fastlock_release( &ctr_priv->ctr_lck );
    return ;
}

static inline void tofu_cntr_ins_ctx_rx(
    struct tofu_cntr *ctr_priv,
    struct tofu_ctx *ctx_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);
    assert(ctx_priv->ctx_fid.fid.fclass == FI_CLASS_RX_CTX);

    fastlock_acquire( &ctr_priv->ctr_lck );
    {
	if (dlist_empty( &ctx_priv->ctx_ent_ctr )) {
	    dlist_insert_tail( &ctx_priv->ctx_ent_ctr, &ctr_priv->ctr_hrx );
	}
	ofi_atomic_inc32( &ctr_priv->ctr_ref );
    }
    fastlock_release( &ctr_priv->ctr_lck );
    return ;
}

static inline void tofu_cntr_rem_ctx_rx(
    struct tofu_cntr *ctr_priv,
    struct tofu_ctx *ctx_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CNTR, "in %s\n", __FILE__);

    fastlock_acquire( &ctr_priv->ctr_lck );
    {
	if ( ! dlist_empty( &ctx_priv->ctx_ent_ctr ) ) {
	    dlist_remove( &ctx_priv->ctx_ent_ctr );
	    dlist_init( &ctx_priv->ctx_ent_ctr );
	}
	ofi_atomic_dec32( &ctr_priv->ctr_ref );
    }
    fastlock_release( &ctr_priv->ctr_lck );
    return ;
}

#endif	/* _TOFU_CTR_H */

