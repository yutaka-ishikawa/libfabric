/*
 * utf_rget_do:
 *	receive engine is controlled by remote vcq_id (ursp->svcqid)
 *		vcqid2recvcntr(vcq_id_t) --> utf_recv_cntr*
 */
static inline void
utf_rget_do(utofu_vcq_id_t vcqh, struct utf_recv_cntr *ursp, int nstate)
{
    struct utf_msgreq	*req = ursp->req;
    size_t		reqsize = req->hdr.size - req->rsize;
    uint64_t		sidx;
    DEBUG(DLEVEL_PROTO_RENDEZOUS|DLEVEL_ADHOC) {
	utf_printf("%s: remote_get  RENDZ reqsize(0x%lx) "
		   "local stadd(0x%lx) remote stadd(0x%lx) "
		   "edata(%d) flags(0x%lx) mypos(%d)\n",
		   __func__, reqsize, req->bufstadd,
		   req->rmtstadd, ursp->sidx, ursp->flags, ursp->mypos);
    }
    if (reqsize > RMA_MAX_SIZE) {
	reqsize = RMA_MAX_SIZE;
    }
    sidx = ursp->dflg != 0 ? EDAT_RGET : ursp->sidx;
    /*
     * LCL_GET event takes care of EDAT_RMA
     * RMT_GET event dose not take care of edata field
     */
    utf_printf("%s: remote_get ursp(%p)->sidx(%ld) sidx(%ld)\n", __func__, ursp, ursp->sidx, sidx);
    remote_get(vcqh, ursp->svcqid, req->bufstadd,
	       req->rmtstadd, reqsize, sidx, ursp->flags, 0);
    DBG_UTF_CMDINFO(UTF_DBG_RENG, UTF_CMD_GET, req->rmtstadd, sidx);
    DBG_UTF_CMDINFO(UTF_DBG_RGET, UTF_CMD_GET, req->rmtstadd, sidx);
    req->rsize += reqsize;
    ursp->state = nstate;
}

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
