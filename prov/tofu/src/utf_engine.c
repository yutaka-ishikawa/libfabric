#include <unistd.h>
#include <utofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_queue.h"
#include "utf_sndmgt.h"
#ifndef UTF_NATIVE
#include <ofi_iov.h>
#include "tofu_debug.h"
#endif

extern int	tofufab_resolve_addrinfo(void *, int rank,
					 utofu_vcq_id_t *vcqid, uint64_t *flgs);
extern sndmgt	*egrmgt;
extern struct utf_msgreq	*utf_msgreq_alloc();
extern struct utf_msglst	*utf_msglst_append(utfslist *head,
						   struct utf_msgreq *req);
extern utfslist utf_wait_rmacq;
extern void	tofu_catch_rma_rmtnotify(void *cntx);

extern int scntr2idx(struct utf_send_cntr *scp);

#include "utf_msgmacro.h"

/* needs to fi it */
struct utf_tofuctx	utf_sndctx[16];
struct utf_tofuctx	utf_rcvctx[16];
int		dbg_tofu_cmd;
uint64_t	dbg_tofu_rstadd;
char		*dbg_tofu_file;
int		dbg_tofu_line;

static struct utf_recv_cntr	rcntr[MSG_PEERS];
static utofu_vcq_id_t		vcqrcntr[MSG_PEERS];

static char *notice_symbol[] =
{	"MRQ_LCL_PUT", "MRQ_RMT_PUT", "MRQ_LCL_GET",
	"MRQ_RMT_GET", "MRQ_LCL_ARMW","MRQ_RMT_ARMW"
};

static char *rstate_symbol[] =
{	"R_FREE", "R_NONE", "R_HEAD", "R_BODY",	"R_WAIT_RNDZ", "R_DO_RNDZ",
	"R_DO_READ", "R_DO_WRITE", "R_DONE" };

static char *sstate_symbol[] =
{	"S_FREE", "S_NONE", "S_REQ_ROOM", "S_HAS_ROOM",
	"S_DO_EGR", "S_DO_EGR_WAITCMPL", "S_DONE_EGR",
	"S_REQ_RDVR", "S_RDVDONE", "S_DONE", "S_WAIT_BUFREADY" };

static char *evnt_symbol[] = {
    "EVT_START", "EVT_LCL", "EVT_RMT_RGETDON", "EVT_RMT_RECVRST",
    "EVT_RMT_GET", "EVT_CONT", "EVT_END"
};

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
	rcntr[i].initialized = 0;
	vcqrcntr[i] = 0;
	// utfslist_init(&rcntr[i].rget_cqlst, NULL);
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

static void *cur_av;

void
show_info()
{
    int	src, rc;
    utofu_vcq_id_t svcqid;
    uint64_t	flags;
    for (src = 0; src < nprocs; src++) {
	rc = tofufab_resolve_addrinfo(cur_av, src, &svcqid, &flags);
	if (rc == 0) {
	    utf_printf("\t: [%d] vcqid(%lx) flags(%lx)\n", src, svcqid, flags);
	} else {
	    utf_printf("\t: [%d] error(%d)\n", src, rc);
	}
    }
}

static inline int
utfgen_explst_match(struct utf_hpacket *pkt)
{
    int	idx;
#ifndef UTF_NATIVE
    utfslist *explst
	= pkt->hdr.flgs&FI_TAGGED ? &utf_fitag_explst : &utf_fimsg_explst;
    idx = tofu_utf_explst_match(explst, pkt->hdr.src, pkt->hdr.tag, 0);
#else
    idx = utf_explst_match(pkt->hdr.src, pkt->hdr.tag, 0);
#endif
    return idx;
}

static inline void
utfgen_uexplst_add(struct utf_hpacket *pkt, struct utf_msgreq *req)
{
    utfslist *uexplst
#ifndef UTF_NATIVE
	= pkt->hdr.flgs&FI_TAGGED ? &utf_fitag_uexplst : &utf_fimsg_uexplst;
#else
	= &utf_uexplst;
#endif
    utf_msglst_append(uexplst, req);
}

static inline void
init_recv_cntr_remote_info(struct utf_recv_cntr *ursp, void *av, int src, int sidx)
{
    if (ursp->initialized == 0) {
	int rc;
	rc = tofufab_resolve_addrinfo(av, src, &ursp->svcqid, &ursp->flags);
	if (rc != 0) {
	    utf_printf("%s: Cannot resolve address rank(%d)\n", src);
	    abort();
	}
	ursp->sidx = sidx;
	vcqrcntr[ursp->mypos] = ursp->svcqid;
	ursp->initialized = 1;
    }
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
		   "EMSG_SIZE(msgp)=%ld req->fi_iov_count(%ld)\n",
		   __func__, req->rsize, req->hdr.size, cpysz, req->expsize, EMSG_SIZE(msgp),
		   req->fi_iov_count);
	utf_showpacket("incoming packet:", msgp);
    }
    if (req->ustatus == REQ_OVERRUN) {
	/* no copy */
    } else if ((req->rsize + cpysz) > req->expsize) { /* overrun */
	size_t	rest = req->expsize - req->rsize;
	req->ustatus = REQ_OVERRUN;
#ifndef UTF_NATIVE /* for Fabric */
	ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, req->rsize,
			EMSG_DATA(msgp), rest);
#else
	utf_printf("%s: Something wrong in utf native mode\n", __func__);
#endif
    }  else {
	if (req->buf) { /* enough buffer area has been allocated */
	    bcopy(EMSG_DATA(msgp), &req->buf[req->rsize], cpysz);
	} else {
#ifndef UTF_NATIVE /* for Fabric */
	    ofi_copy_to_iov(req->fi_msg, req->fi_iov_count, req->rsize,
			    EMSG_DATA(msgp), cpysz);
#else
	utf_printf("%s: Something wrong in utf native mode\n", __func__);
#endif
	}
    }
    req->rsize += cpysz;
    DEBUG(DLEVEL_ADHOC) {
	utf_printf("%s: src(%d) tag(%lx) rsz(%ld) hsz(%ld)\n", __func__, req->hdr.src, req->hdr.tag, req->rsize, req->hdr.size);
    }
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

static inline void
utf_done_rget(utofu_vcq_id_t vcqh, struct utf_recv_cntr *ursp)
{
    struct utf_msgreq *req = ursp->req;
    int sidx = ursp->sidx;
    utofu_stadd_t	stadd = SCNTR_ADDR_CNTR_FIELD(sidx);
    DEBUG(DLEVEL_PROTO_RENDEZOUS|DLEVEL_ADHOC) {
	utf_printf("%s: R_DO_RNDZ done ursp(%p)\n", __func__, ursp);
    }
    /*
     * notification to the sender: local mrq notification is supressed
     * currently local notitication is enabled
     */
    utf_remote_armw4(vcqh, ursp->svcqid, ursp->flags,
		     UTOFU_ARMW_OP_OR, SCNTR_OK,
		     stadd + SCNTR_RGETDONE_OFFST, sidx, 0);
    UTOFU_LATEST_CMDINFO(CMD_ARMW4, stadd + SCNTR_RGETDONE_OFFST);
    if (req->bufstadd) {
	utf_mem_dereg(vcqh, req->bufstadd);
	req->bufstadd = 0;
    }
    req->rsize = req->hdr.size;
    req->status = REQ_DONE;
}

/*
 * receiver engine
 *  State diagram
 *  R_NONE : UTOFU_MRQ_TYPE_RMT_PUT
 *		+> R_DO_RNDZ
 *		+> R_WAIT_RNDZ
 *		+> R_BODY
 *		+> R_BODY +-> R_DONE +-> R_NONE
 *  R_BODY : UTOFU_MRQ_TYPE_RMT_PUT
 *		+> R_BODY  or R_DONE +-> R_NONE
 *  R_DO_RNDZ : UTOFU_MRQ_TYPE_LCL_GET
 *		+> R_DONE +-> R_NONE
 *  Note that state change: R_WAIT_RNDZ --> R_DO_RNDZ is done by 
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
	DEBUG(DLEVEL_ADHOC) {
	    extern char	*tofu_fi_flags_string(uint64_t flags);
	    utf_printf("%s: recvoff(%d) begin receiving src(%d) tag(0x%lx) size(%ld)"
		       " data(0x%ld) flags(%s)\n",
		       __func__, ursp->recvoff, pkt->hdr.src, pkt->hdr.tag,
		       pkt->hdr.size, pkt->hdr.data, tofu_fi_flags_string(pkt->hdr.flgs));
	}
	if ((idx = utfgen_explst_match(pkt)) != -1) {
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: new expected message arrives. idx(%d)\n", __func__, idx);
	    req = utf_idx2msgreq(idx);
	    req->ptype = msgp->ptype;
	    req->hdr = pkt->hdr; /* src, tag, size, data, flgs */
	    ursp->req = req; /* keeping this request */
	    if (req->ptype == PKT_RENDZ) {
		/* req->bufstadd is assigned by tofu_utf_recv_post
		 * req->hdr.size is defined by the receiver
		 * pkt->hdr.size is defined by the sender */
		req->rmtstadd = *(utofu_stadd_t*) pkt->msgdata;
		init_recv_cntr_remote_info(ursp, av, req->hdr.src, sidx);
		utf_do_rget(vcqh, ursp, R_DO_RNDZ);
		/* state is now R_DO_RNDZ */
	    } else {/* eager */
		if (eager_copy_and_check(ursp, req, msgp) == R_DONE) {
		    DEBUG(DLEVEL_ADHOC) utf_printf("%s: expdone src(%d)\n", __func__, pkt->hdr.src);
		    goto done;
		}
		/* state is now R_BODY */
	    }
	    /* state is now either R_DO_RNDZ or R_BODY */
	} else { /* New Unexpected message */
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: new unexpected message arrives.\n", __func__);
	    req = utf_msgreq_alloc();
	    ursp->req = req; /* keeping this request */
	    req->hdr = pkt->hdr;
	    req->rsize = 0; req->ustatus = 0; req->type = REQ_RECV_UNEXPECTED;
	    req->ptype = msgp->ptype;
	    req->expsize = pkt->hdr.size;   /* from sender side */
	    switch(msgp->ptype) {
	    case PKT_EAGER: /* eager */
		/* register it to unexpected queue */
		utfgen_uexplst_add(pkt, req);
		req->buf = utf_malloc(pkt->hdr.size);
		if (eager_copy_and_check(ursp, req, msgp) == R_DONE) {
		    goto done;
		}
		/* state is now R_BODY */
		break;
	    case PKT_RENDZ:
		/* register it to unexpected queue */
		utfgen_uexplst_add(pkt, req);
		req->rcntr = ursp;
		req->rmtstadd = *(utofu_stadd_t*) pkt->msgdata;
		req->status = REQ_WAIT_RNDZ;
		ursp->state = R_WAIT_RNDZ;
		init_recv_cntr_remote_info(ursp, av, req->hdr.src, sidx);
		/* state is now R_WAIT_RNDZ */
		break;
	    }
	}
	/* state is now  R_DO_RNDZ | R_BODY | R_WAIT_RNDZ 
	 *             | R_DO_WRITE | R_DO_READ */
    }
	break;
    case R_BODY:
	req = ursp->req;
	if (eager_copy_and_check(ursp, req, msgp) == R_DONE) {
	    goto done;
	}
	/* state is R_BODY. continue to receive message */
	break;
    case R_WAIT_RNDZ:
	/* this state is handled by tofu_utf_recv_post() or utf_XXX() */
	utf_printf("%s: should never come\n", __func__);
	abort();
	break;
    case R_DO_WRITE:
	utf_done_rget(vcqh, ursp);
	goto reset_state;
    case R_DO_RNDZ: /* R_DO_RNDZ --> R_DONE */
	DEBUG(DLEVEL_ADHOC) {
	    utf_printf("%s: R_DO_RNDZ ursp->req(%p) -->\n", __func__, ursp->req);
	}
	utf_done_rget(vcqh, ursp);
	/* Falls through. */
    case R_DONE:
	req = ursp->req;
    done:
	if (req->type == REQ_RECV_EXPECTED) {
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: Expected message arrived (idx=%d)\n", __func__,
			 utf_msgreq2idx(req));
	    }
	    assert(req->notify != NULL); /* expected queue always have notifier */
	    req->notify(req);
	    /* req will be deallocated in notify, tofu_catch_rcvnotify */
	} else {
	    /* req will be deallocated in tofu_utf_recv_post */
	}
    reset_state:
	/* reset the state */
	ursp->state = R_NONE; ursp->req = NULL;
	break;
    case R_HEAD:
    default:
	break;
    }
}

static inline utofu_stadd_t
calc_recvstadd(struct utf_send_cntr *usp, uint64_t ridx, size_t ssize)
{
    utofu_stadd_t	recvstadd = 0;
    if (usp->recvoff >= (MSGBUF_SIZE - MSG_SIZE)) {
	/* buffer full */
	//utf_printf("%s: YI** WAIT usp->rcvreset(%d) usp->recvoff(%d)\n", __func__, usp->rcvreset, usp->recvoff);
	usp->ostate = usp->state;
	usp->state = S_WAIT_BUFREADY;
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: dst(%d) WAIT\n", __func__, usp->dst);
    } else {
	recvstadd  = erbstadd
	    + (uint64_t)&((struct erecv_buf*)0)->rbuf[MSGBUF_SIZE*ridx]
	    + usp->recvoff;
    }
    return recvstadd;
}

static inline void
utf_recv_info_init(struct utf_send_cntr *usp)
{
    usp->rcvreset = 0;	/* set by the receiver */
    usp->recvoff = 0;	/* next receiver's buffer offset */
}

static inline void
utf_setup_state(struct utf_send_cntr *usp, int off, size_t psz, size_t usz, int nst)
{
    usp->recvoff += off;/* next receiver's buffer offset */
    usp->psize = psz;	/* packet-level size */
    usp->usize = usz;	/* user-level size */
    usp->state = nst;	/* next state */
}

static inline void
utf_update_state(struct utf_send_cntr *usp, int off, size_t psz, size_t usz, int nst)
{
    usp->recvoff += off;/* next receiver's buffer offset */
    usp->psize += psz;	/* packet-level size */
    usp->usize += usz;	/* user-level size */
    usp->state = nst;	/* next state */
}

/*
 * sender engine
 *		
 */
void
utf_sendengine(utofu_vcq_id_t vcqh, struct utf_send_cntr *usp, uint64_t rslt, int evt)
{
    utfslist_entry		*slst;
    struct utf_send_msginfo	*minfo;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: usp(%p)->state(%s), evt(%d) rcvreset(%d) recvoff(%d)\n",
		 __func__, usp, sstate_symbol[usp->state],
		 evt, usp->rcvreset, usp->recvoff);
    }
    if (evt == EVT_RMT_RECVRST && usp->state != S_WAIT_BUFREADY) {
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: RST dst(%d) set(%d) recvoff(%d) st(%s)\n", __func__, usp->dst, usp->rcvreset, usp->recvoff, sstate_symbol[usp->state]);
	/* In other usp->state, this event makes just rcvreset, 
	 * and should not do progress because the progress is 
	 * driven by receivng the MRQ_TYPE_LCL_PUT event */
	utf_recv_info_init(usp);
	return;
    }
    slst = utfslist_head(&usp->smsginfo);
    minfo = container_of(slst, struct utf_send_msginfo, slst);
    if (slst == NULL) {
	utf_printf("%s: why ? slst == NULL usp->state(%s)\n", __func__, sstate_symbol[usp->state]);
	abort();
	return;
    }
progress:
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: usp(%p)->state(%s), evt(%d) minfo(%p)\n",
		   __func__, usp, sstate_symbol[usp->state], evt, minfo);
    }
    //utf_printf("%s: usp(%p) recvoff(%d) dst(%d) minfo(%p) tag(%lx) size(%ld) state(%s)\n", __func__, usp, usp->recvoff, usp->dst, minfo, minfo->sndbuf->msgbdy.payload.h_pkt.hdr.tag, minfo->sndbuf->msgbdy.payload.h_pkt.hdr.size, sstate_symbol[usp->state]);
    switch(usp->state) {
    case S_NONE: /* never comes */
	break;
    case S_REQ_ROOM:
	if ((int64_t)rslt >= 0) {
	    //utf_printf("%s: YI*** set sendok for dst(%d) and index(%d))\n",  __func__, usp->dst, (unsigned)rslt);
	    sndmgt_set_sndok(usp->dst, egrmgt);
	    sndmgt_set_index(usp->dst, egrmgt, rslt);
	    usp->state = S_HAS_ROOM;
	    usp->recvoff = 0;
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("\tRoom is available in remote rank %d. usp(%p)->recvoff(%d)\n", usp->dst, usp, usp->recvoff);
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

	recvstadd = calc_recvstadd(usp, ridx, ssize);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Going to send rvcqid(%lx) size(%ld) "
		       "type(%d) sidx(%d) ridx(%d) "
		       " recvstadd(%lx) usp(%p)->recvoff(%d)\n",
		       __func__, rvcqid,
		       ssize, minfo->cntrtype, usp->mypos, ridx,
		       recvstadd, usp, usp->recvoff);
	    utf_showpacket("\t", &minfo->sndbuf->msgbdy);
	}
	if (recvstadd == 0) {
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_printf("%s: going to sleep\n", __func__);
	    }
	    break;
	}
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
	    /* recvoff, psize, usize, state */
	    utf_setup_state(usp, ssize, ssize, MSG_CALC_EAGER_USIZE(ssize), S_DONE_EGR);
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: BUF done dst(%d) sz(%d) recvoff(%d)\n", __func__, usp->dst, usp->usize, usp->recvoff - ssize);
	    /* this state must be kept until receiving local put event */
	    DEBUG(DLEVEL_PROTOCOL) {
		utf_showpacket("sender packet buffer", &minfo->sndbuf->msgbdy);
		utf_printf("%s: next usp(%p)->state = %d\n",
			 __func__, usp, usp->state);
	    }
	    break;
	case SNDCNTR_INPLACE_EAGER:
	    remote_put(vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, ssize, usp->mypos, flgs, 0);
	    /* recvoff, psize, usize, state */
	    utf_setup_state(usp, ssize, ssize, MSG_CALC_EAGER_USIZE(ssize), S_DO_EGR);
	    if (usp->usize == minfo->msghdr.size) {
		DEBUG(DLEVEL_ADHOC) utf_printf("%s: IN done\n", __func__);
		usp->state = S_DONE_EGR;
	    }
	    break;
	case SNDCNTR_RENDEZOUS:
	    usp->rgetwait = 0; usp->rgetdone = 0;
	    remote_put(vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, ssize, usp->mypos, flgs, 0);
	    /* recvoff, psize, usize, state */ /* usize does not matter */
	    utf_setup_state(usp, ssize, ssize, MSG_EAGER_SIZE, S_REQ_RDVR);
	    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		utf_printf("%s: rendezous send size(%ld) minfo->sndstadd(%lx) "
			 "src(%d) tag(0x%lx) msgsize(%ld) ptype(%d)\n", __func__,
			 ssize, minfo->sndstadd,
			 minfo->sndbuf->msgbdy.payload.h_pkt.hdr.src,
			 minfo->sndbuf->msgbdy.payload.h_pkt.hdr.tag,
			 minfo->sndbuf->msgbdy.payload.h_pkt.hdr.size,
			 minfo->sndbuf->msgbdy.ptype);
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
	    DEBUG(DLEVEL_PROTO_EAGER) {
		utf_printf("%s: S_DO_EGR, mreq(%p) msgsize(%ld) sentsize(%ld) "
			 "rest(%ld) sending usize(%ld) packet ssize(%ld) "
			 "recvstadd(0x%lx) usp->recvoff(%d)\n",
			 __func__, minfo->mreq, minfo->msghdr.size, usp->usize,
			 rest, usize, ssize, recvstadd, usp->recvoff);
	    }
	    if (recvstadd == 0) {
		break;
	    }
	    remote_put(vcqh, rvcqid, minfo->sndstadd,
		       recvstadd, ssize, usp->mypos, flgs, 0);
	    /* recvoff, psize, usize, state */
	    utf_update_state(usp, ssize, ssize, usize, S_DO_EGR);
	    if (minfo->msghdr.size == usp->usize) {
		usp->state = S_DONE_EGR;
		DEBUG(DLEVEL_ADHOC) utf_printf("%s: EG done\n", __func__);
		/* this state must be kept until receiving local put event */
	    }
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: dst(%d) ssz(%d) nsz(%d) tsz(%d) off(%d)\n", __func__, usp->dst, usize, usp->usize, minfo->msghdr.size, usp->recvoff - ssize);
	} else { /* Something wrong */
	    utf_printf("%s: protocol error\n", __func__); abort();
	}
	break;
    }
    case S_DO_EGR_WAITCMPL:
	utf_printf("%s: S_DO_EGR_WAITCMPL rcvreset(%d)\n", __func__, usp->rcvreset);
	break;
    case S_REQ_RDVR:
	DEBUG(DLEVEL_PROTO_RENDEZOUS|DLEVEL_ADHOC) {
	    utf_printf("%s: S_REQ_RDVR evt(%s) rgetwait(%d)\n",
		       __func__, evnt_symbol[evt], usp->rgetwait);
	}
	/*
	 * EVT_RMT_GET is generated as a result of receiver's get operation.
	 * EVT_RMT_RGETDON is generated as a result of receiver's ack operation.
	 */
	usp->rgetwait |= (evt == EVT_RMT_RGETDON) ? 1 : 0 ;
	usp->rgetwait |= (evt == EVT_RMT_GET) ? 2 : 0 ;
	if (usp->rgetwait == 3) { /* remote completion from the receiver */
	    usp->state = S_RDVDONE;
	} else {
	    break;
	}
	/* Falls through. */
    case S_RDVDONE:
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: S_RDVDONE usrstadd(%lx) bufstadd(%lx)\n", __func__, minfo->usrstadd, minfo->mreq->bufstadd);
	if (minfo->usrstadd) {
	    utf_mem_dereg(vcqh, minfo->usrstadd);
	    minfo->usrstadd = 0;
	}
	/* Falls through. */
    case S_DONE_EGR:
    {
	struct utf_msgreq	*req = minfo->mreq;

	DEBUG(DLEVEL_ADHOC) utf_printf("%s: S_EDONE dst(%d)\n", __func__, usp->dst);
	//utf_printf("%s: YI!!!! S_DONE_EGR\n", __func__);
	req->status = REQ_DONE;
	minfo->mreq = NULL;
	if (minfo->sndbuf) {
	    utf_egrsbuf_free(minfo->sndbuf);
	}
	/* usp->smsginfo free first, and then the entry is free.
	 * This is because the same linked list is used */
	utfslist_remove(&usp->smsginfo);
	utf_sndminfo_free(minfo);
	if (req->notify) req->notify(req);

	usp->state = S_HAS_ROOM; /* at the time of start, just checking this state 2020/04/25 */
	/* next waiting message */
	slst = utfslist_head(&usp->smsginfo);
	if (slst != NULL) {
	    minfo = container_of(slst, struct utf_send_msginfo, slst);
	    evt = EVT_CONT;
	    goto progress;
	}
	//utf_printf("%s: YI!!!! S_DONE_EGR slst(%p) usp(%p)\n", __func__, slst, usp);
	break;
    }
    case S_DONE:
	break;
    case S_WAIT_BUFREADY:
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: evt(%s) WAIT RST(%d)\n", __func__, evnt_symbol[evt], usp->rcvreset);
	if (evt != EVT_RMT_RECVRST) {
	    utf_printf("%s: protocol error\n", __func__);
	    abort();
	}
	utf_recv_info_init(usp);
	usp->state = usp->ostate; /* change to the previous status */
	goto progress;
    }
    //utf_printf("%s: return\n", __func__);
}

int
utf_send_start(utofu_vcq_hdl_t vcqh, struct utf_send_cntr *usp)
{
    int	dst = usp->dst;
    /* DO NOT RESET HERE */
    //usp->rcvreset = 0;
    // usp->recvoff = 0;
    if (sndmgt_isset_examed(dst, egrmgt) == 0) {
	/*
	 * checking availability
	 *   edata: usp->mypos for mrqprogress
	 */
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: 1\n", __func__);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Request a room to rank %d: send control(%d)\n"
		       "\trvcqid(%lx) remote stadd(%lx)\n",
		       __func__, dst, usp->mypos, usp->rvcqid, erbstadd);
	}
	utf_remote_add(vcqh, usp->rvcqid,
		       UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE,
		       -1, erbstadd, usp->mypos, 0);
	UTOFU_LATEST_CMDINFO(CMD_ADD, erbstadd);
	usp->state = S_REQ_ROOM;
	sndmgt_set_examed(dst, egrmgt);
	return 0;
    } else if (sndmgt_isset_sndok(dst, egrmgt) != 0) {
	//utf_printf("%s: YI** Has a received room in rank %d: send control(%d)\n",
	//	   __func__, dst, usp->mypos);
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: 2\n", __func__);
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

void
utf_rma_lclcq(struct utofu_mrq_notice mrq_notice, int type)
{
    struct utf_rma_cq	*cq;
    utfslist_entry	*cur, *prev;

    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	utf_printf("%s: mrq_notice: vcqid(%lx) lcl_stadd(%lx) rmt_stadd(%lx)\n",
		   __func__, mrq_notice.vcq_id, mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
    }
    utfslist_foreach2(&utf_wait_rmacq, cur, prev) {
	cq = container_of(cur, struct utf_rma_cq, slst);
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	    utf_printf("%s: cq(%p): addr(%lx) vcqh(%lx) lstadd(%lx) rstadd(%lx) len(%lx) type(%d: %s) %s\n",
		       __func__, cq, cq->addr, cq->vcqh, cq->lstadd, cq->rstadd, cq->len,
		       cq->type, cq->type == UTF_RMA_READ ? "UTF_RMA_READ" : "UTF_RMA_WRITE",
		       (cq->lstadd + cq->len == mrq_notice.lcl_stadd) ? "MATCH" : "NOMATCH");
	}
	if (cq->type == type) goto found;
#if 0
	/* It does not work in fi_write, why ?? 2020/05/05 */
	if (cq->lstadd + cq->len == mrq_notice.lcl_stadd) {
	    DEBUG(DLEVEL_ADHOC) {
		utf_printf("%s: %s is done\n", __func__, type== UTF_RMA_READ ? "fi_read" : "fi_write");
	    }
	    goto found;
	}
#endif
    }
    return;
found:
    utfslist_remove2(&utf_wait_rmacq, cur, prev);
    if (cq->notify) cq->notify(cq);
    return;
}

void
utf_rma_rmtcq(utofu_vcq_hdl_t vcqh, struct utofu_mrq_notice mrq_notice, struct utf_recv_cntr *ursp)
{
    int i;
    switch (mrq_notice.notice_type) {
    case UTOFU_MRQ_TYPE_RMT_PUT: /* 1 */
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	    utf_printf("fi_write has been issued in remote side vcqid(%lx) lcl_stadd(%lx) rmt_stadd(%lx) edata(0x%lx)\n",
		       mrq_notice.vcq_id, mrq_notice.lcl_stadd, mrq_notice.rmt_stadd, mrq_notice.edata);
	    {
		int	i;
		unsigned char	*bp = (unsigned char*) 0x618510;
		int	*ip = (int*) 0x618510;
		char	buf[32];
		memset(buf, 0, 32);
		for (i = 0; i < 8; i++) {
		    snprintf(&buf[i*3], 4, ":%02x", bp[i]);
		}
		utf_printf("\t bp(%p) %s\n", bp, buf);
		for (i = 0; i < 4; i++) {
		    printf("(%p) %d ", ip + i, *(ip + i));
		    fprintf(stderr, "(%p) %d ", ip + i, *(ip + i));
		}
		printf("\n"); fprintf(stderr, "\n"); fflush(NULL);
	    }
	}
	break;
	// tofu_catch_rma_rmtnotify(utf_sndctx[i].fi_ctx);
	break;
    case UTOFU_MRQ_TYPE_RMT_GET: /* 3 */
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	    utf_printf("fi_read has been issued in remote side vcqid(%lx) lcl_stadd(%lx) rmt_stadd(%lx)\n",
		       mrq_notice.vcq_id, mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	}
	break;
    default:
	utf_printf("unkown operation has been issued in remote side vcqid(%lx) lcl_stadd(%lx) rmt_stadd(%lx) noticetype(%s)\n",
		   mrq_notice.vcq_id, mrq_notice.lcl_stadd, mrq_notice.rmt_stadd,
		   notice_symbol[mrq_notice.notice_type]);
	break;
    }
}

int
utf_rmwrite_engine(utofu_vcq_id_t vcqh, struct utf_send_cntr *usp)
{
    struct utf_rma_cq	*cq;
    utfslist_entry	*cur;

    while (usp->rmawait == 0 && (cur = utfslist_remove(&usp->rmawaitlst)) != NULL) {
	uint64_t    ridx;
	cq = container_of(cur, struct utf_rma_cq, slst);
	ridx = sndmgt_get_index(cq->addr, egrmgt);
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	    utf_printf("%s: cq(%p): addr(%lx) vcqh(%lx) lstadd(%lx) rstadd(%lx) len(0x%lx) ridx(%d) type(%d: %s) ramoff(%d)\n",
		       __func__, cq, cq->addr, cq->vcqh, cq->lstadd, cq->rstadd, cq->len, ridx,
		       cq->type, cq->type == UTF_RMA_READ ? "FI_READ" : "FI_WRITE", usp->rmaoff);
	}
	/* real data sent */
	remote_put(vcqh, cq->rvcqid, cq->lstadd, cq->rstadd, cq->len,
		   EDAT_RMA | ridx, cq->utf_flgs, cq);
#if 0
	/* meta info sent */
	if (cq->fi_flags & FI_TRANSMIT_COMPLETE) {
	    struct utf_rma_mdat mdat;
	    utofu_stadd_t	rstadd;

	    rstadd = rmacmplistadd
		+ (uint64_t) &((struct utf_rma_cmplinfo*)0)[ridx].info[usp->rmaoff];
	    mdat.rmt_stadd = cq->rstadd; mdat.len = cq->len; mdat.src = myrank;
	    remote_piggysend(vcqh, cq->rvcqid, &mdat, rstadd,
			     sizeof(struct utf_rma_mdat), EDAT_RMA | ridx, cq->utf_flgs, cq);
	    usp->rmaoff++;
	}
	if (usp->rmaoff == RMA_MDAT_ENTSIZE) {
	    usp->rmawait = 1;
	}
#endif
	utf_rmacq_free(cq);
    }
    return 0;
}


int
utf_mrqprogress(void *av, utofu_vcq_hdl_t vcqh)
{
    int	rc;
    struct utofu_mrq_notice mrq_notice;

    // memset(&mrq_notice, 0, sizeof(struct utofu_mrq_notice));
    rc = utofu_poll_mrq(vcqh, 0, &mrq_notice);
    if (rc == UTOFU_ERR_NOT_FOUND) return rc;
    if (rc != UTOFU_SUCCESS) {
	char msg[1024];
	utofu_get_last_error(msg);
	utf_printf("%s: utofu_poll_mrq ERROR rc(%d)\n\t%s\n", __func__, rc, msg);
	abort();
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

	DEBUG(DLEVEL_UTOFU|DLEVEL_ADHOC) {
	    utf_printf("%s: MRQ_TYPE_LCL_PUT: edata(%d) rmt_val(%ld/0x%lx)"
		       "vcq_id(%lx) sidx(%x)\n",  __func__, mrq_notice.edata,
		       mrq_notice.rmt_value, mrq_notice.rmt_value,
		       mrq_notice.vcq_id, sidx);
	}
	if (sidx & EDAT_RMA) {
	    /* RMA operation */
	    utf_rma_lclcq(mrq_notice, UTF_RMA_WRITE);
	    break;
	}
	usp = utf_idx2scntr(sidx);
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

	if (sidx & EDAT_RMA) {
	    ursp = &rcntr[sidx & ~EDAT_RMA];
	    utf_rma_rmtcq(vcqh, mrq_notice, ursp);
	    break;
	}
	ursp = &rcntr[entry];
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_UTOFU|DLEVEL_ADHOC) {
	    utf_printf("%s: MRQ_TYPE_RMT_PUT: edat(%ld) rmtval(%lx) "
		       "rmt_stadd(%lx) vcqid(%lx) entry(%d) recvoff(%d) sidx(%d)\n",
		       __func__, mrq_notice.edata, mrq_notice.rmt_value,
		       mrq_notice.rmt_stadd,
		       mrq_notice.vcq_id, entry, ursp->recvoff, sidx);
	}
	if (ERECV_LENGTH(mrq_notice.rmt_value) < ursp->recvoff) {
	    /* the data might be the top */
	    DEBUG(DLEVEL_PROTO_EAGER|DLEVEL_ADHOC) {
		utf_printf("%s: src(%d) reset recvoff(%d) and rst_sent\n",
			   __func__, ursp->hdr.src, ursp->recvoff);
	    }
	    ursp->recvoff = 0;
	    ursp->rst_sent = 0;
	}
	msgp = utf_recvbuf_get(entry);
	msgp = (struct utf_msgbdy *) ((char*)msgp + ursp->recvoff);
	{
	    size_t	msz = msgp->psize;
	    utf_recvengine(av, vcqh, ursp, msgp, sidx);
	    /* handling message buffer here in the receiver side */
	    ursp->recvoff += msz;
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: src(%d) msz(%d) recvoff(%d) ent(%d)\n", __func__, EMSG_HDR(msgp).src, msz, ursp->recvoff - msz, entry);
	}
	if (ursp->rst_sent == 0 && ursp->recvoff >= (MSGBUF_SIZE - MSG_SIZE)) {
	    utofu_stadd_t	stadd = SCNTR_ADDR_CNTR_FIELD(sidx);
	    /* rcvreset field is set */
	    init_recv_cntr_remote_info(ursp, av, EMSG_HDR(msgp).src, sidx);
	    DEBUG(DLEVEL_UTOFU) {
		utf_printf("%s: reset request to sender "
			 "svcqid(%lx) flags(%lx) stadd(%lx) sidx(%d)\n",
			 __func__, ursp->svcqid, ursp->flags, stadd, sidx);
	    }
	    utf_remote_armw4(vcqh, ursp->svcqid, ursp->flags,
			     UTOFU_ARMW_OP_OR, SCNTR_OK,
			     stadd + SCNTR_RST_RECVRESET_OFFST, sidx, 0);
	    UTOFU_LATEST_CMDINFO(CMD_ARMW4, stadd + SCNTR_RST_RECVRESET_OFFST);
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: RST sent src(%d) rvcq(%lx) flg(%lx) stadd(%lx)\n",
					   __func__, EMSG_HDR(msgp).src, ursp->svcqid, ursp->flags, stadd);
	    cur_av = av;
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
	if (sidx & EDAT_RMA) {
	    /* RMA operation */
	    utf_rma_lclcq(mrq_notice, UTF_RMA_READ);
	    break;
	}
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_UTOFU|DLEVEL_PROTO_RENDEZOUS) {
	    utf_printf("%s: MRQ_TYPE_LCL_GET: vcq_id(%lx) edata(%d) "
		       "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		       __func__, mrq_notice.vcq_id, mrq_notice.edata,
		       mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	}
	if (ursp->state != R_DO_RNDZ
	    && ursp->state != R_DO_WRITE) {
	    utf_printf("%s: protocol error, expecting R_DO_RNDZ or R_DO_WRITE but %s\n"
		       "\tMRQ_TYPE_LCL_GET: vcq_id(%lx) edata(%d) "
		       "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		       __func__, rstate_symbol[ursp->state],
		       mrq_notice.vcq_id, mrq_notice.edata,
		       mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	    break;
	}
	utf_recvengine(av, vcqh, ursp, NULL, sidx);
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_GET: /* 3 */
    {	/* Sender side: edata is index of sender engine */
	struct utf_send_cntr *usp;
	int	sidx = mrq_notice.edata & ~EDAT_RMA;
	if (mrq_notice.edata & EDAT_RMA) {
	    utf_rma_rmtcq(vcqh, mrq_notice, NULL);
	    break;
	}
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_UTOFU|DLEVEL_PROTO_RENDEZOUS) {
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
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_UTOFU) {
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
	    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_UTOFU|DLEVEL_ADHOC) {
		utf_printf("%s: MRQ_TYPE_RMT_ARMW: edata(%d) rmt_value(0x%lx) evtype(%s)\n",
			   __func__, mrq_notice.edata, mrq_notice.rmt_value, evnt_symbol[evtype]);
	    }
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
	char msg[1024];
	utofu_get_last_error(msg);
	utf_printf("%s: error rc(%d)\n\t%s\n", __func__, rc, msg);
	show_info();
    }
    return rc;
}

static int dbg_prog1st = 0;
static void *dbg_av;
static utofu_vcq_hdl_t dbg_vcqh;

int
utf_progress(void *av, utofu_vcq_hdl_t vcqh)
{
    int rc1, rc2;
    int progressed = 0;

    if (dbg_prog1st == 0) {
	dbg_av = av;
	dbg_vcqh = vcqh;
	dbg_prog1st = 1;
    }
    do {
	progressed = 0;
	while ((rc1 = utf_mrqprogress(av, vcqh)) == UTOFU_SUCCESS) {
	    progressed++;
	}
	while ((rc2 = utf_tcqprogress(vcqh)) == UTOFU_SUCCESS) {
	    progressed++;
	}
    } while (progressed);
    /* NEEDS to check return value if error occurs YI */
    return rc2;
}

int
utf_dbg_progress(int mode)
{
    int	rc;
    if (dbg_prog1st == 0) return 0;
    if (mode) {
	utf_printf("%s: progress\n", __func__);
    }
    rc = utf_progress(dbg_av, dbg_vcqh);
    return rc;
}

/*
 * must fix it 2020/05/06
 */
int
utf_rma_progress()
{
    int rc;
    rc = utf_progress(dbg_av, dbg_vcqh);
    return rc;
}

void
utf_show_recv_cntr(FILE *fp)
{
    extern struct erecv_buf	*erbuf;
    uint64_t		cntr = erbuf->header.cntr;
    int	i;
    utf_fprintf(fp, "# of PEERS: %d\n", MSG_PEERS - cntr);
#if 0
    for (i = MSG_PEERS - 1; i > cntr; --i) {
	fprintf(fp, "\t%07ld", rcntr[i].hdr.src);
	if (((i + 1) % 8) == 0) fprintf(fp, "\n");
    }
    fprintf(fp, "\n"); fflush(fp);
#endif
}
