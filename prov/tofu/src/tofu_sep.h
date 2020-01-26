/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_SEP_H
#define _TOFU_SEP_H

#include "tofu_impl.h"

static inline void tofu_sep_ins_ctx_tx(
    struct tofu_sep *sep_priv,
    struct tofu_ctx *ctx_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(ctx_priv->ctx_fid.fid.fclass == FI_CLASS_TX_CTX);
    assert( dlist_empty( &ctx_priv->ctx_ent_sep ) != 0 );

    fastlock_acquire( &sep_priv->sep_lck );
    {
	dlist_insert_tail( &ctx_priv->ctx_ent_sep, &sep_priv->sep_htx );
	ofi_atomic_inc32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

static inline void tofu_sep_rem_ctx_tx(
    struct tofu_sep *sep_priv,
    struct tofu_ctx *ctx_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert( dlist_empty( &ctx_priv->ctx_ent_sep ) == 0 );

    fastlock_acquire( &sep_priv->sep_lck );
    {
	dlist_remove( &ctx_priv->ctx_ent_sep );
	ofi_atomic_dec32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

static inline void tofu_sep_ins_ctx_rx(
    struct tofu_sep *sep_priv,
    struct tofu_ctx *ctx_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert(ctx_priv->ctx_fid.fid.fclass == FI_CLASS_RX_CTX);
    assert( dlist_empty( &ctx_priv->ctx_ent_sep ) != 0 );

    fastlock_acquire( &sep_priv->sep_lck );
    {
	dlist_insert_tail( &ctx_priv->ctx_ent_sep, &sep_priv->sep_hrx );
	ofi_atomic_inc32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

static inline void tofu_sep_rem_ctx_rx(
    struct tofu_sep *sep_priv,
    struct tofu_ctx *ctx_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    assert( dlist_empty( &ctx_priv->ctx_ent_sep ) == 0 );

    fastlock_acquire( &sep_priv->sep_lck );
    {
	dlist_remove( &ctx_priv->ctx_ent_sep );
	ofi_atomic_dec32( &sep_priv->sep_ref );
    }
    fastlock_release( &sep_priv->sep_lck );
    return ;
}

/* get ctx_priv by index : must lock sep */
static inline struct tofu_ctx *
tofu_sep_lup_ctx_byi_unsafe(struct tofu_sep *sep_priv,
                            size_t fclass,
                            int index)
{
    struct tofu_ctx *found = 0;
    struct dlist_entry *head, *curr;

    assert((fclass == FI_CLASS_TX_CTX) || (fclass == FI_CLASS_RX_CTX));
    if (fclass == FI_CLASS_TX_CTX) {
	head = &sep_priv->sep_htx;
    } else /* if (fclass == FI_CLASS_RX_CTX) */ {
	head = &sep_priv->sep_hrx;
    }
    dlist_foreach(head, curr) {
	struct tofu_ctx *ctx_priv;

	ctx_priv = container_of(curr, struct tofu_ctx, ctx_ent_sep);
	assert(ctx_priv->ctx_sep == sep_priv);

	if (ctx_priv->ctx_idx == index) {
	    found = ctx_priv;
	    break;
	}
    }
    return found;
}

#endif	/* _TOFU_SEP_H */

