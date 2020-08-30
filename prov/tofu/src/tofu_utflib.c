/*
 * Interface Tofu <--> Utf
 */
#include "tofu_impl.h"
#include "tofu_addr.h"
#include <rdma/fabric.h>

extern void	utf_scntr_free(int idx);
extern void	utf_msgreq_free(struct utf_msgreq *req);
extern int	utf_send_start(struct utf_send_cntr *usp, struct utf_send_msginfo *minfo);
extern void	utf_sendengine_prep(utofu_vcq_hdl_t vcqh, struct utf_send_cntr *usp);

extern uint8_t		utf_rank2scntridx[PROC_MAX]; /* dest. rank to sender control index (sidx) */
extern utfslist_t		utf_egr_sbuf_freelst;
extern struct utf_egr_sbuf	utf_egr_sbuf[COM_SBUF_SIZE]; /* eager send buffer per rank */
extern utofu_stadd_t	utf_egr_sbuf_stadd;
extern utfslist_t		utf_scntr_freelst;
extern struct utf_send_cntr	utf_scntr[SND_CNTRL_MAX]; /* sender control */
extern struct utf_msgreq	utf_msgrq[REQ_SIZE];

__attribute__((visibility ("default"), EXTERNALLY_VISIBLE))
void	*fi_tofu_dbgvalue;

#define TFI_UTF_INIT_DONE1	0x1
#define TFI_UTF_INIT_DONE2	0x2
static int tfi_utf_initialized;

#define REQ_SELFSEND	1

static inline struct utf_egr_sbuf *
tfi_utf_egr_sbuf_alloc(utofu_stadd_t *stadd)
{
    utfslist_entry_t *slst = utfslist_remove(&utf_egr_sbuf_freelst);
    struct utf_egr_sbuf *uesp;
    int	pos;
    uesp = container_of(slst, struct utf_egr_sbuf, slst);
    if (!uesp) {
	utf_printf("%s: No more eager sender buffer\n", __func__);
	return NULL;
    }
    pos = uesp - utf_egr_sbuf;
    *stadd = utf_egr_sbuf_stadd + sizeof(struct utf_egr_sbuf)*pos;
    return uesp;
}

static inline int
tfi_utf_scntr_alloc(int dst, struct utf_send_cntr **uspp)
{
    int fc = FI_SUCCESS;
    struct utf_send_cntr *scp;
    uint8_t	headpos;
    headpos = utf_rank2scntridx[dst];
    // utf_printf("%s: dst(%d) headpos(0x%x)\n", __func__, dst);
    if (headpos != 0xff) {
	*uspp = &utf_scntr[headpos];
    } else {
	/* No head */
	utfslist_entry_t *slst = utfslist_remove(&utf_scntr_freelst);
	if (slst == NULL) {
	    fc = -FI_ENOMEM;
	    goto err;
	}
	scp = container_of(slst, struct utf_send_cntr, slst);
	utf_rank2scntridx[dst]  = scp->mypos;
	scp->dst = dst;
	utfslist_init(&scp->smsginfo, NULL);
	scp->state = S_NONE;
	scp->flags = 0;
	scp->svcqid = utf_info.vcqid;
	scp->rvcqid = utf_info.vname[dst].vcqid;
	scp->mient = 0;
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: dst(%d) flag_path(0x%lx)\n", __func__, dst, scp->flags);
	}
	*uspp = scp;
    }
err:
    return fc;
}

/*
 *	See also ofi_events.c in the mpich/src/mpid/ch4/netmod/ofi directory
 *		 fi_errno.h   in the include/rma directory
 *	CQ value  : FI_EAVAIL
 *	CQE value :
 *		FI_ETRUNC	265
 *		FI_ECANCELED	125
 *		FI_ENOMSG	42
 */


char *
utf_msghdr_string(struct utf_msghdr *hdrp, uint64_t fi_data, void *bp)
{
    static char	buf[128];
    snprintf(buf, 128, "src(%d) tag(0x%lx) size(%lu) data(%lu) msg(0x%lx)",
	     hdrp->src, hdrp->tag, (uint64_t) hdrp->size, fi_data,
	     bp != 0 ? *(uint64_t*) bp: 0);
    return buf;
}

void
tfi_dom_setuptinfo(struct tni_info *tinfo, struct tofu_vname *vnmp)
{
    tinfo->vnamp = vnmp;
}

int
tfi_utf_init_1(struct tofu_av *av, struct tofu_ctx *ctx, int class, struct tni_info *inf, size_t pigsz)
{
    int	myrank, nprocs, ppn;

    if (tfi_utf_initialized & TFI_UTF_INIT_DONE1) {
	return 0;
    }
    utf_printf("%s: called\n", __func__);
    tfi_utf_initialized |= TFI_UTF_INIT_DONE1;
    utf_printf("%s: calling utf_init\n", __func__);
    utf_init(0, NULL, &myrank, &nprocs, &ppn);
    utf_printf("%s: returning from utf_init\n", __func__);
    return 0;
}

void
tfi_utf_init_2(struct tofu_av *av, struct tni_info *tinfo, int nprocs)
{
    if (tfi_utf_initialized & TFI_UTF_INIT_DONE2) {
	return;
    }
    tfi_utf_initialized |= TFI_UTF_INIT_DONE2;
    {
	struct utf_msgreq	req;
	extern void utf_debugdebug(struct utf_msgreq*);
	memset(&req, 0, sizeof(req));
	utf_debugdebug(&req);
	utf_printf("%s: req.ustate(%d)\n", __func__, req.ustatus);
    }
    utf_fence();
    utf_printf("%s: UTF INITIALIZED\n", __func__);
}

void
tfi_utf_finalize(struct tni_info *tinfo)
{
    utf_printf("%s: calling utf_finalize\n", __func__);
    utf_finalize(0);
    utf_printf("%s: returning from utf_finalize\n", __func__);
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
	utf_printf("%s: notification received SRC(%d) req(%p)->buf(%p) "
		   "iov_base(%p) msg=%s\n", __func__,
		   req->hdr.src,
		   req, req->buf, req->fi_msg[0].iov_base,
		   utf_msghdr_string(&req->hdr, req->fi_data, req->buf));
    }
    assert(req->type == REQ_RECV_EXPECTED);
    ctx = req->fi_ctx;
    /* received data has been already copied to the specified buffer */
    if (req->ustatus == REQ_OVERRUN) {
	utf_printf("%s: truncated, user expected size(%ld) req(%p)->rsize(%ld) req->fistate(%d)\n", __func__, req->usrreqsz, req, req->rsize, req->fistate);
	tofu_reg_rcveq(ctx->ctx_recv_cq, req->fi_ucontext,
		       flags,
		       req->hdr.size, /* sender expected send size */
		       req->hdr.size - req->usrreqsz, /* overrun length */
		       FI_ETRUNC, FI_ETRUNC,  /* not negative value here */
		       req->fi_msg[0].iov_base, req->fi_data, req->hdr.tag);
    } else {
	if (req->usrreqsz != req->rsize) utf_printf("%s: truncated, expected size(%ld) req->rsize(%ld)\n", __func__, req->usrreqsz, req->rsize);
	tofu_reg_rcvcq(ctx->ctx_recv_cq, req->fi_ucontext,
		       flags, req->rsize,
		       req->fi_msg[0].iov_base, req->fi_data, req->hdr.tag);
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
	| ((flags&FI_RMA) ? 0 : FI_SEND);
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
    assert(!(req->type < REQ_SND_BUFFERED_EAGER));
    ctx = req->fi_ctx;
    assert(ctx != 0);
    cq = ctx->ctx_send_cq;
    assert(cq != 0);
    tofu_reg_sndcq(cq, req->fi_ucontext, req->fi_flgs, req->hdr.size,
		   req->fi_data, req->hdr.tag);
    utf_msgreq_free(req);
    //utf_printf("%s: YI***** 3\n", __func__);
}

int
tfi_utf_sendmsg_self(struct tofu_ctx *ctx,
		      const struct fi_msg_tagged *msg, uint64_t flags)
{
    utfslist_t *explst;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	data = msg->data;
    struct utf_msgreq	*req;
    uint64_t	msgsz;
    int	idx;
    int fc = FI_SUCCESS;

    explst = flags & FI_TAGGED ? &tfi_tag_explst : &tfi_msg_explst;
    msgsz = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	utf_printf("%s: flags(%s) sz(%ld) src(%ld) tag(%lx) data(%ld)\n",
		   __func__, tofu_fi_flags_string(flags), msgsz, src, tag, data);
    }
    utf_printf("%s: YI flags(%s) sz(%ld) SRC(%ld) tag(%lx) data(%ld)\n",
	       __func__, tofu_fi_flags_string(flags), msgsz, src, tag, data);

    if ((idx = tfi_utf_explst_match(explst, src, tag, 0)) != -1) {/* found */
	uint64_t	sndsz;

	req = utf_idx2msgreq(idx);
	if (msgsz > req->usrreqsz) {
	    sndsz = req->usrreqsz;
	    req->ustatus = REQ_OVERRUN;
	} else {
	    sndsz = msgsz;
	}
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: req->usrreqsz(%ld) msgsz(%ld) req->fi_flgs(%s) "
		       "req->fi_msg[0].iov_base(%lx) req->fi_msg[0].iov_len(%ld) "
		       "sndsz(%ld) msg->iov_count(%ld)\n",
		       __func__, req->usrreqsz, msgsz, tofu_fi_flags_string(req->fi_flgs),
		       req->fi_msg[0].iov_base, req->fi_msg[0].iov_len, sndsz, msg->iov_count);
	}
	/* sender data is copied to the specified buffer */
	if (req->fi_iov_count == 1) {
	    /* req->fi_msg[0].iov_base <-- msg->msg_iov */
	    ofi_copy_from_iov(req->fi_msg[0].iov_base, req->fi_msg[0].iov_len,
			      msg->msg_iov, msg->iov_count, 0);
	} else if (msg->iov_count == 1) {
	    /* req->fi_msg <-- msg->msg_iov[0].iov_base */
	    ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, 0,
			    msg->msg_iov[0].iov_base, sndsz);
	} else {
	    /* This is a naive copy. we should optimize this copy */
	    char	*cp = utf_malloc(sndsz);
	    if (cp == NULL) { fc = -FI_ENOMEM; goto err; }
	    ofi_copy_from_iov(cp, sndsz,
			      msg->msg_iov, msg->iov_count, 0);
	    ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, 0,
			    cp, sndsz);
	    utf_free(cp);
	}
	req->fi_flgs |= flags & (FI_REMOTE_CQ_DATA|FI_TAGGED);
	req->fi_data = data;
	req->rsize = msgsz;
	req->hdr.size = msgsz;
	tofu_catch_rcvnotify(req);
    } else { /* insert the new req into the unexpected message queue */
	uint8_t	*cp = utf_malloc(msgsz);
	if (cp == NULL) { fc = -FI_ENOMEM; goto err;	}
	if ((req = utf_msgreq_alloc()) == NULL) {
	    fc = -FI_ENOMEM; goto err;
	}
	/* This is a naive copy. we should optimize this copy */
	ofi_copy_from_iov(cp, msgsz, msg->msg_iov, msg->iov_count, 0);
	req->state = REQ_DONE;
	req->buf = cp;
	req->hdr.src = src;
	req->hdr.tag = tag;
	req->fi_data = data;
	req->hdr.size = msgsz;
	req->rsize = msgsz;
	req->fistate = REQ_SELFSEND;
	req->type = REQ_RECV_UNEXPECTED;
	req->fi_ctx = ctx;	/* used for completion */
	req->fi_flgs = flags;
	req->fi_ucontext = msg->context;
	explst = flags & FI_TAGGED ? &tfi_tag_uexplst : &tfi_msg_uexplst;
	utf_msglst_append(explst, req);
    }
    /* generating send notification */
    if ((req = utf_msgreq_alloc()) == NULL) { // rc = ERR_NOMORE_REQBUF;
	fc = -FI_ENOMEM; goto err;
    }
    req->hdr.src = src;
    req->hdr.tag = tag;
    req->fi_data = data;
    req->hdr.size = msgsz;
    req->state = REQ_NONE;
    req->type = REQ_SND_BUFFERED_EAGER;
    req->notify = tofu_catch_sndnotify;
    req->fi_ctx = ctx;
    req->fi_flgs = flags;
    req->fi_ucontext = msg->context;
    tofu_catch_sndnotify(req);
err:
    return fc;
}


static inline int
minfo_setup(struct utf_send_msginfo *minfo, struct tofu_ctx *ctx, const struct fi_msg_tagged *msg,
	    uint64_t size, uint64_t fi_flgs, struct utf_egr_sbuf *sbufp,
	    struct utf_send_cntr *usp, struct utf_msgreq *req)
{
    int fc = FI_SUCCESS;
    int	srnk = ctx->ctx_sep->sep_myrank;
    minfo->msghdr.src  = srnk;
    minfo->msghdr.tag  = msg->tag;
    minfo->msghdr.hall = 0;
    minfo->msghdr.size = size;
    minfo->msghdr.flgs = MSGHDR_FLGS_FI
	| ((fi_flgs & FI_REMOTE_CQ_DATA) ? TFI_FIFLGS_CQDATA : 0)
	| ((fi_flgs & FI_TAGGED) ? TFI_FIFLGS_TAGGED : 0);
    minfo->msghdr.sidx = usp->mypos; /* 8 bit */
    minfo->mreq = req;
    minfo->fi_context = msg->context;
    sbufp->pkt.hdr = minfo->msghdr; /* header */
    sbufp->pkt.pyld.fi_msg.data = msg->data; /* fi data */
    if (size <= MSG_FI_EAGER_PIGBACK_SZ) {
	minfo->cntrtype = SNDCNTR_BUFFERED_EAGER_PIGBACK;
	if (msg->iov_count > 0) { /* if 0, null message */
	    ofi_copy_from_iov(sbufp->pkt.pyld.fi_msg.msgdata,
			      size, msg->msg_iov, msg->iov_count, 0);
	}
	sbufp->pkt.hdr.pyldsz = size;
	req->type = REQ_SND_BUFFERED_EAGER;
    } else if (size <= MSG_FI_EAGER_SIZE) {
	minfo->cntrtype = SNDCNTR_BUFFERED_EAGER;
	ofi_copy_from_iov(sbufp->pkt.pyld.fi_msg.msgdata,
			  size, msg->msg_iov, msg->iov_count, 0);
	sbufp->pkt.hdr.pyldsz = size;
	req->type = REQ_SND_BUFFERED_EAGER;
    } else if (utf_mode_msg != MSG_RENDEZOUS) {
	if (msg->iov_count != 1) {
	    utf_printf("%s: cannot handle message vector\n", __func__);
	    fc = FI_EIO; goto err;
	}
	minfo->cntrtype = SNDCNTR_INPLACE_EAGER;
	minfo->usrbuf = msg->msg_iov[0].iov_base;
	memcpy(sbufp->pkt.pyld.msgdata, minfo->usrbuf, MSG_FI_PYLDSZ);
	sbufp->pkt.hdr.pyldsz = MSG_FI_PYLDSZ;
	req->type = REQ_SND_INPLACE_EAGER;
    } else { /* rendezvous */
	if (msg->iov_count != 1) {
	    utf_printf("%s: cannot handle message vector\n", __func__);
	    fc = FI_EIO; goto err;
	}
	minfo->cntrtype = SNDCNTR_RENDEZOUS;
	minfo->rgetaddr.nent = 1;
	minfo->usrbuf = msg->msg_iov[0].iov_base;
	sbufp->pkt.pyld.fi_msg.rndzdata.vcqid[0]
	    = minfo->rgetaddr.vcqid[0] = usp->svcqid;
	sbufp->pkt.pyld.fi_msg.rndzdata.stadd[0]
	    = minfo->rgetaddr.stadd[0] = utf_mem_reg(utf_info.vcqh, minfo->usrbuf, size);
	sbufp->pkt.pyld.fi_msg.rndzdata.nent = 1;
	sbufp->pkt.hdr.pyldsz = MSG_RCNTRSZ + sizeof(uint64_t); /* data area */
	sbufp->pkt.hdr.rndz = MSG_RENDEZOUS;
	req->type = REQ_SND_RENDEZOUS;
    }
    minfo->sndbuf = sbufp;
    req->hdr = minfo->msghdr; /* copy the header */
    req->state = REQ_PRG_NORMAL;
    req->notify = tofu_catch_sndnotify;
    req->fi_ctx = ctx;
    req->fi_flgs = fi_flgs;
    req->fi_ucontext = msg->context;
err:
    return fc;
}

/*
 * 
 */
int
tfi_utf_send_post(struct tofu_ctx *ctx,
		   const struct fi_msg_tagged *msg, uint64_t flags)
{
    int fc = FI_SUCCESS;
    fi_addr_t        dst = msg->addr;
    size_t	     msgsize;
    struct utf_send_cntr *usp;
    struct utf_send_msginfo *minfo;
    struct utf_msgreq	*req;
    struct utf_egr_sbuf	*sbufp;

    // INITCHECK();
    msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    DEBUG(DLEVEL_PROTO_RENDEZOUS|DLEVEL_ADHOC|DLEVEL_CHAIN) utf_printf("%s: SRC DST(%d) LEN(%ld)\n", __func__, dst, msgsize);
    if ((req = utf_msgreq_alloc()) == NULL) {
	/*rc = UTF_ERR_NOMORE_REQBUF; */
	utf_printf("%s: YI!!!!! return msgreq alloc fail: -FI_ENOMEM = %d\n", __func__, fc);
	fc = -FI_ENOMEM; goto err1;
    }
    /* av = ctx->ctx_sep->sep->sep_av */
    fc = tfi_utf_scntr_alloc(dst, &usp);
    if (fc != FI_SUCCESS) {
	utf_printf("%s: YI!!!!! return sender cntrl fail: -FI_ENOMEM = %d\n", __func__, fc);
	goto err2;
    }
    /* wait for completion of previous send massages */
retry:
    minfo = &usp->msginfo[usp->mient];
    if (minfo->cntrtype != SNDCNTR_NONE) {
	tfi_utf_progress(NULL);
	goto retry;
    }
    usp->mient = (usp->mient + 1) % COM_SCNTR_MINF_SZ;
    sbufp = (minfo->sndbuf == NULL) ?
	tfi_utf_egr_sbuf_alloc(&minfo->sndstadd) : minfo->sndbuf;
    if (sbufp == NULL) {/*rc = UTF_ERR_NOMORE_SNDBUF;*/
	utf_printf("%s: usp(%p)->mient(%d)\n", __func__, usp, usp->mient);
	fc = -FI_ENOMEM; goto err3;
    }
    fc = minfo_setup(minfo, ctx, msg, msgsize, flags, sbufp, usp, req);
    if (dst == 128 || usp->rvcqid == 0x410200000f000005) {
	utf_printf("%s: dst(%d) rvcqid(0x%lx) minfo->stadd(0x%lx) size(%ld) sbuf->stadd(0x%lx)\n",
		   __func__, dst, usp->rvcqid, minfo->rgetaddr.stadd[0], sbufp->pkt.hdr.size, sbufp->pkt.pyld.fi_msg.rndzdata.stadd[0]);
    }
    usp->dbg_idx = 0; /* for debugging */
    if (fc != FI_SUCCESS) {
	/* error message was shown in minfo_setup */
	goto err3;
    }
    if (usp->state == S_NONE) {
	utf_send_start(usp, minfo);
    }
    return fc;
err3:
    utf_scntr_free(dst);
err2:
    utf_msgreq_free(req);
err1:
    utf_printf("%s: YI***** return error(%d)\n", __func__, fc);
    return fc;
}

int
tfi_utf_recv_post(struct tofu_ctx *ctx,
		  const struct fi_msg_tagged *msg, uint64_t flags)
{
    int	fc = FI_SUCCESS;
    int	idx, peek = 0;
    struct utf_msgreq	*req = 0;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	data = msg->data;
    uint64_t	ignore = msg->ignore;
    utfslist_t *uexplst;

    DEBUG(DLEVEL_PROTO_RENDEZOUS|DLEVEL_ADHOC|DLEVEL_CHAIN) utf_printf("%s: SRC(%ld)\n", __func__, src);
    if (flags & FI_TAGGED) {
	uexplst =  &tfi_tag_uexplst;
	if ((flags & FI_PEEK) && ~(flags & FI_CLAIM)) {
	    /* tagged message with just FI_PEEK option */
	    peek = 1;
	} /* else, non-tagged or tagged message with FI_PEEK&FI_CLAIM option */
    } else {
	uexplst = &tfi_msg_uexplst;
    }
    DEBUG(DLEVEL_ADHOC) {
	utf_printf("%s: exp src(%d) tag(%lx) data(%lx) ignore(%lx) flags(%s)\n",
		   __func__, src, tag, data, ignore,
		   tofu_fi_flags_string(flags));
    }
    if ((idx=tfi_utf_uexplst_match(uexplst, src, tag, ignore, peek)) != -1) {
	/* found in unexpected queue */
	size_t	sz;
	size_t  msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	uint64_t myflags;

	req = utf_idx2msgreq(idx);
	DEBUG(DLEVEL_PROTO_RENDEZOUS|DLEVEL_ADHOC) utf_printf("\tfound src(%d) req(%p)->state(%d) REQ_DONE(%d) req->rsize(%ld) req->hdr.size(%ld) req->rcntr(%p)\n", src, req, req->state, REQ_DONE, req->rsize, req->hdr.size, req->rcntr);
	if (req->state == REQ_WAIT_RNDZ) { /* rendezous */
	    struct utf_recv_cntr *ursp = req->rcntr;
	    if (peek == 1) {
		/* A control message has arrived in the unexpected queue,
		 * but not yet receiving the data at this moment.
		 * Thus, just return with FI_ENOMSG for FI_PEEK */
		goto peek_nomsg_ext;
	    }
	    if (ursp->req != req) {
		utf_printf("%s: current ursp(%p)->req(%p) req(%p): ursp->req->hdr.src(%d) ursp->req->hdr.size(%ld) ursp->req->rsize(%ld)\n",
			   __func__, ursp, ursp->req, req, ursp->req->hdr.src, ursp->req->hdr.size, ursp->req->rsize);
		utf_printf("%s: req->hdr.src(%d) req->hdr.size(%ld) req->rsize(%ld)\n",
			   __func__, req->hdr.src, req->hdr.size, req->rsize);
	    }
	    assert(ursp->req == req);
	    req->fi_ctx = ctx;
	    req->fi_flgs = flags;
	    req->fi_ucontext = msg->context;
	    req->notify = tofu_catch_rcvnotify;
	    req->type = REQ_RECV_EXPECTED;
	    req->buf = msg->msg_iov[0].iov_base;
	    req->usrreqsz = msgsize;
	    if (req->hdr.size != req->usrreqsz) {
		utf_printf("%s; YI###### SENDER SIZE(%ld) RECEIVER SIZE(%ld)\n", __func__, req->hdr.size, req->usrreqsz);
	    }
	    rget_start(ursp, req);
	    if (peek == 1) {
		fc = -FI_ENOMSG;
	    }
	    goto ext;
	}
	if (req->state != REQ_DONE) {
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
			     req->buf, msgsize);
	/* now user request size is set */
	req->usrreqsz = msgsize;
	if (peek == 0 
	    || ((flags & FI_CLAIM) && (req->fi_ucontext == msg->context))) {
	    /* reclaim unexpected resources */
	    DEBUG(DLEVEL_PROTO_RENDEZOUS| DLEVEL_ADHOC) utf_printf("%s:\t free src(%d) buf(%p)\n", __func__, src, req->buf);
	    if (req->buf) {
		utf_free(req->buf); /* allocated dynamically and must be free */
		req->buf = NULL;
	    }
	    // utf_msgreq_free(req);
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
	myflags = (req->hdr.flgs & TFI_FIFLGS_CQDATA ? FI_REMOTE_CQ_DATA : 0);
	myflags |= (req->hdr.flgs & TFI_FIFLGS_TAGGED ? FI_TAGGED : 0);
	myflags |= flags;
	if (peek == 0 && (sz < req->rsize)) {
	    /* overrun */
	    utf_printf("%s: overrun, expected size(%ld) req->rsize(%ld)\n", __func__, msgsize, req->rsize);
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: req->state(%d) REG_RCVEQ SRC(%d)\n", __func__, req->state, req->hdr.src);
	    }
	    tofu_reg_rcveq(ctx->ctx_recv_cq, msg->context, myflags,
			   sz, /* received message size */
			   req->rsize - msgsize, /* overrun length */
			   FI_ETRUNC, FI_ETRUNC, /* not negative value here */
			   req->fi_msg[0].iov_base, req->fi_data, req->hdr.tag);
	} else {
	    if (msgsize != req->rsize) utf_printf("%s: truncated, expected size(%ld) req->rsize(%ld)\n", __func__, msgsize, req->rsize);
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: req->state(%d) REG_RCVCQ SRC(%d)\n", __func__, req->state, req->hdr.src);
	    }
	    if (peek == 1 && msgsize == 0) {
		tofu_reg_rcvcq(ctx->ctx_recv_cq, msg->context, myflags, req->rsize,
			       req->fi_msg[0].iov_base, req->fi_data, req->hdr.tag);
	    } else {
		tofu_reg_rcvcq(ctx->ctx_recv_cq, msg->context, myflags, sz,
			       req->fi_msg[0].iov_base, req->fi_data, req->hdr.tag);
	    }
	}
	goto ext;
    }
req_setup:
    if (peek == 0) { /* register this request to the expected queue if it is new */
	int	req_new = 0;
	utfslist_t *explst;
	struct utf_msglst *mlst;
	size_t	i;

	//utf_printf("%s: enqueu expected\n", __func__);
	if (req == 0) {
	    req_new = 1;
	    if ((req = utf_msgreq_alloc()) == NULL) {
		fc = -FI_ENOMEM; goto ext;
	    }
	    req->hdr.src = src;
	    /* req->hdr.size will be set at the message arrival */
	    req->fi_data = data;
	    req->hdr.tag = tag;
	    req->rsize = 0;
	    req->state = REQ_NONE;
	    req->usrreqsz = req->rcvexpsz = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	    req->fi_iov_count = msg->iov_count;
	    for (i = 0; i < msg->iov_count; i++) {
		req->fi_msg[i].iov_base = msg->msg_iov[i].iov_base;
		req->fi_msg[i].iov_len = msg->msg_iov[i].iov_len;
	    }
	    req->buf = NULL;
	} else {
	    /* moving unexp to expected state, but no registration is needed 2020/04/25
	     * expsize is reset though req was in unexp queue */
	    req->usrreqsz = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	    req->rcvexpsz = req->usrreqsz;
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
		size_t	cpysz;
		if (req->rsize > req->usrreqsz) {
		    cpysz =req->usrreqsz;
		    req->ustatus = REQ_OVERRUN;
		} else {
		    cpysz = req->rsize;
		}
		ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, 0,
				req->buf, cpysz);
		utf_free(req->buf);
		req->buf = 0;
	    }
	    DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC) utf_printf("%s: rz(%ld) reqsz(%ld) src(%d) stat(%d) NOT MOVING\n", __func__, req->usrreqsz, req->rsize, src, req->state);
	}
	if (req->usrreqsz > CONF_TOFU_INJECTSIZE && utf_mode_msg == MSG_RENDEZOUS) {
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
	req->fistate = 0;
	req->fi_ucontext = msg->context;
	if (req_new) { /* This is a new entry */
	    explst = flags & FI_TAGGED ? &tfi_tag_explst : &tfi_msg_explst;
	    mlst = utf_msglst_append(explst, req);
	    mlst->fi_ignore = ignore;
	    //mlst->fi_context = msg->context;
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s:\tExp req(%p) SRC(%d) sz(%ld)\n", __func__, req, src, req->usrreqsz);
		//utf_printf("%s: YI!!!! message(size=%ld) has not arrived. register to %s expected queue\n",
		//__func__, req->expsize, explst == &tfi_tag_explst ? "TAGGED": "REGULAR");
		//utf_printf("%s: Insert mlst(%p) to expected queue, fi_ignore(%lx)\n", __func__, mlst, mlst->fi_ignore);
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
    tfi_utf_progress(ctx->ctx_sep->sep_dom->tinfo);
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


static inline int
utf_rma_prepare(struct tofu_ctx *ctx, const struct fi_msg_rma *msg, uint64_t flags,
		utofu_vcq_hdl_t *vcqh, utofu_vcq_id_t *rvcqid, 
		utofu_stadd_t *lstadd, utofu_stadd_t *rstadd, uint64_t *utf_flgs,
		struct utf_rma_cq **rma_cq, ssize_t *lenp)
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
    *lenp = rmalen;
bad:
    return fc;
}

ssize_t
tfi_utf_read_post(struct tofu_ctx *ctx,
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

    fc = utf_rma_prepare(ctx, msg, flags,
			  &vcqh, &rvcqid, &lstadd, &rstadd, &flgs, &rma_cq, &len);
    if (fc == 0) {
	struct utf_send_cntr *newusp = NULL;
	newusp = remote_get(vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA, flgs, rma_cq);
	if (newusp) {
	    utf_sendengine_prep(utf_info.vcqh, newusp);
	}
	rma_cq->notify = tofu_catch_rma_lclnotify;
	rma_cq->type = UTF_RMA_READ;
	utf_rmacq_waitappend(rma_cq);
	//utfslist_append(&utf_wait_rmacq, &rma_cq->slst);
    }
#if 0
    {
	int i;
	void	*tinfo = ctx->ctx_sep->sep_dom->tinfo;
	for (i = 0; i < 10; i++) {
	    tfi_utf_progress(tinfo);
	    usleep(10000);
	}
    }
#endif
    return fc;
}

ssize_t
tfi_utf_write_post(struct tofu_ctx *ctx,
		   const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t	fc = 0;
    ssize_t	len = 0;
    uint64_t		flgs = 0;
    utofu_vcq_hdl_t	vcqh;
    utofu_vcq_id_t	rvcqid;
    utofu_stadd_t	lstadd, rstadd;
    struct utf_rma_cq	*rma_cq;

    fc = utf_rma_prepare(ctx, msg, flags,
			 &vcqh, &rvcqid, &lstadd, &rstadd, &flgs, &rma_cq, &len);
    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	if (fc == 0) {
	    utf_printf("%s: calling remote_put ctx(%p)->ctx_send_cq(%p) "
		       "ctx_recv_cq(%p) ctx_send_ctr(%p) ctx_recv_ctr(%p) "
		       "vcqh(%lx) rvcqid(%lx) lstadd(%lx) rstadd(%lx) len(0x%x) edata(0x%x) cq(%p)\n",
		       __func__, ctx, ctx->ctx_send_cq, ctx->ctx_recv_cq, 
		       ctx->ctx_send_ctr, ctx->ctx_recv_ctr,
		       vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA, flgs, rma_cq);
	    tofu_dbg_show_rma(__func__, msg, flags);
	}
    }
    if (fc == 0) {
	remote_put(vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA, flgs, rma_cq);
	rma_cq->notify = tofu_catch_rma_lclnotify;
	rma_cq->type = UTF_RMA_WRITE;
	utf_rmacq_waitappend(rma_cq);
    }
    {
	int i;
	void	*tinfo = ctx->ctx_sep->sep_dom->tinfo;
	
	for (i = 0; i < 10; i++) {
	    tfi_utf_progress(tinfo);
	    //usleep(100);
	}
    }
    return fc;
}
