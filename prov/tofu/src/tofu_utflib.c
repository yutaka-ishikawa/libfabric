/*
 * Interface Tofu <--> Utf
 */
#include "tofu_impl.h"
#include "tofu_addr.h"
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_queue.h"
#include "utf_sndmgt.h"
#include <rdma/fabric.h>

#define REQ_SELFSEND	1

extern struct utf_msgreq	*utf_msgreq_alloc();
extern void			utf_msgreq_free(struct utf_msgreq *req);
extern struct utf_msglst	*utf_msglst_insert(utfslist *head,
						   struct utf_msgreq *req);
extern char     *tofu_fi_msg_string(const struct fi_msg_tagged *msgp);
extern char	*utf_msghdr_string(struct utf_msghdr *hdrp, void *bp);

static char	*rstate_symbol[] = {
    "R_FREE", "R_NONE", "R_HEAD", "R_BODY", "R_DO_RNDZ", "R_DONE"
};

static char	*sstate_symbol[] = {
    "S_FREE", "S_NONE", "S_REQ_ROOM", "S_HAS_ROOM",
    "S_DO_EGR", "S_DO_EGR_WAITCMPL", "S_DONE_EGR",
    "S_REQ_RDVR", "S_RDVDONE", "S_DONE", "S_WAIT_BUFREADY"
};

void
tofu_show_msgreq(struct utf_msgreq *req)
{
    switch(req->type) {
    case REQ_RECV_EXPECTED:
	utf_printf("req(%p) Expected Recv status(%d)\n", req, req->status);
	break;
    case REQ_RECV_UNEXPECTED:
	utf_printf("req(%p) Unexpected Recv status(%s)\n", req, rstate_symbol[req->status]);
	break;
    case REQ_SND_REQ:
	utf_printf("req(%p) Send status(%s)\n", req, sstate_symbol[req->status]);
	break;
    }
}


/*
 * CQ Error
 */
static inline int
tofu_reg_rcveq(struct tofu_cq *cq, void *context, uint64_t flags, size_t len,
	       size_t olen, int err, int prov_errno,
	       void *bufp, uint64_t data, uint64_t tag)
{
    int fc = FI_SUCCESS;
    struct fi_cq_err_entry cq_e[1], *comp;

    utf_printf("%s: context(%p), flags(%s) len(%ld) data(%ld) tag(%lx)\n",
	       __func__, context, tofu_fi_flags_string(flags), len, data, tag);
    if (cq->cq_rsel && !(flags & FI_COMPLETION)) {
	/* no needs to completion */
	utf_printf("%s: no receive completion is generated\n",  __func__);
	return fc;
    }
    fastlock_acquire(&cq->cq_lck);
    if (ofi_cirque_isfull(cq->cq_ccq)) {
	utf_printf("%s: cq(%p) CQ is full\n",  __func__, cq);
	fc = -FI_EAGAIN; goto bad;
    }
    /* FI_CLAIM */
    cq_e->op_context	= context;
    cq_e->flags		=   FI_RECV
		| (flags & (FI_MULTI_RECV|FI_TAGGED|FI_REMOTE_CQ_DATA));
    cq_e->len		= len;
    cq_e->buf		= bufp;
    cq_e->data		= data;
    cq_e->tag		= tag;
    cq_e->olen		= olen;
    cq_e->err		= err;
    cq_e->prov_errno	= prov_errno;
    cq_e->err_data	= 0;
    cq_e->err_data_size	= 0;

    /* get an entry pointed by w.p. */
    comp = ofi_cirque_tail(cq->cq_cceq);
    assert(comp != 0);
    comp[0] = cq_e[0]; /* structure copy */
    /* advance w.p. by one  */
    ofi_cirque_commit(cq->cq_cceq);
bad:
    fastlock_release(&cq->cq_lck);
    return fc;
}

static inline int
tofu_reg_rcvcq(struct tofu_cq *cq, void *context, uint64_t flags, size_t len,
	       void *bufp, uint64_t data, uint64_t tag)
{
    int fc = FI_SUCCESS;
    struct fi_cq_tagged_entry cq_e[1], *comp;

    utf_printf("%s: flags = %s\n", __func__, tofu_fi_flags_string(flags));
    if (cq->cq_rsel && !(flags & FI_COMPLETION)) {
	/* no needs to completion */
	utf_printf("%s: no completion is generated\n",  __func__);
	return fc;
    }
    fastlock_acquire(&cq->cq_lck);
    if (ofi_cirque_isfull(cq->cq_ccq)) {
	utf_printf("%s: cq(%p) CQ is full\n",  __func__, cq);
	fc = -FI_EAGAIN; goto bad;
    }
    /* FI_CLAIM */
    cq_e->op_context	= context;
    cq_e->flags		=   FI_RECV
		| (flags & (FI_MULTI_RECV|FI_TAGGED|FI_REMOTE_CQ_DATA|FI_COMPLETION));
    cq_e->len		= len;
    cq_e->buf		= bufp;
    cq_e->data		= data;
    cq_e->tag		= tag;
    utf_printf("%s: context(%p), newflags(%s) len(%ld) data(%ld) tag(%lx)\n",
	       __func__, context, tofu_fi_flags_string(cq_e->flags), len, data, tag);
    /* get an entry pointed by w.p. */
    comp = ofi_cirque_tail(cq->cq_ccq);
    assert(comp != 0);
    comp[0] = cq_e[0]; /* structure copy */
    /* advance w.p. by one  */
    ofi_cirque_commit(cq->cq_ccq);
bad:
    fastlock_release(&cq->cq_lck);
    utf_printf("%s: YI**** 2 cq(%p)->cq_ccq(%p) return\n", __func__, cq, cq->cq_ccq);
    return fc;
}


static void
tofu_catch_rcvnotify(struct utf_msgreq *req)
{
    struct tofu_ctx *ctx;

    utf_printf("%s: notification received req(%p)\n", __func__, req);
    utf_printf("%s:\t msg=%s\n", __func__,
	       utf_msghdr_string(&req->hdr, req->buf));
    assert(req->type == REQ_RECV_EXPECTED);
    ctx = req->fi_ctx;
    /* received data has been already copied to the specified buffer */
    if (req->ustatus == REQ_OVERRUN) {
	utf_printf("%s: overrun\n", __func__);
	tofu_reg_rcveq(ctx->ctx_recv_cq, req->fi_ucontext,
		       req->hdr.flgs, req->rsize,
		       req->hdr.size, -1, -1,
		       0, req->hdr.data, req->hdr.tag);
    } else {
	tofu_reg_rcvcq(ctx->ctx_recv_cq, req->fi_ucontext,
		       req->hdr.flgs, req->rsize,
		       req->buf, req->hdr.data, req->hdr.tag);
    }
    utf_msgreq_free(req);
}

static void
tofu_reg_sndcq(struct tofu_cq *cq, void *context, uint64_t flags, size_t len,
	       uint64_t data, uint64_t tag)
{
    struct fi_cq_tagged_entry cq_e[1], *comp;

    cq_e->op_context	= context;
    cq_e->flags		= (flags&(FI_TAGGED|FI_COMPLETION)) | FI_SEND;
    cq_e->len		= len;
    cq_e->buf		= 0;
    cq_e->data		= data;
    cq_e->tag		= tag;
    utf_printf("%s: context(%p), newflags(%s) len(%ld) data(%ld) tag(%lx) cq(%p)->cq_ccq(%p)\n",
	       __func__, context, tofu_fi_flags_string(cq_e->flags), len, data, tag, cq, cq->cq_ccq);
    if (flags & FI_INJECT
	|| (cq->cq_ssel && !(flags & FI_COMPLETION))) {
	utf_printf("%s: no send completion is generated\n",  __func__);
	goto skip;
    }
    fastlock_acquire(&cq->cq_lck);
    /* get an entry pointed by w.p. */
    comp = ofi_cirque_tail(cq->cq_ccq);
    assert(comp != 0);
    comp[0] = cq_e[0]; /* structure copy */
    /* advance w.p. by one  */
    ofi_cirque_commit(cq->cq_ccq);
    fastlock_release(&cq->cq_lck);
skip:
    utf_printf("%s: YI***** 3\n", __func__);
}

static void
tofu_catch_sndnotify(struct utf_msgreq *req)
{
    struct tofu_ctx *ctx;
    struct tofu_cq  *cq;

    utf_printf("%s: notification received req(%p)->type(%d) flgs(%s)\n",
	       __func__, req, req->type, tofu_fi_flags_string(req->fi_flgs));
    assert(req->type == REQ_SND_REQ);
    ctx = req->fi_ctx;
    assert(ctx != 0);
    cq = ctx->ctx_send_cq;
    assert(cq != 0);
    tofu_reg_sndcq(cq, req->fi_ucontext, req->fi_flgs, req->hdr.size,
		   req->hdr.data, req->hdr.tag);
    utf_msgreq_free(req);
    utf_printf("%s: YI***** 3\n", __func__);
}

int
tofu_utf_sendmsg_self(struct tofu_ctx *ctx,
		      const struct fi_msg_tagged *msg, uint64_t flags)
{
    utfslist *explst;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    struct utf_msgreq	*req;
    uint64_t	sndsz;
    int	idx;
    int fc = FI_SUCCESS;

    utf_printf("%s: YI***** \n", __func__);
    explst = flags & FI_TAGGED ? &utf_fitag_explst : &utf_fimsg_explst;
    sndsz = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    if ((idx = tofu_utf_explst_match(explst, src, tag, 0)) != -1) {/* found */
	req = utf_idx2msgreq(idx);
	/* sender data is copied to the specified buffer */
	if (req->fi_iov_count == 1) {
	    ofi_copy_from_iov(req->fi_msg[0].iov_base, req->fi_msg[0].iov_len,
			      msg->msg_iov, req->expsize, 0);
	} else if (msg->iov_count == 1) {
	    ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, 0,
			    msg->msg_iov[0].iov_base, req->expsize);
	} else {
	    /* This is a naive copy. we should optimize this copy */
	    char	*cp = malloc(req->expsize);
	    if (cp == NULL) {
		fc = -FI_ENOMEM; goto err;
	    }
	    ofi_copy_from_iov(cp, req->expsize,
			      msg->msg_iov, msg->iov_count, 0);
	    ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, 0,
			    cp, req->expsize);
	    free(cp);
	}
	/* completion */
	/* generating completion event to receiver CQ  */
	/*
	 * how we should handle flags specified by Receve post and message's flags specified by sender
	 *	2020/04/06
	 */
	if (req->expsize < sndsz) {  /* overrun, error cq */
	    tofu_reg_rcveq(req->fi_ctx, req->fi_ucontext,
			   req->fi_flgs, sndsz,
			   sndsz, -1, -1, 0, req->hdr.data, tag);
	} else {
	    tofu_reg_rcvcq(req->fi_ctx, req->fi_ucontext,
			   req->hdr.flgs, req->expsize, 0, req->hdr.data, tag);
	}
	/* generating completion event to sender CQ  */
	tofu_reg_sndcq(ctx->ctx_send_cq, msg->context, flags, sndsz,
		       msg->data, tag);
    } else { /* insert the new req into the unexpected message queue */
	uint8_t	*cp = malloc(sndsz);
	if (cp == NULL) { fc = -FI_ENOMEM; goto err;	}
	if ((req = utf_msgreq_alloc()) == NULL) {
	    fc = -FI_ENOMEM; goto err;
	}
	/* This is a naive copy. we should optimize this copy */
	ofi_copy_from_iov(cp, sndsz, msg->msg_iov, msg->iov_count, 0);
	req->buf = cp;
	req->hdr.src = src;
	req->hdr.tag = tag;
	req->hdr.size = sndsz;
	req->rsize = sndsz;
	req->fistatus = REQ_SELFSEND;
	req->type = REQ_RECV_UNEXPECTED;
	req->fi_ctx = ctx;	/* used for completion */
	req->fi_flgs = flags;
	req->fi_ucontext = msg->context;
	explst = flags & FI_TAGGED ? &utf_fitag_uexplst : &utf_fimsg_uexplst;
	utf_msglst_insert(explst, req);
    }
err:
    return fc;
}

/*
 * 
 */
int
tofu_utf_send_post(struct tofu_ctx *ctx,
		   const struct fi_msg_tagged *msg, uint64_t flags)
{
    int fc = FI_SUCCESS;
    int	rc = 0;
    fi_addr_t        dst = msg->addr;
    size_t	     msgsize;
    struct tofu_av   *av;
    utofu_vcq_id_t   r_vcqid;
    uint64_t	     flgs;
    struct utf_send_cntr *usp;
    struct utf_send_msginfo *minfo;
    struct utf_msgreq	*req;
    struct utf_egr_sbuf	*sbufp;
    utfslist_entry	*ohead;

    INITCHECK();
    utf_printf("%s: msghdr(%s) flags(%s)\n",  __func__, tofu_fi_msg_string(msg), tofu_fi_flags_string(flags));
    if (msg->msg_iov[0].iov_base) {
	rdbg_iovec(__func__, __LINE__, msg->iov_count, msg->msg_iov);
        rdbg_mpich_cntrl(__func__, __LINE__, msg->msg_iov[0].iov_base);
    }
    msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    if (msgsize > CONF_TOFU_INJECTSIZE && FI_INJECT) {
	fc = FI_EIO;
	utf_printf("%s: message size(%ld) exceeds in the FI_INJECT option\n",
		   __func__, msgsize);
	goto err1;
    }
    /* convert destination fi_addr to utofu_vcq_id_t: r_vcqid */
    av = ctx->ctx_sep->sep_av_;
    fc = tofu_av_lookup_vcqid_by_fia(av, dst, &r_vcqid, &flgs);
    if (fc != FI_SUCCESS) { rc = fc; goto err1; }
    {
	char	buf1[128], buf2[128];
	R_DBG("YI********* src(%d) = %s dest(%ld) = %s flgs(%ld)",
	      ctx->ctx_sep->sep_myrank,
	      vcqid2string(buf1, 128, ctx->ctx_sep->sep_myvcqid),
	      dst, vcqid2string(buf2, 128, r_vcqid), flgs);
    }
    /* sender control structure is allocated for the destination */
    usp = utf_scntr_alloc(dst, r_vcqid, flgs);
    if (usp == NULL) {
	/* rc = ERR_NOMORE_SNDCNTR; */
	fc = -FI_ENOMEM;
	goto err1;
    }
    /*
     * sender buffer is allocated, whose size is only 1920 Byte:
     *	   2 B for payload size
     *	  32 B for FI header
     *	1885 B for FI message (MSG_EAGER_SIZE)
     * If the message size is more than 1885 byte, 
     * the user's buf area is pinned down (utf_mem_reg) and its
     * steering address is registered 
     */
    if ((minfo = utf_sndminfo_alloc()) == NULL) { // rc = ERR_NOMORE_MINFO;
	fc = -FI_ENOMEM; goto err2;
    }
    memset(minfo, 0, sizeof(*minfo));
    minfo->sndbuf = sbufp = utf_egrsbuf_alloc(&minfo->sndstadd);
    if (sbufp == NULL) { // rc = ERR_NOMORE_SNDBUF;
	fc = -FI_ENOMEM; goto err3;
    }
    if ((req = utf_msgreq_alloc()) == NULL) { // rc = ERR_NOMORE_REQBUF;
	fc = -FI_ENOMEM; goto err4;
    }
    minfo->msghdr.src = ctx->ctx_sep->sep_myrank; /* 64 bit */
    minfo->msghdr.tag = msg->tag; /* 64 bit */
    minfo->msghdr.size = msgsize; /* 64 bit */
    if (flags & FI_REMOTE_CQ_DATA) {
	minfo->msghdr.data = msg->data; /* 64 bit */
    }
    minfo->msghdr.flgs = flags;
    minfo->mreq = req;
    minfo->context = msg->context;
    req->hdr = minfo->msghdr; /* copy the header */
    req->status = REQ_NONE;
    req->type = REQ_SND_REQ;
    req->notify = tofu_catch_sndnotify;
    req->fi_ctx = ctx;
    req->fi_flgs = flags;
    req->fi_ucontext = msg->context;
    if (msgsize <= MSG_EAGER_SIZE) {
	minfo->usrstadd = 0;
	minfo->cntrtype = SNDCNTR_BUFFERED_EAGER;
	sbufp->msgbdy.psize = MSG_MAKE_PSIZE(msgsize);
	/* header copy */
	bcopy(&minfo->msghdr, &sbufp->msgbdy.payload.h_pkt.hdr,
	      sizeof(struct utf_msghdr));
	/* message copy */
	if (msg->iov_count > 0) {
	    ofi_copy_from_iov(sbufp->msgbdy.payload.h_pkt.msgdata,
			      msgsize, msg->msg_iov, msg->iov_count, 0);
	} else { /* null message ? */

	}
    } else {
	utf_printf("%s: YI**** NEEDS to implement\n", __func__);
	fc = FI_EIO;
	goto err4;
    }
#if 0
    else if (utf_msgmode != MSG_RENDEZOUS) {
	/* Eager in-place*/
	DEBUG(DLEVEL_PROTO_EAGER) {
	    utf_printf("EAGER INPLACE\n");
	}
	minfo->usrbuf = msagdata;
	minfo->cntrtype = SNDCNTR_INPLACE_EAGER;
	bcopy(&minfo->msghdr, &sbufp->msgbdy.payload.h_pkt.hdr,
	      sizeof(struct utf_msghdr));
	bcopy(msgdtp, sbufp->msgbdy.payload.h_pkt.msgdata, MSG_EAGER_SIZE);
	sbufp->msgbdy.psize = MSG_MAKE_PSIZE(MSG_EAGER_SIZE);
    } else {
	/* Rendezvous */
	minfo->usrstadd = utf_mem_reg(vcqh, msgdtp, size);
	minfo->cntrtype = SNDCNTR_RENDEZOUS;
	bcopy(&minfo->msghdr, &sbufp->msgbdy.payload.h_pkt.hdr,
	      sizeof(struct utf_msghdr));
	bcopy(&minfo->usrstadd, sbufp->msgbdy.payload.h_pkt.msgdata,
	      sizeof(utofu_stadd_t));
	sbufp->msgbdy.psize = MSG_MAKE_PSIZE(sizeof(minfo->usrstadd));
	sbufp->msgbdy.rndz = MSG_RENDEZOUS;
    }
#endif
    /* for utf progress */
    ohead = utfslist_append(&usp->smsginfo, &minfo->slst);
    usp->dst = dst;
    if (ohead == NULL) { /* this is the first entry */
	rc = utf_send_start(ctx->ctx_sep->sep_myvcqh, usp);
	if (rc != 0) {
	    fc = FI_EIO;
	}
    }
    utf_printf("%s: YI***** return FI_SUCESS\n", __func__);
    return fc;
err4:
    utf_msgreq_free(req);
err3:
    utf_sndminfo_free(minfo);
err2:
    utf_scntr_free(dst);
err1:
    utf_printf("%s: YI***** return error(%d)\n", __func__, fc);
    return fc;
}

int
tofu_utf_recv_post(struct tofu_ctx *ctx,
		  const struct fi_msg_tagged *msg, uint64_t flags)
{
    int	fc = FI_SUCCESS;
    int	idx, peek = 0;
    struct utf_msgreq	*req;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	ignore = msg->ignore;
    utfslist *uexplst;

    R_DBG("ctx(%p) msg(%s) flags(%lx) = %s",
	  ctx, tofu_fi_msg_string(msg), flags, tofu_fi_flags_string(flags));
    if (flags & FI_TAGGED) {
	uexplst =  &utf_fitag_uexplst;
	if ((flags & FI_PEEK) && ~(flags & FI_CLAIM)) {
	    /* tagged message with just FI_PEEK option */
	    peek = 1;
	} /* else, non-tagged or tagged message with FI_PEEK&FI_CLAIM option */
    } else {
	uexplst = &utf_fimsg_uexplst;
    }
    if ((idx=tofu_utf_uexplst_match(uexplst, src, tag, ignore, peek)) != -1) {
	/* found in unexpected queue */
	size_t	sz;
	req = utf_idx2msgreq(idx);
	/* received data is copied to the specified buffer */
	sz = ofi_copy_to_iov(msg->msg_iov, msg->iov_count, 0,
				 req->buf, req->rsize);
	utf_printf("%s: YIxxxxx copy: req->buf[%ld]\n", __func__, (unsigned int)req->buf[0]);
	/* NO FI_MULTI_RECV is supported */
	/* CQ is immediately generated */
	if (peek == 1 && (flags & FI_CLAIM)
	    && req->fi_ucontext != msg->context) {
	    /* How should we do ? */
	    utf_printf("%s: FI_PEEK&FI_CLAIM but not context matching"
		       " previous context(%p) now context(%p)\n",
		       __func__, req->fi_ucontext, msg->context);
	}
	/*
	 * how we should handle flags specified by Receve post and message's flags specified by sender
	 *	2020/04/05
	 */
	if (sz != req->rsize) {
	    tofu_reg_rcveq(ctx->ctx_recv_cq, msg->context, req->hdr.flgs, sz,
			   req->rsize, -1, -1,
			   0, req->hdr.data, tag);
	} else {
	    tofu_reg_rcvcq(ctx->ctx_recv_cq, msg->context, req->hdr.flgs, sz,
			   0, req->hdr.data, tag);
	}
	if (peek == 0) { /* reclaim unexpected resources */
	    if (req->fistatus == REQ_SELFSEND) {
		/* This request has been created by tofu_utf_sendmsg_self */
		/* generating completion event to sender CQ  */
		tofu_reg_sndcq(((struct tofu_ctx*)(req->fi_ctx))->ctx_send_cq,
			       req->fi_ucontext,
			       req->fi_flgs, req->rsize,
			       req->hdr.data, req->hdr.tag);
	    }
	    free(req->buf); /* allocated dynamically and must be free */
	    utf_msgreq_free(req);
	} else if (~(flags & FI_CLAIM)) {
	    req->fi_ucontext = msg->context;
	}
	goto ext;
    }
    if (peek == 0) { /* register this request to the expected queue */
	utfslist *explst;
	struct utf_msglst *mlst;
	size_t	i;
	if ((req = utf_msgreq_alloc()) == NULL) {
	    fc = -FI_ENOMEM; goto ext;
	}
	req->hdr.src = src;
	req->hdr.data = msg->data;
	req->hdr.tag = tag;
	req->expsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	req->fi_iov_count = msg->iov_count;
	for (i = 0; i < msg->iov_count; i++) {
	    req->fi_msg[i].iov_base = msg->msg_iov[i].iov_base;
	    req->fi_msg[i].iov_len = msg->msg_iov[i].iov_len;
	}
	req->type = REQ_RECV_EXPECTED;	req->rsize = 0;
	req->status = R_NONE;
	req->fi_ignore = ignore;
	req->notify = tofu_catch_rcvnotify;
	req->fi_ctx = ctx;
	req->fi_flgs = flags;
	req->fistatus = 0; req->status = REQ_NONE;
	req->fi_ucontext = msg->context;
	explst = flags & FI_TAGGED ? &utf_fitag_explst : &utf_fimsg_explst;
	mlst = utf_msglst_insert(explst, req);
	mlst->fi_ignore = ignore;
	mlst->fi_context = msg->context;
	utf_printf("%s: Insert mlst(%p) to expected queue, fi_ignore(%lx)\n", __func__, mlst, mlst->fi_ignore);
    }
ext:
    return fc;
}
