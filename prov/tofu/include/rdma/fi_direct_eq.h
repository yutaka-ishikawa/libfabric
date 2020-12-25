#ifndef FI_DIRECT_EQ_H

#define FI_DIRECT_EQ_H
#define FABRIC_DIRECT_EQ

extern ssize_t	tofu_cq_read(struct fid_cq *fid_cq, void *buf, size_t count);
extern ssize_t	tofu_cq_readerr(struct fid_cq *fid_cq, struct fi_cq_err_entry *buf,
				uint64_t flags);
extern uint64_t	tofu_cntr_read(struct fid_cntr *cntr_fid);
extern uint64_t	tofu_cntr_readerr(struct fid_cntr *cntr_fid);
extern int	tofu_cntr_add(struct fid_cntr *cntr_fid, uint64_t value);
extern int	tofu_cntr_adderr(struct fid_cntr *cntr_fid, uint64_t value);
extern int	tofu_cntr_set(struct fid_cntr *cntr_fid, uint64_t value);
extern int	tofu_cntr_seterr(struct fid_cntr *cntr_fid, uint64_t value);
extern int	tofu_cntr_wait(struct fid_cntr *cntr_fid, uint64_t threshold, int timeout);

static inline int
fi_trywait(struct fid_fabric *fabric, struct fid **fids, int count)
{
    return -FI_ENOSYS;
}

static inline int
fi_wait(struct fid_wait *waitset, int timeout)
{
    return -FI_ENOSYS;
}

/*
 * pollset and eq use util functions
 */

static inline int
fi_poll(struct fid_poll *pollset, void **context, int count)
{
    return -FI_ENOSYS;
}

static inline int
fi_poll_add(struct fid_poll *pollset, struct fid *event_fid, uint64_t flags)
{
    return -FI_ENOSYS;
}

static inline int
fi_poll_del(struct fid_poll *pollset, struct fid *event_fid, uint64_t flags)
{
    return -FI_ENOSYS;
}

static inline int
fi_eq_open(struct fid_fabric *fabric, struct fi_eq_attr *attr,
	   struct fid_eq **eq, void *context)
{
    return -FI_ENOSYS;
}

static inline ssize_t
fi_eq_read(struct fid_eq *eq, uint32_t *event, void *buf,
	   size_t len, uint64_t flags)
{
    return -FI_ENOSYS;
}

static inline ssize_t
fi_eq_readerr(struct fid_eq *eq, struct fi_eq_err_entry *buf, uint64_t flags)
{
    return -FI_ENOSYS;
}

static inline ssize_t
fi_eq_write(struct fid_eq *eq, uint32_t event, const void *buf,
	    size_t len, uint64_t flags)
{
    return -FI_ENOSYS;
}

static inline ssize_t
fi_eq_sread(struct fid_eq *eq, uint32_t *event, void *buf, size_t len,
	    int timeout, uint64_t flags)
{
    return -FI_ENOSYS;
}

static inline const char *
fi_eq_strerror(struct fid_eq *eq, int prov_errno, const void *err_data,
	       char *buf, size_t len)
{
    return eq->ops->strerror(eq, prov_errno, err_data, buf, len);;
}


static inline ssize_t fi_cq_read(struct fid_cq *cq, void *buf, size_t count)
{
    return tofu_cq_read(cq, buf, count);
}

static inline ssize_t
fi_cq_readfrom(struct fid_cq *cq, void *buf, size_t count, fi_addr_t *src_addr)
{
    return -FI_ENOSYS;
}

static inline ssize_t
fi_cq_readerr(struct fid_cq *cq, struct fi_cq_err_entry *buf, uint64_t flags)
{
    return tofu_cq_readerr(cq, buf, flags);
}

static inline ssize_t
fi_cq_sread(struct fid_cq *cq, void *buf, size_t count, const void *cond, int timeout)
{
    return -FI_ENOSYS;
}

static inline ssize_t
fi_cq_sreadfrom(struct fid_cq *cq, void *buf, size_t count,
		fi_addr_t *src_addr, const void *cond, int timeout)
{
    return -FI_ENOSYS;
}

static inline int fi_cq_signal(struct fid_cq *cq)
{
    return -FI_ENOSYS;
}

static inline const char *
fi_cq_strerror(struct fid_cq *cq, int prov_errno, const void *err_data,
	       char *buf, size_t len)
{
    return cq->ops->strerror(cq, prov_errno, err_data, buf, len);
}


static inline uint64_t fi_cntr_read(struct fid_cntr *cntr)
{
    return tofu_cntr_read(cntr);
}

static inline uint64_t fi_cntr_readerr(struct fid_cntr *cntr)
{
    return tofu_cntr_readerr(cntr);
}

static inline int fi_cntr_add(struct fid_cntr *cntr, uint64_t value)
{
    return tofu_cntr_add(cntr, value);
}

static inline int fi_cntr_adderr(struct fid_cntr *cntr, uint64_t value)
{
    return tofu_cntr_adderr(cntr, value);
}

static inline int fi_cntr_set(struct fid_cntr *cntr, uint64_t value)
{
    return tofu_cntr_set(cntr, value);
}

static inline int fi_cntr_seterr(struct fid_cntr *cntr, uint64_t value)
{
    return tofu_cntr_seterr(cntr, value);
}

static inline int
fi_cntr_wait(struct fid_cntr *cntr, uint64_t threshold, int timeout)
{
    return tofu_cntr_wait(cntr, threshold, timeout);
}

#endif /* FI_DIRECT_EQ_H */
