/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */

//extern int ulib_toqc_prog_ackd(struct ulib_toqc *toqc);
//extern int ulib_toqc_prog_tcqd(struct ulib_toqc *toqc);

/*
 * fi_close
 */
static int
tofu_cq_close(struct fid *fid)
{
    int fc = FI_SUCCESS;

    FI_INFO(&tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid != 0);
    return fc;
}

static struct fi_ops tofu_cq_fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_cq_close,
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
    .ops_open	    = fi_no_ops_open,
};

/*
 * ssize_t tofu_progress(struct tofu_cep *cep_priv)
 *      critical region must be gurded by cep_priv->cq__lck
 *              fastlock_acquire(&cq__priv->cq__lck);
 *                ment = tofu_progress(cq__priv);
 *              fastlock_release(&cq__priv->cq__lck);
 */
ssize_t
tofu_progress(struct tofu_cq *cq)
{
    struct dlist_entry *head, *curr, *next;
    int ment = 0;

    /* 
     * cq->cq_ccq keeps CQ list. A CQ is inserted into this list
     * when some posted send/receive operation is completed. It happens
     * inside the utf_progress function or the send/receive functions.
     */
    ment = ofi_cirque_usedcnt(cq->cq_ccq);
    if (ment == 0) {
        /* transmit progress */
        head = &cq->cq_htx;
        dlist_foreach_safe(head, curr, next) {
            struct tofu_ctx *ctx;
            void	*tinfo;
            ctx = container_of(curr, struct tofu_ctx, ctx_ent_cq);
            assert(ctx->ctx_fid.fid.fclass == FI_CLASS_TX_CTX);
            tinfo = ctx->ctx_sep->sep_dom->tinfo;
            tfi_utf_progress(tinfo);
        }
        /* receive progress */
        head = &cq->cq_hrx;
        dlist_foreach_safe(head, curr, next) {
            struct tofu_ctx *ctx;
            void	*tinfo;
            ctx = container_of(curr, struct tofu_ctx, ctx_ent_cq);
            assert(ctx->ctx_fid.fid.fclass == FI_CLASS_RX_CTX);
            tinfo = ctx->ctx_sep->sep_dom->tinfo;
            tfi_utf_progress(tinfo);
        }
        ment = ofi_cirque_usedcnt(cq->cq_ccq);
    }
#if 0
    {
        extern int	utf_printf(const char *fmt, ...);
        static ssize_t  oment = -1;

        if(ment > 0
           || (ment == 0 && oment != 0)) {
            utf_printf("%s: YI********* cq(%p) ment(%ld)\n", __func__, cq, ment);
        }
        oment = ment;
    }
#endif
    return ment;
}

/*
 * fi_cq_read
 */
ssize_t
tofu_cq_read(struct fid_cq *fid_cq, void *buf, size_t count)
{
    ssize_t        ret = 0;
    struct tofu_cq *cq;
    ssize_t        ent, i;

    utf_tmr_begin(TMR_FI_CQREAD);
    FI_INFO(&tofu_prov, FI_LOG_CQ, " count(%ld) in %s\n", count, __FILE__);
    assert(fid_cq != 0);

    cq = container_of(fid_cq, struct tofu_cq, cq_fid);

    ent = ofi_cirque_usedcnt(cq->cq_ccq);
    if (ent < tfi_progress_compl_pending) {
        /*
         * The progress engine is invoked if the number of pending completions
         * is lower than CONF_TOFU_FI_COMPL_PENDING in default.
         */
        tofu_progress(cq);
        ent = ofi_cirque_usedcnt(cq->cq_ccq);
    }
    ent = (ent > count) ? count : ent;
    //fprintf(stderr, "%s: COUNT(%ld) ENT(%ld) TOTAL(%ld)\n", __func__, count, ent, ofi_cirque_usedcnt(cq->cq_ccq)); fflush(stderr);
    fastlock_acquire(&cq->cq_lck);
    if (ent > 0) {
        struct fi_cq_tagged_entry *cq_etag = (struct fi_cq_tagged_entry *) buf;
        for (i = 0; i < ent; i++) {
            struct fi_cq_tagged_entry *comp = ofi_cirque_head(cq->cq_ccq);
            assert(comp != 0);
            *cq_etag = *comp;
            // fprintf(stdout, "YI!!! libfabric %s: tag = 0x%lx\n", __func__, cq_etag->tag); fflush(stdout);
            cq_etag++;
            ofi_cirque_discard(cq->cq_ccq);
        }
        ret = ent;
    } else {
        ssize_t eent = ofi_cirque_usedcnt(cq->cq_cceq);
        if (eent > 0) {
            ret = -FI_EAVAIL;
        } else {
            ret = -FI_EAGAIN;
        }
    }
    fastlock_release(&cq->cq_lck);
    utf_tmr_end(TMR_FI_CQREAD);
    return ret;
}

/*
 * fi_cq_readerr
 *                      flags ?
 */
ssize_t
tofu_cq_readerr(struct fid_cq *fid_cq, struct fi_cq_err_entry *buf,
                uint64_t flags)
{
    struct tofu_cq *cq;
    ssize_t        ent;
    struct fi_cq_err_entry *comp;
    int fc;

    utf_tmr_begin(TMR_FI_CQREAD);    
    FI_INFO(&tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid_cq != 0);
    cq = container_of(fid_cq, struct tofu_cq, cq_fid);
    fastlock_acquire(&cq->cq_lck);
    ent = ofi_cirque_usedcnt(cq->cq_cceq);
    if (ent > 0) {
	comp = ofi_cirque_head(cq->cq_cceq);
        assert(comp != 0);
        /* copy */
        ((struct fi_cq_err_entry *)buf)[0] = *comp;
        ofi_cirque_discard(cq->cq_cceq);
        fc = 1; /* one retrieve */
    } else {
        fc = -FI_EAGAIN;
    }
    fastlock_release(&cq->cq_lck);
    utf_tmr_end(TMR_FI_CQREAD);
    return fc;
}

static struct fi_ops_cq tofu_cq_ops = {
    .size	    = sizeof (struct fi_ops_cq),
    .read	    = tofu_cq_read,
    .readfrom	    = fi_no_cq_readfrom,
    .readerr	    = tofu_cq_readerr,
    .sread	    = fi_no_cq_sread,
    .sreadfrom	    = fi_no_cq_sreadfrom,
    .signal	    = fi_no_cq_signal,
    .strerror	    = fi_no_cq_strerror,
};

/*
 * fi_cq_open
 */
int tofu_cq_open(struct fid_domain *fid_dom, struct fi_cq_attr *attr,
                 struct fid_cq **fid_cq_, void *context)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_cq *cq_priv = 0;

    FI_INFO(&tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom_priv = container_of(fid_dom, struct tofu_domain, dom_fid);

    DEBUG(DLEVEL_INIFIN) {
        fprintf(stderr, "%s: YYI!!!!!!!\n", __func__); fflush(stderr);
    }
    if (attr != 0) {
	FI_INFO(&tofu_prov, FI_LOG_CQ, "Requested: FI_CQ_FORMAT_%s\n",
	    (attr->format == FI_CQ_FORMAT_UNSPEC)?  "UNSPEC":
	    (attr->format == FI_CQ_FORMAT_CONTEXT)? "CONTEXT":
	    (attr->format == FI_CQ_FORMAT_MSG)?     "MSG":
	    (attr->format == FI_CQ_FORMAT_DATA)?    "DATA":
	    (attr->format == FI_CQ_FORMAT_TAGGED)?  "TAGGED": "<unknown>");
	fc = tofu_chck_cq_attr(attr);
	if (fc != 0) {
	    goto bad;
	}
        if (attr->format != FI_CQ_FORMAT_TAGGED) {
            fprintf(stderr, "FI_CQ_FORMAT_TAGGED is only supported in this version\n");
            fc = -1;
            goto bad;
        }
        if (attr->size != 0
            && attr->size != sizeof(struct fi_cq_tagged_entry)) {
            fc = -1;
            goto bad;
        }
    }

    cq_priv = calloc(1, sizeof (struct tofu_cq));
    if (cq_priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
    /* initialize cq_priv */
    cq_priv->cq_dom = dom_priv;
    ofi_atomic_initialize32(&cq_priv->cq_ref, 0);
    //R_DBG("cq_priv->cq_lck(%p)\n", &cq_priv->cq_lck);
    fastlock_init(&cq_priv->cq_lck);
    cq_priv->cq_fid.fid.fclass    = FI_CLASS_CQ;
    cq_priv->cq_fid.fid.context   = context;
    cq_priv->cq_fid.fid.ops       = &tofu_cq_fi_ops;
    cq_priv->cq_fid.ops           = &tofu_cq_ops;
    dlist_init(&cq_priv->cq_htx);
    dlist_init(&cq_priv->cq_hrx);

    /* tofu_comp_cirq */
    cq_priv->cq_ccq = tofu_ccirq_create(CONF_TOFU_CQSIZE);
    if (cq_priv->cq_ccq == 0) {
        fc = -FI_ENOMEM; goto bad;
    }
    /* for fi_cq_err_entry */
    cq_priv->cq_cceq = tofu_ccireq_create(CONF_TOFU_CQSIZE);
    if (cq_priv->cq_cceq == 0) {
        fc = -FI_ENOMEM; goto bad;
    }
    R_DBG0(RDBG_LEVEL1, "fi_cq_open: cq(%p)", cq_priv);
    /* return fid_dom */
    fid_cq_[0] = &cq_priv->cq_fid;
    goto ok;
bad:
    if (cq_priv != 0) {
	tofu_cq_close(&cq_priv->cq_fid.fid);
    }
ok:
    DEBUG(DLEVEL_INIFIN) {
        fprintf(stderr, "%s: YYI!!!!!!! return %d\n", __func__, fc); fflush(stderr);
    }
    return fc;
}
