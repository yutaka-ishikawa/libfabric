/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include <assert.h>	    /* for assert() */

extern ssize_t  tfi_utf_read_post(struct tofu_ctx *ctx,
                                   const struct fi_msg_rma *msg, uint64_t flags);
extern ssize_t  tfi_utf_write_post(struct tofu_ctx *ctx,
                                    const struct fi_msg_rma *msg, uint64_t flags);
extern ssize_t  tfi_utf_write_inject(struct tofu_ctx *ctx, const void *buf, size_t len,
                                     fi_addr_t dest_addr, uint64_t addr, uint64_t key);

/*
 * fi_read:
 */
ssize_t
tofu_ctx_rma_read(struct fid_ep *fid_ep, void *buf, size_t len, void *desc,
                  fi_addr_t src_addr, uint64_t addr, uint64_t key,
                  void *context)
{
    ssize_t             ret = FI_SUCCESS;
    struct tofu_ctx     *ctx_priv;
    uint64_t            flags = FI_COMPLETION | FI_READ | FI_RMA;
    struct iovec        iov[1];
    struct fi_rma_iov   rma_iov[1];
    struct fi_msg_rma   tmsg;

    utf_tmr_begin(TMR_FI_READ);
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "buf(%p) len(%ld) desc(%p) addr(0x%lx) key(%lx) context(%p) in %s\n", buf, len, desc, addr, key, context, __FILE__);
    // fprintf(stderr, "YI****** %s is called buf(%p) len(%ld) src_addr(%ld) addr(0x%lx) key(0x%lx)\n", __func__, buf, len, src_addr, addr, key);
    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    iov->iov_base  = buf;
    iov->iov_len   = len;
    rma_iov->addr = addr;
    rma_iov->len = len;
    rma_iov->key = key;
    /**/
    tmsg.msg_iov = iov;
    tmsg.desc = 0;
    tmsg.iov_count = 1;
    tmsg.addr = src_addr;       /* peer rank */
    tmsg.rma_iov = rma_iov;
    tmsg.rma_iov_count = 1;
    tmsg.context = context;
    tmsg.data = 0;
    ret = tfi_utf_read_post(ctx_priv, &tmsg, flags);

    FI_DBG(&tofu_prov, FI_LOG_EP_DATA, "return %ld\n", ret);
    utf_tmr_end(TMR_FI_READ);
    return ret;
}

/*
 * fi_readmsg
 */
ssize_t
tofu_ctx_rma_readmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_ctx *ctx_priv;
    uint64_t    flgs = flags | FI_READ | FI_RMA;

    utf_tmr_begin(TMR_FI_READ);
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);

    // fprintf(stderr, "YI****** %s is called buf(%p) len(%ld) src_addr(%ld) addr(0x%lx) key(0x%lx)\n", __func__, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, msg->addr, msg->rma_iov[0].addr, msg->rma_iov[0].key);

    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    ret = tfi_utf_read_post(ctx_priv, msg, flgs);

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "fi_errno %ld\n", ret);
    utf_tmr_end(TMR_FI_READ);
    return ret;
}


/*
 * fi_writemsg
 */
ssize_t 
tofu_ctx_rma_writemsg(struct fid_ep *fid_ep,
                      const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_ctx *ctx_priv;
    uint64_t    flgs = flags | FI_WRITE | FI_RMA;

    utf_tmr_begin(TMR_FI_WRITE);
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);

    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    ret = tfi_utf_write_post(ctx_priv, msg, flgs);
    
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "fi_errno %ld\n", ret);
    utf_tmr_end(TMR_FI_WRITE);
    return ret;
}

/*
 * fi_inject_write
 */
ssize_t 
tofu_ctx_rma_inject(struct fid_ep *fid_ep, const void *buf, size_t len,
		fi_addr_t dest_addr, uint64_t addr, uint64_t key)
{
    ssize_t ret = 0;
    struct tofu_ctx *ctx_priv;

    utf_tmr_begin(TMR_FI_WRITE);
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    R_DBG0(RDBG_LEVEL3, "fi_inject_write: fid_ep(%p) buf(%p) len(%ld) dst(%ld) addr(%ld) key(%lx)",
           fid_ep, buf, len, dest_addr, addr, key);

    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    ret = tfi_utf_write_inject(ctx_priv, buf, len, dest_addr, addr, key);
    utf_tmr_end(TMR_FI_WRITE);
    return ret;
}

struct fi_ops_rma tofu_ctx_ops_rma = {
    .size	    = sizeof (struct fi_ops_rma),
    .read	    = tofu_ctx_rma_read,
    .readv	    = fi_no_rma_readv,
    .readmsg	    = tofu_ctx_rma_readmsg,
    .write	    = fi_no_rma_write,
    .writev	    = fi_no_rma_writev,
    .writemsg	    = tofu_ctx_rma_writemsg,
    .inject	    = tofu_ctx_rma_inject,
    .writedata	    = fi_no_rma_writedata,
    .injectdata	    = fi_no_rma_injectdata,
};
