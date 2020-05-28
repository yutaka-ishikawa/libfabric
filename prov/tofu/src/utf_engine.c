#include <unistd.h>
#include <utofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#define USE_UTFSLIST_NEXT
#include "utf_queue.h"
#include "utf_sndmgt.h"
#ifndef UTF_NATIVE
#include <ofi_iov.h>
#include "tofu_debug.h"
#include "utf_engine.h"
#endif

extern int	tofufab_resolve_addrinfo(void *, int rank,
					 utofu_vcq_id_t *vcqid, uint64_t *flgs);
extern sndmgt	*egrmgt;
extern struct utf_msgreq	*utf_msgreq_alloc();
extern struct utf_msglst	*utf_msglst_append(utfslist *head,
						   struct utf_msgreq *req);
extern utfslist	utf_pcmd_head, utf_pcmd_save;
extern void	utf_pcmd_free(struct utf_pending_utfcmd *);
extern utfslist utf_wait_rmacq;
extern void	tofu_catch_rma_rmtnotify(void *cntx);

extern int scntr2idx(struct utf_send_cntr *scp);

extern void utf_showstadd();

#include "utf_msgmacro.h"

/* needs to fi it */
struct utf_tofuctx	utf_sndctx[16];
struct utf_tofuctx	utf_rcvctx[16];

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
	"S_REQ_RDVR", "S_RDVDONE", "S_DONE", "S_WAIT_BUFREADY",
	"S_DONE_FINALIZE1_1", "S_DONE_FINALIZE1_2",
	"S_DONE_FINALIZE1_3", "S_DONE_FINALIZE2"
};

static char *evnt_symbol[] = {
    "EVT_START", "EVT_LCL", "EVT_LCL_REQ", "EVT_RMT_RGETDON", "EVT_RMT_RECVRST",
    "EVT_RMT_GET", "EVT_RMT_CHNRDY", "EVT_RMT_CHNUPDT", "EVT_CONT", "EVT_END"
};

static inline utofu_stadd_t utf_sndctr_stadd()
{
    extern utofu_stadd_t sndctrstadd;
    return sndctrstadd;
}

static utfslist	utf_slistrget;

void
utf_engine_init()
{
    int		i;
    for (i = 0; i < MSG_PEERS; i++) {
	rcntr[i].state = R_NONE;
	rcntr[i].mypos = i;
	rcntr[i].initialized = 0;
	rcntr[i].dflg = 0;
	vcqrcntr[i] = 0;
	// utfslist_init(&rcntr[i].rget_cqlst, NULL);
    }
    utfslist_init(&utf_slistrget, NULL);
}

static struct utf_recv_cntr	*
find_rget_recvcntr(utofu_vcq_id_t vcqid)
{
    utfslist_entry	*cur;
    struct utf_recv_cntr *ursp;
    utf_printf("%s: vcqid(%lx)\n", __func__, vcqid);
    utfslist_foreach(&utf_slistrget, cur) {
	ursp = container_of(cur, struct utf_recv_cntr, rget_slst);
	utf_printf("%s: ursp(%p)->svcqid(%lx) vcqid(%lx)\n", __func__, ursp, ursp->svcqid, vcqid);
	if (ursp->svcqid == vcqid) goto find;
    }
    ursp = 0;
find:
    return ursp;
}

struct utf_recv_cntr	*
vcqid2recvcntr(utofu_vcq_id_t vcqid)
{
    int	i;

    utf_printf("%s: vcqid(%lx)\n", __func__, vcqid);
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

inline static char *
chain_addr_string(uint64_t val)
{
    static char	buf[128];
    snprintf(buf, 128, "rank(%d) sidx(%d) recvoff(%d)",
	     ((union chain_addr)val).rank,
	     ((union chain_addr)val).sidx,
	     ((union chain_addr)val).recvoff);
    return buf;
}

void
utf_tofu_error(int rc)
{
    int	i;
    char msg[1024];
    utofu_get_last_error(msg);
    utf_printf("%s: utofu error: rc(%d) %s\n", __func__, rc, msg);
    utf_showstadd();
    for (i = 0; i < 3; i++) {
	utf_printf("utf_dbg_info[%d].cmd    = %d\n", i, utf_dbg_info[i].cmd);
	utf_printf("utf_dbg_info[%d].rsatdd = 0x%lx\n", i, utf_dbg_info[i].rstadd);
	utf_printf("utf_dbg_info[%d].etc    = 0x%lx\n", i, utf_dbg_info[i].etc);
	utf_printf("utf_dbg_info[%d].file   = %s\n", i, utf_dbg_info[i].file);
	utf_printf("utf_dbg_info[%d].line   = %d\n", i, utf_dbg_info[i].line);
    }
    show_info();
    abort();
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
    if (ursp->initialized == 0 || ursp->mypos == EGRCHAIN_RECVPOS) {
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
utf_rget_done(utofu_vcq_id_t vcqh, struct utf_recv_cntr *ursp)
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
    utf_printf("YIRGET3: rget_done src(%d) svcqid(%lx) pos(%d)\n", req->hdr.src, ursp->svcqid, ursp->mypos);
    utf_remote_armw4(vcqh, ursp->svcqid, ursp->flags,
		     UTOFU_ARMW_OP_OR, SCNTR_OK,
		     stadd + SCNTR_RGETDONE_OFFST, sidx, 0);
    DBG_UTF_CMDINFO(UTF_DBG_RENG, UTF_CMD_ARMW4, stadd + SCNTR_RGETDONE_OFFST, sidx);
    if (req->bufstadd) {
	utf_mem_dereg(vcqh, req->bufstadd);
	req->bufstadd = 0;
    }
    req->rsize = req->hdr.size;
    req->status = REQ_DONE;
}

static inline struct utf_recv_cntr *
utf_rget_progalloc(struct utf_recv_cntr *ursp)
{
    struct utf_recv_cntr	*cpcntr;
    cpcntr = utf_malloc(sizeof(struct utf_recv_cntr));
    memcpy(cpcntr, ursp, sizeof(struct utf_recv_cntr));
    cpcntr->dflg = 1;
    utfslist_append(&utf_slistrget, &cpcntr->rget_slst);
    utf_printf("%s: Chainmode temp ursp is allocated(%p)\n", __func__, cpcntr);
    return cpcntr;
}

static inline void
utf_rget_progfree(struct utf_recv_cntr *ursp)
{
    if (utf_slistrget.head == &ursp->rget_slst) {
	utfslist_remove(&utf_slistrget);
	utf_free(ursp);
    } else {
	/* We do not expect this happen */
	utf_printf("%s: something wrong\n", __func__);
	abort();
    }
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

    utf_printf("%s: ursp(%p)->state(%s:%d)\n",
	       __func__, ursp, rstate_symbol[ursp->state], ursp->state);
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: ursp(%p)->state(%s:%d)\n",
		   __func__, ursp, rstate_symbol[ursp->state], ursp->state);
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
		req->rsize = 0;
		utf_printf("YIRGET: src(%d) svcqid(%lx) pos(%d) sidx(%ld)\n", req->hdr.src, ursp->svcqid, ursp->mypos, sidx);
		if (ursp->mypos == EGRCHAIN_RECVPOS) {
		    /* ursp is copied to another erecv, and
		     * EGRCHAIN_RECVPOS ursp is now ready to receive another message */
		    req->rcntr = utf_rget_progalloc(ursp);
		    init_recv_cntr_remote_info(req->rcntr, av, req->hdr.src, sidx);
		    utf_rget_do(vcqh, req->rcntr, R_DO_RNDZ);
		    ursp->state = R_NONE; ursp->req = NULL;
		} else {
		    utf_rget_do(vcqh, ursp, R_DO_RNDZ);
		}
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
		req->rmtstadd = *(utofu_stadd_t*) pkt->msgdata;
		req->status = REQ_WAIT_RNDZ;
		utf_printf("YIRGET2: src(%d) svcqid(%lx) pos(%d)\n", req->hdr.src, ursp->svcqid, ursp->mypos);
		if (ursp->mypos == EGRCHAIN_RECVPOS) {
		    /* ursp is copied to another erecv, and
		     * EGRCHAIN_RECVPOS ursp is now ready to receive another message */
		    req->rcntr = utf_rget_progalloc(ursp);
		    init_recv_cntr_remote_info(req->rcntr, av, req->hdr.src, sidx);
		    ursp->state = R_NONE; ursp->req = NULL;
		} else {
		    /* state is now R_WAIT_RNDZ */
		    init_recv_cntr_remote_info(ursp, av, req->hdr.src, sidx);
		    req->rcntr = ursp;
		    ursp->state = R_WAIT_RNDZ;
		}
		break;
	    }
	}
	/* state is now  R_DO_RNDZ | R_BODY | R_WAIT_RNDZ 
	 *             | R_DO_WRITE | R_DO_READ | R_NONE(takeover) */
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
	utf_rget_done(vcqh, ursp);
	goto reset_state;
    case R_DO_RNDZ: /* R_DO_RNDZ --> R_DONE */
	DEBUG(DLEVEL_ADHOC) {
	    utf_printf("%s: R_DO_RNDZ ursp->req(%p) -->\n", __func__, ursp->req);
	}
	req = ursp->req;
	utf_printf("YIRGETXX: req(%p)->rsize(%ld) req->hdr.size(%ld)\n", req, req->rsize, req->hdr.size);
	if (req->rsize < req->hdr.size) { /* continue to get data */
	    utf_rget_do(vcqh, ursp, R_DO_RNDZ);
	    break;
	} else { /* finish */
	    utf_rget_done(vcqh, ursp);
	}
	/* Falls through. */
    case R_DONE:
	req = ursp->req;
    done:
	utf_printf("YIRGET8: receive done src(%d) reqtype(%d)\n", req->hdr.src, req->type);
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
	if (ursp->dflg) {
	    utf_printf("%s: Chainmode ursp(%p) is free\n", __func__, ursp);
	    utf_rget_progfree(ursp);
	} else {
	    ursp->state = R_NONE; ursp->req = NULL;
	}
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
    if (IS_MSGBUF_FULL(usp)) {
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


static inline uint64_t
make_chain_addr(int rank, int mypos, int offset)
{
    union chain_addr	addr;
    addr.rank = rank; addr.sidx = mypos;
    addr.recvoff = offset;
    return addr.rank_sidx;
}

/*
 * Set myinfo into the last rank in chain mode
 * As a result of this operation,
 *	EVT_LCL is gnerated in the local sendengine
 *	EVT_RMT_CHNUPDT is generated in the remote side sendengine
 */
static inline void
utf_chain_set_tail(void *av, utofu_vcq_id_t vcqh, int rank, uint64_t addrinfo,
		   int mypos, struct utf_send_msginfo *minfo)
{
    union chain_addr last_rank;
    uint64_t	data;
    utofu_vcq_id_t	rvcqid;
    uint64_t		flags;
    last_rank.rank_sidx = addrinfo;
    data = make_chain_addr(rank, mypos, 0);
    tofufab_resolve_addrinfo(av, last_rank.rank, &rvcqid, &flags);
    /* sindex is remote one */
    remote_piggysend_nolevt(vcqh, rvcqid, &data, SCNTR_CHAIN_NXT(last_rank.sidx),
			    sizeof(uint64_t), last_rank.sidx, flags, minfo);
    utf_printf("%s: chain mode: put rank(%d) sidx(%d) into %s (EVT_RMT_CHNUPDT)\n",
	       __func__, rank, mypos, chain_addr_string(addrinfo));
    DEBUG(DLEVEL_CHAIN) {
	utf_printf("%s: chain mode: put rank(%d) sidx(%d) into %s (EVT_RMT_CHNUPDT)\n",
		   __func__, rank, mypos, chain_addr_string(addrinfo));
    }
}

/*
 * Ready to send flag in chain mode is set to the remote process
 * As a result of this operation, 
 *	EVT_LCL is generated.	NOT GENERATED 2020/05/25
 *	EVT_RMT_CHNRDY is gerated in the remote side sendengine.
 */
static inline void
utf_chain_inform_ready(void *av, utofu_vcq_hdl_t vcqh,
		       struct utf_send_cntr *usp,
		       struct utf_send_msginfo *minfo)
{
    utofu_vcq_id_t	rvcqid;
    uint64_t	flgs = 0;
    union chain_rdy	ready;
    ready.rdy = 1;
    ready.recvoff = usp->recvoff;
    tofufab_resolve_addrinfo(av, usp->chn_next.rank, &rvcqid, &flgs);
    utf_printf("%s: rank(%d) sidx(%d) recvoff(%d) remote_stad(%lx) usp->evtupdt(%d) usp->expevtupdt(%d)\n",
	       __func__, usp->chn_next.rank, usp->chn_next.sidx, usp->recvoff,
	       SCNTR_CHAIN_READY(usp->chn_next.sidx), usp->evtupdt, usp->expevtupdt);
    DEBUG(DLEVEL_CHAIN) {
	utf_printf("%s: rank(%d) sidx(%d) recvoff(%d) remote_stad(%lx)\n",
		   __func__, usp->chn_next.rank, usp->chn_next.sidx, usp->recvoff,
		   SCNTR_CHAIN_READY(usp->chn_next.sidx));
    }
    usp->chn_informed = 1;
    /* sindex is remote one */
    remote_piggysend_nolevt(vcqh, rvcqid, &ready,
			    SCNTR_CHAIN_READY(usp->chn_next.sidx),
			    sizeof(uint64_t),  usp->chn_next.sidx, flgs, minfo);
}

static inline void
utf_reset_recv_chntail(utofu_vcq_hdl_t vcqh, struct utf_send_cntr *usp, 
		       struct utf_send_msginfo *minfo)
{
    uint64_t	flgs = 0;
    uint64_t	oval = make_chain_addr(myrank, usp->mypos, 0);
    uint64_t	nval = make_chain_addr(-1, -1, usp->recvoff);
    /* new value has the last recvoff */
    DEBUG(DLEVEL_CHAIN) {
	utf_printf("%s: dst(%d) DONE_EGR|RDVDONE: oval.rank(%d) oval.sidx(%d) "
		   "nval.rank(%d) nval.sidx(0x%x) nval.recvoff(%d) going to issue remote_cswap\n",
		   __func__, usp->dst, ((union chain_addr)oval).rank,
		   ((union chain_addr)oval).sidx, ((union chain_addr)nval).rank,
		   ((union chain_addr)nval).sidx, ((union chain_addr)nval).recvoff);
    }
    /* This is the Compare and Swap */
    utf_remote_cswap(vcqh, usp->rvcqid, flgs,
		     oval, nval, EGRCHAIN_RECV_CHNTAIL, usp->mypos, minfo);
    DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_SWAP, EGRCHAIN_RECV_CHNTAIL, usp->mypos);
}

/*
 * sender engine
 *		
 */
void
utf_sendengine(void *av, utofu_vcq_hdl_t vcqh, struct utf_send_cntr *usp, uint64_t rslt, int evt)
{
    utfslist_entry		*slst;
    struct utf_send_msginfo	*minfo;

    utf_printf("%s: dst(%d) usp(%p)->state(%s:%d), smode(%s) evt(%s:%d) rcvreset(%d) recvoff(%d) sidx(%d)\n",
	       __func__, usp->dst, usp, sstate_symbol[usp->state], usp->state,
	       usp->smode == TRANSMODE_CHND ? "Chained" : "Aggressive",  evnt_symbol[evt],
	       evt, usp->rcvreset, usp->recvoff, usp->mypos);
    DEBUG(DLEVEL_CHAIN|DLEVEL_ADHOC|DLEVEL_PROTOCOL) {
	utf_printf("%s: dst(%d) usp(%p)->state(%s:%d), smode(%s) evt(%s:%d) rcvreset(%d) recvoff(%d) sidx(%d)\n",
		   __func__, usp->dst, usp, sstate_symbol[usp->state], usp->state,
		   usp->smode == TRANSMODE_CHND ? "Chained" : "Aggressive",  evnt_symbol[evt],
		   evt, usp->rcvreset, usp->recvoff, usp->mypos);
    }
    if (evt == EVT_RMT_RECVRST && usp->state != S_WAIT_BUFREADY) {
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: RST dst(%d) set(%d) recvoff(%d) st(%s)\n", __func__, usp->dst, usp->rcvreset, usp->recvoff, sstate_symbol[usp->state]);
	/* In other usp->state, this event makes just rcvreset, 
	 * and should not do progress because the progress is 
	 * driven by receivng the MRQ_TYPE_LCL_PUT event */
	utf_recv_info_init(usp);
	return;
    }
    if (evt == EVT_RMT_CHNUPDT) {
	usp->evtupdt = 1;
	if (usp->state < S_REQ_RDVR) {
	    DEBUG(DLEVEL_CHAIN) {
		utf_printf("%s: remote process set next field (%s)\n",
			   __func__, chain_addr_string(usp->chn_next.rank_sidx));
	    }
	    return;
	}
    }
    slst = utfslist_head(&usp->smsginfo);
    minfo = container_of(slst, struct utf_send_msginfo, slst);
    if (usp->state < S_DONE_FINALIZE1_1 && slst == NULL) {
	utf_printf("%s: why ? slst == NULL usp->state(%s)\n", __func__, sstate_symbol[usp->state]);
	abort();
	return;
    }
progress:
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("%s: usp(%p)->state(%s), evt(%d) minfo(%p)\n",
		   __func__, usp, sstate_symbol[usp->state], evt, minfo);
    }
    switch(usp->state) {
    case S_NONE: /* never comes */
	break;
    case S_REQ_ROOM:
	switch (evt) {
	case EVT_LCL_REQ: /* a result of remote_add (armw8)  in TRANSMODE_AGGR
			   * or utf_remote_swap (armw8) in TRANSMODE_CHND */
	    if (usp->smode == TRANSMODE_AGGR) { /* aggressive recv allocation */
		if ((int64_t)rslt >= 0) {
		    sndmgt_set_sndok(usp->dst, egrmgt);
		    sndmgt_set_index(usp->dst, egrmgt, rslt);
		    usp->state = S_HAS_ROOM;
		    usp->recvoff = 0;
		    DEBUG(DLEVEL_PROTOCOL) {
			utf_printf("\tRoom (%d) is available in remote rank %d. usp(%p)->recvoff(%d)\n", (int64_t)rslt, usp->dst, usp, usp->recvoff);
		    }
		    goto s_has_room;
		} else {
		    /* No room in remote rank */
		    utf_printf("No room in remote rank\n");
		    /* how to do */
		}
	    } else { /* sender chaining, TRANSMODE_CHND */
		// utf_printf("%s: result = %s\n", __func__, chain_addr_string(rslt));
		if (IS_CHAIN_EMPTY((union chain_addr)rslt)) { /* success */
		    sndmgt_set_sndok(usp->dst, egrmgt);
		    sndmgt_set_index(usp->dst, egrmgt, EGRCHAIN_RECVPOS);
		    usp->state = S_HAS_ROOM;
		    usp->recvoff = ((union chain_addr)rslt).recvoff;
		    // utf_printf("%s: EVT_LCL_REQ recvoff(%d)\n", __func__, usp->recvoff);
		    goto s_has_room;
		} else {
		    /* put my address to the tail of chain */
		    utf_chain_set_tail(av, vcqh, myrank, rslt, usp->mypos, minfo);
		    /* waiting, state is still the same */
		    /* This results in EVT_RMT_ */
		    break;
		}
	    }
	    break;
	case EVT_LCL:
	    /* result of the utf_chain_set_tail, ignore */
	    break;
	case EVT_RMT_CHNRDY:/* as a result of remote_put to CHAIN_RDY */
	    /* remote recv buf index is always EGRCHAIN_RECVPOS */
	    /* sendok flag is never set */
	    sndmgt_set_index(usp->dst, egrmgt, EGRCHAIN_RECVPOS);
	    usp->state = S_HAS_ROOM;
	    usp->recvoff = usp->chn_ready.recvoff;
	    // utf_printf("%s: EVT_RMT_CHNRDY revoff(%d)\n", __func__, usp->recvoff);
	    goto s_has_room;
	default:
	    utf_printf("%s: something wrong evt(%d) in S_REQ_ROOM\n", __func__, evt);
	    abort();
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
		DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_PUT_PIGGY, recvstadd, minfo);
	    } else {
		remote_put(vcqh, rvcqid, minfo->sndstadd,
			   recvstadd, ssize, usp->mypos, flgs, minfo);
		DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_PUT, recvstadd, minfo);
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
	    DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_PUT, recvstadd, minfo);
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
	    DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_PUT, recvstadd, minfo);
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
	    if (usp->smode == TRANSMODE_CHND) {
		if (IS_CHAIN_EMPTY(usp->chn_next)) { /* I'm last entry, reset the remote entry */
		    utf_reset_recv_chntail(vcqh, usp, minfo);
		    /* EVT_LCL_REQ will be generated in the sendengine */
		} else {
		    /* inform ready to the next rank */
		    DEBUG(DLEVEL_CHAIN) {
			utf_printf("%s: calling inform_ready in S_DONE_EGR, going to S_DONE_FINALIZE2\n", __func__);
		    }
		    utf_chain_inform_ready(av, vcqh, usp, minfo);
		    /* no event will be generated in the local side for the above operation */
		    if (usp->evtupdt == 0) {
			/* if usp->evtupdt == 0, EVT_RMT_CHNUPDT will be received later */
			usp->expevtupdt = 1;
		    }
		}
	    }
	    /* next state is S_REQ_RDVR */
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
	    DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_PUT, recvstadd, minfo);
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
	 * EVT_LCL_REQ is generated as a result of utf_reset_recv_chntail in TRANMODE_CHND.
	 * EVT_LCL is generated as a result of request to receiver (put).
	 * EVT_RMT_CHNUPDT can be generated as a result of remote set.
	 * EVT_RMT_RGETDON is generated as a result of receiver's ack operation.
	 * EVT_RMT_GET is generated as a result of receiver's get operation.
	 *	 now supressed 2020/05/10
	 */
	switch (evt) {
	case EVT_LCL_REQ:
	case EVT_RMT_CHNUPDT:
	    break;
	case EVT_LCL:
	    usp->rgetwait |= 1;
	    break;
	case EVT_RMT_RGETDON:
	    usp->rgetwait |= 2;
	    break;
	default:
	    utf_printf("%s: protocol error evt(%s)\n", __func__, evnt_symbol[evt]);
	    abort();
	    break;
	}
	if (usp->chn_informed == 0 && usp->evtupdt != 0) {
	    /* we have not expected EVT_RMT_CHNUPDT, but received */
	    utf_chain_inform_ready(av, vcqh, usp, minfo);
	}
	if (usp->rgetwait == 3
	    && (usp->expevtupdt == 0
		|| (usp->expevtupdt && usp->evtupdt))) { /* remote completion from the receiver */
	    /* utf_reset_chain_info will be called later */
	    usp->state = S_RDVDONE;
	} else {
	    break;
	}
	/* Falls through. */
    case S_RDVDONE:
	utf_printf("%s: S_RDVDONE usrstadd(%lx) bufstadd(%lx)\n", __func__, minfo->usrstadd, minfo->mreq->bufstadd);
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: S_RDVDONE usrstadd(%lx) bufstadd(%lx)\n", __func__, minfo->usrstadd, minfo->mreq->bufstadd);
	if (minfo->usrstadd) {
	    utf_mem_dereg(vcqh, minfo->usrstadd);
	    minfo->usrstadd = 0;
	}
	/* Falls through. */
    case S_DONE_EGR:
	if (utfslist_next(&usp->smsginfo)
	    || usp->smode == TRANSMODE_AGGR) {
	    /* at least, one more request has been enqueued.
	     *	keeping receive buffer
	     * or smode is TRANSMODE_AGGR */
	    goto going_done;
	}
	/* Here is for TRANSMODE_CHND */
	/* checking if the receiver's buffer is full */
	if (IS_MSGBUF_FULL(usp)) {
	    utf_printf("%s: YI!!!!!!!!!!!!!!!! RECEIVER'S BUFFER FULL\n", __func__);
	    usp->ostate = usp->state; /* S_RDVDONE or S_DONE_EGR */
	    usp->state = S_WAIT_BUFREADY;
	    break;
	}
	/* the receiver's buffer is now free */
	sndmgt_clr_examed(usp->dst, egrmgt);
	sndmgt_clr_sndok(usp->dst, egrmgt);
	if (usp->state == S_RDVDONE) {
	    goto finalize;
	} else {
	    if (IS_CHAIN_EMPTY(usp->chn_next)) { /* I'm last entry, reset the remote entry */
		utf_reset_recv_chntail(vcqh, usp, minfo);
		usp->state = S_DONE_FINALIZE1_1;
		/* 
		 * EVT_LCL_REQ in the sendengine.
		 * checking the swapped data in the FINALIZE1 state
		 * No event in the receiver side, ignore UTOFU_RMT_ARMW event
		 */
	    } else {
		/* inform ready to the next rank */
		DEBUG(DLEVEL_CHAIN) {
		    utf_printf("%s: calling inform_ready in S_DONE_EGR, going to S_DONE_FINALIZE2\n", __func__);
		}
		utf_chain_inform_ready(av, vcqh, usp, minfo);
		usp->state = S_DONE_FINALIZE2;
		goto done_finalize2; /* 2020/05/25 */
	    }
	}
	break;
    case S_DONE_FINALIZE1_1:
    {
	/*  
	 * two types events: we can receive
	 *	EVT_LCL_REQ (utf_remote_cswap in local)
	 *	EVT_RMT_CHNUPDT (utf_chain_set_tail in a remote)
	 */
	union chain_addr chaddr;
	chaddr.rank_sidx = rslt;
	DEBUG(DLEVEL_CHAIN) {
	    utf_printf("%s: DONE_FINALIZE1_1: oval.rank(%d) oval.sidx(%d)\n",
		       __func__, chaddr.rank, chaddr.sidx);
	}
	if (evt == EVT_LCL_REQ) {
	    if (chaddr.rank == myrank) { /* nobody changed */
		/* no EVT_RMT_CHNUPDT event will be received */
		goto finalize;
	    }
	    /* somebody, will update / has updated, my chain area, but not yet receive the event */
	    /* after receiving EVT_RMT_CHNUPDT, needs reaction */
	    usp->state = S_DONE_FINALIZE1_2;
	} else if (evt == EVT_RMT_CHNUPDT) {
	    /* this is a race condition, somebody get my info
	     * during this finalization, and update my chain */
	    /* inform ready to the next rank */
	    usp->state = S_DONE_FINALIZE1_3;
	} else {
	    utf_printf("%s: something wrong evt(%s:%d)\n", __func__, evnt_symbol[evt], evt);
	    abort();
	}
	break;
    }
    case S_DONE_FINALIZE1_2:
	assert(evt == EVT_RMT_CHNUPDT);
	goto finalize1;
    case S_DONE_FINALIZE1_3:
	assert(evt == EVT_LCL_REQ);
    finalize1:
	/* inform ready to the next rank */
	DEBUG(DLEVEL_CHAIN) {
	    utf_printf("%s: calling inform_ready in S_DONE_FINALIZE1_2|3, going to S_DONE_FINALIZE2\n", __func__);
	}
	utf_chain_inform_ready(av, vcqh, usp, minfo);
	// usp->chn_ready.data = 0; /* reset for future use */
	usp->state = S_DONE_FINALIZE2;
	// break;    2020/05/25
	/* Falls through. */ /* 2020/05/25 */
    case S_DONE_FINALIZE2: /* as a result of remote_add to inform to the next rank */
    done_finalize2:
	if (usp->evtupdt == 0) {
	    DEBUG(DLEVEL_CHAIN) {
		utf_printf("%s: waiting to handle EVT_RMT_CHNUPDT\n", __func__);
	    }
	    break;
	}
    finalize:/* real finalization for chain mode */
	DEBUG(DLEVEL_CHAIN) {
	    utf_printf("%s: dst(%d) DONE_FINALIZE2 and now state is S_NONE\n", __func__, usp->dst);
	}
	/* reset chain info */
	utf_reset_chain_info(usp);
	/* update mode for future use */
	sndmg_update_chainmode(usp->dst, egrmgt);
    going_done:
    {
	struct utf_msgreq	*req = minfo->mreq;

	utf_printf("%s: S_EDONE dst(%d)\n", __func__, usp->dst);
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: S_EDONE dst(%d)\n", __func__, usp->dst);
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

	/* next waiting message */
	slst = utfslist_head(&usp->smsginfo);
	if (slst != NULL) {
	    minfo = container_of(slst, struct utf_send_msginfo, slst);
	    usp->state = S_HAS_ROOM;
	    evt = EVT_CONT;
	    DEBUG(DLEVEL_CHAIN) {
		utf_printf("%s: next waiting request minfo(%p)\n", __func__, minfo);
	    }
	    if (sndmgt_isset_examed(usp->dst, egrmgt) == 0) {
		/* restart */
		utf_send_start(av, vcqh, usp);
		break;
	    } else {
		goto progress;
	    }
	}
	usp->state = S_DONE;
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
utf_send_start(void *av, utofu_vcq_hdl_t vcqh, struct utf_send_cntr *usp)
{
    int	dst = usp->dst;

    utf_printf("%s: dst(%d) examed(%d)\n", __func__, dst, sndmgt_isset_examed(dst, egrmgt));
    if (sndmgt_isset_examed(dst, egrmgt) == 0) {
	/*
	 * checking availability
	 *   edata: usp->mypos for mrqprogress
	 */
	utf_printf("YIRGET4: 1 dst(%d)\n", dst);
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: 1\n", __func__);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Request a room to rank %d: send control(%d)\n"
		       "\trvcqid(%lx) remote stadd(%lx)\n",
		       __func__, dst, usp->mypos, usp->rvcqid, erbstadd);
	}
	usp->smode = sndmgt_get_smode(usp->dst, egrmgt);
	if (usp->smode == TRANSMODE_CHND) {
	    /* swap tail address of request chain */
	    uint64_t	flgs = 0;
	    uint64_t	val = make_chain_addr(myrank, usp->mypos, 0);
	    utf_printf("YIRGET41: rvcqid(%lx)\n", usp->rvcqid);
	    utf_remote_swap(vcqh, usp->rvcqid, flgs,
			    val, EGRCHAIN_RECV_CHNTAIL, usp->mypos, usp);
	    DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_SWAP, EGRCHAIN_RECV_CHNTAIL, usp->mypos);
	    /* 
	     * EVT_LCL_REQ in the sendengine.
	     *		checking the swapped data in the S_REQ_ROOM state
	     * No event in the receiver side, ignore UTOFU_RMT_ARMW event
	     */
	} else { /* receive buffer is allocated aggressively */
	    uint64_t	flgs = UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE;
	    utf_remote_add(vcqh, usp->rvcqid, flgs,
			   -1, EGRCHAIN_RECV_CNTR, usp->mypos, 0);
	    DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_ADD, erbstadd, usp->mypos);
	    /* 
	     * EVT_LCL_REQ in the sendengine.
	     * No event in the receiver side, ignore UTOFU_RMT_ARMW event
	     */
	}
	sndmgt_set_examed(dst, egrmgt);
	usp->state = S_REQ_ROOM;
	return 0;
    } else if (sndmgt_isset_sndok(dst, egrmgt) != 0) {
	//utf_printf("%s: YI** Has a received room in rank %d: send control(%d)\n",
	//	   __func__, dst, usp->mypos);
	utf_printf("%s: 2\n", __func__);
	DEBUG(DLEVEL_ADHOC) utf_printf("%s: 2\n", __func__);
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("%s: Has a received room in rank %d: send control(%d)\n",
		     __func__, dst, usp->mypos);
	}
	usp->state = S_HAS_ROOM;
	utf_sendengine(av, vcqh, usp, 0, EVT_START);
	return 0;
    } else {
	utf_printf("%s: ERROR!!!! dst(%d)\n", __func__, dst);
	abort();
	return 0; /* never come */
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
	DBG_UTF_CMDINFO(UTF_DBG_RMAENG, UTF_CMD_PUT, cq->rstadd, ridx);
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
	utf_tofu_error(rc);
	/* never return */
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
	utf_sendengine(av, vcqh, usp, 0, EVT_LCL);
	break;
    }
    case UTOFU_MRQ_TYPE_RMT_PUT: /* 1 */
    {
	/* rmt_vlue is the last stadd address of put data */
	uint64_t		entry = ERECV_INDEX(mrq_notice.rmt_value);
	struct utf_recv_cntr	*ursp = &rcntr[entry];
	int			sidx = mrq_notice.edata;
	struct utf_msgbdy	*msgp;
	int	evtype;

	if (sidx & EDAT_RMA) {
	    ursp = &rcntr[sidx & ~EDAT_RMA];
	    utf_rma_rmtcq(vcqh, mrq_notice, ursp);
	    break;
	}
	if (is_scntr(mrq_notice.rmt_stadd, &evtype)) {
	    struct utf_send_cntr *usp;
	    usp = utf_idx2scntr(sidx);
	    utf_sendengine(av, vcqh, usp, mrq_notice.rmt_stadd, evtype);
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
	DEBUG(DLEVEL_CHAIN) {
	    utf_printf("%s: src(%d) ERECV_LENGTH(mrq_notice.rmt_value)=%d "
		       "recvoff(%d) sidx(%d) entry(%d) state(%s:%d)\n",
		       __func__, ursp->hdr.src, ERECV_LENGTH(mrq_notice.rmt_value),
		       ursp->recvoff, sidx, entry, rstate_symbol[ursp->state], ursp->state);
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
	    utf_printf("YIRGETXXXXXX RMT_PUT\n");
	    utf_recvengine(av, vcqh, ursp, msgp, sidx);
	    /* handling message buffer here in the receiver side */
	    ursp->recvoff += msz;
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: src(%d) msz(%d) recvoff(%d) ent(%d)\n", __func__, EMSG_HDR(msgp).src, msz, ursp->recvoff - msz, entry);
	}
	if (ursp->rst_sent == 0 && IS_MSGBUF_FULL(ursp)) {
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
	    DBG_UTF_CMDINFO(UTF_DBG_RENG, UTF_CMD_ARMW4, stadd + SCNTR_RST_RECVRESET_OFFST, sidx);
	    DEBUG(DLEVEL_ADHOC) utf_printf("%s: RST sent src(%d) rvcq(%lx) flg(%lx) stadd(%lx) edata(%d)\n",
					   __func__, EMSG_HDR(msgp).src, ursp->svcqid, ursp->flags, stadd, sidx);
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
	uint8_t	sidx = mrq_notice.edata;
	struct utf_recv_cntr	*ursp;

	utf_printf("%s: LCL_GET sidx(0x%x)\n", __func__, sidx);
	if (sidx & EDAT_RMA) {
	    if (sidx == EDAT_RGET) {
		/* remote get in Chain mode */
		ursp = find_rget_recvcntr(mrq_notice.vcq_id);
	    } else {
		/* RMA operation */
		utf_rma_lclcq(mrq_notice, UTF_RMA_READ);
		break;
	    }
	} else {
	    ursp = vcqid2recvcntr(mrq_notice.vcq_id);
	}
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_UTOFU|DLEVEL_PROTO_RENDEZOUS) {
	    utf_printf("%s: MRQ_TYPE_LCL_GET: vcq_id(%lx) edata(%d) "
		       "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		       __func__, mrq_notice.vcq_id, mrq_notice.edata,
		       mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	}
	if (ursp == NULL
	    || (ursp->state != R_DO_RNDZ
		&& ursp->state != R_DO_WRITE)) {
	    utf_printf("%s: ursp(%p) protocol error, expecting R_DO_RNDZ or R_DO_WRITE but %s\n"
		       "\tMRQ_TYPE_LCL_GET: vcq_id(%lx) edata(%d) "
		       "lcl_stadd(%lx) rmt_stadd(%lx)\n",
		       __func__, ursp, ursp != NULL ? rstate_symbol[ursp->state] : "",
		       mrq_notice.vcq_id, mrq_notice.edata,
		       mrq_notice.lcl_stadd, mrq_notice.rmt_stadd);
	    abort();
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
	utf_sendengine(av, vcqh, usp, mrq_notice.rmt_stadd, EVT_RMT_GET);
    }
	break;
    case UTOFU_MRQ_TYPE_LCL_ARMW:/* 4 */
    {	/* Sender side:
	 * rmt_value is a result of this atomic operation.
	 * edata is index of sender engine */
	struct utf_send_cntr *usp;
	int	evtype;
	uint8_t	sidx = mrq_notice.edata;

	if (sidx == EDAT_RGET) {
	    /* as a result of utf_rget_done, ignore */
	    break;
	} else {
	    usp = utf_idx2scntr(sidx);
	}
	DEBUG(DLEVEL_PROTO_RMA|DLEVEL_UTOFU) {
	    utf_printf("%s: MRQ_LCL_ARM: edata(%d) rmt_val(%ld) usp(%p) RST ?\n",
		     __func__, mrq_notice.edata, mrq_notice.rmt_value, usp);
	}
	if (is_scntr(mrq_notice.rmt_stadd, &evtype)) {
	    /* as a result of utf_rget_done, ignore */
	    /* this is for a receiver's remote OR operation
	     * except for EVT_RMT_CHNRDY */
	} else { /* accessing erbstadd area, erecv_buf's header  */
	    //utf_sendengine(vcqh, usp, mrq_notice.rmt_value, EVT_LCL);
	    utf_sendengine(av, vcqh, usp, mrq_notice.rmt_value, EVT_LCL_REQ);
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
	    utf_sendengine(av, vcqh, usp, mrq_notice.rmt_value, evtype);
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
	utf_tofu_error(rc);
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
	rc1 = utf_mrqprogress(av, vcqh);
	while ((rc2 = utf_tcqprogress(vcqh)) == UTOFU_SUCCESS);
	progressed++;
    } while (rc1 == UTOFU_SUCCESS && progressed < 10);
#if 0
    do {
	utfslist_entry		*slst;
	progressed = 0;
	while ((rc1 = utf_mrqprogress(av, vcqh)) == UTOFU_SUCCESS) {
	    progressed++;
	}
	utfslist_init(&utf_pcmd_save, NULL);
	while ((slst = utfslist_remove(&utf_pcmd_head)) != NULL) {
	    int	rc;
	    struct utf_pending_utfcmd *upu = container_of(slst, struct utf_pending_utfcmd, slst);
	    /* re-issue */
	    utf_printf("%s: upu(%p) reissue rvcqid(%p) cmd(%d) sz(%ld)\n", __func__, upu, upu->rvcqid, upu->cmd, upu->sz);
	    UTOFU_CALL_RC(rc, utofu_post_toq, upu->vcqh, upu->desc, upu->sz, upu);
	    if (rc == UTOFU_ERR_BUSY) {
		/* re-schedule */
		utfslist_append(&utf_pcmd_save, &upu->slst);
	    } else if (rc != UTOFU_SUCCESS) {
		/* error */
		utf_tofu_error(rc);
		/* never return */
	    } else {
		utf_pcmd_free(upu);
	    }
	}
	utf_pcmd_head = utf_pcmd_save;
	while ((rc2 = utf_tcqprogress(vcqh)) == UTOFU_SUCCESS) {
	    progressed++;
	}
    } while (progressed);
#endif
    /* NEEDS to check return value if error occurs YI */
    return rc2;
}

extern utfslist	*utf_scntr_busy();

void
utf_chnclean(void *av, utofu_vcq_hdl_t vcqh)
{
    utfslist	*slst;
    utfslist_entry	*cur;
    while (!utf_recvbuf_is_chain_clean()) {
	utf_progress(av, vcqh);
    }
    slst = utf_scntr_busy();
    utfslist_foreach(slst, cur) {
	struct utf_send_cntr	*usp = container_of(cur, struct utf_send_cntr, busy);
	utf_printf("%s: usp(%p)->state(%s) dst(%d) egrmgt.count(%d)\n",
		   __func__, usp, sstate_symbol[usp->state], usp->dst, egrmgt[usp->dst].count);
	while (usp->state != S_DONE) {
	    utf_progress(av, vcqh);
	}
    }
    utf_printf("%s: done\n", __func__);
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
    utf_fprintf(fp, "# of PEERS: %d\n", RECV_BUFCNTR_INIT - cntr);
#if 0
    for (i = MSG_PEERS - 1; i > cntr; --i) {
	fprintf(fp, "\t%07ld", rcntr[i].hdr.src);
	if (((i + 1) % 8) == 0) fprintf(fp, "\n");
    }
    fprintf(fp, "\n"); fflush(fp);
#endif
}
