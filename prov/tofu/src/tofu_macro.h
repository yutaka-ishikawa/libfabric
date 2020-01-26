/* get ctx_priv by index : must lock sep */
static inline struct tofu_ctx *
tofu_sep_lup_ctx_byi_unsafe(struct tofu_sep *sep_priv,  size_t fclass,
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
