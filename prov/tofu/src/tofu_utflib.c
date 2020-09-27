/*
 * Interface Tofu <--> Utf
 */
#include "tofu_impl.h"
#include "tofu_addr.h"
#include <rdma/fabric.h>

extern void	utf_scntr_free(int idx);
extern void	utf_sendreq_free(struct utf_msgreq *req);
extern void	utf_recvreq_free(struct utf_msgreq *req);
extern int	utf_send_start(struct utf_send_cntr *usp, struct utf_send_msginfo *minfo);
extern void	utf_sendengine_prep(utofu_vcq_hdl_t vcqh, struct utf_send_cntr *usp);

extern uint8_t		utf_rank2scntridx[PROC_MAX]; /* dest. rank to sender control index (sidx) */
extern utfslist_t		utf_egr_sbuf_freelst;
extern struct utf_egr_sbuf	utf_egr_sbuf[COM_SBUF_SIZE]; /* eager send buffer per rank */
extern utofu_stadd_t	utf_egr_sbuf_stadd;
extern utfslist_t		utf_scntr_freelst;
extern struct utf_send_cntr	utf_scntr[SND_CNTRL_MAX]; /* sender control */
extern struct utf_msgreq	utf_msgrq[MSGREQ_SIZE];
extern struct utf_rma_cq	utf_rmacq_pool[COM_RMACQ_SIZE];
extern utofu_stadd_t		utf_rmacq_stadd;

__attribute__((visibility ("default"), EXTERNALLY_VISIBLE))
void	*fi_tofu_dbgvalue;

#define TFI_UTF_INIT_DONE1	0x1
#define TFI_UTF_INIT_DONE2	0x2
static int tfi_utf_initialized;

static struct utf_msgreq *dbg_fly = 0;

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
	utf_printf("%s: req.reclaim(%d)\n", __func__, req.reclaim);
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
 *	cq_e[0].err :
 *		FI_ETRUNC, FI_ECANCELED, FI_ENOMSG
 */
static inline int
tofu_reg_rcveq(struct tofu_cq *cq, void *context, uint64_t flags, size_t len,
	       size_t olen, int err, int prov_errno,
	       void *bufp, uint64_t data, uint64_t tag)
{
    int fc = FI_SUCCESS;
    struct fi_cq_err_entry cq_e[1], *comp;

    utf_printf("%s: CQERROR context(%p), flags(%s) len(%ld) data(%ld) tag(%lx)\n", __func__, context, tofu_fi_flags_string(flags), len, data, tag);
    DEBUG(DLEVEL_ADHOC) utf_printf("%s:DONE error len(%ld) olen(%ld) err(%d)\n", __func__, len, olen, err);
    if (cq->cq_rsel && !(flags & FI_COMPLETION)) {
	/* no needs to completion */
	utf_printf("%s: no receive completion is generated\n",  __func__);
	return fc;
    }
    fastlock_acquire(&cq->cq_lck);
    if (ofi_cirque_isfull(cq->cq_cceq)) {
	utf_printf("%s: YI########### cq(%p) CQE is full\n",  __func__, cq);
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
	utf_printf("%s: cq(%p) context(%p) flags = %s bufp(%p) len(%ld) data(%ld) cq_rsel(%p) buf(%s)\n",
		   __func__, cq, context, tofu_fi_flags_string(flags), bufp, len, data, cq->cq_rsel,
		   tofu_data_dump(bufp, len));
    }
    DEBUG(DLEVEL_ADHOC) utf_printf("%s:DONE len(%ld)\n", __func__, len);
    if (cq->cq_rsel && !(flags & FI_COMPLETION)) {
	/* no needs to completion */
	utf_printf("%s: YI############ no recv completion is generated\n",  __func__);
	return fc;
    }
    fastlock_acquire(&cq->cq_lck);
//    if (ofi_cirque_isfull(cq->cq_ccq)) {
    if (ofi_cirque_usedcnt(cq->cq_ccq) >= CONF_TOFU_CQSIZE) {
	/* Never hapens if configuration is correct */
	utf_printf("%s: YI############# ERROR cq(%p) CQ is full usedcnt(%ld)\n",  __func__, cq, ofi_cirque_usedcnt(cq->cq_ccq));
	utf_printf("%s: checking CONF_TOFU_CQSIZE, MSGREQ_SEND_SZ, MSGREQ_RECV_SZ\n",  __func__);
	abort();
    }
    /* FI_CLAIM is not set because no FI_BUFFERED_RECV is set*/
    cq_e->op_context	= context;
    cq_e->flags		=   FI_RECV
		| (flags & (FI_MULTI_RECV|FI_MSG|FI_TAGGED|FI_REMOTE_CQ_DATA|FI_COMPLETION));
    cq_e->len		= len;
    cq_e->data		= data;
    cq_e->tag		= tag;
    cq_e->buf		= bufp;
    /* get an entry pointed by w.p. */
    comp = ofi_cirque_tail(cq->cq_ccq);
    assert(comp != 0);
    comp[0] = cq_e[0]; /* structure copy */
    /* advance w.p. by one  */
    ofi_cirque_commit(cq->cq_ccq);
    fastlock_release(&cq->cq_lck);
    // utf_printf("%s: YI**** 2 cq(%p)->cq_ccq(%p) return\n", __func__, cq, cq->cq_ccq);
    return fc;
}


/*
 * This is for expected message. it is invoked by
 *	progress engine, recv_post, and sendmsg_self
 * Return value is only effective on MULTI_RECV
 */
static int
tofu_catch_rcvnotify(struct utf_msgreq *req, int reent)
{
    struct tofu_ctx *ctx;
    uint64_t	flags = (req->hdr.flgs & TFI_FIFLGS_CQDATA ? FI_REMOTE_CQ_DATA : 0)
			| (req->hdr.flgs & TFI_FIFLGS_TAGGED ? FI_TAGGED : 0)
			| FI_RECV;

    if (dbg_fly) {
	utf_printf("%s: CHANGED TO EXP req(%p) DONE\n", __func__, dbg_fly);
	dbg_fly = 0;
    }
    DEBUG(DLEVEL_PROTO_AM) {
	if (req->fi_flgs & FI_MULTI_RECV) {
	    utf_printf("%s: NOTIFY SRC(%d) req(%p) rsize(%ld) buf(%p)\n", __func__, req->hdr.src, req, req->rsize, req->buf);
	}
    }
    DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	utf_printf("%s: NOTIFY SRC(%d) type(%d) rsize(%ld) overrun(%d) req(%p)->buf(%p), "
		   "req->fi_flags(%s) flags(%s) iov_base(%p) iov_len(%ld) fi_recvd(%ld), ucntxt(%p)\n",
		   __func__,  req->hdr.src, req->type, req->rsize, req->overrun,
		   req, req->buf, tofu_fi_flags_string(req->fi_flgs),
		   tofu_fi_flags_string(flags),
		   req->fi_msg[0].iov_base,
		   req->fi_msg[0].iov_len, req->fi_recvd, req->fi_ucontext);
	/* utf_msghdr_string(&req->hdr, req->fi_data, req->buf) */
    }
    assert(req->type <= REQ_RECV_EXPECTED2);
    ctx = req->fi_ctx;
    /*
     * 16384 is set by fi_seopt FI_OPT_MIN_MULTI_RECV
     */
    if (req->fi_flgs&FI_MULTI_RECV
	&& (req->fi_msg[0].iov_len - req->fi_recvd - req->rsize) < 16384) {
	/* overvflow */
	flags |= FI_MULTI_RECV;
	DEBUG(DLEVEL_PROTOCOL|DLEVEL_PROTO_AM) {
	    utf_printf("%s: RAISE FI_MULTI_RECV flags(%s) req(%p) len(%ld) rcvd(%ld) + now(%ld)\n", __func__, tofu_fi_flags_string(flags), req, req->fi_msg[0].iov_len, req->fi_recvd, req->rsize);
	}
    }
    /* received data has been already copied to the specified buffer */
    if (req->overrun) {
	tofu_reg_rcveq(ctx->ctx_recv_cq, req->fi_ucontext,
		       flags,
		       req->hdr.size, /* sender expected send size */
		       req->hdr.size - req->usrreqsz, /* overrun length */
		       FI_ETRUNC, FI_ETRUNC,  /* not negative value here */
		       req->buf, /* buf is only valid for MULTI_RECV */
		       req->fi_data, req->hdr.tag);
    } else {
	tofu_reg_rcvcq(ctx->ctx_recv_cq, req->fi_ucontext,
		       flags, req->rsize,
		       req->buf, /* buf is only valid for MULTI_RECV */
		       req->fi_data, req->hdr.tag);
    }
    if (req->fi_flgs&FI_MULTI_RECV && !(flags & FI_MULTI_RECV)) {
	/* req->fi_flgs is FI_MULT_RECV and buffer is not overflow.
	 * re-enter */
	req->buf += req->rsize;
	req->fi_recvd += req->rsize;
	req->type = REQ_RECV_EXPECTED;
	req->notify = tofu_catch_rcvnotify;
	req->state = REQ_NONE;
	req->hdr = req->fi_svdhdr; /* req->hdr.size will be set at message arrival */
	req->rsize = 0;
	req->rcvexpsz = req->fi_msg[0].iov_len - req->fi_recvd; /* needs to check overrun */
	if (reent) {
	    utf_msglst_insert(&tfi_msg_explst, req);
	    DEBUG(DLEVEL_PROTOCOL|DLEVEL_PROTO_AM) {
		utf_printf("%s:\t RE-ENTER FI_MULTI_RECV req(%p)->fi_flgs(%s) SRC(%d)\n", __func__, req, tofu_fi_flags_string(req->fi_flgs), req->hdr.src);
		// utf_msglist_show("\tTFI_MSG_EXPLST:", &tfi_msg_explst);
	    }
	}
	return 1;
    } else {
	utf_recvreq_free(req);
	return 0;
    }
}

static inline void
tofu_reg_sndcq(struct tofu_cq *cq, void *context, uint64_t flags, size_t len,
	       uint64_t data, uint64_t tag, void *buf)
{
    struct fi_cq_tagged_entry cq_e[1], *comp;

    cq_e->op_context	= context;
    cq_e->flags		= (flags & (FI_TAGGED|FI_COMPLETION|FI_TRANSMIT_COMPLETE|FI_DELIVERY_COMPLETE|FI_RMA|FI_WRITE|FI_READ))
	| ((flags&FI_RMA) ? 0 : FI_SEND);
    cq_e->len		= len;
    cq_e->buf		= buf;
    cq_e->data		= data;
    cq_e->tag		= tag;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: context(%p), newflags(%s) len(%ld) data(%ld) tag(%lx) cq(%p)->cq_ccq(%p) cq_ssel(%d)\n",
		   __func__, context, tofu_fi_flags_string(cq_e->flags), len, data, tag, cq, cq->cq_ccq, cq->cq_ssel);
    }
    if ((flags & FI_INJECT)
	|| (cq->cq_ssel && !(flags & (FI_COMPLETION|FI_TRANSMIT_COMPLETE|FI_DELIVERY_COMPLETE)))) {
	DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	    utf_printf("%s: YI################ no send completion is generated\n",  __func__);
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

/*
 * The second argument is not used for send notification
 */
static int
tofu_catch_sndnotify(struct utf_msgreq *req, int dmmy)
{
    struct tofu_ctx *ctx;
    struct tofu_cq  *cq;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: SEND NOTIFY req(%p)->buf(%p)\n", __func__, req, req->buf);
    }
    if ((req->type < REQ_SND_BUFFERED_EAGER)) {
	utf_printf("%s: notification received req(%p)->type(%d) flgs(%s)\n",
		   __func__, req, req->type, tofu_fi_flags_string(req->fi_flgs));
    }
    assert(!(req->type < REQ_SND_BUFFERED_EAGER));
    ctx = req->fi_ctx;
    assert(ctx != 0);
    cq = ctx->ctx_send_cq;
    assert(cq != 0);
    tofu_reg_sndcq(cq, req->fi_ucontext, req->fi_flgs, req->hdr.size,
		   req->fi_data, req->hdr.tag, 0); /* buf is onliy valid in recv */
    utf_sendreq_free(req);
    //utf_printf("%s: YI***** 3\n", __func__);
    return 0;
}

int
tfi_utf_sendmsg_self(struct tofu_ctx *ctx,
		      const struct fi_msg_tagged *msg, uint64_t flags)
{
    utfslist_t *explst;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	data = msg->data;
    struct utf_msgreq	*snd_req, *rcv_req;
    uint64_t	msgsz;
    int	idx;
    int fc = FI_SUCCESS;

    explst = flags & FI_TAGGED ? &tfi_tag_explst : &tfi_msg_explst;
    msgsz = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC|DLEVEL_PROTO_AM) {
	utf_printf("%s: flags(%s) sz(%ld) src(%ld) tag(%lx) data(%ld) context(%p) msg(%s)\n",
		   __func__, tofu_fi_flags_string(flags), msgsz, src, tag, data,
		   msg->context, tofu_fi_msg_data(msg));
    }
    if (flags & FI_INJECT) {
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: YI################ no send completion is generated\n",  __func__);
	}
	snd_req = NULL;
    } else {
	if ((snd_req = utf_sendreq_alloc()) == NULL) { // rc = ERR_NOMORE_REQBUF;
	    DEBUG(DLEVEL_ERR) {
		utf_printf("%s: YI########### EAGAIN no more sendreq resource\n", __func__);
	    }
	    fc = -FI_EAGAIN; goto ext;
	}
    }
    if ((idx = tfi_utf_explst_match(explst, src, tag, 0)) != -1) {/* found */
	uint64_t	sndsz;

	rcv_req = utf_idx2msgreq(idx);
	if (msgsz > rcv_req->usrreqsz) {
	    sndsz = rcv_req->usrreqsz;
	    rcv_req->overrun = 1;
	} else {
	    rcv_req->overrun = 0;
	    sndsz = msgsz;
	}
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: EXPECT rcv_req(%p)->usrreqsz(%ld) msgsz(%ld) rcv_req->fi_flgs(%s) "
		       "rcv_req->fi_msg[0].iov_base(%lx) rcv_req->fi_msg[0].iov_len(%ld) "
		       "sndsz(%ld) msg->iov_count(%ld)\n",
		       __func__, rcv_req, rcv_req->usrreqsz, msgsz, tofu_fi_flags_string(rcv_req->fi_flgs),
		       rcv_req->fi_msg[0].iov_base, rcv_req->fi_msg[0].iov_len, sndsz, msg->iov_count);
	    utf_printf("%s:\tflags(%s)\n", __func__, tofu_fi_flags_string(flags));
	}
	/* sender data is copied to the specified buffer */
	if (rcv_req->fi_flgs & FI_MULTI_RECV) { /* copy to the buffer */
	    ofi_copy_from_iov(rcv_req->buf, sndsz,
			      msg->msg_iov, msg->iov_count, 0);
	} else {
	    if (rcv_req->fi_iov_count == 1) {
		/* rcv_req->fi_msg[0].iov_base <-- msg->msg_iov */
		ofi_copy_from_iov(rcv_req->fi_msg[0].iov_base, sndsz,
				  msg->msg_iov, msg->iov_count, 0);
	    } else if (msg->iov_count == 1) {
		/* rcv_req->fi_msg <-- msg->msg_iov[0].iov_base */
		ofi_copy_to_iov(rcv_req->fi_msg, rcv_req->fi_iov_count, 0,
				msg->msg_iov[0].iov_base, sndsz);
	    } else {
		/* This is a naive copy. we should optimize this copy */
		char	*cp = utf_malloc(sndsz);
		if (cp == NULL) {
		    fc = -FI_ENOMEM; goto ext;
		}
		ofi_copy_from_iov(cp, sndsz,
				  msg->msg_iov, msg->iov_count, 0);
		ofi_copy_to_iov(rcv_req->fi_msg, rcv_req->fi_iov_count, 0,
				cp, sndsz);
		utf_free(cp);
	    }
	}
	/** rcv_req->fi_flgs |= flags & (FI_REMOTE_CQ_DATA|FI_TAGGED);**/
	rcv_req->fi_data = data;
	rcv_req->rsize = sndsz; /* actual received size */
	rcv_req->hdr.src = src;
	rcv_req->hdr.flgs = MSGHDR_FLGS_FI
	    | ((flags & FI_REMOTE_CQ_DATA) ? TFI_FIFLGS_CQDATA : 0)
	    | ((flags & FI_TAGGED) ? TFI_FIFLGS_TAGGED : 0);
	rcv_req->hdr.size = msgsz;
	rcv_req->state = REQ_DONE;
	rcv_req->overrun = 0;
	if (rcv_req->notify) rcv_req->notify(rcv_req, 1);
    } else { /* insert the new req into the unexpected message queue */
	uint8_t	*cp = utf_malloc(msgsz);
	if (cp == NULL) {
	    fc = -FI_ENOMEM; goto ext;
	}
	if ((rcv_req = utf_recvreq_alloc()) == NULL) {
	    DEBUG(DLEVEL_ERR) {
		utf_printf("%s: YI########### EAGAIN no more recvreq resource\n", __func__);
	    }
	    if (snd_req) {
		utf_sendreq_free(snd_req);
	    }
	    fc = -FI_EAGAIN; goto ext;
	}
	/* This is a naive copy. we should optimize this copy */
	ofi_copy_from_iov(cp, msgsz, msg->msg_iov, msg->iov_count, 0);
	rcv_req->allflgs = 0;
	rcv_req->state = REQ_DONE;
	rcv_req->buf = cp;
	rcv_req->hdr.src = src;
	rcv_req->hdr.flgs = MSGHDR_FLGS_FI
	    | ((flags & FI_REMOTE_CQ_DATA) ? TFI_FIFLGS_CQDATA : 0)
	    | ((flags & FI_TAGGED) ? TFI_FIFLGS_TAGGED : 0);
	rcv_req->hdr.tag = tag;
	rcv_req->hdr.rndz = 0;
	rcv_req->hdr.size = msgsz;
	rcv_req->rsize = msgsz;
	rcv_req->fi_data = data;
	rcv_req->fistate = REQ_SELFSEND;
	rcv_req->type = REQ_RECV_UNEXPECTED;
	rcv_req->fi_ctx = NULL;
	rcv_req->fi_flgs = 0;
	rcv_req->fi_ucontext = NULL;
	rcv_req->rcntr = NULL;
	explst = flags & FI_TAGGED ? &tfi_tag_uexplst : &tfi_msg_uexplst;
	utf_msglst_append(explst, rcv_req);
	utf_printf("%s: req(%p) UNEXP, CHANGED TO EXP req(%p)\n", rcv_req, dbg_fly);
    }
    if (snd_req) {
	snd_req->hdr.src = src;
	snd_req->hdr.flgs = MSGHDR_FLGS_FI
	    | ((flags & FI_REMOTE_CQ_DATA) ? TFI_FIFLGS_CQDATA : 0)
	    | ((flags & FI_TAGGED) ? TFI_FIFLGS_TAGGED : 0);
	snd_req->hdr.tag = tag;
	snd_req->allflgs = 0;
	snd_req->fi_data = data;
	snd_req->hdr.size = msgsz;
	snd_req->state = REQ_NONE;
	snd_req->type = REQ_SND_BUFFERED_EAGER;
	snd_req->buf = msg->msg_iov[0].iov_base;
	snd_req->notify = tofu_catch_sndnotify;
	snd_req->fi_ctx = ctx;
	snd_req->fi_flgs = flags;
	snd_req->fi_ucontext = msg->context;
	tofu_catch_sndnotify(snd_req, 0);
    }
ext:
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
    minfo->scntr = usp;
    //QWE minfo->fi_context = msg->context;
    sbufp->pkt.hdr = minfo->msghdr; /* header */
    sbufp->pkt.pyld.fi_msg.data = msg->data; /* fi data */
    req->allflgs = 0;
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
    } else if (size <= MSG_FI_EAGER_INPLACE_SZ
	       || utf_mode_msg != MSG_RENDEZOUS
	       || (fi_flgs & FI_TAGGED) == 0) {
	if (msg->iov_count == 1) {
	    minfo->cntrtype = SNDCNTR_INPLACE_EAGER1;
	    minfo->usrbuf = msg->msg_iov[0].iov_base;
	} else {
	    //utf_printf("%s: INPLACE_EAGER. totalsize(%ld) iov_count(%d)\n", __func__, size, msg->iov_count);
	    minfo->usrbuf = utf_malloc(size);
	    ofi_copy_from_iov(minfo->usrbuf, size, msg->msg_iov, msg->iov_count, 0);
	    minfo->cntrtype = SNDCNTR_INPLACE_EAGER2;
	}
	memcpy(sbufp->pkt.pyld.fi_msg.msgdata, minfo->usrbuf, MSG_FI_PYLDSZ);
	sbufp->pkt.hdr.pyldsz = MSG_FI_PYLDSZ;
	req->type = REQ_SND_INPLACE_EAGER;
    } else { /* rendezvous */
	minfo->cntrtype = SNDCNTR_RENDEZOUS;
	if (msg->iov_count == 1) {
	    minfo->rgetaddr.nent = 1;
	    minfo->usrbuf = msg->msg_iov[0].iov_base;
	    sbufp->pkt.pyld.fi_msg.rndzdata.vcqid[0]
		= minfo->rgetaddr.vcqid[0] = usp->svcqid;
	    sbufp->pkt.pyld.fi_msg.rndzdata.stadd[0]
		= minfo->rgetaddr.stadd[0] = utf_mem_reg(utf_info.vcqh, minfo->usrbuf, size);
	    sbufp->pkt.pyld.fi_msg.rndzdata.len[0] = size;
	    sbufp->pkt.pyld.fi_msg.rndzdata.nent = 1;
	} else {
	    int	i;
	    minfo->rgetaddr.nent = 
		sbufp->pkt.pyld.fi_msg.rndzdata.nent = msg->iov_count;
	    for (i = 0; i < msg->iov_count; i++) {
		sbufp->pkt.pyld.fi_msg.rndzdata.vcqid[i]
		    = minfo->rgetaddr.vcqid[i] = usp->svcqid;
		sbufp->pkt.pyld.fi_msg.rndzdata.stadd[i]
		    = minfo->rgetaddr.stadd[i]
		    = utf_mem_reg(utf_info.vcqh,
				  msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		sbufp->pkt.pyld.fi_msg.rndzdata.len[i] = msg->msg_iov[i].iov_len;
	    }
	}
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
    int	retry;
    fi_addr_t        dst = msg->addr;
    size_t	     msgsize;
    struct utf_send_cntr *usp;
    struct utf_send_msginfo *minfo;
    struct utf_msgreq	*req;
    struct utf_egr_sbuf	*sbufp;

    // INITCHECK();
    msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: SRC DST(%d) LEN(%ld) buf(%p) flags(%s) tag(0x%lx) data(%ld) context(%p)\n",
		   __func__, dst, msgsize, msg->msg_iov[0].iov_base, tofu_fi_flags_string(flags),
		   msg->tag, msg->data, msg->context);
	/* tofu_fi_msg_data(msg) */
    }
    if ((req = utf_sendreq_alloc()) == NULL) {
	/*rc = UTF_ERR_NOMORE_REQBUF; */
	// utf_printf("%s: YI!!!!! return msgreq alloc fail: -FI_EAGAIN = %d\n", __func__, fc);
	DEBUG(DLEVEL_ERR) {
	    utf_printf("%s: YI########### EAGAIN no more recvreq resource\n", __func__);
	}
	fc = -FI_EAGAIN; goto err1;
    }
    /* av = ctx->ctx_sep->sep->sep_av */
    fc = tfi_utf_scntr_alloc(dst, &usp);
    if (fc != FI_SUCCESS) {
	// utf_printf("%s: YI!!!!! return sender cntrl fail: -FI_ENOMEM = %d\n", __func__, fc);
	goto err2;
    }
    if (usp->inflight > MSGREQ_SENDMAX_INFLIGHT) {
	/* rendevous messages more than MSGREQ_SENDMAX_INFLIGHT */
	DEBUG(DLEVEL_ERR) {
	    utf_printf("%s: YI########### EAGAIN MAX FLIGHT\n", __func__);
	}
	fc = -FI_EAGAIN;
	goto err2;
    }
    /* wait for completion of previous send massages */
    for (retry = 0; retry < 3; retry++) {
	minfo = &usp->msginfo[usp->mient];
	if (minfo->cntrtype == SNDCNTR_NONE) goto has_room;
	tfi_utf_progress(NULL);
    }
    DEBUG(DLEVEL_ERR) {
	utf_printf("%s: YI########### EAGAIN no more sendctr send area\n", __func__);
    }
    fc = -FI_EAGAIN;
    goto err2;
has_room:
    usp->mient = (usp->mient + 1) % COM_SCNTR_MINF_SZ; /* advanced */
    sbufp = (minfo->sndbuf == NULL) ?
	tfi_utf_egr_sbuf_alloc(&minfo->sndstadd) : minfo->sndbuf;
    if (sbufp == NULL) {/*rc = UTF_ERR_NOMORE_SNDBUF;*/
	utf_printf("%s: YI######## FI_ENOMEM usp(%p)->mient(%d)\n", __func__, usp, usp->mient);
	fc = -FI_ENOMEM; goto err2;
    }
    fc = minfo_setup(minfo, ctx, msg, msgsize, flags, sbufp, usp, req);
    if (fc != FI_SUCCESS) {
	/* error message was shown in minfo_setup */
	goto err2;
    }
    req->fi_data = msg->data;
    usp->inflight++;
    usp->dbg_idx = 0; /* for debugging */
    if (usp->state == S_NONE) {
	utf_send_start(usp, minfo);
    }
    /* progress */
    tfi_utf_progress(ctx->ctx_sep->sep_dom->tinfo);
    DEBUG(DLEVEL_PROTOCOL|DLEVEL_PROTO_AM) {
	utf_printf("%s: POSTED SRC DST(%d) LEN(%ld) buf(%p) flags(%s)\n",
		   __func__, dst, msgsize, msg->msg_iov[0].iov_base, tofu_fi_flags_string(flags));
	/* tofu_fi_msg_data(msg) */
    }
    return fc;
err2:
    utf_sendreq_free(req);
err1:
    /* progress */
    tfi_utf_progress(ctx->ctx_sep->sep_dom->tinfo);
#if 0
    utf_printf("%s: YI***** return error(%d:%s) usp(%p)->inflight(%d)\n",
	       __func__, fc, tofu_fi_err_string(fc), usp, usp->inflight);
#endif
    return fc;
}

static struct utf_msgreq	*claimed_req = 0;

static inline void
reqfield_setup(struct utf_msgreq *req, struct tofu_ctx *ctx, uint64_t flags, const struct fi_msg_tagged *msg,
	       void *bufp, size_t rcvsz)
{
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	ignore = msg->ignore;
    void	*ucontext = msg->context;

    req->fi_ctx = ctx;
    req->fi_flgs = flags;
    req->fi_ucontext = ucontext;
    req->buf = bufp;
    req->fi_svdhdr.hall = 0;
    req->fi_svdhdr.src = src;
    req->fi_svdhdr.tag = tag;
    req->fi_ignore = ignore;
    req->fi_recvd = rcvsz;
    req->fi_msg[0].iov_base= msg->msg_iov[0].iov_base;
    req->fi_msg[0].iov_len = msg->msg_iov[0].iov_len;
}

static void
recv_multi_progress(int idx, struct tofu_ctx *ctx,
		    const struct fi_msg_tagged *msg, uint64_t flags, size_t msgsize)
{
    struct utf_msgreq	*req = 0;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	ignore = msg->ignore;
    size_t	restsize = msgsize;
    uint8_t	*bufp = msg->msg_iov[0].iov_base;
    size_t	sz, rcvsz = 0;

    utf_printf("%s: START\n", __func__);
    do {
	if (req) {
	    /* this previous request is discarded */
	    utf_recvreq_free(req);
	}
	req = utf_idx2msgreq(idx);
	// utf_printf("%s:\t req(%p) idx(%d) rsize(%d)\n", __func__, req, idx, req->rsize);
	if (req->state != REQ_DONE) {
	    int	err = 0;
	    /* MULTI_RECV must be always Eager mode. Must be implemented 2020/09/24 */
	    /* checking if other pending unexpected messages exist */
	    while ((idx=tfi_utf_uexplst_match(&tfi_msg_uexplst, src, tag, ignore, 0)) != -1) {
		req = utf_idx2msgreq(idx);
		utf_printf("%s: ERROR This message should be sent. src(%d) size(%ld)\n",
			   __func__, req->hdr.src, req->hdr.size);
		err = 1;
	    }
	    if (err) abort();
	    /* received data is copied to the specified buffer */
	    if (restsize < req->rsize) {
		/* should never happen!! */
		utf_printf("%s: MULTIRECV OVERRUN buffer size(%ld) received size(%ld)\n", __func__, restsize, req->rsize);
		assert(restsize < req->rsize);
		sz = restsize;
		req->overrun = 1;
	    } else {
		sz = req->rsize;
		req->overrun = 0;
	    }
	    memcpy(bufp, req->buf, sz);
	    utf_free(req->buf);
	    /* keeping req->hdr */
	    reqfield_setup(req, ctx, flags, msg, bufp, rcvsz);
	    req->type = REQ_RECV_EXPECTED;
	    req->notify = tofu_catch_rcvnotify;
	    req->usrreqsz = restsize + sz;
	    /*
	     * The current recv_cntr's req, urp->req, points to this request
	     */
	    utf_printf("%s:\t CHANGED TO EXP req(%p) index(%d) rsize(%d) sendsize(%ld) rcvexpsz(%ld)\n",
		       __func__, req, utf_msgreq2idx(req), req->rsize, req->hdr.size, req->rcvexpsz);
	    dbg_fly = req;
	    if (req->rcntr != NULL && req->rcntr->req != req) {
		assert(req->rcntr != NULL && req->rcntr->req != req);
	    }
	    goto out;
	}
	/* received data is copied to the specified buffer */
	if (restsize < req->rsize) {
	    /* should never happen!! */
	    utf_printf("%s: MULTIRECV OVERRUN buffer size(%ld) received size(%ld)\n", __func__, msgsize, req->rsize);
	    assert(restsize < req->rsize);
	    req->overrun = 1;
	    req->rsize = restsize;
	}
	memcpy(bufp, req->buf, req->rsize);
	utf_free(req->buf);
	req->type = REQ_RECV_EXPECTED;
	req->notify = tofu_catch_rcvnotify;
	reqfield_setup(req, ctx, flags, msg, bufp, rcvsz);
	if (tofu_catch_rcvnotify(req, 0) == 1) {
	    bufp = req->buf;
	    rcvsz = req->fi_recvd;
	    restsize = msgsize - rcvsz;
	    /* This request will be discared if more request has been registered
	     * in unexpected queue */
	} else {
	    /* No more buffer area is available */
	    utf_printf("%s: NOMORE BUFFER\n", __func__);
	    goto out;
	}
    } while ((idx=tfi_utf_uexplst_match(&tfi_msg_uexplst, src, tag, ignore, 0)) != -1);
    /*
     * The expected request has been constructed by tofu_catch_rcvnotify()
     * Enqueue it to expected queue.
     */
    /* req->usrreqsz is checked at self message send */
    req->rcvexpsz = req->usrreqsz = restsize;
    utf_msglst_insert(&tfi_msg_explst, req);
    DEBUG(DLEVEL_PROTOCOL|DLEVEL_PROTO_AM) {
	utf_printf("%s:\t RE-ENTER FI_MULTI_RECV req(%p)->fi_flgs(%s) rcvexpsz(%ld) SRC(%d)\n",
		   __func__, req, tofu_fi_flags_string(req->fi_flgs), req->rcvexpsz, req->hdr.src);
    }
out:
    return;
}


int
tfi_utf_recv_post(struct tofu_ctx *ctx,
		  const struct fi_msg_tagged *msg, uint64_t flags)
{
    int	fc = FI_SUCCESS;
    int	idx, peek = 0, no_rm = 0;
    struct utf_msgreq	*req = 0;
    fi_addr_t	src = msg->addr;
    uint64_t	tag = msg->tag;
    uint64_t	data = msg->data;
    uint64_t	ignore = msg->ignore;
    size_t	msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
    utfslist_t *uexplst;

    if (flags & FI_TAGGED) {
	uexplst =  &tfi_tag_uexplst;
	if (flags & FI_PEEK) {
	    peek = 1;
	    /* if FI_CLAIM is also set, the entry must be removed */
	    no_rm = (flags & FI_CLAIM) ? 0 : 1;
	} else if ((flags & FI_CLAIM)
		   && (claimed_req->fi_ucontext == msg->context)) {
	    req = claimed_req; claimed_req = NULL;
	    req->fi_ctx = ctx; req->fi_ucontext = msg->context;
	    req->fi_flgs = flags;
	    DEBUG(DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
		utf_printf("%s: ENTER CLAIM SRC(%d) tag(%lx) flags(%s) context(%lx)\n",
			   __func__, src, tag, tofu_fi_flags_string(flags), msg->context);
	    }
	    goto do_claimed_req;
	}
    } else {
	uexplst = &tfi_msg_uexplst;
    }
    DEBUG(DLEVEL_PROTO_AM) {
	size_t  msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	if (src == -1) {
	    utf_printf("%s: ENTER SRC(%d) tag(%lx) size(%ld) buf(%p) data(%lx) ignore(%lx) flags(%s) peek(%d) ucntxt(%lx)\n",
		       __func__, src, tag, msgsize, msg->iov_count > 0 ? msg->msg_iov[0].iov_base : 0, data, ignore,
		       tofu_fi_flags_string(flags), peek, msg->context);
	}
    } else DEBUG(DLEVEL_PROTOCOL) {
	size_t  msgsize = ofi_total_iov_len(msg->msg_iov, msg->iov_count);
	utf_printf("%s: ENTER SRC(%d) tag(%lx) size(%ld) buf(%p) data(%lx) ignore(%lx) flags(%s) peek(%d) ucntxt(%lx)\n",
		   __func__, src, tag, msgsize, msg->iov_count > 0 ? msg->msg_iov[0].iov_base : 0, data, ignore,
		   tofu_fi_flags_string(flags), peek, msg->context);
    }
    if ((idx=tfi_utf_uexplst_match(uexplst, src, tag, ignore, no_rm)) != -1) {
	/* found in unexpected queue */
	size_t	sz;
	uint64_t myflags;

	if (flags & FI_MULTI_RECV) { /* buf is now pointing to user buffer */
	    if (msg->iov_count != 1) {
		utf_printf("%s: ERROR cannot handle IOV count (%d) in MULTI_RECV\n", __func__, msg->iov_count);
		abort();
	    }
	    recv_multi_progress(idx, ctx, msg, flags, msgsize);
	    goto ext;
	}
	req = utf_idx2msgreq(idx);
	req->fi_ctx = ctx;
	req->fi_flgs = flags;
	req->fi_ucontext = msg->context;
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("\tfound src(%d) req(%p)->state(%d) REQ_DONE(%d) req->rsize(%ld) req->hdr.size(%ld) req->rcntr(%p) req->fi_ucontext(%p)\n", src, req, req->state, REQ_DONE, req->rsize, req->hdr.size, req->rcntr, req->fi_ucontext);
	}
    do_claimed_req:
	if (req->state == REQ_WAIT_RNDZ) { /* rendezous */
	    if (peek == 1) {
		/* A control message has arrived in the unexpected queue,
		 * but not yet receiving the data at this moment. */
		myflags = (req->hdr.flgs & TFI_FIFLGS_CQDATA ? FI_REMOTE_CQ_DATA : 0);
		myflags |= (req->hdr.flgs & TFI_FIFLGS_TAGGED ? FI_TAGGED : 0);
		myflags |= flags;
		tofu_reg_rcvcq(ctx->ctx_recv_cq, msg->context, myflags, req->hdr.size,
			       req->fi_msg[0].iov_base, req->fi_data, req->hdr.tag);
		if (flags & FI_CLAIM) {
		    if (claimed_req != NULL) {
			utf_printf("%s: ###### ERROR claimed_req is not NULL\n", __func__);
			assert(claimed_req == NULL);
		    }
		    claimed_req = req;
		}
		goto ext;
	    }
	    req->notify = tofu_catch_rcvnotify;
	    req->type = REQ_RECV_EXPECTED;
	    req->buf = msg->msg_iov[0].iov_base;
	    req->usrreqsz = msgsize;
	    if (req->hdr.size != req->usrreqsz) {
		utf_printf("%s; YI###### RGET_START SENDER req(%p)->fi_ucontext(%p) SIZE(%ld) RECEIVER SIZE(%ld) "
			   "flags(%s) hdr.flgs(0x%x)\n", __func__, req, req->fi_ucontext, req->hdr.size, req->usrreqsz,
			   tofu_fi_flags_string(flags), req->hdr.flgs);
	    }
	    rget_start(req);
	    goto ext;
	}
	if (req->state != REQ_DONE) {
	    /* return value is -FI_ENOMSG */
	    goto req_setup;
	}
	/* received data is copied to the specified buffer */
	sz = ofi_copy_to_iov(msg->msg_iov, msg->iov_count, 0, req->buf, req->rsize);

	/* now user request size is set */
	req->usrreqsz = msgsize;
	if (peek == 0 && req->buf) { /* reclaim unexpected resources */
	    DEBUG(DLEVEL_PROTOCOL) utf_printf("%s:\t free src(%d) buf(%p) req->rsize(%ld) sz(%ld)\n", __func__, src, req->buf, req->rsize, sz);
	    utf_free(req->buf); /* allocated dynamically and must be free */
	    req->buf = NULL;
	}
	/* CQ is immediately generated */
	if (peek == 0) {
	    tofu_catch_rcvnotify(req, 0);
	} else {
	    /* PEEK */
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: PEEK req->state(%d) REG_RCVCQ SRC(%d) "
			   "%s, expected size(%ld) req->rsize(%ld) sz(%ld) req->hdr.size(%ld) \n",
			   __func__, req->state, req->hdr.src, msgsize != req->rsize ? "TRUNCATED" : "",
			   msgsize, req->rsize, sz, req->hdr.size);
	    }
	    if (flags & FI_CLAIM) {
		req->fi_ucontext = msg->context;
		if (claimed_req != NULL) {
		    utf_printf("%s: ###### ERROR claimed_req is not NULL\n", __func__);
		    assert(claimed_req == NULL);
		}
		claimed_req = req;
	    }
	    myflags = (req->hdr.flgs & TFI_FIFLGS_CQDATA ? FI_REMOTE_CQ_DATA : 0)
		    | (req->hdr.flgs & TFI_FIFLGS_TAGGED ? FI_TAGGED : 0)
		    | (flags&FI_MULTI_RECV);
	    tofu_reg_rcvcq(ctx->ctx_recv_cq, msg->context, myflags, req->hdr.size,
			   req->fi_msg[0].iov_base, req->fi_data, req->hdr.tag);
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
	    if ((req = utf_recvreq_alloc()) == NULL) {
		DEBUG(DLEVEL_ERR) {
		    utf_printf("%s: YI########### EAGAIN no more recvreq resource\n", __func__);
		}
		fc = -FI_EAGAIN; goto ext;
	    }
	    req->hdr.src = src;
	    /* req->hdr.size will be set at message arrival */
	    req->fi_data = data;
	    req->hdr.tag = tag;
	    req->allflgs = 0;
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
		    req->overrun = 1;
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
	    explst = (flags & FI_TAGGED) ? &tfi_tag_explst : &tfi_msg_explst;
	    if (flags & FI_MULTI_RECV) {
		req->buf = msg->msg_iov[0].iov_base;
		req->fi_recvd = 0;
		req->fi_svdhdr = req->hdr;
	    }
	    mlst = utf_msglst_append(explst, req);
	    mlst->fi_ignore = ignore;
	    //mlst->fi_context = msg->context;
	    DEBUG(DLEVEL_PROTO_AM) {
		if (src == -1) {
		    utf_printf("%s:\tAppend Explist req(%p) SRC(%d) sz(%ld) fi_flgs(%s)\n", __func__, req, src, req->usrreqsz, tofu_fi_flags_string(req->fi_flgs));
		    utf_msglist_show("\tTFI_MSG_EXPLST:", &tfi_msg_explst);
		}
	    } else DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s:\tAppend Explist req(%p) SRC(%d) sz(%ld) fi_flgs(%s)\n", __func__, req, src, req->usrreqsz, tofu_fi_flags_string(req->fi_flgs));
	    }
	}
    } else {
	/* FI_PEEK with no message, Generating CQ Error */
	/* In MPICH implementation,
	 * CQE will not be examined when -FI_ENOMSG is returned.
	 * See src/mpid/ch4/netmod/ofi/ofi_probe.h
	 *	Is this OK to generate CQE here ? 2020/04/27
	 */
	DEBUG(DLEVEL_PROTOCOL) {
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
		       cq->len, cq->lmemaddr, cq->data, 0, 0);
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
 * This is for local completion of fi_read/fi_write/fi_inject_write. it is invoked by progress engine
 */
static void
tofu_catch_rma_lclnotify(struct utf_rma_cq *cq)
{
    struct tofu_ctx *ctx = cq->ctx;
    uint64_t	flags = cq->fi_flags;

    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	utf_printf("%s: RMA notification received type(%d) cq(%p)->ctx(%p): addr(%lx) vcqh(%lx) "
		   "lstadd(%lx) rstadd(%lx) lmemaddr(%p) len(%lx) flags(%lx: %s)\n",
		   __func__, cq->type, cq, cq->ctx, cq->addr, cq->vcqh, cq->lstadd, cq->rstadd,
		   cq->lmemaddr, cq->len, cq->fi_flags, tofu_fi_flags_string(flags));
    }
    if (cq->type != UTF_RMA_WRITE_INJECT && ctx->ctx_send_cq != NULL) { /* send notify */
	if (cq->fi_ucontext == 0) {
	    DEBUG(DLEVEL_PROTO_RMA) {
		utf_printf("%s: skipping send CQ notification\n", __func__);
	    }
	} else {
	    tofu_reg_sndcq(ctx->ctx_send_cq, cq->fi_ucontext, flags,
			   cq->len, cq->data, 0, 0);
	}
    }
    if (ctx->ctx_send_ctr) { /* counter */
	struct tofu_cntr *ctr = ctx->ctx_send_ctr;
	ofi_atomic_inc64(&ctr->ctr_ctr);
	DEBUG(DLEVEL_PROTO_RMA) {
	    utf_printf("%s: COUNRTER INCREMENT ctr->ctr_ctr(%ld)\n", __func__, ctr->ctr_ctr);
	}
    }
    if (cq->type != UTF_RMA_WRITE_INJECT && cq->lstadd != 0) {
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
    DEBUG(DLEVEL_PROTO_RMA) {
	utf_printf("%s: msglen(%ld), rmalen(%ld) INJECTSIZE(%d) remote key(0x%lx)\n",
		   __func__, msglen, rmalen, CONF_TOFU_INJECTSIZE, msg->rma_iov[0].key);
    }
    if (msglen != rmalen) {
	utf_printf("%s: ERROR msglen(%ld) != rmalen(%ld)\n", __func__, msglen, rmalen);
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
	newusp = remote_get(vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA | rma_cq->mypos, flgs, rma_cq);
	if (newusp) {
	    utf_sendengine_prep(utf_info.vcqh, newusp);
	}
	rma_cq->notify = tofu_catch_rma_lclnotify;
	rma_cq->type = UTF_RMA_READ;
	// utf_rmacq_waitappend(rma_cq);
    }
    {
	int i;
	void	*tinfo = ctx->ctx_sep->sep_dom->tinfo;
	for (i = 0; i < 10; i++) {
	    tfi_utf_progress(tinfo);
	}
    }
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
	utf_printf("%s: calling remote_put ctx(%p)->ctx_send_cq(%p) "
		   "ctx_recv_cq(%p) ctx_send_ctr(%p) ctx_recv_ctr(%p) "
		   "vcqh(%lx) rvcqid(%lx) lstadd(%lx) rstadd(%lx) len(0x%x) edata(0x%x) cq(%p)\n",
		   __func__, ctx, ctx->ctx_send_cq, ctx->ctx_recv_cq, 
		   ctx->ctx_send_ctr, ctx->ctx_recv_ctr,
		   vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA, flgs, rma_cq);
	tofu_dbg_show_rma(__func__, msg, flags);
    }
    if (fc == 0) {
	remote_put(vcqh, rvcqid, lstadd, rstadd, len, EDAT_RMA | rma_cq->mypos, flgs, rma_cq);
	rma_cq->notify = tofu_catch_rma_lclnotify;
	rma_cq->type = UTF_RMA_WRITE;
	// utf_rmacq_waitappend(rma_cq);
    }
    {
	int i;
	void	*tinfo = ctx->ctx_sep->sep_dom->tinfo;
	
	for (i = 0; i < 10; i++) {
	    tfi_utf_progress(tinfo);
	}
    }
    return fc;
}

void
tofu_catch_rma_inject(struct utf_rma_cq *cq)
{
    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_PROTOCOL|DLEVEL_ADHOC) {
	utf_printf("%s: RMA WRITE INJECTION notification received cq(%p) cq->mypos(%d)\n",
		   __func__, cq, cq->mypos);
    }
    utf_rmacq_free(cq);
}

ssize_t
tfi_utf_write_inject(struct tofu_ctx *ctx, const void *buf, size_t len,
		     fi_addr_t dst, uint64_t addr, uint64_t key)
{
    ssize_t	fc = 0;
    struct tofu_sep	*sep;
    utofu_vcq_hdl_t	vcqh;
    utofu_vcq_id_t	rvcqid;
    struct utf_rma_cq	*rma_cq;

    rma_cq = utf_rmacq_alloc();
    if (rma_cq == NULL) {
	fc = -FI_ENOMEM; goto err;
    }
    rma_cq->ctx = ctx;
    rvcqid = rma_cq->rvcqid = utf_info.vname[dst].vcqid;
    sep = ctx->ctx_sep;
    vcqh = rma_cq->vcqh = sep->sep_myvcqh;
    memcpy(rma_cq->inject, buf, len);
    rma_cq->lstadd = utf_rmacq_stadd + ((uint64_t)rma_cq - (uint64_t) utf_rmacq_pool) + (uint64_t) &((struct utf_rma_cq*)0)->inject;
    remote_put(vcqh, rvcqid, rma_cq->lstadd, key, len, EDAT_RMA | rma_cq->mypos, 0, rma_cq);

    DEBUG(DLEVEL_PROTO_RMA) {
	utf_printf("%s: YI##### buf(%p) len(%ld) DST(%ld) addr(0x%lx) key=rmtadd(0x%lx) "
		   "vcqh(%lx) rvcqid(%lx) lstadd(%lx) rma_cq(%p) rma_cq->mypos(%d)\n",
		   __func__, buf, len, dst, addr, key,
		   vcqh, rvcqid, rma_cq->lstadd, rma_cq, rma_cq->mypos);
    }
    rma_cq->notify = tofu_catch_rma_lclnotify;
    rma_cq->type = UTF_RMA_WRITE_INJECT;
    rma_cq->fi_flags = FI_INJECT_COMPLETE;
    //utf_rmacq_waitappend(rma_cq); // no needs
err:
    return fc;
}

static struct utf_msgreq *
tfi_queue_match(utfslist_t *head, void *context)
{
    struct utf_msglst	*msl;
    struct utf_msgreq	*req = NULL;
    utfslist_entry_t	*cur, *prev;

    utfslist_foreach2(head, cur, prev) {
	int	idx;
	msl = container_of(cur, struct utf_msglst, slst);
	idx = msl->reqidx;
	req = utf_idx2msgreq(idx);
	if (req->fi_ucontext == context) goto find;
    }
    /* not found, go through */
    return NULL;
find:
    utfslist_remove2(head, cur, prev);
    return req;
}

int
tfi_utf_cancel(struct tofu_ctx *ctx, void *context)
{
    struct utf_msgreq	*req;

    /* no need to search on tfi_msg_explst, maybe */
    req = tfi_queue_match(&tfi_tag_explst, context);
    if (req) {
	tofu_reg_rcveq(ctx->ctx_recv_cq, req->fi_ucontext,
		       req->fi_flgs,
		       req->usrreqsz,			/* length */
		       0,				/* overrun length */
		       FI_ECANCELED, FI_ECANCELED,	/* not negative value here */
		       0,				/* bufp */
		       req->fi_data, req->hdr.tag);
	utf_recvreq_free(req);
    }
    return FI_SUCCESS;
}
