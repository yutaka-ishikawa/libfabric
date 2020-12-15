#ifndef FI_DIRECT_TAGGED_H
#define FI_DIRECT_TAGGED_H

extern ssize_t	tofu_ctx_tag_recv(struct fid_ep *fid_ep, void *buf, size_t len, void *desc, fi_addr_t src_addr,
				  uint64_t tag, uint64_t ignore,  void *context);
extern ssize_t	tofu_ctx_tag_recvv(struct fid_ep *fid_ep, const struct iovec *iov,
				   void **desc, size_t count, fi_addr_t src_addr,
				   uint64_t tag, uint64_t ignore, void *context);
extern ssize_t	tofu_ctx_msg_recvmsg(struct fid_ep *fid_ep, const struct fi_msg *msg, uint64_t flags);
extern ssize_t	tofu_ctx_msg_send(struct fid_ep *fid_ep, const void *buf, size_t len,
				  void *desc, fi_addr_t dest_addr, void *context);
extern ssize_t	tofu_ctx_tag_sendv(struct fid_ep *fid_ep, const struct iovec *iov,
				   void **desc, size_t count, fi_addr_t dest_addr,
				   uint64_t tag, void *context);
extern ssize_t	tofu_ctx_msg_sendmsg(struct fid_ep *fid_ep,const struct fi_msg *msg, uint64_t flags);
extern ssize_t	tofu_ctx_msg_inject(struct fid_ep *fid_ep,  const void *buf, size_t len, fi_addr_t dest_addr);

static inline ssize_t
fi_trecv(struct fid_ep *ep, void *buf, size_t len, void *desc,
	 fi_addr_t src_addr, uint64_t tag, uint64_t ignore, void *context)
{
    return tofu_ctx_tag_recv(ep, buf, len, desc, src_addr, tag, ignore,	context);
}

static inline ssize_t
fi_trecvv(struct fid_ep *ep, const struct iovec *iov, void **desc,
	  size_t count, fi_addr_t src_addr, uint64_t tag, uint64_t ignore,
	  void *context)
{
    return tofu_ctx_tag_recvv(ep, iov, desc, count, src_addr, tag, ignore, context);
}

static inline ssize_t
fi_trecvmsg(struct fid_ep *ep, const struct fi_msg_tagged *msg, uint64_t flags)
{
    return tofu_ctx_msg_recvmsg(ep, msg, flags);
}

static inline ssize_t
fi_tsend(struct fid_ep *ep, const void *buf, size_t len, void *desc,
	 fi_addr_t dest_addr, uint64_t tag, void *context)
{
    return tofu_ctx_msg_send(ep, buf, len, desc, dest_addr, tag, context);
}

static inline ssize_t
fi_tsendv(struct fid_ep *ep, const struct iovec *iov, void **desc,
	  size_t count, fi_addr_t dest_addr, uint64_t tag, void *context)
{
    return tofu_ctx_msg_sendv(ep, iov, desc, count, dest_addr,tag, context);
}

static inline ssize_t
fi_tsendmsg(struct fid_ep *ep, const struct fi_msg_tagged *msg, uint64_t flags)
{
    return tofu_ctx_msg_sendmsg(ep, msg, flags);
}

static inline ssize_t
fi_tinject(struct fid_ep *ep, const void *buf, size_t len,
	   fi_addr_t dest_addr, uint64_t tag)
{
    return tofu_ctx_msg_inject(ep, buf, len, dest_addr, tag);
}

static inline ssize_t
fi_tsenddata(struct fid_ep *ep, const void *buf, size_t len, void *desc,
	     uint64_t data, fi_addr_t dest_addr, uint64_t tag, void *context)
{
    return -FI_NOSYS;
}

static inline ssize_t
fi_tinjectdata(struct fid_ep *ep, const void *buf, size_t len,
	       uint64_t data, fi_addr_t dest_addr, uint64_t tag)
{
    return -FI_NOSYS;
}

#endif /* FI_DIRECT_TAGGED_H */
