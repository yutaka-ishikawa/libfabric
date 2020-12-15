#ifndef FI_DIRECT_RMA_H

#define FI_DIRECT_RMA_H
#define FABRIC_DIRECT_RMA

extern ssize_t	tofu_ctx_rma_read(struct fid_ep *fid_ep, void *buf, size_t len, void *desc,
				  fi_addr_t src_addr, uint64_t addr, uint64_t key, void *context);
extern ssize_t	tofu_ctx_rma_readmsg(struct fid_ep *fid_ep, const struct fi_msg_rma *msg, uint64_t flags)
extern ssize_t	tofu_ctx_rma_inject(struct fid_ep *fid_ep, const void *buf, size_t len,
				    fi_addr_t dest_addr, uint64_t addr, uint64_t key);

static inline ssize_t
fi_read(struct fid_ep *ep, void *buf, size_t len, void *desc,
	fi_addr_t src_addr, uint64_t addr, uint64_t key, void *context)
{
    return tofu_ctx_rma_read(ep, buf, len, desc, src_addr, addr, key, context);
}

static inline ssize_t
fi_readv(struct fid_ep *ep, const struct iovec *iov, void **desc,
	 size_t count, fi_addr_t src_addr, uint64_t addr, uint64_t key,
	 void *context)
{
    return -FI_NOSYS;
}

static inline ssize_t
fi_readmsg(struct fid_ep *ep, const struct fi_msg_rma *msg, uint64_t flags)
{
    return tofu_ctx_rma_readmsg(ep, msg, flags);
}

static inline ssize_t
fi_write(struct fid_ep *ep, const void *buf, size_t len, void *desc,
	 fi_addr_t dest_addr, uint64_t addr, uint64_t key, void *context)
{
    return -FI_NOSYS;
}

static inline ssize_t
fi_writev(struct fid_ep *ep, const struct iovec *iov, void **desc,
	 size_t count, fi_addr_t dest_addr, uint64_t addr, uint64_t key,
	 void *context)
{
    return -FI_NOSYS;
}

static inline ssize_t
fi_writemsg(struct fid_ep *ep, const struct fi_msg_rma *msg, uint64_t flags)
{
    return tofu_ctx_rma_writemsg(ep, msg, flags);
}

static inline ssize_t
fi_inject_write(struct fid_ep *ep, const void *buf, size_t len,
		fi_addr_t dest_addr, uint64_t addr, uint64_t key)
{
    return tofu_ctx_rma_inject(ep, buf, len, dest_addr, addr, key);
}

static inline ssize_t
fi_writedata(struct fid_ep *ep, const void *buf, size_t len, void *desc,
	       uint64_t data, fi_addr_t dest_addr, uint64_t addr, uint64_t key,
	       void *context)
{
    return -FI_NOSYS;
}

static inline ssize_t
fi_inject_writedata(struct fid_ep *ep, const void *buf, size_t len,
		uint64_t data, fi_addr_t dest_addr, uint64_t addr, uint64_t key)
{
    return -FI_NOSYS;
}

#endif /* FI_DIRECT_RMA_H */
