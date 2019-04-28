/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_MSG_H
#define _TOFU_MSG_H

#include "tofu_impl.h"

/*
 * 2019/04/15
 *  The message request is copied to the send entry (struct tofu_recv_en*)
 *      entry -- send_entry
 *      cep_priv -- context endpoint (has scalabe endpoint)
 *      msg -- tagged message
 *      flag --
 */
static inline int tofu_cep_msg_recv_fill(
    struct tofu_recv_en *entry,
    struct tofu_cep *cep_priv,
    const struct fi_msg_tagged *msg,
    uint64_t flags
)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "in %s\n", __FILE__);
    dlist_init( &entry->entry );
    if (msg->iov_count > TOFU_IOV_LIMIT) {
	FI_INFO( &tofu_prov, FI_LOG_EP_DATA, "iov_count %ld > 1\n",
	    msg->iov_count);
	fc = -FI_EINVAL; goto bad;
    }
    {
	size_t ii;
	for (ii = 0; ii < msg->iov_count; ii++) {
	    entry->iovs[ii] = msg->msg_iov[ii];
	}
    }
    /* info->domain_attr->mr_mode & FI_MR_LOCAL */
    if (msg->desc != 0) { /* XXX */
	size_t ii;
	for (ii = 0; ii < msg->iov_count; ii++) {
	    entry->dscs[ii] = msg->desc[ii];
	}
    }
    entry->tmsg.msg_iov   = entry->iovs;
    entry->tmsg.desc      = entry->dscs;
    entry->tmsg.iov_count = msg->iov_count;
    entry->tmsg.addr      = msg->addr;
    entry->tmsg.context   = msg->context;
    entry->tmsg.data      = msg->data;

    entry->tmsg.tag       = msg->tag; /* YI 2019/04/11 */
    entry->tmsg.ignore    = msg->ignore; /* YI 2019/04/11 */

    entry->fidp           = &cep_priv->cep_fid.fid;
    entry->flags          = flags;
    
bad:
    FI_INFO( &tofu_prov, FI_LOG_EP_DATA, " fc(%d)\n", fc);
    return fc;
}

#endif	/* _TOFU_MSG_H */
