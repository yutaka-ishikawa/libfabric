/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

static int
ulib_conv_flg_fabric(uint64_t dflg)
{
    uint64_t    flgs;
    
    flgs = ((dflg & ULIB_SHEA_DATA_TFLG) != 0)? FI_TAGGED: 0;
    flgs |= ((dflg & ULIB_SHEA_DATA_CFLG) != 0)? FI_COMPLETION: 0;
    return flgs;
}

/*
 * Nofity Receive Competion to Counter
 */
static inline void
ulib_notify_recvcmpl_cntr(struct tofu_cntr *ctr_priv,
                          const struct ulib_shea_expd *expd)
{
    if (ctr_priv == 0) { /* no fi_ep_bind */
        //printf("YI**** no counter for recv op\n"); fflush(stdout);
        return;
    }
    if (ctr_priv->ctr_rsl && !(expd->flgs & FI_COMPLETION)) {
        /* FI_SELECTIVE_COMPLETION and not FI_COMPLETION in receive */
        //printf("YI***** (%s) No counter incr. for recv op\n", __func__);
        //fflush(stdout);
        return;
    }
    ofi_atomic_inc64(&ctr_priv->ctr_ctr);
}

static inline void
ulib_notify_sndcmpl_cntr(struct tofu_cntr *ctr_priv,
                         const struct ulib_shea_data *udat)
{
    uint64_t   dflg;

    if (ctr_priv == 0) { /* no fi_ep_bind */
        //printf("YI**** no counter for send op\n"); fflush(stdout);
        return;
    }
    if (udat) {
        dflg = ulib_shea_data_flag(udat);
        if (ctr_priv->ctr_tsl && !(dflg & ULIB_SHEA_DATA_CFLG)) {
            /* FI_SELECTIVE_COMPLETION and not FI_COMPLETION in receive */
            //printf("YI***** (%s) No counter incr. for send op\n", __func__);
            //fflush(stdout);
            return;
        }
    }
    ofi_atomic_inc64(&ctr_priv->ctr_ctr);
}

/*
 * Notify receive completion to Completion Queue
 *      Handling the FI_MULTI_RECV flag 2019/4/27
 *  called by
 *      ulib_icep_shea_send_prog()
 *      tofu_impl_ulib_sendmsg_self()
 *      ulib_icep_recv_call_back()
 *      ulib_icep_shea_recv_post()
 */
static inline int
ulib_notify_recvcmpl_cq(void *vp_cq__priv,
                    const struct ulib_shea_expd *expd)
{
    int uc = UTOFU_SUCCESS;
    struct tofu_cq *cq__priv = vp_cq__priv;
    struct fi_cq_tagged_entry cq_e[1];

    if (cq__priv == 0) { /* no fi_ep_bind */
        //printf("YI***** (%s) No CQE generated for recv op (1)\n", __func__);
        //fflush(stdout);
        return uc;
    }
    if (cq__priv->cq_rsel && !(expd->flgs & FI_COMPLETION)) {
        /* FI_SELECTIVE_COMPLETION and not FI_COMPLETION in receive */
        //printf("YI***** (%s) No CQE generated for recv op\n", __func__);
        //fflush(stdout);
        return uc;
    }

    cq_e->op_context	= expd->tmsg.context;
    cq_e->flags		=   FI_RECV
                            | (expd->flgs & FI_MULTI_RECV)
			    | (expd->flgs & FI_TAGGED)
			    | (expd->rflg & FI_REMOTE_CQ_DATA)
			    ;
    cq_e->len		= expd->wlen;
    cq_e->buf		= expd->iovs[0].iov_base;
    cq_e->data		= expd->idat; /* FI_REMOTE_CQ_DATA */
    cq_e->tag		= expd->rtag;

    if (cq_e->buf) {
        R_DBG0(RDBG_LEVEL3,
               "\tReceive Completion: flags(0x%lx) data(%ld) len(%ld) buf[]=%d",
               cq_e->flags, cq_e->data, cq_e->len, *(int*)cq_e->buf);
    } else {
        R_DBG0(RDBG_LEVEL3,
               "\tReceive Completion: flags(0x%lx) data(%ld) len(%ld) buf=nil",
               cq_e->flags, cq_e->data, cq_e->len);
    }

    /* enter the entry into CQ */
    //printf("YI***** (%s) data(%lx) cntxt(%p) CQE generated for recv op\n", __func__, cq_e->data, cq_e->op_context); fflush(stdout);
    uc = tofu_cq_comp_tagged(vp_cq__priv, cq_e);
    if (uc != 0) {
        uc = UTOFU_ERR_OUT_OF_RESOURCE;
    }
    return uc;
}


/*
 * Notify send completion
 *      For self message completion, see tofu_impl_ulib_sendmsg_self
 */
static inline int 
ulib_notify_sndcmpl_cq(void *vp_cq__priv, const struct ulib_shea_data *udat,
                       struct fi_cq_tagged_entry *cqp)
{
    int uc = UTOFU_SUCCESS;
    struct tofu_cq *cq__priv = vp_cq__priv;
    struct fi_cq_tagged_entry cq_e[1];
    uint64_t   dflg;
    void      *ctxt;
    uint64_t   flgs;
    size_t     tlen;
    uint64_t    tag;
    
    if (cq__priv == 0) { /* no fi_ep_bind */
        //printf("YI***** (%s) No CQE generated for send op\n", __func__);
        //fflush(stdout);
        return uc;
    }
    if (cqp) {
        if (cq__priv->cq_ssel && !(cqp->flags & FI_COMPLETION)) {
            /* FI_SELECTIVE_COMPLETION and not FI_COMPLETION in sender */
            fflush(stdout);
            return uc;
        }
    } else {
        dflg = ulib_shea_data_flag(udat);
        /* converted to fi_flags */
        flgs = ulib_conv_flg_fabric(dflg);
        if (cq__priv->cq_ssel && !(flgs & FI_COMPLETION)) {
            /* FI_SELECTIVE_COMPLETION and not FI_COMPLETION in sender */
            fflush(stdout);
            return uc;
        }
        /* tlen */
        {
            uint32_t nblk, llen;

            nblk = ulib_shea_data_nblk(udat);
            llen = ulib_shea_data_llen(udat);

            assert(nblk > 0);
            tlen = ((nblk - 1) * ULIB_SHEA_DBLK) + llen; /* YYY MACRO ? */
        }
        ctxt = ulib_shea_data_ctxt(udat);
        tag = ulib_shea_data_utag(udat);
        ulib_init_cqe(cq_e, ctxt, FI_SEND|flgs, tlen, 0, 0, tag);
        cqp = cq_e;
    }

    R_DBG0(RDBG_LEVEL3,
           "\tSend Completion: flags(0x%lx) rank(%d) tag(%ld) len(%ld)",
           cqp->flags, udat ? udat->rank : -1, cqp->tag, cqp->len);

    /* enter the entry into CQ */
    //printf("%d: YI***** (%s) cntxt(%p) CQE generated for send op\n", getpid(), __func__, ctxt); fflush(stdout);
    uc = tofu_cq_comp_tagged(vp_cq__priv, cqp);
    if (uc != 0) {
        uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
bad:
    return uc;
}
