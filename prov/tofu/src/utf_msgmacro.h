static inline void
utf_do_rget(utofu_vcq_id_t vcqh, struct utf_recv_cntr *ursp, int nstate)
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
    remote_get(vcqh, ursp->svcqid, req->bufstadd,
	       req->rmtstadd, reqsize, ursp->sidx, ursp->flags, 0);
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
    ursp->state = nstate;
}
