#ifndef FI_DIRECT_DOMAIN_H

#define FI_DIRECT_DOMAIN_H
#define FABRIC_DIRECT_DOMAIN

extern int	tofu_domain_open(struct fid_fabric *fid_fab, struct fi_info *info,
				     struct fid_domain **fid_dom, void *context);
extern int	tofu_cq_open(struct fid_domain *fid_dom, struct fi_cq_attr *attr,
				 struct fid_cq **fid_cq_, void *context);
extern int	tofu_cntr_open(struct fid_domain *fid_dom, struct fi_cntr_attr *attr,
				   struct fid_cntr **fid_ctr, void *context);
extern int	tofu_mr_reg(struct fid *fid, const void *buf,  size_t len,
				uint64_t access, uint64_t offset,
				uint64_t requested_key,  uint64_t flags,
				struct fid_mr **fid_mr_, void *context);
extern int	tofu_av_open(struct fid_domain *fid_dom, struct fi_av_attr *attr,
				 struct fid_av **fid_av_, void *context);
extern int	tofu_av_insert(struct fid_av *fid_av_,  const void *addr,  size_t count,
				   fi_addr_t *fi_addr,  uint64_t flags, void *context);
extern int	tofu_av_remove(struct fid_av *fid_av_,  fi_addr_t *fi_addr, size_t count, uint64_t flags);
extern int	tofu_av_lookup(struct fid_av *fid_av_,  fi_addr_t fi_addr, void *addr,  size_t *addrlen);
extern const char *tofu_av_straddr(struct fid_av *fid_av_, const void *addr, char *buf, size_t *len);

/*
 * struct fi_ops_fabric tofu_fab_ops in tofu_fab.c
 *	.domain
 */
static inline int
fi_domain(struct fid_fabric *fabric, struct fi_info *info,
	   struct fid_domain **domain, void *context)
{
    return tofu_domain_open(fabric, info, domain, context);
}

/*
 * struct fi_ops_fabric tofu_fab_fi_ops in tofu_fab.c
 *	.bind
 */
static inline int
fi_domain_bind(struct fid_domain *domain, struct fid *fid, uint64_t flags)
{
    return -FI_ENOSYS;
}

/*
 * struct fi_ops_domain tofu_dom_ops in tofu_dom.c
 *	cq_open
 */
static inline int
fi_cq_open(struct fid_domain *domain, struct fi_cq_attr *attr,
	   struct fid_cq **cq, void *context)
{
    return tofu_cq_open(domain, attr, cq, context);
}

/*
 * struct fi_ops_domain tofu_dom_ops in tofu_dom.c
 *	.cntr_open
 */
static inline int
fi_cntr_open(struct fid_domain *domain, struct fi_cntr_attr *attr,
	      struct fid_cntr **cntr, void *context)
{
    return tofu_cntr_open(domain, attr, cntr, context);
}

/*
 * struct fi_ops_fabric tofu_fab_ops in tofu_fab.c
 *	>wait_open
 */
static inline int
fi_wait_open(struct fid_fabric *fabric, struct fi_wait_attr *attr,
	     struct fid_wait **waitset)
{
    return -FI_ENOSYS;
}

/*
 * struct fi_ops_domain tofu_dom_ops in tofu_dom.c
 *	.poll_open
 */
static inline int
fi_poll_open(struct fid_domain *domain, struct fi_poll_attr *attr,
	     struct fid_poll **pollset)
{
    return -FI_ENOSYS;
}

/*
 * struct fi_ops_mr tofu_mr_ops in tofu_mr.c
 *	.reg
 */
static inline int
fi_mr_reg(struct fid_domain *domain, const void *buf, size_t len,
	  uint64_t access, uint64_t offset, uint64_t requested_key,
	  uint64_t flags, struct fid_mr **mr, void *context)
{
    return tofu_mr_reg(&domain->fid, buf, len, access, offset,
		       requested_key, flags, mr, context);
}

/*
 * .regv
 */
static inline int
fi_mr_regv(struct fid_domain *domain, const struct iovec *iov,
			size_t count, uint64_t access,
			uint64_t offset, uint64_t requested_key,
			uint64_t flags, struct fid_mr **mr, void *context)
{
    return -FI_NOSYS;
}

/*
 * .regattr
 */
static inline int
fi_mr_regattr(struct fid_domain *domain, const struct fi_mr_attr *attr,
			uint64_t flags, struct fid_mr **mr)
{
    return -FI_NOSYS;
}

static inline void *fi_mr_desc(struct fid_mr *mr)
{
    return mr->mem_desc;
}

static inline uint64_t fi_mr_key(struct fid_mr *mr)
{
    return mr->key;
}

static inline int
fi_mr_raw_attr(struct fid_mr *mr, uint64_t *base_addr,
	       uint8_t *raw_key, size_t *key_size, uint64_t flags)
{
    struct fi_mr_raw_attr attr;

    attr.flags = flags;
    attr.base_addr = base_addr;
    attr.raw_key = raw_key;
    attr.key_size = key_size;
    return mr->fid.ops->control(&mr->fid, FI_GET_RAW_MR, &attr);
}

static inline int
fi_mr_map_raw(struct fid_domain *domain, uint64_t base_addr,
	      uint8_t *raw_key, size_t key_size, uint64_t *key, uint64_t flags)
{
    struct fi_mr_map_raw map;

    map.flags = flags;
    map.base_addr = base_addr;
    map.raw_key = raw_key;
    map.key_size = key_size;
    map.key = key;
    return domain->fid.ops->control(&domain->fid, FI_MAP_RAW_MR, &map);
}

static inline int
fi_mr_unmap_key(struct fid_domain *domain, uint64_t key)
{
    return -FI_ENOSYS;
}

static inline int fi_mr_bind(struct fid_mr *mr, struct fid *bfid, uint64_t flags)
{
    return -FI_ENOSYS;
}

static inline int
fi_mr_refresh(struct fid_mr *mr, const struct iovec *iov, size_t count,
	      uint64_t flags)
{
    return -FI_ENOSYS;
}

static inline int fi_mr_enable(struct fid_mr *mr)
{
    return -FI_ENOSYS;
}

/*
 * struct fi_ops_domain tofu_dom_ops in tofu_dom.c
 *	.av_open
 */
static inline int
fi_av_open(struct fid_domain *domain, struct fi_av_attr *attr,
	   struct fid_av **av, void *context)
{
    return tofu_av_open(domain, attr, av, context);
}

/*
 * static struct fi_ops tofu_av_fi_ops in tofu_av.c
 */
static inline int
fi_av_bind(struct fid_av *av, struct fid *fid, uint64_t flags)
{
    return -FI_ENOSYS;
}

/*
 * static struct fi_ops_av tofu_av_ops in tofu_av.c
 *	.insert
 */
static inline int
fi_av_insert(struct fid_av *av, const void *addr, size_t count,
	     fi_addr_t *fi_addr, uint64_t flags, void *context)
{
    return tofu_av_insert(av, addr, count, fi_addr, flags, context);
}

static inline int
fi_av_insertsvc(struct fid_av *av, const char *node, const char *service,
		fi_addr_t *fi_addr, uint64_t flags, void *context)
{
	return -FI_ENOSYS;
}

static inline int
fi_av_insertsym(struct fid_av *av, const char *node, size_t nodecnt,
		const char *service, size_t svccnt,
		fi_addr_t *fi_addr, uint64_t flags, void *context)
{
    return -FI_ENOSYS;
}

/*
 * .remove
 */
static inline int
fi_av_remove(struct fid_av *av, fi_addr_t *fi_addr, size_t count, uint64_t flags)
{
    return tofu_av_remove(av, fi_addr, count, flags);
}

/*
 * .lookup
 */
static inline int
fi_av_lookup(struct fid_av *av, fi_addr_t fi_addr, void *addr, size_t *addrlen)
{
    return tofu_av_lookup(av, fi_addr, addr, addrlen);
}

/*
 * .stradd
 */
static inline const char *
fi_av_straddr(struct fid_av *av, const void *addr, char *buf, size_t *len)
{
	return tofu_av_straddr(av, addr, buf, len);
}

fi_rx_addr(fi_addr_t fi_addr, int rx_index, int rx_ctx_bits)
{
	return (fi_addr_t) (((uint64_t) rx_index << (64 - rx_ctx_bits)) | fi_addr);
}

#endif /* FI_DIRECT_DOMAIN_H */
