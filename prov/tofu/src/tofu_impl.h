/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_IMPL_H
#define _TOFU_IMPL_H

#include "tofu_conf.h"

#include <rdma/fi_errno.h>		/* for FI_ENOMEM */
#include <rdma/providers/fi_prov.h>	/* for struct fi_provider */
#include <rdma/providers/fi_log.h>	/* for FI_INFO() */
#include <rdma/fabric.h>		/* for struct fi_info */

#include <ofi_util.h>			/* for ofi_check_info() */
#include <ofi_iov.h>			/* for ofi_copy_io_iov() */
#include <ofi_atomic.h>			/* for ofi_datatype_size() */

#define TOFU_IOV_LIMIT	    4

/* === internal structures ============================================ */

#ifdef	CONF_TOFU_RECV	/* DONE */
/* #include <ofi_list.h> */

/*
 * diff fi_msg fi_msg_tagged
 * +  uint64_t tag;
 * +  uint64_t ignore;
 */
struct tofu_recv_en {
    struct dlist_entry	    entry;
    struct fi_msg_tagged    tmsg;
    struct iovec	    iovs[TOFU_IOV_LIMIT];
    void *		    dscs[TOFU_IOV_LIMIT];
    struct fid *	    fidp;
    uint64_t		    flags;
};

/*
 *   struct tofu_recv_fs;
 *   void tofu_recv_fs_init(struct tofu_recv_fs *fs, size_t size);
 *   struct tofu_recv_fs *tofu_recv_fs_create(size_t size);
 *   void tofu_recv_fs_free(struct tofu_recv_fs *fs);
 *   int tofu_recv_fs_index(struct tofu_recv_fs *fs, struct tofu_recv_en *en);
 */
DECLARE_FREESTACK(struct tofu_recv_en, tofu_recv_fs); /* see ofi_mem.h */

#endif	/* CONF_TOFU_RECV */

/* -------------------------------------------------------------------- */

/* #include <ofi_rbuf.h> */

/*
 * circular queue for completion queue (tofu_comp_cirq)
 */
OFI_DECLARE_CIRQUE(struct fi_cq_tagged_entry, tofu_ccirq);
OFI_DECLARE_CIRQUE(struct fi_cq_err_entry, tofu_ccireq);

/* === structures ===================================================== */
#include "tofu_prv.h"

struct tofu_fabric {
    struct fid_fabric	fab_fid;
    ofi_atomic32_t	fab_ref;
    fastlock_t		fab_lck;
/*  struct dlist_entry	fab_ent; */
};

struct tofu_domain {
    struct fid_domain	dom_fid;
    struct tofu_fabric *dom_fab;
    ofi_atomic32_t	dom_ref;
    fastlock_t		dom_lck;
/*  struct dlist_entry	dom_ent; */
    uint32_t		dom_fmt;    /* addr_format */
    /* For MR */
    uintptr_t           dom_vcqh[8];
    int                 dom_nvcq;
};

/*
 * cq__htx is set if fi_ep_bind(..., FI_TRANSMIT)
 * cq_ssel is also set if fi_ep_bind(.., FI_TRANSMIT| FI_SELECTIVE_COMPLETION)
 * cq__rtx is set in fi_ep_bind(..., ..., FI_RECV)
 * cq_rsel is also set if fi_ep_bind(.., FI_RECV | FI_SELECTIVE_COMPLETION)
 */
#define TOFU_CQ_SELECTIVE       1
struct tofu_cq {
    struct fid_cq	cq__fid;
    struct tofu_domain *cq__dom;
    ofi_atomic32_t	cq__ref;
    fastlock_t		cq__lck;
/*  struct dlist_entry	cq__ent; */
    struct dlist_entry	cq__htx;   /* head for ep tx ctxs */
    struct dlist_entry	cq__hrx;   /* head for ep rx ctxs */
    struct tofu_ccirq  *cq__ccq;   /* circular queue for fi_cq_tagged_entry */
    struct tofu_ccireq *cq_cceq;   /* circular queue for fi_cq_err_entry */
    int                 cq_ssel;   /* for FI_SELECTIVE_COMPLETION */
    int                 cq_rsel;   /* for FI_SELECTIVE_COMPLETION */
};

struct tofu_cntr {
    struct fid_cntr	ctr_fid;
    struct tofu_domain *ctr_dom;
    ofi_atomic32_t	ctr_ref;
    fastlock_t		ctr_lck;
/*  struct dlist_entry	ctr_ent; */
    struct dlist_entry	ctr_htx;    /* head for ep transmit ctxs */
    struct dlist_entry	ctr_hrx;    /* head for ep read ctxs */
    ofi_atomic64_t	ctr_ctr;
    ofi_atomic64_t	ctr_err;
    int                 ctr_tsl;   /* for FI_SELECTIVE_COMPLETION */
    int                 ctr_rsl;   /* for FI_SELECTIVE_COMPLETION */
};

struct tofu_av {
    struct fid_av	av__fid;
    struct tofu_domain *av__dom;
    int			av__rxb;    /* rx_ctx_bits */
    ofi_atomic32_t	av__ref;
    fastlock_t		av__lck;
/*  struct dlist_entry	av__ent; */
    struct tofu_av_tab {
	size_t		mct;
	size_t		nct;
	void *		tab;
    }			av__tab;
};

/* === variables ====================================================== */

extern struct fi_provider		tofu_prov;

extern struct fi_ops_mr			tofu_mr__ops;

extern struct fi_ops_cm			tofu_sep_ops_cm;
extern struct fi_ops_cm			tofu_cep_ops_cm;

extern struct fi_ops_msg		tofu_cep_ops_msg;
extern struct fi_ops_tagged		tofu_cep_ops_tag;

extern struct fi_ops_rma		tofu_cep_ops_rma;
extern struct fi_ops_atomic		tofu_cep_ops_atomic;

/* === function prototypes ============================================ */

extern int	tofu_init_prov_info(
		    const struct fi_info *hints,
		    struct fi_info **infos
		);
extern int	tofu_chck_prov_info(
		    uint32_t api_version,
		    const struct fi_info *hints,
		    struct fi_info **infos
		);

extern int	tofu_fabric_open(
		    struct fi_fabric_attr *attr,
		    struct fid_fabric **fid,
		    void *context
		);
extern int	tofu_domain_open(
		    struct fid_fabric *fabric,
		    struct fi_info *info,
		    struct fid_domain **fid,
		    void *context
		);
extern int	tofu_cq_open(
		    struct fid_domain *fid_dom,
		    struct fi_cq_attr *attr,
		    struct fid_cq **fid_cq_,
		    void *context
		);
extern int	tofu_cntr_open(
		    struct fid_domain *fid_dom,
		    struct fi_cntr_attr *attr,
		    struct fid_cntr **fid_ctr,
		    void *context
		);
extern int	tofu_av_open(
		    struct fid_domain *fid_dom,
		    struct fi_av_attr *attr,
		    struct fid_av **fid_av_,
		    void *context
		);
extern int	tofu_sep_open(
		    struct fid_domain *fid_dom,
		    struct fi_info *info,
		    struct fid_ep **fid_sep,
		    void *context
		);
extern int	tofu_cep_tx_context(
		    struct fid_ep *fid_sep,
		    int index,
		    struct fi_tx_attr *attr,
		    struct fid_ep **fid_cep_tx,
		    void *context
		);
extern int	tofu_cep_rx_context(
		    struct fid_ep *fid_sep,
		    int index,
		    struct fi_rx_attr *attr,
		    struct fid_ep **fid_cep_rx,
		    void *context
		);
extern int	tofu_cep_ctrl(struct fid *fid, int command, void *arg);

extern int	tofu_chck_fab_attr(
		    const struct fi_fabric_attr *prov_attr,
		    const struct fi_fabric_attr *user_attr
		);
extern int	tofu_chck_dom_attr(
		    const struct fi_domain_attr *prov_attr,
		    const struct fi_info *user_info
		);
extern int	tofu_chck_av_attr(
		    const struct fi_av_attr *user_attr
		);
extern int	tofu_chck_cq_attr(
		    const struct fi_cq_attr *user_attr
		);
extern int	tofu_chck_cep_tx_attr(
		    const struct fi_tx_attr *prov_attr,
		    const struct fi_tx_attr *user_attr,
		    uint64_t user_info_mode
		);
extern int	tofu_chck_cep_rx_attr(
		    const struct fi_info *prov_info,
		    const struct fi_rx_attr *user_attr,
		    uint64_t user_info_mode
		);
extern int	tofu_chck_ep_attr(
		    const struct fi_info *prov_info,
		    const struct fi_info *user_info
		);

#include "tofu_utl.h"
#include "tofu_int.h"

#include "tofu_av.h"
#include "tofu_cq.h"
#include "tofu_ctr.h"
#include "tofu_sep.h"
#include "tofu_msg.h"

#endif	/* _TOFU_IMPL_H */
