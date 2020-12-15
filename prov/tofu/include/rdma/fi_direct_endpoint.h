#ifndef FI_DIRECT_ENDPOINT_H

#define FI_DIRECT_ENDPOINT_H
#define FABRIC_DIRECT_ENDPOINT

extern int	tofu_endpoint(struct fid_domain *domain, struct fi_info *info, struct fid_ep **ep, void *context);
extern int	tofu_sep_open(struct fid_domain *fid_dom,  struct fi_info *info, struct fid_ep **fid_sep,   void *context);
extern ssize_t	tofu_ctx_msg_recv(struct fid_ep *fid_ep, void *buf,  size_t len, void *desc, fi_addr_t src_addr, void *context);
extern ssize_t	tofu_ctx_msg_recvv(struct fid_ep *fid_ep, const struct iovec *iov,
				   void **desc, size_t count, fi_addr_t src_addr, void *context);
extern ssize_t	tofu_ctx_msg_recvmsg(struct fid_ep *fid_ep, const struct fi_msg *msg, uint64_t flags);
extern ssize_t	tofu_ctx_msg_send_common(struct fid_ep *fid_ep, const struct fi_msg_tagged *msg, uint64_t flags);
extern ssize_t	tofu_ctx_msg_sendv(struct fid_ep *fid_ep, const struct iovec *iov, void **desc,
				   size_t count, fi_addr_t dest_addr, void *context);
extern ssize_t	tofu_ctx_msg_sendmsg(struct fid_ep *fid_ep,const struct fi_msg *msg, uint64_t flags);
extern ssize_t	tofu_ctx_msg_inject(struct fid_ep *fid_ep,  const void *buf, size_t len, fi_addr_t dest_addr);

/*
 * struct fi_ops_fabric tofu_fab_ops in tofu_fab.c
 *	.passive_ep
 */
static inline int
fi_passive_ep(struct fid_fabric *fabric, struct fi_info *info,
	     struct fid_pep **pep, void *context)
{
    return -FI_ENOSYS;
}

/*
 *  struct fi_ops_domain tofu_dom_ops in tofu_dom.c
 *	.endpoint
 */
static inline int
fi_endpoint(struct fid_domain *domain, struct fi_info *info,
	    struct fid_ep **ep, void *context)
{
    return tofu_endpoint(domain, info, ep, context);
}

/*
 * struct fi_ops_domain tofu_dom_ops in tofu_dom.c
 *	.scalable_ep
 */
static inline int
fi_scalable_ep(struct fid_domain *domain, struct fi_info *info,
	    struct fid_ep **sep, void *context)
{
    return tofu_sep_open(domain, info, sep, context);
}

/*
 * static struct fi_ops tofu_ctx_fi_ops  in tofu_ctx
 *	.bind
 */
static inline int fi_ep_bind(struct fid_ep *ep, struct fid *bfid, uint64_t flags)
{
    return ep->fid.ops->bind(&ep->fid, bfid, flags);
    // return tofu_ctx_bind(ep, bfid, flags);
}

/*
 *	pep->fid.ops->bind
 */
static inline int fi_pep_bind(struct fid_pep *pep, struct fid *bfid, uint64_t flags)
{
    return pep->fid.ops->bind(&pep->fid, bfid, flags);
}

/*
 * static struct fi_ops tofu_sep_fi_ops  in tofu_ctx
 *	
 */
static inline int fi_scalable_ep_bind(struct fid_ep *sep, struct fid *bfid, uint64_t flags)
{
    return sep->fid.ops->bind(&sep->fid, bfid, flags);
}

static inline int fi_enable(struct fid_ep *ep)
{
    return ep->fid.ops->control(&ep->fid, FI_ENABLE, NULL);
    // return tofu_ctx_ctrl(&ep->fid, FI_ENABLE, NULL);
}

static inline ssize_t fi_cancel(fid_t fid, void *context)
{
    struct fid_ep *ep = container_of(fid, struct fid_ep, fid);
    return ep->ops->cancel(fid, context);
    // return tofu_ctx_cancel(fid, context);
}

static inline int
fi_setopt(fid_t fid, int level, int optname, const void *optval, size_t optlen)
{
    struct fid_ep *ep = container_of(fid, struct fid_ep, fid);
    return ep->ops->setopt(fid, level, optname, optval, optlen);
    // return tofu_ctx_setopt(fid, level, optname, optval, optlen);
}

static inline int
fi_getopt(fid_t fid, int level, int optname, void *optval, size_t *optlen)
{
    struct fid_ep *ep = container_of(fid, struct fid_ep, fid);
    return ep->ops->getopt(fid, level, optname, optval, optlen);
    // return tofu_ctx_getopt(fid, level, optname, optval, optlen);
}

static inline int fi_ep_alias(struct fid_ep *ep, struct fid_ep **alias_ep,
			      uint64_t flags)
{
	int ret;
	struct fid *fid;
	ret = fi_alias(&ep->fid, &fid, flags);
	if (!ret)
		*alias_ep = container_of(fid, struct fid_ep, fid);
	return ret;
}

static inline int
fi_tx_context(struct fid_ep *ep, int index, struct fi_tx_attr *attr,
	      struct fid_ep **tx_ep, void *context)
{
    return -FI_NOSYS;
}

static inline int
fi_rx_context(struct fid_ep *ep, int index, struct fi_rx_attr *attr,
	      struct fid_ep **rx_ep, void *context)
{
    return -FI_NOSYS;
}

static inline FI_DEPRECATED_FUNC ssize_t
fi_rx_size_left(struct fid_ep *ep)
{
    return -FI_NOSYS;
}

static inline FI_DEPRECATED_FUNC ssize_t
fi_tx_size_left(struct fid_ep *ep)
{
    return -FI_NOSYS;
}


static inline int
fi_stx_context(struct fid_domain *domain, struct fi_tx_attr *attr,
	       struct fid_stx **stx, void *context)
{
    return -FI_NOSYS;
}

static inline int
fi_srx_context(struct fid_domain *domain, struct fi_rx_attr *attr,
	       struct fid_ep **rx_ep, void *context)
{
    return -FI_ENOSYS;
}

/*
 * struct fi_ops_msg tofu_ctx_ops_msg in tofu_msg.c
 *	.recv
 */
static inline ssize_t
fi_recv(struct fid_ep *ep, void *buf, size_t len, void *desc, fi_addr_t src_addr,
	void *context)
{
    return tofu_ctx_msg_recv(ep, buf, len, desc, src_addr, context);
}

/*
 * .recvv
 */
static inline ssize_t
fi_recvv(struct fid_ep *ep, const struct iovec *iov, void **desc,
	 size_t count, fi_addr_t src_addr, void *context)
{
    return tofu_ctx_msg_recvv(ep, iov, desc, count, src_addr, context);
}

/*
 * .recmsg
 */
static inline ssize_t
fi_recvmsg(struct fid_ep *ep, const struct fi_msg *msg, uint64_t flags)
{
    return tofu_ctx_msg_recvmsg(ep, msg, flags);
}

/*
 * .send
 */
static inline ssize_t
fi_send(struct fid_ep *ep, const void *buf, size_t len, void *desc,
	fi_addr_t dest_addr, void *context)
{
    return tofu_ctx_msg_send(ep, buf, len, desc, dest_addr, context);
}

/*
 * .sendv
 */
static inline ssize_t
fi_sendv(struct fid_ep *ep, const struct iovec *iov, void **desc,
	 size_t count, fi_addr_t dest_addr, void *context)
{
    return tofu_ctx_msg_sendv(ep, iov, desc, count, dest_addr, context);
}

/*
 * .sendmsg
 */
static inline ssize_t
fi_sendmsg(struct fid_ep *ep, const struct fi_msg *msg, uint64_t flags)
{
    return tofu_ctx_msg_sendmsg(ep, msg, flags);
}

/*
 * .inject
 */
static inline ssize_t
fi_inject(struct fid_ep *ep, const void *buf, size_t len, fi_addr_t dest_addr)
{
    return tofu_ctx_msg_inject(ep, buf, len, dest_addr);
}

/*
 * .senddata
 */
static inline ssize_t
fi_senddata(struct fid_ep *ep, const void *buf, size_t len, void *desc,
	      uint64_t data, fi_addr_t dest_addr, void *context)
{
    return -FI_NOSYS;
}

/*
 * .injectdata
 */
static inline ssize_t
fi_injectdata(struct fid_ep *ep, const void *buf, size_t len,
		uint64_t data, fi_addr_t dest_addr)
{
    return -FI_NOSYS
}

#endif /* FI_DIRECT_ENDPOINT */
