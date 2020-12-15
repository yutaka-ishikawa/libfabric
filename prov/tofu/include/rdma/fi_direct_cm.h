#ifndef FI_DIRECT_CM_H

#define FI_DIRECT_CM_H
#define FABRIC_DIRECT_CM

extern int	tofu_sep_cm_getname(struct fid *fid, void *addr, size_t *addrlen);

/*
 * struct fi_ops_cm tofu_sep_ops_cm in tofu_cm.c
 * .setname
 */
static inline int fi_setname(fid_t fid, void *addr, size_t addrlen)
{
    return -FI_ENOSYS;
}

/*
 * .getname
 */
static inline int fi_getname(fid_t fid, void *addr, size_t *addrlen)
{
    return tofu_sep_cm_getname(fid, addr, addrlen);
}

/*
 * .getpeer
 */
static inline int fi_getpeer(struct fid_ep *ep, void *addr, size_t *addrlen)
{
    return -FI_ENOSYS;
}

/*
 * .listen
 */
static inline int fi_listen(struct fid_pep *pep)
{
    return -FI_ENOSYS;
}

/*
 * .connect
 */
static inline int
fi_connect(struct fid_ep *ep, const void *addr,
	   const void *param, size_t paramlen)
{
    return -FI_ENOSYS;
}

/*
 * .accept
 */
static inline int
fi_accept(struct fid_ep *ep, const void *param, size_t paramlen)
{
    return -FI_ENOSYS;
}

/*
 * .reject
 */
static inline int
fi_reject(struct fid_pep *pep, fid_t handle,
	  const void *param, size_t paramlen)
{
    return -FI_ENOSYS;
}

/*
 * .shutdown
 */
static inline int fi_shutdown(struct fid_ep *ep, uint64_t flags)
{
    return -FI_ENOSYS;
}

/*
 * .join
 */
static inline int fi_join(struct fid_ep *ep, const void *addr, uint64_t flags,
			  struct fid_mc **mc, void *context)
{
    return -FI_ENOSYS;
}

static inline fi_addr_t fi_mc_addr(struct fid_mc *mc)
{
    return mc->fi_addr;
}

#endif /* FI_DIRECT_CM_H */
