/*
 * Interface Tofu <--> Utf
 */
#include "tofu_impl.h"
#include "tofu_addr.h"
#include "utf_conf.h"
#include "utf_errmacros.h"
#include "utf_externs.h"
#include "utf_queue.h"
#include "utf_sndmgt.h"
#include "utf_cqmacro.h"
#include <rdma/fabric.h>

/*
 *	See also ofi_events.c in the mpich/src/mpid/ch4/netmod/ofi directory
 *		 fi_errno.h   in the include/rma directory
 *	CQ value  : FI_EAVAIL
 *	CQE value :
 *		FI_ETRUNC	265
 *		FI_ECANCELED	125
 *		FI_ENOMSG	42
 */

__attribute__((visibility ("default"), EXTERNALLY_VISIBLE))
void	*fi_tofu_dbgvalue;
utfslist *dbg_uexplst;

#define REQ_SELFSEND	1

extern int			utf_msgmode;
extern struct utf_msgreq	*utf_msgreq_alloc();
extern void			utf_msgreq_free(struct utf_msgreq *req);
extern struct utf_msglst	*utf_msglst_append(utfslist *head,
						   struct utf_msgreq *req);
extern char     *tofu_fi_msg_string(const struct fi_msg_tagged *msgp);
extern char	*utf_msghdr_string(struct utf_msghdr *hdrp, void *bp);

#include "utf_msgmacro.h"

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

void
tofu_dom_setuptinfo(struct tni_info *tinfo, struct tofu_vname *vnmp)
{
    tinfo->vnamp = vnmp;
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

    //utf_printf("%s: context(%p), flags(%s) len(%ld) data(%ld) tag(%lx)\n",
    //__func__, context, tofu_fi_flags_string(flags), len, data, tag);
    DEBUG(DLEVEL_ADHOC) utf_printf("%s:DONE error len(%ld) olen(%ld) err(%d)\n", __func__, len, olen, err);
    if (cq->cq_rsel && !(flags & FI_COMPLETION)) {
	/* no needs to completion */
	utf_printf("%s: no receive completion is generated\n",  __func__);
	return fc;
    }
    fastlock_acquire(&cq->cq_lck);
    if (ofi_cirque_isfull(cq->cq_cceq)) {
	utf_printf("%s: cq(%p) CQE is full\n",  __func__, cq);
	fc = -FI_EAGAIN; goto bad;
    }
    /* FI_CLAIM is not set because no FI_BUFFERED_RECV is set*/
    cq_e->op_context	= context;
    cq_e->flags		=   FI_RECV
		| (flags & (FI_MULTI_RECV|FI_MSG|FI_TAGGED|FI_REMOTE_CQ_DATA|FI_COMPLETION));
    cq_e->len		= len;
    //cq_e->buf		= (flags & FI_MULTI_RECV) ? bufp : 0;
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

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: cq(%p) flags = %s bufp(%p) len(%ld)\n",
		   __func__, cq, tofu_fi_flags_string(flags), bufp, len);
	if (bufp) utf_show_data("\tdata = ", bufp, len);
    }
    DEBUG(DLEVEL_ADHOC) utf_printf("%s:DONE len(%ld)\n", __func__, len);
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
    /* FI_CLAIM is not set because no FI_BUFFERED_RECV is set*/
    cq_e->op_context	= context;
    cq_e->flags		=   FI_RECV
		| (flags & (FI_MULTI_RECV|FI_MSG|FI_TAGGED|FI_REMOTE_CQ_DATA|FI_COMPLETION));
    cq_e->len		= len;
    //cq_e->buf		= (flags & FI_MULTI_RECV) ? bufp : 0;
    cq_e->buf		= bufp;
    cq_e->data		= data;
    cq_e->tag		= tag;
    /* get an entry pointed by w.p. */
    comp = ofi_cirque_tail(cq->cq_ccq);
    assert(comp != 0);
    comp[0] = cq_e[0]; /* structure copy */
    /* advance w.p. by one  */
    ofi_cirque_commit(cq->cq_ccq);
bad:
    fastlock_release(&cq->cq_lck);
    // utf_printf("%s: YI**** 2 cq(%p)->cq_ccq(%p) return\n", __func__, cq, cq->cq_ccq);
    return fc;
}


/*
 * This is for expected message. it is invoked by progress engine
 */
static void
tofu_catch_rcvnotify(struct utf_msgreq *req)
{
    struct tofu_ctx *ctx;
    uint64_t	flags = req->hdr.flgs | (req->fi_flgs&FI_MULTI_RECV);

    DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	utf_printf("%s: notification received req(%p)->buf(%p) "
		   "iov_base(%p) msg=%s\n", __func__,
		   req, req->buf, req->fi_msg[0].iov_base, utf_msghdr_string(&req->hdr, req->buf));
    }
    assert(req->type == REQ_RECV_EXPECTED);
    ctx = req->fi_ctx;
    /* received data has been already copied to the specified buffer */
    if (req->ustatus == REQ_OVERRUN) {
	tofu_reg_rcveq(ctx->ctx_recv_cq, req->fi_ucontext,
		       flags,
		       req->rsize, /* received size */
		       req->rsize - req->expsize, /* overrun length */
		       FI_ETRUNC, FI_ETRUNC,  /* not negative value here */
		       req->fi_msg[0].iov_base, req->hdr.data, req->hdr.tag);
    } else {
	tofu_reg_rcvcq(ctx->ctx_recv_cq, req->fi_ucontext,
		       flags, req->rsize,
		       req->fi_msg[0].iov_base, req->hdr.data, req->hdr.tag);
    }
    utf_msgreq_free(req);
}

static void
tofu_reg_sndcq(struct tofu_cq *cq, void *context, uint64_t flags, size_t len,
	       uint64_t data, uint64_t tag)
{
    struct fi_cq_tagged_entry cq_e[1], *comp;

    cq_e->op_context	= context;
    cq_e->flags		= (flags & (FI_TAGGED|FI_COMPLETION|FI_TRANSMIT_COMPLETE|FI_DELIVERY_COMPLETE|FI_RMA|FI_WRITE|FI_READ))
			| (flags&FI_RMA) ? 0 : FI_SEND;
    cq_e->len		= len;
    cq_e->buf		= 0;
    cq_e->data		= data;
    cq_e->tag		= tag;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: context(%p), newflags(%s) len(%ld) data(%ld) tag(%lx) cq(%p)->cq_ccq(%p) cq_ssel(%d)\n",
		   __func__, context, tofu_fi_flags_string(cq_e->flags), len, data, tag, cq, cq->cq_ccq, cq->cq_ssel);
    }
    if (flags & FI_INJECT
	|| (cq->cq_ssel && !(flags & (FI_COMPLETION|FI_TRANSMIT_COMPLETE|FI_DELIVERY_COMPLETE)))) {
	DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	    utf_printf("%s: no send completion is generated\n",  __func__);
	}
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
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: return\n", __func__);
    }
    return;
}

static void
tofu_catch_sndnotify(struct utf_msgreq *req)
{
    struct tofu_ctx *ctx;
    struct tofu_cq  *cq;

    DEBUG(DLEVEL_ADHOC) utf_printf("%s: DONE\n", __func__);
    //utf_printf("%s: notification received req(%p)->type(%d) flgs(%s)\n",
    //__func__, req, req->type, tofu_fi_flags_string(req->fi_flgs));
    assert(req->type == REQ_SND_REQ);
    ctx = req->fi_ctx;
    assert(ctx != 0);
    cq = ctx->ctx_send_cq;
    assert(cq != 0);
    tofu_reg_sndcq(cq, req->fi_ucontext, req->fi_flgs, req->hdr.size,
		   req->hdr.data, req->hdr.tag);
    utf_msgreq_free(req);
    //utf_printf("%s: YI***** 3\n", __func__);
}

int
tofu_utf_sendmsg_self(struct tofu_ctx *ctx,
		      const struct fi_msg_tagged *msg, uint64_t flags)
{
    utfslist *explst;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	data = msg->data;
    struct utf_msgreq	*req;
    uint64_t	msgsz;
    int	idx;
    int fc = FI_SUCCESS;

    explst = flags & FI_TAGGED ? &utf_fitag_explst : &utf_fimsg_explst;
    msgsz = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	utf_printf("%s: flags(%s) sz(%ld) src(%ld) tag(%lx) data(%ld)\n",
		   __func__, tofu_fi_flags_string(flags), msgsz, src, tag, data);
    }
    if ((idx = tofu_utf_explst_match(explst, src, tag, 0)) != -1) {/* found */
	uint64_t	sndsz;

	req = utf_idx2msgreq(idx);
	sndsz = req->expsize >= msgsz ? msgsz : req->expsize;
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: req->expsize(%ld) msgsz(%ld) req->fi_flgs(%s) "
		       "req->fi_msg[0].iov_base(%lx) req->fi_msg[0].iov_len(%ld) "
		       "sndsz(%ld) msg->iov_count(%ld)\n",
		       __func__, req->expsize, msgsz, tofu_fi_flags_string(req->fi_flgs),
		       req->fi_msg[0].iov_base, req->fi_msg[0].iov_len, sndsz, msg->iov_count);
	}
	/* sender data is copied to the specified buffer */
	if (req->fi_iov_count == 1) {
	    ofi_copy_from_iov(req->fi_msg[0].iov_base, req->fi_msg[0].iov_len,
			      msg->msg_iov, msg->iov_count, 0);
	} else if (msg->iov_count == 1) {
	    ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, 0,
			    msg->msg_iov[0].iov_base, sndsz);
	} else {
	    /* This is a naive copy. we should optimize this copy */
	    char	*cp = utf_malloc(sndsz);
	    if (cp == NULL) { fc = -FI_ENOMEM; goto err; }
	    ofi_copy_from_iov(cp, req->expsize,
			      msg->msg_iov, msg->iov_count, 0);
	    ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, 0,
			    cp, sndsz);
	    utf_free(cp);
	}
	req->fi_flgs |= flags & (FI_REMOTE_CQ_DATA|FI_TAGGED);
	tofu_catch_rcvnotify(req);
    } else { /* insert the new req into the unexpected message queue */
	uint8_t	*cp = utf_malloc(msgsz);
	if (cp == NULL) { fc = -FI_ENOMEM; goto err;	}
	if ((req = utf_msgreq_alloc()) == NULL) {
	    fc = -FI_ENOMEM; goto err;
	}
	/* This is a naive copy. we should optimize this copy */
	ofi_copy_from_iov(cp, msgsz, msg->msg_iov, msg->iov_count, 0);
	req->status = REQ_DONE;
	req->buf = cp;
	req->hdr.src = src;
	req->hdr.tag = tag;
	req->hdr.data = data;
	req->hdr.size = msgsz;
	req->rsize = msgsz;
	req->fistatus = REQ_SELFSEND;
	req->type = REQ_RECV_UNEXPECTED;
	req->fi_ctx = ctx;	/* used for completion */
	req->fi_flgs = flags;
	req->fi_ucontext = msg->context;
	explst = flags & FI_TAGGED ? &utf_fitag_uexplst : &utf_fimsg_uexplst;
	utf_msglst_append(explst, req);
    }
    /* generating send notification */
    if ((req = utf_msgreq_alloc()) == NULL) { // rc = ERR_NOMORE_REQBUF;
	fc = -FI_ENOMEM; goto err;
    }
    req->hdr.src = src;
    req->hdr.tag = tag;
    req->hdr.data = data;
    req->hdr.size = msgsz;
    req->status = REQ_NONE;
    req->type = REQ_SND_REQ;
    req->notify = tofu_catch_sndnotify;
    req->fi_ctx = ctx;
    req->fi_flgs = flags;
    req->fi_ucontext = msg->context;
    tofu_catch_sndnotify(req);
err:
    return fc;
}

/* a multi-rail implementation
 * The MPI sender's vcqid and stadd is set, expose it to the MPI receiver */
static inline void
tofu_utf_recv_rget_expose(struct tni_info *tinfo,  struct utf_send_msginfo *minfo)
{
    struct utf_vcqid_stadd *rgetaddr = &minfo->rgetaddr;
    utofu_vcq_hdl_t	*vcqhdl = minfo->rgethndl;
    int	i;
    memset(rgetaddr, 0, sizeof(struct utf_vcqid_stadd));
    utf_cqselect_rget_expose(tinfo, rgetaddr, vcqhdl);
    for (i = 0; i < rgetaddr->nent; i++) {
	if (rgetaddr->vcqid[i]) {
	    rgetaddr->stadd[i] = utf_mem_reg(vcqhdl[i], minfo->usrbuf, minfo->msghdr.size);
	} else {
	    break;
	}
    }
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
    struct tofu_sep  *sep;
    struct tofu_av   *av;
    utofu_vcq_id_t	vcqh;
    utofu_vcq_id_t   r_vcqid;
    uint64_t	     flgs;
    struct utf_send_cntr *usp;
    struct utf_send_msginfo *minfo;
    struct utf_msgreq	*req;
    struct utf_egr_sbuf	*sbufp;
    utfslist_entry	*ohead;

    INITCHECK();
    msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    /* convert destination fi_addr to utofu_vcq_id_t: r_vcqid */
    sep = ctx->ctx_sep;
    av = sep->sep_av_;
    vcqh = sep->sep_myvcqh;
    {
	utofu_vcq_id_t	tvcqh;
	utf_cqselect_sendone(ctx->ctx_tinfo, &tvcqh, msgsize);
	// utf_printf("%s: vcqh(%lx) tvcqh(%lx) msgsize(%ld)\n", __func__, vcqh, tvcqh, msgsize);
    }
    fc = tofu_av_lookup_vcqid_by_fia(av, dst, &r_vcqid, &flgs);
    if (fc != FI_SUCCESS) { rc = fc; goto err1; }
#if 0
    {
	char	buf1[128], buf2[128];
	R_DBG("YI********* src(%d) = %s dest(%ld) = %s flgs(%ld)",
	      ctx->ctx_sep->sep_myrank,
	      vcqid2string(buf1, 128, ctx->ctx_sep->sep_myvcqid),
	      dst, vcqid2string(buf2, 128, r_vcqid), flgs);
    }
#endif
    /* sender control structure is allocated for the destination */
    usp = utf_scntr_alloc(dst, r_vcqid, flgs);
    // usp->av = av; this field is set at the initialization time. 2020/06/27
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
    /* header copy */
    bcopy(&minfo->msghdr, &sbufp->msgbdy.payload.h_pkt.hdr,
	  sizeof(struct utf_msghdr));
    if (msgsize <= CONF_TOFU_INJECTSIZE) {
	//minfo->usrstadd = 0;
	minfo->cntrtype = SNDCNTR_BUFFERED_EAGER;
	sbufp->msgbdy.psize = MSG_MAKE_PSIZE(msgsize);
	sbufp->msgbdy.ptype = PKT_EAGER;
	/* message copy */
	if (msg->iov_count > 0) { /* if 0, null message */
	    ofi_copy_from_iov(sbufp->msgbdy.payload.h_pkt.msgdata,
			      msgsize, msg->msg_iov, msg->iov_count, 0);
	}
    } else {
	void	*msgdtp = msg->msg_iov[0].iov_base;
	/* We only handle one vector length so far ! */
	if (msg->iov_count != 1) {
	    utf_printf("%s: cannot handle message vector\n", __func__);
	    fc = FI_EIO;
	    goto err4;
	}
	if (utf_msgmode != MSG_RENDEZOUS) { /* Eager in-place*/
	    DEBUG(DLEVEL_PROTO_EAGER) {
		utf_printf("EAGER INPLACE\n");
	    }
	    minfo->usrbuf = msgdtp;
	    bcopy(minfo->usrbuf, sbufp->msgbdy.payload.h_pkt.msgdata, MSG_EAGER_SIZE);
	    sbufp->msgbdy.psize = MSG_MAKE_PSIZE(MSG_EAGER_SIZE);
	    minfo->cntrtype = SNDCNTR_INPLACE_EAGER;
	    sbufp->msgbdy.ptype = PKT_EAGER;
	} else { /* Rendezvous */
	    /* rget initialization for MPI sender side */
	    minfo->usrbuf = msgdtp;
	    tofu_utf_recv_rget_expose(ctx->ctx_tinfo, minfo);
	    minfo->cntrtype = SNDCNTR_RENDEZOUS;
	    /* packet body for request remote get: msgbdy <-- rgetaddr */
	    memcpy(&RNDZ_RADDR(&sbufp->msgbdy), &minfo->rgetaddr,
		   sizeof(struct utf_vcqid_stadd)); /* my stadd's and vcqid's */
	    sbufp->msgbdy.psize = MSG_MAKE_PSIZE(sizeof(struct utf_vcqid_stadd));
	    sbufp->msgbdy.ptype = PKT_RENDZ;
	    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		utf_printf("%s: RENDEZOUS\n", __func__);
		utf_show_vcqid_stadd(&sbufp->msgbdy.payload.h_pkt.rndzdata);
	    }
	}
    }
    /* for utf progress */
    ohead = utfslist_append(&usp->smsginfo, &minfo->slst);
    DEBUG(DLEVEL_CHAIN|DLEVEL_ADHOC) utf_printf("%s: dst(%d) tag(%lx) sz(%ld)  hd(%p)\n", __func__, dst, msg->tag, msgsize, ohead);
    // utf_printf("%s: YI!!!!! ohead(%p) usp->smsginfo(%p) &minfo->slst=(%p)\n", __func__, ohead, usp->smsginfo, &minfo->slst);
    //fi_tofu_dbgvalue = ohead;
    if (ohead == NULL) { /* this is the first entry */
	rc = utf_send_start(av, vcqh, usp);
	if (rc != 0) {
	    fc = FI_EIO;
	}
    }
#if 0
    /* progress */
    utf_progress(sep->sep_dom->tinfo);
#endif
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
    struct utf_msgreq	*req = 0;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	data = msg->data;
    uint64_t	ignore = msg->ignore;
    utfslist *uexplst;

    DEBUG(DLEVEL_ADHOC|DLEVEL_CHAIN) utf_printf("%s: src(%ld)\n", __func__, src);
    //utf_printf("%s: ctx(%p)->ctx_recv_cq(%p)\n", __func__, ctx, ctx->ctx_recv_cq);
    //R_DBG("ctx(%p) msg(%s) flags(%lx) = %s",
    //ctx, tofu_fi_msg_string(msg), flags, tofu_fi_flags_string(flags));
    if (flags & FI_TAGGED) {
	uexplst =  &utf_fitag_uexplst;
	if ((flags & FI_PEEK) && ~(flags & FI_CLAIM)) {
	    /* tagged message with just FI_PEEK option */
	    peek = 1;
	} /* else, non-tagged or tagged message with FI_PEEK&FI_CLAIM option */
    } else {
	uexplst = &utf_fimsg_uexplst;
    }
    DEBUG(DLEVEL_ADHOC) {
	utf_printf("%s: exp src(%d) tag(%lx) data(%lx) ignore(%lx) flags(%s)\n",
		   __func__, src, tag, data, ignore,
		   tofu_fi_flags_string(flags));
    }
    if ((idx=tofu_utf_uexplst_match(uexplst, src, tag, ignore, peek)) != -1) {
	/* found in unexpected queue */
	size_t	sz;
	size_t  msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	uint64_t myflags;

	req = utf_idx2msgreq(idx);
	DEBUG(DLEVEL_ADHOC) utf_printf("\tfound src(%d)\n", src);
	if (req->status == REQ_WAIT_RNDZ && req->ptype) { /* rendezous */
	    struct utf_recv_cntr *ursp = req->rcntr;
	    if (peek == 1) {
		/* A control message has arrived in the unexpected queue,
		 * but not yet receiving the data at this moment.
		 * Thus, just return with FI_ENOMSG for FI_PEEK */
		goto peek_nomsg_ext;
	    }
	    assert(ursp->req == req);
	    req->fi_ctx = ctx;
	    req->fi_flgs = flags;
	    req->fi_ucontext = msg->context;
	    req->notify = tofu_catch_rcvnotify;
	    req->type = REQ_RECV_EXPECTED;
	    req->rsize = 0;
	    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		utf_printf("%s: rget_start req(%p) idx(%d) src(%ld)\n", __func__, req, idx, req->hdr.src);
	    }
	    /* Here is a multi-rail implementation.
	     *	req->rgetsender has been set by utf_recvengine.
	     */
	    utf_rget_start(ctx->ctx_tinfo, msg->msg_iov[0].iov_base, msgsize, ursp, R_DO_RNDZ);
	    if (peek == 1) {
		fc = -FI_ENOMSG;
	    }
	    goto ext;
	}
	if (req->status != REQ_DONE) {
	    /* return value is -FI_ENOMSG */
	    goto req_setup;
	}
	if (peek == 1 && (flags & FI_CLAIM)
	    && req->fi_ucontext != msg->context) {
	    /* How should we do ? */
	    utf_printf("%s: FI_PEEK&FI_CLAIM but not context matching"
		       " previous context(%p) now context(%p)\n",
		       __func__, req->fi_ucontext, msg->context);
	}
	/* received data is copied to the specified buffer */
	sz = ofi_copy_to_iov(msg->msg_iov, msg->iov_count, 0,
			     req->buf, req->rsize);
	if (peek == 0 
	    || ((flags & FI_CLAIM) && (req->fi_ucontext == msg->context))) {
	    /* reclaim unexpected resources */
#if 0	/* at this time the message has been copied */
	    if (req->fistatus == REQ_SELFSEND) {
		/* This request has been created by tofu_utf_sendmsg_self */
		/* generating completion event to sender CQ  */
		tofu_reg_sndcq(((struct tofu_ctx*)(req->fi_ctx))->ctx_send_cq,
			       req->fi_ucontext,
			       req->fi_flgs, req->rsize,
			       req->hdr.data, req->hdr.tag);
	    }
#endif
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s:\t free src(%d)\n", __func__, src);
	    utf_free(req->buf); /* allocated dynamically and must be free */
	    utf_msgreq_free(req);
	} else { /* FI_PEEK, no needs to copy message */
	    if (~(flags & FI_CLAIM)) {
		/* FI_PEEK but not FI_CLIAM */
		DEBUG(DLEVEL_ADHOC) utf_printf("%s:\t set fi_ucontext(%ld) src(%d)\n", __func__, req->fi_ucontext, src);
		req->fi_ucontext = msg->context;
	    }
	}
	//utf_printf("%s: done src(%d)\n", __func__, src);
	/* CQ is immediately generated */
	/* sender's flag or receiver's flag */
	myflags = req->hdr.flgs | flags;
	if (peek == 0 && (sz < req->rsize)) {
	    /* overrun */
	    utf_printf("%s: overrun expected size(%ld) req->rsize(%ld)\n", __func__, msgsize, req->rsize);
	    tofu_reg_rcveq(ctx->ctx_recv_cq, msg->context, myflags,
			   sz, /* received message size */
			   req->rsize - msgsize, /* overrun length */
			   FI_ETRUNC, FI_ETRUNC, /* not negative value here */
			   req->fi_msg[0].iov_base, req->hdr.data, req->hdr.tag);
	} else {
	    if (peek == 1 && msgsize == 0) {
		tofu_reg_rcvcq(ctx->ctx_recv_cq, msg->context, myflags, req->rsize,
			       req->fi_msg[0].iov_base, req->hdr.data, req->hdr.tag);
	    } else {
		tofu_reg_rcvcq(ctx->ctx_recv_cq, msg->context, myflags, sz,
			       req->fi_msg[0].iov_base, req->hdr.data, req->hdr.tag);
	    }
	}
	goto ext;
    }
req_setup:
    if (peek == 0) { /* register this request to the expected queue if it is new */
	utfslist *explst;
	struct utf_msglst *mlst;
	size_t	i;

	//utf_printf("%s: enqueu expected\n", __func__);
	if (req == 0) {
	    if ((req = utf_msgreq_alloc()) == NULL) {
		fc = -FI_ENOMEM; goto ext;
	    }
	    req->hdr.src = src;
	    req->hdr.data = data;
	    req->hdr.tag = tag;
	    req->rsize = 0;
	    req->status = R_NONE;
	    req->expsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	    req->fi_iov_count = msg->iov_count;
	    for (i = 0; i < msg->iov_count; i++) {
		req->fi_msg[i].iov_base = msg->msg_iov[i].iov_base;
		req->fi_msg[i].iov_len = msg->msg_iov[i].iov_len;
	    }
	} else {
	    /* moving unexp to exp. but no registration is needed 2020/04/25
	     * expsize is reset though req was in unexp queue */
	    req->expsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	    req->fi_iov_count = msg->iov_count;
	    for (i = 0; i < msg->iov_count; i++) {
		req->fi_msg[i].iov_base = msg->msg_iov[i].iov_base;
		req->fi_msg[i].iov_len = msg->msg_iov[i].iov_len;
	    }
	    /*
	     * 2020/04/23: checking
	     * copy so far transferred data in case of eager
	     */
	    if (req->buf) {
		ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, 0,
				req->buf, req->rsize);
		utf_free(req->buf);
		req->buf = 0;
	    }
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: rz(%ld) sz(%ld) src(%d) stat(%d) NOT MOVING\n", __func__, req->expsize, req->rsize, src, req->status);
	}
	if (req->expsize > CONF_TOFU_INJECTSIZE && utf_msgmode == MSG_RENDEZOUS) {
	    /* rendezous */
	    if (msg->iov_count > 1) {
		utf_printf("%s: iov is not supported now\n", __func__);
		fc = -FI_EAGAIN; goto ext;
	    }
	    /* receiver's memory area, bufinfo,  will be initialized at utf_rget_start */
	}
	req->type = REQ_RECV_EXPECTED;	
	req->fi_ignore = ignore;
	req->notify = tofu_catch_rcvnotify;
	req->fi_ctx = ctx;
	req->fi_flgs = flags;
	req->fistatus = 0;
	req->fi_ucontext = msg->context;

	if (req->status == R_NONE) {
	    explst = flags & FI_TAGGED ? &utf_fitag_explst : &utf_fimsg_explst;
	    mlst = utf_msglst_append(explst, req);
	    mlst->fi_ignore = ignore;
	    mlst->fi_context = msg->context;
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s:\tregexp src(%d) sz(%ld)\n", __func__, src, req->expsize);
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: YI!!!! message(size=%ld) has not arrived. register to %s expected queue\n",
			   __func__, req->expsize, explst == &utf_fitag_explst ? "TAGGED": "REGULAR");
		utf_printf("%s: Insert mlst(%p) to expected queue, fi_ignore(%lx)\n", __func__, mlst, mlst->fi_ignore);
	    }
	}
    } else {
	/* FI_PEEK with no message, Generating CQ Error */
    peek_nomsg_ext:
	/* In MPICH implementation,
	 * CQE will not be examined when -FI_ENOMSG is returned.
	 * See src/mpid/ch4/netmod/ofi/ofi_probe.h
	 *	Is this OK to generate CQE here ? 2020/04/27
	 */
	DEBUG(DLEVEL_ADHOC) {
	    utf_printf("%s:\t No message arrives. just return FI_ENOMSG. Is this OK ?\n", __func__);
	}
#if 0
	DEBUG(DLEVEL_ADHOC) {
	    utf_printf("%s:\t gen CQE src(%d) cntxt(%lx)\n",
		       __func__, src, msg->context);
	}
	tofu_reg_rcveq(ctx->ctx_recv_cq, msg->context, flags,
		       0, 0, FI_ENOMSG, -1, 0, data, req->hdr.tag);
#endif
	fc = -FI_ENOMSG;
    }
ext:
#if 0
    /* progress */
    utf_progress(ctx->ctx_sep->sep_dom->tinfo);
#endif
    return fc;
}

/*
 * This is for remote completion of fi_write. it is invoked by progress engine
 */
void
tofu_catch_rma_rmtnotify(void *fi_ctx)
{
    struct tofu_ctx *ctx = (struct tofu_ctx*) fi_ctx;

    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	utf_printf("%s: ctx(%p) class(%s)\n", __func__, ctx, tofu_fi_class_string[ctx->ctx_fid.fid.fclass]);
    }
#if 0
    if (ctx->ctx_send_cq) { /* send notify */
	tofu_reg_sndcq(ctx->ctx_send_cq, cq->fi_ucontext, flags,
		       cq->len, cq->lmemaddr, cq->data, 0);
    }
    if (ctx->ctx_send_ctr) { /* counter */
	struct tofu_cntr *ctr = ctx->ctx_send_ctr;
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	    utf_printf("%s: cntr->ctr_tsl(%d)\n", __func__, ctr->ctr_tsl);
	}
	if (ctr->ctr_tsl && !(flags & FI_COMPLETION)) {
	    goto skip;
	}
	ofi_atomic_inc64(&ctr->ctr_ctr);
    }
#endif
}

/*
 * This is for local completion of fi_read/fi_write. it is invoked by progress engine
 */
static void
tofu_catch_rma_lclnotify(struct utf_rma_cq *cq)
{
    struct tofu_ctx *ctx = cq->ctx;
    uint64_t	flags = cq->fi_flags;

    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	utf_printf("%s: RMA notification received cq(%p)->ctx(%p): addr(%lx) vcqh(%lx) "
		   "lstadd(%lx) rstadd(%lx) lmemaddr(%p) len(%lx) flags(%lx: %s) type(%d: %s)\n",
		   __func__, cq, cq->ctx, cq->addr, cq->vcqh, cq->lstadd, cq->rstadd,
		   cq->lmemaddr, cq->len, cq->fi_flags, tofu_fi_flags_string(flags),
		   cq->type, cq->type == UTF_RMA_READ ? "UTF_RMA_READ" : "UTF_RMA_WRITE");
    }
    if (ctx->ctx_send_cq) { /* send notify */
	if (cq->fi_ucontext == 0) {
	    utf_printf("%s: skipping send CQ notification\n", __func__);
	} else {
	    tofu_reg_sndcq(ctx->ctx_send_cq, cq->fi_ucontext, flags,
			   cq->len, cq->data, 0);
	}
    }
    if (ctx->ctx_send_ctr) { /* counter */
	struct tofu_cntr *ctr = ctx->ctx_send_ctr;
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	    utf_printf("%s: cntr->ctr_tsl(%d)\n", __func__, ctr->ctr_tsl);
	}
	if (flags & (FI_COMPLETION|FI_DELIVERY_COMPLETE|FI_INJECT_COMPLETE)) {
	    ofi_atomic_inc64(&ctr->ctr_ctr);
	    utf_printf("%s: ctr->ctr_ctr(%ld)\n", __func__, ctr->ctr_ctr);
	}
    }
    if (cq->lstadd) {
	struct tofu_sep	*sep = ctx->ctx_sep;
	utofu_vcq_hdl_t vcqh = sep->sep_myvcqh;
	utf_mem_dereg(vcqh, cq->lstadd);
    }
    utf_rmacq_free(cq);
}

static inline void
tofu_dbg_show_rma(const char *fname, const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t		msgsz = 0;
    unsigned char	*bp = msg->msg_iov[0].iov_base;
    char	buf[32];
    int	i;

    if (msg->msg_iov) {
	msgsz = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    }
    memset(buf, 0, 32);
    for (i = 0; i < ((msgsz > 8) ? 8 : msgsz);  i++) {
	snprintf(&buf[i*3], 4, ":%02x", bp[i]);
    }
    utf_printf("%s: YI**** RMA src(%ld) "
	       "desc(%p) msg_iov(%p) msgsz(%ld) iov_count(%ld) "
	       "rma_iov(%p) rma_iov_count(%ld) "
	       "rma_iov[0].addr(%p) rma_iov[0].len(0x%lx) rma_iov[0].key(0x%lx) "
	       "context(%p) data(%ld) flags(%lx: %s) data(%p) %s\n",
	       fname, msg->addr,
	       msg->desc, msg->msg_iov, msgsz, msg->iov_count,
	       msg->rma_iov, msg->rma_iov_count,
	       msg->rma_iov[0].addr, msg->rma_iov[0].len, msg->rma_iov[0].key,
	       msg->context, msg->data, flags, tofu_fi_flags_string(flags),
	       msg->msg_iov[0].iov_base, buf);
}

static inline ssize_t
tofu_total_rma_iov_len(const struct fi_rma_iov *rma_iov,  size_t rma_iov_count)
{                                                                                              
    size_t i;
    ssize_t len = 0;
    for (i = 0; i < rma_iov_count; i++) {
        if ((rma_iov[i].len > 0) && (rma_iov[i].addr == 0)) {
            return -FI_EINVAL; 
        }
        len += rma_iov[i].len;
    }                                                                                          
    return len;
}


static inline ssize_t
utf_rma_prepare(struct tofu_ctx *ctx, const struct fi_msg_rma *msg, uint64_t flags,
		utofu_vcq_hdl_t *vcqh, utofu_vcq_id_t *rvcqid, 
		utofu_stadd_t *lstadd, utofu_stadd_t *rstadd, uint64_t *utf_flgs,
		struct utf_rma_cq **rma_cq)
{
    ssize_t	fc = 0;
    ssize_t	msglen, rmalen;
    struct tofu_sep	*sep;
    struct tofu_av	*av;
    struct utf_rma_cq	*cq;

    msglen = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    rmalen = tofu_total_rma_iov_len(msg->rma_iov, msg->rma_iov_count);
    if (msglen != rmalen) {
	utf_printf("%s: msglen(%ld) != rmalen(%ld)\n", __func__, msglen, rmalen);
	fc = -FI_EINVAL; goto bad;
    }
    if (msg->iov_count != 0 && msg->rma_iov_count != 1) {
	utf_printf("%s: Cannot handle vector length is more than 1: "
		   "msg->iov_count(%d) msg->rma_iov_count(%d)\n", __func__, msg->iov_count, msg->rma_iov_count);
	fc = -FI_EINVAL; goto bad;
    }
    sep = ctx->ctx_sep;
    *vcqh = sep->sep_myvcqh;
    av = sep->sep_av_;
    /* convert destination fi_addr to utofu_vcq_id_t: r_vcqid */
    fc = tofu_av_lookup_vcqid_by_fia(av, msg->addr, rvcqid, utf_flgs);
    if (fc != FI_SUCCESS) goto bad;
    *rstadd = msg->rma_iov[0].key;
    *lstadd = utf_mem_reg(*vcqh, msg->msg_iov[0].iov_base, rmalen);
    if (flags & FI_REMOTE_CQ_DATA) {
	utf_printf("FI_REMOTE_CQ_DATA is not supported in RMA operations\n");
	fc = -FI_EINVAL; goto bad;
    } else if (flags & FI_INJECT) {
	utf_printf("FI_INJECT is currently not supported in RMA operations\n");
	fc = -FI_EINVAL; goto bad;
    }
    /* RMA CQ is created */
    cq = utf_rmacq_alloc();
    cq->ctx = ctx; cq->vcqh = *vcqh; cq->rvcqid = *rvcqid;
    cq->lstadd = *lstadd; cq->rstadd = *rstadd;
    cq->lmemaddr = msg->msg_iov[0].iov_base;
    cq->addr = msg->addr;
    cq->len = rmalen;
    cq->data = msg->data;
    cq->utf_flgs = *utf_flgs;
    cq->fi_flags = flags;
    cq->fi_ctx = ctx;
    cq->fi_ucontext = msg->context;
    *rma_cq = cq;
    fc = rmalen;
bad:
    return fc;
}

extern utfslist utf_wait_rmacq;

ssize_t
tofu_utf_read_post(struct tofu_ctx *ctx,
		   const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t	fc = 0;
    ssize_t	len = 0;
    uint64_t		flgs = 0;
    utofu_vcq_hdl_t	vcqh;
    utofu_vcq_id_t	rvcqid;
    utofu_stadd_t	lstadd, rstadd;
    struct utf_rma_cq	*rma_cq;

    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	utf_printf("%s: ctx(%p)->ctx_send_cq(%p) "
		   "ctx_recv_cq(%p) ctx_send_ctr(%p) ctx_recv_ctr(%p)\n",
		   __func__, ctx, ctx->ctx_send_cq, ctx->ctx_recv_cq, 
		   ctx->ctx_send_ctr, ctx->ctx_recv_ctr);
	tofu_dbg_show_rma(__func__, msg, flags);
    }

    len = utf_rma_prepare(ctx, msg, flags,
			  &vcqh, &rvcqid, &lstadd, &rstadd, &flgs, &rma_cq);
    if (len >= 0) {
	remote_get(vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA, flgs, rma_cq);
	rma_cq->notify = tofu_catch_rma_lclnotify;
	rma_cq->type = UTF_RMA_READ;
	utfslist_append(&utf_wait_rmacq, &rma_cq->slst);
    } else {
	fc = len;
    }
#if 0
    {
	int i;
	void	*tinfo = ctx->ctx_sep->sep_dom->tinfo;
	for (i = 0; i < 10; i++) {
	    utf_progress(tinfo);
	    usleep(10000);
	}
    }
#endif
    return fc;
}

ssize_t
tofu_utf_write_post(struct tofu_ctx *ctx,
		   const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t	fc = 0;
    ssize_t	len = 0;
    uint64_t		flgs = 0;
    utofu_vcq_hdl_t	vcqh;
    utofu_vcq_id_t	rvcqid;
    utofu_stadd_t	lstadd, rstadd;
    struct utf_rma_cq	*rma_cq;

    len = utf_rma_prepare(ctx, msg, flags,
			  &vcqh, &rvcqid, &lstadd, &rstadd, &flgs, &rma_cq);
    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	utf_printf("%s: calling remote_put ctx(%p)->ctx_send_cq(%p) "
		   "ctx_recv_cq(%p) ctx_send_ctr(%p) ctx_recv_ctr(%p) "
		   "vcqh(%lx) rvcqid(%lx) lstadd(%lx) rstadd(%lx) len(0x%x) edata(0x%x) cq(%p)\n",
		   __func__, ctx, ctx->ctx_send_cq, ctx->ctx_recv_cq, 
		   ctx->ctx_send_ctr, ctx->ctx_recv_ctr,
		   vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA, flgs, rma_cq);
	tofu_dbg_show_rma(__func__, msg, flags);
    }
    if (len >= 0) {
	remote_put(vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA, flgs, rma_cq);
	rma_cq->notify = tofu_catch_rma_lclnotify;
	rma_cq->type = UTF_RMA_WRITE;
	utfslist_append(&utf_wait_rmacq, &rma_cq->slst);
#if 0
	struct utf_send_cntr *usp;
	utfslist_entry	*ohead;
	usp = utf_scntr_alloc(rma_cq->addr, rma_cq->rvcqid, rma_cq->utf_flgs);
	ohead = utfslist_append(&usp->rmawaitlst, &rma_cq->slst);
	if (ohead ==NULL) {
	    utf_rmwrite_engine(vcqh, usp);
	}
#endif
    } else {
	fc = len;
    }
//#if 0
    {
	int i;
	void	*tinfo = ctx->ctx_sep->sep_dom->tinfo;
	
	for (i = 0; i < 10; i++) {
	    utf_progress(tinfo);
	    //usleep(100);
	}
    }
//#endif
    return fc;
}
