#include <unistd.h>
#include <utofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_queue.h"
#include "utf_sndmgt.h"
#ifndef UTF_NATIVE
#include <ofi_iov.h>
#endif

extern void	tofufab_resolve_addrinfo(void *, int rank,
					 utofu_vcq_id_t *vcqid, uint64_t *flgs);
extern sndmgt	*egrmgt;
extern struct utf_msgreq	*utf_msgreq_alloc();
extern struct utf_msglst	*utf_msglst_insert(utfslist *head,
						   struct utf_msgreq *req);
    
extern int scntr2idx(struct utf_send_cntr *scp);
static struct utf_recv_cntr	rcntr[MSG_PEERS];
static utofu_vcq_id_t		vcqrcntr[MSG_PEERS];

static char *notice_symbol[] =
{	"MRQ_LCL_PUT", "MRQ_RMT_PUT", "MRQ_LCL_GET",
	"MRQ_RMT_GET", "MRQ_LCL_ARMW","MRQ_RMT_ARMW"
};

static char *rstate_symbol[] =
{	"R_FREE", "R_NONE", "R_HEAD", "R_BODY",	"R_DO_RNDZ", "R_DONE" };

static char *sstate_symbol[] =
{	"S_FREE", "S_NONE", "S_REQ_ROOM", "S_HAS_ROOM",
	"S_DO_EGR", "S_DO_EGR_WAITCMPL", "S_DONE_EGR",
	"S_REQ_RDVR", "S_RDVDON", "S_DONE", "S_WAIT_BUFREADY" };

static inline utofu_stadd_t utf_sndctr_stadd()
{
    extern utofu_stadd_t sndctrstadd;
    return sndctrstadd;
}

void
utf_engine_init()
{
    int		i;
    for (i = 0; i < MSG_PEERS; i++) {
	rcntr[i].state = R_NONE;
	rcntr[i].mypos = i;
	vcqrcntr[i] = 0;
    }
}

struct utf_recv_cntr	*
vcqid2recvcntr(utofu_vcq_id_t vcqid)
{
    int	i;
    for (i = 0; i < MSG_PEERS; i++) {
	if (vcqrcntr[i] == vcqid) {
	    return &rcntr[i];
	}
    }
    return NULL;
}

static inline int
eager_copy_and_check(struct utf_recv_cntr *ursp,
		     struct utf_msgreq *req, struct utf_msgbdy *msgp)
{
    size_t	cpysz;

    // cpysz = req->hdr.size > (req->rsize + EMSG_SIZE(msgp)) ?
    // req->rsize + EMSG_SIZE(msgp) : req->hdr.size;
    cpysz = EMSG_SIZE(msgp);
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: req->rsize(%ld) req->hdr.size(%ld) cpysz(%ld) expsize(%ld) "
		   "EMSG_SIZE(msgp)=%ld\n",
		   __func__, req->rsize, req->hdr.size, cpysz, req->expsize, EMSG_SIZE(msgp));
    }
    if ((req->rsize + cpysz) > req->expsize) { /* overrun */
	req->ustatus = REQ_OVERRUN;
    }  else {
	if (req->buf) { /* enough buffer area has been allocated */
	    bcopy(EMSG_DATA(msgp), &req->buf[req->rsize], cpysz);
	    utf_printf("%s: YIxxxxx copy: req->buf[%ld]\n", __func__, (unsigned int)req->buf[req->rsize]);
	} else {
#ifndef UTF_NATIVE /* for Fabric */
	    /* ofi_copy_to_iov(const struct iovec *iov, size_t iov_count, uint64_t iov_offset,
	       void *buf, uint64_t bufsize) */
	    uint64_t	sz;
	    sz = ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, req->rsize,
			    EMSG_DATA(msgp), cpysz);
	    if (cpysz > 0) {
		utf_printf("%s: YIxxxxx copy: iov_count(%d)\n", __func__, req->fi_iov_count);
		utf_printf("%s: YIxxxxx copy: ofi_cpy_to_iov size(%ld) cpysz(%ld) copied(%ld)\n", __func__, req->rsize, cpysz, sz);
		utf_printf("%s: msg=%d copied data = %d\n", __func__, *((int*)EMSG_DATA(msgp)), *(int*)req->fi_msg[0].iov_base);
	    }
#else
	utf_printf("%s: Something wrong in utf native mode\n", __func__);
#endif
	}
    }
    req->rsize += cpysz;
    if (req->hdr.size == req->rsize) { /* all data has come */
	req->status = REQ_DONE;
	ursp->state = R_DONE;
    } else {
	/* More data will arrive, so prepare header for future matching */
	ursp->hdr = EMSG_HDR(msgp);
	ursp->state = R_BODY;
    }
    return ursp->state;
}

/*
 * receiver engine
 */
void
utf_recvengine(void *av, utofu_vcq_id_t vcqh,
	       struct utf_recv_cntr *ursp, struct utf_msgbdy *msgp, int sidx)
{
    struct utf_msgreq	*req;
    struct utf_hpacket	*pkt = &msgp->payload.h_pkt;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: ursp(%p)->state(%s)\n",
		 __func__, ursp, rstate_symbol[ursp->state]);
    }
    switch (ursp->state) {
    case R_NONE: /* Begin receiving message */
    {
	int	idx;
	{
	    extern char	*tofu_fi_flags_string(uint64_t flags);
	    utf_printf("%s: begin receiving src(%d) tag(0x%lx) data(0x%ld) flags(%s)\n",
		       __func__, pkt->hdr.src, pkt->hdr.tag, pkt->hdr.data, tofu_fi_flags_string(pkt->hdr.flgs));
	}
#ifndef UTF_NATIVE
        utfslist *explst
	    = pkt->hdr.flgs&FI_TAGGED ? &utf_fitag_explst : &utf_fimsg_explst;
	if ((idx = tofu_utf_explst_match(explst, pkt->hdr.src, pkt->hdr.tag, 0)) != -1) {
#else
	if ((idx = utf_explst_match(pkt->hdr.src, pkt->hdr.tag, 0)) != -1) {
#endif
	    req = utf_idx2msgreq(idx);
	    req->rndz = msgp->rndz;
	    if (req->rndz == MSG_RENDEZOUS) goto rendezous;
	    /* eager */
	    req->hdr.size = pkt->hdr.size;
#ifndef UTF_NATIVE
	    req->hdr.tag = pkt->hdr.tag;
	    req->hdr.data = pkt->hdr.data;
	    req->hdr.flgs = pkt->hdr.flgs;
#endif
	    if (eager_copy_and_check(ursp, req, msgp) == R_DONE) goto done;
	} else { /* New Unexpected message */
	    req = utf_msgreq_alloc();
	    req->hdr = pkt->hdr;
	    req->rsize = 0; req->ustatus = 0; req->type = REQ_RECV_UNEXPECTED;
	    req->rndz = msgp->rndz;
	    utf_printf("%s: new unexpected message arrives. new req(%p)->rndz=%d\n", __func__, req, req->rndz);
	    if (req->rndz == MSG_RENDEZOUS) {
		req->rmtstadd = (utofu_stadd_t) pkt->msgdata;
		goto rendezous;
	    } else {/* eager */
		req->expsize = pkt->hdr.size;
		req->buf = malloc(pkt->hdr.size);
		if (eager_copy_and_check(ursp, req, msgp) == R_DONE) goto done;
	    }
	}
	ursp->req = req;
	goto exiting;
    rendezous:
	{
	    size_t		reqsize = req->hdr.size;
	    utofu_stadd_t	rmtstadd = *(utofu_stadd_t*) &pkt->msgdata;
	    if (ursp->initialized == 0) {
		ursp->hdr = req->hdr;
		ursp->req = req;
		tofufab_resolve_addrinfo(av, ursp->hdr.src, &ursp->svcqid,
					 &ursp->flags);
		ursp->initialized = 1;
	    }
	    ursp->hdr = req->hdr;
	    ursp->req = req;
	    ursp->state = R_DO_RNDZ;
	    req->bufstadd = utf_mem_reg(vcqh, req->buf, reqsize);
	    /* req->hdr.size is defined by the receiver
	     * pkt->hdr.size is defined by the sender */
	    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		utf_printf("%s: Receiving Rendezous reqsize(0x%lx) "
			 "local stadd(0x%lx) remote stadd(0x%lx) "
			 "edata(%d) mypos(%d)\n",
			 __func__, reqsize, req->bufstadd,
			 rmtstadd, sidx, ursp->mypos);
	    }
	    vcqrcntr[ursp->mypos] = ursp->svcqid;
	    remote_get(vcqh, ursp->svcqid, req->bufstadd,
		       rmtstadd, reqsize, sidx, ursp->flags, 0);
	}
    }
	break;
    case R_BODY:
	req = ursp->req;
	if (eager_copy_and_check(ursp, req, msgp) == R_DONE) goto done;
	/* otherwise, continue to receive message */
	break;
    case R_DO_RNDZ: /* R_DO_RNDZ --> R_DONE */
    { /* Be sure no msgp pointer is passed from the mrqprogress engine */
	utofu_stadd_t	stadd = SCNTR_ADDR_CNTR_FIELD(sidx);
	DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	    utf_printf("%s: R_DO_RNDZ done ursp(%p)\n", __func__, ursp);
	}
	/*
	 * notification to the sender: local mrq notification is supressed
	 * currently local notitication is enabled
	 */
	utf_remote_armw4(vcqh, ursp->svcqid, ursp->flags,
			 UTOFU_ARMW_OP_OR, SCNTR_OK,
			 stadd + SCNTR_RGETDONE_OFFST, sidx, 0);
	req = ursp->req;
	if (req->bufstadd) {
	    utf_mem_dereg(vcqh, req->bufstadd);
	    req->bufstadd = 0;
	}
	req->rsize = req->hdr.size;
	req->status = REQ_DONE;
    }	/* Falls through. */
    case R_DONE: done:
	if (req->type == REQ_RECV_UNEXPECTED) {
	    /* Regiger it to unexpected queue */
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: register it to unexpected queue\n", __func__);
	    }
#ifndef UTF_NATIVE
	    {
		utfslist *uexplst
		  = pkt->hdr.flgs&FI_TAGGED ? &utf_fitag_uexplst : &utf_fimsg_uexplst;
		utf_msglst_insert(uexplst, req);
		utf_printf("%s: register req(%p) to unexpected queue(%p)\n", __func__, req, uexplst);
	    }
#else
	    utf_msglst_insert(&utf_uexplst, req);
#endif
	} else {
	    utf_printf("%s: Expected message arrived (idx=%d)\n", __func__,
		       utf_msgreq2idx(req));
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: Expected message arrived (idx=%d)\n", __func__,
			 utf_msgreq2idx(req));
	    }
	    if (req->notify) req->notify(req);
	}
	/* reset the state */
	ursp->state = R_NONE;
	break;
    case R_HEAD:
    default:
	break;
    }
    exiting:;
}

static inline utofu_stadd_t
calc_recvstadd(struct utf_send_cntr *usp, uint64_t ridx, size_t ssize)
{
    utofu_stadd_t	recvstadd = 0;
    if ((usp->recvoff + ssize) > MSGBUF_SIZE) {
	/* buffer full */
	//utf_printf("%s: YI** WAIT usp->rcvreset(%d) usp->recvoff(%d)\n",
	//__func__, usp->rcvreset, usp->recvoff);
	usp->state = S_WAIT_BUFREADY;
    } else {
	recvstadd  = erbstadd
	    + (uint64_t)&((struct erecv_buf*)0)->rbuf[MSGBUF_SIZE*ridx]
	    + usp->recvoff;
    }
    return recvstadd;
}

char *str_evnt[EVT_END] = {
    "EVT_START", "EVT_LCL", "EVT_RMT", "EVT_CONT"
};

/*
 * sender engine
 *		
 */
void
utf_sendengine(utofu_vcq_id_t vcqh, struct utf_send_cntr *usp, uint64_t rslt, int evt)
{
    utfslist_entry		*slst;
    struct utf_send_msginfo	*minfo;

    utf_printf("%s: usp(%p)->state(%s), evt(%d) rcvreset(%d) recvoff(%d)\n",
	       __func__, usp, sstate_symbol[usp->state],
	       evt, usp->rcvreset, usp->recvoff);
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: usp(%p)->state(%s), evt(%d) rcvreset(%d) recvoff(%d)\n",
		 __func__, usp, sstate_symbol[usp->state],
		 evt, usp->rcvreset, usp->recvoff);
    }
    if (evt == EVT_RMT_RECVRST) {
	if (usp->rcvreset) {
	    usp->rcvreset = 0;  usp->recvoff = 0;
	    if (usp->state == S_WAIT_BUFREADY) {
		usp->state = S_HAS_ROOM;
		goto progress0;
	    }
	}
	return;
    } else if (usp->state == S_REQ_RDVR
	       && (evt == EVT_RMT_RGETDON || evt == EVT_RMT_GET)) {
	usp->rgetwait++;
	if (usp->rgetwait == 2) { /* remote completion from the receiver */
	    usp->state = S_RDVDONE;
	    goto progress0;
	}
	return;
    }
progress0:
    slst = utfslist_head(&usp->smsginfo);
    minfo = container_of(slst, struct utf_send_msginfo, slst);
    if (slst == NULL) {
	utf_printf("%s: why ? slst == NULL\n", __func__);
	return;
    }
progress:
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: usp(%p)->state(%d), evt(%d) minfo(%p)\n",
		 __func__, usp, usp->state, evt, minfo);
    }
    switch(usp->state) {
    case S_NONE: /* never comes */
	break;
    case S_REQ_ROOM:
	if ((int64_t)rslt >= 0) {
	    utf_printf("%s: YI*** set sendok for dst(%d) and index(%d))\n",  __func__, usp->dst, (unsigned)rslt);
	    sndmgt_set_sndok(usp->dst, egrmgt);
	    sndmgt_set_index(usp->dst, egrmgt, rslt);
	    usp->state = S_HAS_ROOM;
	    usp->recvoff = 0;
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("\tRoom is available in remote rank\n");
	    }
	    goto s_has_room;
	} else {
	    /* No room in remote rank */
	    utf_printf("No room in remote rank\n");
	    /* how to do */
	}
	break;
    case S_HAS_ROOM: /* 3 */
    s_has_room:
    {
	utofu_vcq_id_t	rvcqid = usp->rvcqid;
	unsigned long	flgs = usp->flags;
	uint64_t	ridx = sndmgt_get_index(usp->dst, egrmgt);
	size_t		ssize = minfo->sndbuf->msgbdy.psize;
	utofu_stadd_t	recvstadd;

	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Going to send rvcqid(%lx) size(%ld) "
		     "type(%d) sidx(%d) ridx(%d)\n",
		     __func__, rvcqid, ssize, minfo->cntrtype, usp->mypos, ridx);
	}
	recvstadd = calc_recvstadd(usp, ridx, ssize);
	if (recvstadd == 0) {
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: going to sleep\n", __func__);
	    }
	    break;
	}
	utf_printf("%s: Going to send rvcqid(%lx) size(%ld) "
		   "type(%d) sidx(%d) ridx(%d) recvstadd(%lx) minfo->cntrtype(%d)\n",
		   __func__, rvcqid, ssize, minfo->cntrtype, usp->mypos, ridx, recvstadd, minfo->cntrtype);
	switch (minfo->cntrtype) {
	case SNDCNTR_BUFFERED_EAGER:
	    if (ssize <= utf_pig_size) {
		/* pig_size might be 32 byte and the header size is 16 byte
		 * + 2 byte for packet size.
		 * So 14 byte payload is here  */
		remote_piggysend(vcqh, rvcqid, &minfo->sndbuf->msgbdy,
				 recvstadd, ssize, usp->mypos, flgs, minfo);
	    } else {
		remote_put(vcqh, rvcqid, minfo->sndstadd,
			   recvstadd, ssize, usp->mypos, flgs, minfo);
	    }
	    usp->recvoff += ssize;
	    usp->psize = ssize; /* packet level */
	    usp->usize = MSG_CALC_EAGER_USIZE(ssize);
	    usp->state = S_DONE_EGR;
	    utf_printf("%s: next usp(%p)->state = %s\n", __func__, usp, sstate_symbol[usp->state]);
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_showpacket("sender packet buffer", &minfo->sndbuf->msgbdy);
		utf_printf("%s: next usp(%p)->state = %d\n",
			 __func__, usp, usp->state);
	    }
	    break;
	case SNDCNTR_INPLACE_EAGER:
	    usp->psize = ssize;
	    usp->usize = MSG_EAGER_SIZE;
	    usp->state = S_DO_EGR;
	    remote_put(vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, ssize, usp->mypos, flgs, 0);
	    usp->recvoff += ssize;
	    break;
	case SNDCNTR_RENDEZOUS:
	    usp->psize = ssize;
	    usp->usize = MSG_EAGER_SIZE;
	    usp->state = S_REQ_RDVR;
	    remote_put(vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, ssize, usp->mypos, flgs, 0);
	    usp->recvoff += ssize;
	    usp->rgetwait = 0; usp->rgetdone = 0;
	    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		utf_printf("%s: rendezous send size(%ld) minfo->sndstadd(%lx) "
			 "src(%d) tag(%d) rndz(%d)\n", __func__,
			 ssize, minfo->sndstadd,
			 minfo->sndbuf->msgbdy.payload.h_pkt.hdr.src,
			 minfo->sndbuf->msgbdy.payload.h_pkt.hdr.tag,
			 minfo->sndbuf->msgbdy.rndz);
	    }
	    break;
	default:
	    utf_printf("%s: BUG!!! Unknown ctrltype(%d)\n", __func__, minfo->cntrtype);
	}
	break;
    }
    case S_DO_EGR: /* eager message */
    {
	utofu_vcq_id_t rvcqid = usp->rvcqid;
	unsigned long flgs = usp->flags;
#if 0 /* The eager in place mode progressing is driven by local completion */
	if (evt == EVT_LCL) { /* local completion is ignored in this state */
	    return;
	}
#endif
	//usleep(1000);
	if (minfo->cntrtype == SNDCNTR_INPLACE_EAGER) {
	    utofu_stadd_t recvstadd;
	    size_t	rest, usize, ssize;
	    uint64_t	ridx = sndmgt_get_index(usp->dst, egrmgt);

	    rest = minfo->msghdr.size - usp->usize;
	    usize = rest > MSG_EAGER_SIZE ? MSG_EAGER_SIZE : rest;
	    /*
	     * copy to eager send buffer. minfo->sndbuf is minfo->sndstadd
	     * in stearing address.
	     */
	    bcopy((char*) minfo->usrbuf + usp->usize,
		  minfo->sndbuf->msgbdy.payload.h_pkt.msgdata, usize);
	    minfo->sndbuf->msgbdy.psize = ssize = MSG_MAKE_PSIZE(usize);
	    recvstadd = calc_recvstadd(usp, ridx, ssize);
	    if (recvstadd == 0) {
		DEBUG(DLEVEL_PROTOCOL) {
		    utf_printf("%s: S_DO_EGR, mreq(%p) msgsize(%ld) sentsize(%ld)"
			     " rest(%ld) sending usize(%ld) ssize(%ld) "
			     "recvstadd(0x%lx) going to sleep\n",
			     __func__, minfo->mreq, minfo->msghdr.size,
			     usp->usize, rest, usize, ssize, recvstadd);
		}
		break;
	    }
	    DEBUG(DLEVEL_PROTO_EAGER) {
		utf_printf("%s: S_DO_EGR, mreq(%p) msgsize(%ld) sentsize(%ld) "
			 "rest(%ld) sending usize(%ld) packet ssize(%ld) "
			 "recvstadd(0x%lx) usp->recvoff(%d)\n",
			 __func__, minfo->mreq, minfo->msghdr.size, usp->usize,
			 rest, usize, ssize, recvstadd, usp->recvoff);
	    }
	    remote_put(vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, ssize, usp->mypos, flgs, 0);
	    usp->psize += ssize;
	    usp->usize += usize;
	    usp->recvoff += ssize;
	    if (minfo->msghdr.size == usp->usize) {
		usp->state = S_DONE_EGR;
	    }
	} else { /* Something wrong */
	    utf_printf("%s: protocol error\n", __func__); exit(-1);
	}
	break;
    }
    case S_REQ_RDVR:
	DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	    utf_printf("%s: S_REQ_RDVR evt(%s)\n", __func__, str_evnt[evt]);
	}
	if (evt == EVT_LCL) { /* completion of the remote put */
	    /* nothing */
	} else {
	    utf_printf("%s: Should never come here. remove this case\n",
		     __func__);
#if 0
	    usp->rgetwait++;
	    if (usp->rgetwait == 2) {
		/* remote completion from the receiver */
		usp->state = S_RDVDONE;
		goto rdvdone;
	    } else {
		/* wait for receiving rget */
	    }
#endif
	}
	break;
    case S_DO_EGR_WAITCMPL:
	utf_printf("%s: S_DO_EGR_WAITCMPL rcvreset(%d)\n", __func__, usp->rcvreset);
	break;
    case S_RDVDONE:
	if (minfo->mreq->bufstadd) {
	    utf_mem_dereg(vcqh, minfo->mreq->bufstadd);
	    minfo->mreq->bufstadd = 0;
	}
	//usleep(10000);
	/* Falls through. */
    case S_DONE_EGR:
    {
	struct utf_msgreq	*req = minfo->mreq;
	req->status = REQ_DONE;
	minfo->mreq = NULL;
	utfslist_remove(&usp->smsginfo);
	slst = utfslist_head(&usp->smsginfo);
	utf_sndminfo_free(minfo);
	if (req->notify) req->notify(req);
	if (slst != NULL) {
	    minfo = container_of(slst, struct utf_send_msginfo, slst);
	    usp->state = S_HAS_ROOM;
	    evt = EVT_CONT;
	    goto progress;
	}
	break;
    }
    case S_DONE:
	break;
    case S_WAIT_BUFREADY:
	if (usp->rcvreset) {
	    usp->rcvreset = 0;
	    usp->recvoff = 0;
	    usp->state = S_HAS_ROOM;
	    //utf_printf("%s: YI*** Sender rcvreset during wait\n", __func__);
	    goto s_has_room;
	} else {
	    utf_printf("%s: protocol error in S_WAIT_BUFREADY\n", __func__);
	}
	break;
    }
    utf_printf("%s: return\n", __func__);
}

int
utf_send_start(utofu_vcq_id_t vcqh, struct utf_send_cntr *usp)
{
    int	dst = usp->dst;
    if (sndmgt_isset_examed(dst, egrmgt) == 0) {
	/*
	 * checking availability
	 *   edata: usp->mypos for mrqprogress
	 */
	utf_printf("%s: Request a room to rank %d: send control(%d) <- edata\n"
		   "\trvcqid(%lx) remote stadd(%lx)\n",
		   __func__, dst, usp->mypos, usp->rvcqid, erbstadd);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Request a room to rank %d: send control(%d)\n"
		     "\trvcqid(%lx) remote stadd(%lx)\n",
		     __func__, dst, usp->mypos, usp->rvcqid, erbstadd);
	}
	utf_remote_add(vcqh, usp->rvcqid,
		       UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE,
		       -1, erbstadd, usp->mypos, 0);
	usp->state = S_REQ_ROOM;
	sndmgt_set_examed(dst, egrmgt);
	return 0;
    } else if (sndmgt_isset_sndok(dst, egrmgt) != 0) {
	utf_printf("%s: YI** Has a received room in rank %d: send control(%d)\n",
		   __func__, dst, usp->mypos);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Has a received room in rank %d: send control(%d)\n",
		     __func__, dst, usp->mypos);
	}
	usp->state = S_HAS_ROOM;
	utf_sendengine(vcqh, usp, 0, EVT_START);
	return 0;
    } else {
	utf_printf("%s: ERROR!!!! dst(%d)\n", __func__, dst);
	abort();
    }
}

int
utf_mrqprogress(void *av, utofu_vcq_hdl_t vcqh)
{
    int	rc;
    struct utofu_mrq_notice mrq_notice;

    rc = utofu_poll_mrq(vcqh, 0, &mrq_notice);
    if (rc == UTOFU_ERR_NOT_FOUND) return rc;
    if (rc != UTOFU_SUCCESS) {
	utf_printf("%s: utofu_poll_mrq ERROR rc(%d)\n", __func__, rc);
	return rc;
    }
    DEBUG(DLEVEL_UTOFU) {
	utf_printf("%s: type(%s)\n", __func__,
		 notice_symbol[mrq_notice.notice_type]);
    }
    switch(mrq_notice.notice_type) {
    case UTOFU_MRQ_TYPE_LCL_PUT: /* 0 */
    {	/* Sender side: edata is index of sender engine */
	struct utf_send_cntr *usp;
	int	sidx = mrq_notice.edata;
	usp = utf_idx2scntr(sidx);
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_TYPE_LCL_PUT: edata(%d) rmt_val(%ld)"
		     "vcq_id(%lx) usp(%p)\n",
		     __func__, mrq_notice.edata, mrq_notice.rmt_value,
		     mrq_notice.vcq_id, usp);
	}
	utf_sendengine(vcqh, usp, 0, EVT_LCL);
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_PUT: /* 1 */
    {
	/* rmt_vlue is the last stadd address of put data */
	uint64_t		entry = ERECV_INDEX(mrq_notice.rmt_value);
	struct utf_recv_cntr	*ursp = &rcntr[entry];
	int			sidx = mrq_notice.edata;
	struct utf_msgbdy	*msgp;

	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_TYPE_RMT_PUT: edat(%ld) rmtval(%lx) "
		     "rmt_stadd(%lx) vcqid(%lx) entry(%d)\n",
		     __func__, mrq_notice.edata, mrq_notice.rmt_value,
		     mrq_notice.rmt_stadd,
		     mrq_notice.vcq_id, entry);
	}
	if (ERECV_LENGTH(mrq_notice.rmt_value) < ursp->recvoff) {
	    /* the data might be the top */
	    DEBUG(DLEVEL_PROTO_EAGER) {
		utf_printf("%s: reset recvoff and rst_sent\n", __func__);
	    }
	    ursp->recvoff = 0;
	    ursp->rst_sent = 0;
	}
	msgp = utf_recvbuf_get(entry);
	msgp = (struct utf_msgbdy *) ((char*)msgp + ursp->recvoff);
	utf_printf("%s: MRQ_TYPE_RMT_PUT: edat(%ld) rmtval(%lx) "
		   "rmt_stadd(%lx) vcqid(%lx) entry(%d) msgp(%p)\n",
		   __func__, mrq_notice.edata, mrq_notice.rmt_value,
		   mrq_notice.rmt_stadd,
		   mrq_notice.vcq_id, entry, msgp);
	utf_recvengine(av, vcqh, ursp, msgp, sidx);
	ursp->recvoff += msgp->psize;
	if (ursp->rst_sent == 0 && ursp->recvoff > MSGBUF_THR) {
	    utofu_stadd_t	stadd = SCNTR_ADDR_CNTR_FIELD(sidx);
	    /* rcvreset field is set */
	    if (ursp->svcqid == 0) {
		tofufab_resolve_addrinfo(av, EMSG_HDR(msgp).src,
					 &ursp->svcqid, &ursp->flags);
	    }
	    DEBUG(DLEVEL_UTOFU) {
		utf_printf("%s: reset request to sender "
			 "svcqid(%lx) flags(%lx) stadd(%lx) sidx(%d)\n",
			 __func__, ursp->svcqid, ursp->flags, stadd, sidx);
	    }
	    utf_remote_armw4(vcqh, ursp->svcqid, ursp->flags,
			     UTOFU_ARMW_OP_OR, SCNTR_OK,
			     stadd + SCNTR_RST_RECVRESET_OFFST, sidx, 0);
	    ursp->rst_sent = 1;
	} else if (ursp->recvoff > MSGBUF_SIZE) {
	    utf_printf("%s: receive buffer overrun\n", __func__);
	    abort();
	}
	break;
    }
    case UTOFU_MRQ_TYPE_LCL_GET: /* 2 */
    {
	int	sidx = mrq_notice.edata;
	struct utf_recv_cntr	*ursp = vcqid2recvcntr(mrq_notice.vcq_id);
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_TYPE_LCL_GET: vcq_id(%lx) edata(%d) "
		     "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		     __func__, mrq_notice.vcq_id, mrq_notice.edata,
		     mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	}
	if (ursp->state != R_DO_RNDZ) {
	    utf_printf("%s: protocol error, expecting R_DO_RNDZ but %d\n"
		     "\tMRQ_TYPE_LCL_GET: vcq_id(%lx) edata(%d) "
		     "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		     __func__, mrq_notice.vcq_id, mrq_notice.edata,
		     mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	    break;
	}
	utf_recvengine(av, vcqh, ursp, NULL, sidx);
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_GET: /* 3 */
    {	/* Sender side: edata is index of sender engine */
	struct utf_send_cntr *usp;
	int	sidx = mrq_notice.edata;
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_TYPE_RMT_GET: edata(%d) "
		     "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		     __func__, mrq_notice.edata,
		     mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	}
	usp = utf_idx2scntr(sidx);
	utf_sendengine(vcqh, usp, mrq_notice.rmt_stadd, EVT_RMT_GET);
    }
	break;
    case UTOFU_MRQ_TYPE_LCL_ARMW:/* 4 */
    {	/* Sender side:
	 * rmt_value is a result of this atomic operation.
	 * edata is index of sender engine */
	struct utf_send_cntr *usp;
	int	evtype;
	int	sidx = mrq_notice.edata;
	usp = utf_idx2scntr(sidx);
	utf_printf("%s: MRQ_LCL_ARM: edata(%d) rmt_val(%ld) usp(%p)\n",
		   __func__, mrq_notice.edata, mrq_notice.rmt_value, usp);
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_LCL_ARM: edata(%d) rmt_val(%ld) usp(%p)\n",
		     __func__, mrq_notice.edata, mrq_notice.rmt_value, usp);
	}
	if (is_scntr(mrq_notice.rmt_stadd, &evtype)) {
	    /* this is for a receiver's remote OR operation  */
	} else {
	    utf_sendengine(vcqh, usp, mrq_notice.rmt_value, EVT_LCL);
	}
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_ARMW:/* 5 */
    {
	int	evtype;
	/* Receiver side:
	 * rmt_value is the stadd address changed by the sender
	 * edata represents the sender's index of sender engine 
	 * The corresponding notification in the sender side is
	 * UTOFU_MRQ_TYPE_LCL_ARMW */
	if (is_scntr(mrq_notice.rmt_value, &evtype)) {
	    /* access utf_ send control structure */
	    struct utf_send_cntr *usp;
	    int			sidx = mrq_notice.edata;
	    usp = utf_idx2scntr(sidx);
	    utf_sendengine(vcqh, usp, mrq_notice.rmt_value, evtype);
	} else if (is_recvbuf(mrq_notice.rmt_value)) {
	    /*
	     * This is the result of recvbuf assess by the sender
	     */
	} else {
	    /* receive buffer is accessedd by the sender */
	    utf_printf("%s: MRQ_RMT_ARMW where ? rmt_value(%lx) rmt_stadd(%lx)\n",
		     __func__, mrq_notice.rmt_value, mrq_notice.rmt_stadd);
	}
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_TYPE_RMT_ARMW: edata(%d) rmt_value(0x%lx)\n",
		     __func__, mrq_notice.edata, mrq_notice.rmt_value);
	}
	break;
    }
    default:
	utf_printf("%s: Unknown notice type(%d): edata(%d) rmt_value(%ld)\n",
		 __func__, mrq_notice.notice_type, mrq_notice.edata, mrq_notice.rmt_value);
	break;
    }
    return rc;
}

int
utf_tcqprogress(utofu_vcq_hdl_t vcqh)
{
    int	rc;
    void	*cbdata;

    rc = utofu_poll_tcq(vcqh, 0, &cbdata);
    if (rc != UTOFU_ERR_NOT_FOUND && rc != UTOFU_SUCCESS) {
	utf_printf("%s: error rc(%d)\n", __func__, rc);
    }
    return rc;
}

int
utf_progress(void *av, utofu_vcq_hdl_t vcqh)
{
    int rc1, rc2;

    rc1 = utf_tcqprogress(vcqh);
    rc2 = utf_mrqprogress(av, vcqh);
    /* NEEDS to check return value if error occurs YI */
    return rc2;
}
