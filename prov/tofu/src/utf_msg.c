#include <unistd.h>
#include <utofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_queue.h"
#include "utf_sndmgt.h"

int	utf_msgmode;
extern struct utf_msgreq	*utf_msgreq_alloc();
extern void	utf_msgreq_free(struct utf_msgreq *req);
extern struct utf_msglst	*utf_msglst_append(utfslist *head,
						   struct utf_msgreq *req);

void
utf_setmsgmode(int mode)
{
    utf_msgmode = mode;
}

void
utf_show_msgmode(FILE *fp)
{
    char *md;
    switch (utf_msgmode) {
    case MSG_EAGERONLY:	md = "EARGER ONLY"; break;
    case MSG_RENDEZOUS:	md = "RENDEZOUS"; break;
    default: md = "UNKNOWN"; break;
    }
    fprintf(fp, "MSGMODE is %s\n", md);
}

int
remote_piggysend(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, void *data,  utofu_stadd_t rstadd,
		 size_t len, uint64_t edata, unsigned long flgs, void *cbdata)
{
    char	desc[128];
    size_t	sz;

    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    UTOFU_CALL(1, utofu_prepare_put_piggyback,
	       vcqh, rvcqid, data, rstadd, len, edata, flgs, desc, &sz);
    assert(sz <= 128);
    UTOFU_CALL(1, utofu_post_toq, vcqh, desc, sz, cbdata);
    DEBUG(DLEVEL_UTOFU) {
	utf_printf("remote_piggyback: desc size(%ld) cbdata(%ld)\n", sz, cbdata);
    }
    return 0;
}


int
remote_put(utofu_vcq_hdl_t vcqh,
	   utofu_vcq_id_t rvcqid, utofu_stadd_t lstadd, utofu_stadd_t rstadd,
	   size_t len, uint64_t edata, unsigned long flgs, void *cbdata)
{
    char	desc[128];
    size_t	sz;

    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE
    	 | UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    UTOFU_CALL(1, utofu_prepare_put,
	       vcqh, rvcqid,  lstadd, rstadd, len, edata, flgs, desc, &sz);
    assert(sz <= 128);
    UTOFU_CALL(1, utofu_post_toq, vcqh, desc, sz, cbdata);
    DEBUG(DLEVEL_UTOFU) {
	utf_printf("remote_put: desc size(%ld) cbdata(%ld)\n", sz, cbdata);
    }
    return 0;
}

int
remote_get(utofu_vcq_hdl_t vcqh,
	   utofu_vcq_id_t rvcqid, utofu_stadd_t lstadd, utofu_stadd_t rstadd,
	   size_t len, uint64_t edata, unsigned long flgs, void *cbdata)
{
    char	desc[128];
    size_t	sz;

    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE
    	 | UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
    UTOFU_CALL(1, utofu_prepare_get,
	       vcqh, rvcqid,  lstadd, rstadd, len, edata, flgs, desc, &sz);
    assert(sz <= 128);
    UTOFU_CALL(1, utofu_post_toq, vcqh, desc, sz, cbdata);
    DEBUG(DLEVEL_UTOFU|DLEVEL_PROTO_RENDEZOUS) {
	utf_printf("remote_get: desc size(%ld) cbdata(%ld) lcl_stadd(%lx) rmt_stadd(%lx)\n",
		   sz, cbdata, lstadd, rstadd);
    }
    return 0;
}

#ifdef UTF_NATIVE
int
utf_recv(int src, size_t size, int tag, void *buf, int *ridx)
{
    struct utf_msgreq	*req;
    int	idx;

    INITCHECK();
    if ((idx = utf_uexplst_match(src, tag, 0)) != -1) {
	req = utf_idx2msgreq(idx);
	bcopy(req->buf, buf, size);
	/* Thi is unexpected message.
	 * This means the buffer is allocated dynamically */
	utf_free(req->buf);
	utf_msgreq_free(req);
	*ridx = -1;
	return 0;
    } else {
	if ((req = utf_msgreq_alloc()) == NULL) goto err;
	// utf_printf("%s: expected msg req(%p)\n", __func__, req);
	req->hdr.src = src; req->hdr.tag = tag;
	req->expsize = size;
	req->buf = buf;
	req->ustatus = REQ_NONE; req->status = REQ_NONE;
	req->type = REQ_RECV_EXPECTED;	req->rsize = 0;
	utf_msglst_append(&utf_explst, req);
	*ridx = utf_msgreq2idx(req);
	return 1;
    }
err:
    return -1;
}
#endif


int
utf_test(void *av, utofu_vcq_hdl_t vcqh, int reqid)
{
    struct utf_msgreq	*req;

    if (reqid < 0) return -1;
    utf_progress(av, vcqh);
    req = utf_idx2msgreq(reqid);
    if (req->status == REQ_DONE) return 0;
    return 1;
}

int
utf_wait(void *av, utofu_vcq_hdl_t vcqh, int reqid)
{
    struct utf_msgreq	*req;

    if (reqid < 0) return -1;
    do {
	utf_progress(av, vcqh);
	req = utf_idx2msgreq(reqid);
    } while (req->status != REQ_DONE);
    utf_msgreq_free(req);
    return 0;
}

int
utf_remote_add(utofu_vcq_hdl_t vcqh,
	       utofu_vcq_id_t rvcqid, unsigned long flgs, uint64_t val,
	       utofu_stadd_t rstadd, uint64_t edata, void *cbdata)
{
    char	desc[128];
    size_t	sz;

    flgs |= 0    /*UTOFU_ONESIDED_FLAG_TCQ_NOTICE*/
	| UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	| UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	| UTOFU_ONESIDED_FLAG_STRONG_ORDER;
	    /* | UTOFU_MRQ_TYPE_LCL_ARMW; remote notification */
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("remote_add: val(%ld) rvcqid(%lx)\n", val, rvcqid);
    }
    UTOFU_CALL(1, utofu_prepare_armw8,
	       vcqh, rvcqid,
	       UTOFU_ARMW_OP_ADD,
	       val, rstadd, edata, flgs, desc, &sz);
    UTOFU_CALL(1, utofu_post_toq, vcqh, desc, sz, cbdata);
    return 0;
}

int
utf_remote_armw4(utofu_vcq_hdl_t vcqh,
		 utofu_vcq_id_t rvcqid, unsigned long flgs,
		 enum utofu_armw_op op, uint64_t val,
		 utofu_stadd_t rstadd, uint64_t edata, void *cbdata)
{
    /* local mrq notification is supressed */
    flgs |= 0    /*UTOFU_ONESIDED_FLAG_TCQ_NOTICE*/
	 | UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE
	 | UTOFU_ONESIDED_FLAG_STRONG_ORDER;
	    /* | UTOFU_MRQ_TYPE_LCL_ARMW; remote notification */
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("utf_remote_armw4: val(%ld) rvcqid(%lx) op(%x) rstadd(%lx)\n",
		 val, rvcqid, op, rstadd);
    }
    UTOFU_CALL(1, utofu_armw4, vcqh, rvcqid, op,
	       val, rstadd, edata, flgs, cbdata);
    return 0;
}

#define ERR_NOMORE_SNDCNTR	-2
#define ERR_NOMORE_SNDBUF	-3
#define ERR_NOMORE_REQBUF	-4
#define ERR_NOMORE_MINFO	-5

int
utf_send(utofu_vcq_hdl_t vcqh,
	 int dst, utofu_vcq_id_t rvcqid, uint64_t flgs, int utf_rank,
	 size_t size, int tag, void *buf,  int *ridx)
{
    int	rc;
    utfslist_entry	*ohead;
    struct utf_send_cntr *usp;
    struct utf_egr_sbuf	*sbufp;
    struct utf_msgreq	*req;
    struct utf_send_msginfo *minfo;

    INITCHECK();
#if 0
    /* destination rank must be checked by the receiver
     * nprocs does not know the utf engine */
    if (dst < 0 || dst > nprocs) {
	return -1;
    }
#endif
    usp = utf_scntr_alloc(dst, rvcqid, flgs);
    if (usp == NULL) {
	rc = ERR_NOMORE_SNDCNTR;
	goto err1;
    }
    /*
     * sender buffer is allocated, whose size is only 1920 Byte:
     *	  2 B for payload size
     *	  16 B for MPI header
     *	1902 B for message (MSG_EAGER_SIZE)
     * If the message size is more than 1902 byte, 
     * the user's buf area is pinned down (utf_mem_reg) and its
     * steering address is registered 
     */
    if ((minfo = utf_sndminfo_alloc()) == NULL) {
	rc = ERR_NOMORE_MINFO;
	goto err2;
    }
    memset(minfo, 0, sizeof(*minfo));
    minfo->msghdr.src = utf_rank;
    minfo->msghdr.tag = tag;
    minfo->msghdr.size = size;
    minfo->sndbuf = sbufp = utf_egrsbuf_alloc(&minfo->sndstadd);
    if (sbufp == NULL) {
	rc = ERR_NOMORE_SNDBUF;
	goto err3;
    }
    if ((req = utf_msgreq_alloc()) == NULL) {
	rc = ERR_NOMORE_REQBUF;
	goto err4;
    }
    minfo->mreq = req;
    req->type = REQ_SND_REQ;
    req->status = REQ_NONE;
    *ridx = utf_msgreq2idx(req);
    if (size <= MSG_EAGER_SIZE) {
	/* In case of error, the program is terminated */
	minfo->usrstadd = 0;
	minfo->cntrtype = SNDCNTR_BUFFERED_EAGER;
	bcopy(&minfo->msghdr, &sbufp->msgbdy.payload.h_pkt.hdr,
	      sizeof(struct utf_msghdr));
	bcopy(buf, sbufp->msgbdy.payload.h_pkt.msgdata, size);
	sbufp->msgbdy.psize = MSG_MAKE_PSIZE(size);
    } else if (utf_msgmode != MSG_RENDEZOUS) {
	/* Eager in-place, need to wait for remote completion */
	//minfo->usrstadd = utf_mem_reg(vcqh, buf, size);
	DEBUG(DLEVEL_PROTO_EAGER) {
	    utf_printf("EAGER INPLACE\n");
	}
	minfo->usrbuf = buf;
	minfo->cntrtype = SNDCNTR_INPLACE_EAGER;
	bcopy(&minfo->msghdr, &sbufp->msgbdy.payload.h_pkt.hdr,
	      sizeof(struct utf_msghdr));
	bcopy(buf, sbufp->msgbdy.payload.h_pkt.msgdata, MSG_EAGER_SIZE);
	sbufp->msgbdy.psize = MSG_MAKE_PSIZE(MSG_EAGER_SIZE);
    } else {
	/* Rendezvous */
	minfo->usrstadd = utf_mem_reg(vcqh, buf, size);
	//utf_printf("%s: Rendezous message usrstadd(%lx) size(%ld) \n",
	//__func__, minfo->usrstadd, size);
	minfo->cntrtype = SNDCNTR_RENDEZOUS;
	bcopy(&minfo->msghdr, &sbufp->msgbdy.payload.h_pkt.hdr,
	      sizeof(struct utf_msghdr));
	bcopy(&minfo->usrstadd, sbufp->msgbdy.payload.h_pkt.msgdata,
	      sizeof(utofu_stadd_t));
	sbufp->msgbdy.psize = MSG_MAKE_PSIZE(sizeof(minfo->usrstadd));
	sbufp->msgbdy.ptype = PKT_RENDZ;
    }
    ohead = utfslist_append(&usp->smsginfo, &minfo->slst);
    if (ohead == NULL) { /* this is the first entry */
	rc = utf_send_start(vcqh, usp);
    }
    return rc;
err4:
    utf_msgreq_free(req);
err3:
    utf_sndminfo_free(minfo);
err2:
    utf_scntr_free(dst);
err1:
    return rc;
}
