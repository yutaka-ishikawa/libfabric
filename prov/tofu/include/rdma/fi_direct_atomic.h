#ifndef FI_DIRECT_ATOMIC_H

#define FI_DIRECT_ATOMIC_H
#define FABRIC_DIRECT_ATOMIC

extern ssize_t	tofu_ctx_atm_writemsg(struct fid_ep *fid_ep, const struct fi_msg_atomic *msg,
				      uint64_t flags);
extern ssize_t	tofu_ctx_atm_readwritemsg(struct fid_ep *fid_ep, const struct fi_msg_atomic *msg,
					  struct fi_ioc *resultv, void **result_desc, size_t result_count, uint64_t flags);
extern ssize_t	tofu_ctx_atm_compwritemsg(struct fid_ep *fid_ep, const struct fi_msg_atomic *msg,
					  const struct fi_ioc *comparev,
					  void **compare_desc, size_t compare_count,
					  struct fi_ioc *resultv, void **result_desc,
					  size_t result_count, uint64_t flags);
extern int	tofu_ctx_atm_readwritevalid(struct fid_ep *ep, enum fi_datatype datatype, enum fi_op op, size_t *count);
extern int	tofu_ctx_atm_writevalid(struct fid_ep *ep, enum fi_datatype datatype, enum fi_op op, size_t *count);
extern int	tofu_ctx_atm_compwritevalid(struct fid_ep *ep, enum fi_datatype datatype, enum fi_op op, size_t *count);

/*
 * .write
 */
static inline ssize_t
fi_atomic(struct fid_ep *ep, const void *buf, size_t count, void *desc,
	  fi_addr_t dest_addr, uint64_t addr, uint64_t key,
	  enum fi_datatype datatype, enum fi_op op, void *context)
{
    return -FI_ENOSYS;
}

/*
 * .writev
 */
static inline ssize_t
fi_atomicv(struct fid_ep *ep, const struct fi_ioc *iov, void **desc, size_t count,
	   fi_addr_t dest_addr, uint64_t addr, uint64_t key,
	   enum fi_datatype datatype, enum fi_op op, void *context)
{
    return -FI_ENOSYS;
}

/*
 * .writemsg
 */
static inline ssize_t
fi_atomicmsg(struct fid_ep *ep, const struct fi_msg_atomic *msg, uint64_t flags)
{
    return tofu_ctx_atm_writemsg(ep, msg, flags);
}

/*
 * .inject
 */
static inline ssize_t
fi_inject_atomic(struct fid_ep *ep, const void *buf, size_t count,
		 fi_addr_t dest_addr, uint64_t addr, uint64_t key,
		 enum fi_datatype datatype, enum fi_op op)
{
    return -FI_ENOSYS;
}

/*
 * .readwrite
 */
static inline ssize_t
fi_fetch_atomic(struct fid_ep *ep, const void *buf, size_t count, void *desc,
		void *result, void *result_desc, fi_addr_t dest_addr,
		uint64_t addr, uint64_t key,
		enum fi_datatype datatype, enum fi_op op, void *context)
{
    return -FI_ENOSYS;
}

/*
 * .readwritev
 */
static inline ssize_t
fi_fetch_atomicv(struct fid_ep *ep, const struct fi_ioc *iov, void **desc, size_t count,
		 struct fi_ioc *resultv, void **result_desc, size_t result_count,
		 fi_addr_t dest_addr, uint64_t addr, uint64_t key,
		 enum fi_datatype datatype, enum fi_op op, void *context)
{
    return -FI_ENOSYS;
}

/*
 * .readwritemsg
 */
static inline ssize_t
fi_fetch_atomicmsg(struct fid_ep *ep, const struct fi_msg_atomic *msg,
		   struct fi_ioc *resultv, void **result_desc, size_t result_count, uint64_t flags)
{
    return tofu_ctx_atm_readwritemsg(ep, msg, resultv, result_desc,
				     result_count, flags);
}

/*
 * .compwrite
 */
static inline ssize_t
fi_compare_atomic(struct fid_ep *ep, const void *buf, size_t count, void *desc,
		  const void *compare, void *compare_desc,
		  void *result, void *result_desc,
		  fi_addr_t dest_addr, uint64_t addr, uint64_t key,
		  enum fi_datatype datatype, enum fi_op op, void *context)
{
    return -FI_ENOSYS;
}

/*
 * .compwritev
 */
static inline ssize_t
fi_compare_atomicv(struct fid_ep *ep, const struct fi_ioc *iov, void **desc, size_t count,
		   const struct fi_ioc *comparev, void **compare_desc, size_t compare_count,
		   struct fi_ioc *resultv, void **result_desc, size_t result_count,
		   fi_addr_t dest_addr, uint64_t addr, uint64_t key,
		   enum fi_datatype datatype, enum fi_op op, void *context)
{
    return -FI_ENOSYS;
}

/*
 * .compwritemsg
 */
static inline ssize_t
fi_compare_atomicmsg(struct fid_ep *ep,  const struct fi_msg_atomic *msg,
		     const struct fi_ioc *comparev, void **compare_desc, size_t compare_count,
		     struct fi_ioc *resultv, void **result_desc, size_t result_count,
		     uint64_t flags)
{
    return tofu_ctx_atm_compwritemsg(ep, msg,
				     comparev, compare_desc, compare_count,
				     resultv, result_desc, result_count, flags);
}

/*
 * .writevalid
 */
static inline int
fi_atomicvalid(struct fid_ep *ep,
	       enum fi_datatype datatype, enum fi_op op, size_t *count)
{
    return tofu_ctx_atm_writevalid(ep, datatype, op, count);
}

/*
 * .readwritevalid
 */
static inline int
fi_fetch_atomicvalid(struct fid_ep *ep, enum fi_datatype datatype, enum fi_op op, size_t *count)
{
    return tofu_ctx_atm_readwritevalid(ep, datatype, op, count);
}

/*
 * .compwritevalid
 */
static inline int
fi_compare_atomicvalid(struct fid_ep *ep, enum fi_datatype datatype, enum fi_op op, size_t *count)
{
    return tofu_ctx_atm_compwritevalid(ep, datatype, op, count);
}

/*
 * See tofu_dom.c static struct fi_ops_domain tofu_dom_ops = { .query_atomic }
 */
static inline int
fi_query_atomic(struct fid_domain *domain, enum fi_datatype datatype, enum fi_op op,
		struct fi_atomic_attr *attr, uint64_t flags)
{
    return -FI_ENOSYS;
}

#endif /* FI_DIRECT_ATOMIC_H */
