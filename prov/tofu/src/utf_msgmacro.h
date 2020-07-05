#define STADD_OFFSET(stadd, off)	(uint64_t)((uint8_t*)(stadd) + off)
/*
 * utf_rget_start:
 *	receive engine is controlled by remote vcq_id (ursp->svcqid)
 *		vcqid2recvcntr(vcq_id_t) --> utf_recv_cntr*
 */
static inline void
utf_rget_start(struct tni_info *tinfo, void *msgbuf, size_t msgsize,
	       struct utf_recv_cntr *ursp, int nstate)
{
    struct utf_msgreq	*req = ursp->req;
    uint64_t	flags = 0; /* path information is integrated into vcqid */
    uint64_t	sidx;
    int		rntni;
    utofu_vcq_id_t	vcqh;
    extern utfslist	utf_slistrget;

    /* register for future use */
    ursp->rdma = 1;
    utfslist_append(&utf_slistrget, &ursp->rget_slst);
    ursp->rgetcnt = 0;
    sidx = ursp->dflg != 0 ? EDAT_RGET : ursp->sidx;
    DEBUG(DLEVEL_PROTO_RENDEZOUS|DLEVEL_ADHOC) {
	utf_printf("%s: RENDZ msgsize(0x%lx) ursp(%p)->sidx(0x%x) sidx(0x%x)\n",
		   __func__, msgsize,  ursp, ursp->sidx, sidx);
	utf_show_vcqid_stadd(&req->rgetsender);
    }
    /*
     * req->rgetsender: sender's vcqid and stadd
     * req->bufinfo: receiver's vcqhdl and stadd
     */
    rntni = req->rgetsender.nent;
    /* MUST BE configurable for threshould 8192 2020/07/05 */
    if (rntni == 1 || msgsize < 8192) { /* single rail */
	utf_cqselect_sendone(tinfo, &vcqh, msgsize);
	flags = UTOFU_ONESIDED_FLAG_PATH(req->rgetsender.pathid[0]);
	msgsize = (msgsize > RMA_MAX_SIZE) ? RMA_MAX_SIZE: msgsize;
	req->bufinfo.nent = 1;
	req->bufinfo.vcqhdl[0] = vcqh;
	req->bufinfo.stadd[0] = utf_mem_reg(vcqh, msgbuf, msgsize);
	/*
	 * LCL_GET event takes care of EDAT_RMA
	 * RMT_GET event dose not take care of edata field
	 */
	/* path_id has been set by the utf_cqselect_rget_setremote fucntion
	 * issued by utf_recvengine */
	ursp->rgetcnt++;
	remote_get(vcqh, req->rgetsender.vcqid[0], req->bufinfo.stadd[0],
		   req->rgetsender.stadd[0], msgsize, sidx, flags, 0);
	req->rsize = msgsize;
    } else { /* multi rail */
	int	sntni, tni;
	int	off = 0;
	size_t		restsz = msgsize;
	DEBUG(DLEVEL_PROTO_RENDEZOUS) {
	    utf_printf("%s: YI!!!!!! MRAIL\n", __func__);
	}
	sntni = utf_cqselect_sendmultiple(tinfo, &req->bufinfo, msgsize);
	flags = UTOFU_ONESIDED_FLAG_PATH(req->rgetsender.pathid[0]);
	tni = 0;
	if (sntni > 1) {
	    uint64_t	frsz = restsz/sntni;
	    while (tni < sntni - 1) {
		vcqh = req->bufinfo.vcqhdl[tni];
		req->bufinfo.stadd[tni] = utf_mem_reg(vcqh, msgbuf, msgsize);
		DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		    utf_printf("%s: vcqh(0x%lx) vcqid(0x%lx) stadd(0x%lx) off(0x%lx) flags(0x%lx) frsz(0x%lx)\n", __func__, vcqh, req->rgetsender.vcqid[tni], req->bufinfo.stadd[tni], off, flags, frsz);
		}
		remote_get(vcqh, req->rgetsender.vcqid[tni], 
			   STADD_OFFSET(req->bufinfo.stadd[tni], off),
			   STADD_OFFSET(req->rgetsender.stadd[tni], off),
			   frsz, sidx, flags, 0);
		off += frsz;
		restsz -= frsz;
		req->rsize += frsz;
		tni++;
		ursp->rgetcnt++;
	    }
	}
	if (restsz > 0) {
	    /* single sender CQ */
	    restsz = restsz > RMA_MAX_SIZE ? RMA_MAX_SIZE : restsz;
	    vcqh = req->bufinfo.vcqhdl[tni];
	    req->bufinfo.stadd[tni] = utf_mem_reg(vcqh, msgbuf, msgsize);
	    DEBUG(DLEVEL_PROTO_RENDEZOUS) {
		utf_printf("%s: vcqh(0x%lx) vcqid(0x%lx) stadd(0x%lx) off(0x%lx) flags(0x%lx) restsz(0x%lx)\n", __func__, vcqh, req->rgetsender.vcqid[tni], req->bufinfo.stadd[tni], off, flags, restsz);
	    }
	    remote_get(vcqh, req->rgetsender.vcqid[tni],
		       STADD_OFFSET(req->bufinfo.stadd[tni], off),
		       STADD_OFFSET(req->rgetsender.stadd[tni], off),
		       restsz, sidx, flags, 0);
	    req->rsize += restsz;
	    ursp->rgetcnt++;
	}
	/**/
    }
    ursp->state = nstate;  
}


#if 0
static inline void
utf_do_rput(utofu_vcq_id_t vcqh, struct utf_recv_cntr *ursp, int nstate)
{
    struct utf_msgreq	*req = ursp->req;
    size_t		reqsize = req->hdr.size;
    DEBUG(DLEVEL_PROTO_RENDEZOUS|DLEVEL_ADHOC) {
	utf_printf("%s: remote_get  RENDZ reqsize(0x%lx) "
		   "local stadd(0x%lx) remote stadd(0x%lx) "
		   "edata(%d) flags(0x%lx) mypos(%d)\n",
		   __func__, reqsize, req->bufstadd,
		   req->rmtstadd, ursp->sidx, ursp->flags, ursp->mypos);
    }
    remote_put(vcqh, ursp->svcqid, req->bufstadd,
	       req->rmtstadd, reqsize, ursp->sidx, ursp->flags, 0);
    DBG_UTF_CMDINFO(UTF_DBG_SENG, UTF_CMD_GET, req->rmtstadd, ursp->sidx);
    ursp->state = nstate;
}
#endif
