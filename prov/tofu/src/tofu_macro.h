/* get cep_priv by index : must lock sep */
static inline struct tofu_cep *
tofu_sep_lup_cep_byi_unsafe(struct tofu_sep *sep_priv,  size_t fclass,
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
