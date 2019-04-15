/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_CQ__H
#define _TOFU_CQ__H

#include "tofu_impl.h"

static inline void tofu_cq_ins_cep_tx(
    struct tofu_cq *cq__priv,
    struct tofu_cep *cep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(cep_priv->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
    assert( dlist_empty( &cep_priv->cep_ent_cq ) != 0 );

    fastlock_acquire( &cq__priv->cq__lck );
    {
	dlist_insert_tail( &cep_priv->cep_ent_cq, &cq__priv->cq__htx );
	ofi_atomic_inc32( &cq__priv->cq__ref );
    }
    fastlock_release( &cq__priv->cq__lck );
    return ;
}

static inline void tofu_cq_rem_cep_tx(
    struct tofu_cq *cq__priv,
    struct tofu_cep *cep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert( dlist_empty( &cep_priv->cep_ent_cq ) == 0 );

    fastlock_acquire( &cq__priv->cq__lck );
    {
	dlist_remove( &cep_priv->cep_ent_cq );
	ofi_atomic_dec32( &cq__priv->cq__ref );
    }
    fastlock_release( &cq__priv->cq__lck );
    return ;
}

static inline void tofu_cq_ins_cep_rx(
    struct tofu_cq *cq__priv,
    struct tofu_cep *cep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(cep_priv->cep_fid.fid.fclass == FI_CLASS_RX_CTX);
    assert( dlist_empty( &cep_priv->cep_ent_cq ) != 0 );

    fastlock_acquire( &cq__priv->cq__lck );
    {
	dlist_insert_tail( &cep_priv->cep_ent_cq, &cq__priv->cq__hrx );
	ofi_atomic_inc32( &cq__priv->cq__ref );
    }
    fastlock_release( &cq__priv->cq__lck );
    return ;
}

static inline void tofu_cq_rem_cep_rx(
    struct tofu_cq *cq__priv,
    struct tofu_cep *cep_priv
)
{
    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert( dlist_empty( &cep_priv->cep_ent_cq ) == 0 );

    fastlock_acquire( &cq__priv->cq__lck );
    {
	dlist_remove( &cep_priv->cep_ent_cq );
	ofi_atomic_dec32( &cq__priv->cq__ref );
    }
    fastlock_release( &cq__priv->cq__lck );
    return ;
}

static inline int tofu_cq_comp_tagged(
    void *vp_cq__priv /* struct tofu_cq *cq__priv */ ,
    const struct fi_cq_tagged_entry *cq_e
)
{
    int fc = FI_SUCCESS;
    struct tofu_cq *cq__priv = vp_cq__priv;
    struct fi_cq_tagged_entry *comp;

    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    fastlock_acquire( &cq__priv->cq__lck );

    assert(cq__priv->cq__ccq != 0);
    if (ofi_cirque_isfull( cq__priv->cq__ccq )) {
	FI_DBG(&tofu_prov, FI_LOG_CQ, "comp cirq  FULL\n");
	fc = -FI_EAGAIN; goto bad;
    }

    /* get an entry pointed by w.p. */
    comp = ofi_cirque_tail( cq__priv->cq__ccq );
    assert(comp != 0);

    FI_INFO( &tofu_prov, FI_LOG_CQ, "context %p\n", cq_e->op_context);
    fprintf(stderr, "YI****** comp(%p) cq_e(%p) cq__ccq(%p) \n", comp, cq_e, cq__priv->cq__ccq);
    comp[0] = cq_e[0]; /* structure copy */
    fprintf(stderr, "YI****** 1 \n"); fflush(stderr);
    
    /* advance w.p. by one  */
    ofi_cirque_commit( cq__priv->cq__ccq );
    fprintf(stderr, "YI****** 2\n"); fflush(stderr);

bad:
    fastlock_release( &cq__priv->cq__lck );
    FI_INFO( &tofu_prov, FI_LOG_CQ, "return %d\n", fc);
    return fc;
}

#endif	/* _TOFU_CQ__H */
