/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include <assert.h>	    /* for assert() */

extern ssize_t  tofu_utf_read_post(struct tofu_ctx *ctx,
                                   const struct fi_msg_rma *msg, uint64_t flags);
extern ssize_t  tofu_utf_write_post(struct tofu_ctx *ctx,
                                    const struct fi_msg_rma *msg, uint64_t flags);

/*
 * fi_read:
 */
static ssize_t
tofu_ctx_rma_read(struct fid_ep *fid_ep, void *buf, size_t len, void *desc,
                  fi_addr_t src_addr, uint64_t addr, uint64_t key,
                  void *context)
{
    ssize_t             ret = FI_SUCCESS;
//    uint64_t            flags = FI_COMPLETION; /* must be set 2019/08/15 */

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "buf(%p) len(%ld) desc(%p) addr(0x%lx) key(%lx) context(%p) in %s\n", buf, len, desc, addr, key, context, __FILE__);
//    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);

    FI_DBG(&tofu_prov, FI_LOG_EP_DATA, "return %ld\n", ret);
    return ret;
}

/*
 * fi_readmsg
 */
static ssize_t
tofu_ctx_rma_readmsg(struct fid_ep *fid_ep,
                     const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_ctx *ctx_priv;
    uint64_t    flgs = flags | FI_READ | FI_RMA;

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);

    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    ret = tofu_utf_read_post(ctx_priv, msg, flgs);

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "fi_errno %ld\n", ret);
    return ret;
}


/*
 * fi_writemsg
 */
static ssize_t 
tofu_ctx_rma_writemsg(struct fid_ep *fid_ep,
                      const struct fi_msg_rma *msg, uint64_t flags)
{
    ssize_t ret = 0;
    struct tofu_ctx *ctx_priv;
    uint64_t    flgs = flags | FI_WRITE | FI_RMA;

    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);

    ctx_priv = container_of(fid_ep, struct tofu_ctx, ctx_fid);
    ret = tofu_utf_write_post(ctx_priv, msg, flgs);
    
    FI_INFO(&tofu_prov, FI_LOG_EP_DATA, "fi_errno %ld\n", ret);
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
    .inject	    = fi_no_rma_inject,
    .writedata	    = fi_no_rma_writedata,
    .injectdata	    = fi_no_rma_injectdata,
};
