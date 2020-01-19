static ssize_t
tofu_cep_msg_recv_common(struct fid_ep *fid_ep,
                         const struct fi_msg_tagged *msg,
                         uint64_t flags)
{
    ssize_t             ret = FI_SUCCESS;
    struct tofu_cep     *cep_priv = 0;
    struct ulib_icep    *icep;

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL,
            "\tsrc(%ld) iovcount(%ld) buf(%p) size(%ld) flags(0x%lx) cntxt(%p) in %s\n",
            msg->addr, msg->iov_count, 
            msg->msg_iov ? msg->msg_iov[0].iov_base : 0,
            msg->msg_iov ? msg->msg_iov[0].iov_len : 0, flags,
            msg->context, __FILE__);
    if (fid_ep->fid.fclass != FI_CLASS_RX_CTX) {
	ret = -FI_EINVAL; goto bad;
    }
    if ((flags & FI_TRIGGER) != 0) {
	ret = -FI_ENOSYS; goto bad;
    }
    if (msg->iov_count > TOFU_IOV_LIMIT) {
	ret = -FI_EINVAL; goto bad;
    }
    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    icep = (struct ulib_icep*)(cep_priv + 1);

    fastlock_acquire(&cep_priv->cep_lck);
    ret = ulib_icep_shea_recv_post(icep, msg, flags);
    fastlock_release(&cep_priv->cep_lck);
bad:
    return ret;
}

/*
 * fi_recvv
 */
static ssize_t
tofu_cep_msg_recvv(struct fid_ep *fid_ep,
                   const struct iovec *iov,
                   void **desc, size_t count, fi_addr_t src_addr,
                   void *context)
{
    ssize_t ret = FI_SUCCESS;
    struct fi_msg_tagged tmsg;

    tmsg.msg_iov    = iov;
    tmsg.desc	    = desc;
    tmsg.iov_count  = count;
    tmsg.addr	    = src_addr;
    tmsg.tag	    = 0;	/* no tag */
    tmsg.ignore	    = -1ULL;	/* All ignore */
    tmsg.context    = context;
    tmsg.data	    = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    R_DBG1(RDBG_LEVEL3, "fi_recvv src(%s) len(%ld) buf(%p) flags(0)",
          fi_addr2string(buf1, 128, src_addr, fid_ep),
           iov[0].iov_len, iov[0].iov_base);

    ret = tofu_cep_msg_recv_common(fid_ep, &tmsg, 0 /* flags */ );
    if (ret != 0) { goto bad; }

bad:
    return ret;
}

/*
 * tofu_cep_ops_msg is reffered to in tofu_cep.c
 */
struct fi_ops_msg tofu_cep_ops_msg = {
    .size	    = sizeof (struct fi_ops_msg),
    .recv	    = tofu_cep_msg_recv,
    .recvv	    = tofu_cep_msg_recvv,
    .recvmsg	    = tofu_cep_msg_recvmsg,
    .send	    = tofu_cep_msg_send,
    .sendv	    = tofu_cep_msg_sendv,
    .sendmsg	    = tofu_cep_msg_sendmsg,
    .inject	    = tofu_cep_msg_inject,
    .senddata	    = fi_no_msg_senddata,
    .injectdata	    = fi_no_msg_injectdata,
};

/*
 * tofu_cep_ops_tag is reffered to in tofu_cep.c
 */
struct fi_ops_tagged tofu_cep_ops_tag = {
    .size	    = sizeof (struct fi_ops_tagged),
    .recv	    = tofu_cep_tag_recv,
    .recvv	    = tofu_cep_tag_recvv,
    .recvmsg	    = tofu_cep_tag_recvmsg,
    .send	    = tofu_cep_tag_send,
    .sendv	    = tofu_cep_tag_sendv,
    .sendmsg	    = tofu_cep_tag_sendmsg,
    .inject	    = tofu_cep_tag_inject,
    .senddata	    = tofu_cep_tag_senddata,
    .injectdata	    = tofu_cep_tag_injectdata,
};
