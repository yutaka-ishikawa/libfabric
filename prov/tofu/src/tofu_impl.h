/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_IMPL_H
#define _TOFU_IMPL_H

#include "tofu_conf.h"
#include "tofu_debug.h"
#include <utofu.h>
#include <jtofu.h>
//#include "tofu_prv.h"

#include <rdma/fi_errno.h>		/* for FI_ENOMEM */
#include <rdma/providers/fi_prov.h>	/* for struct fi_provider */
#include <rdma/providers/fi_log.h>	/* for FI_INFO() */
#include <rdma/fabric.h>		/* for struct fi_info */
#include <ofi_util.h>			/* for ofi_check_info() */
#include <ofi_iov.h>			/* for ofi_copy_io_iov() */
#include <ofi_atomic.h>			/* for ofi_datatype_size() */

#define TOFU_IOV_LIMIT	    4

struct tofu_fabric {
    struct fid_fabric	fab_fid;
    ofi_atomic32_t	fab_ref;
    fastlock_t		fab_lck;
/*  struct dlist_entry	fab_ent; */
};

struct tofu_domain {
    struct fid_domain	dom_fid;
    struct tofu_fabric *dom_fab;
    struct tofu_sep    *dom_sep;
    ofi_atomic32_t	dom_ref;
    fastlock_t		dom_lck;
/*  struct dlist_entry	dom_ent; */
    uint32_t		dom_fmt;    /* addr_format */
    /* tofu dependent */
    utofu_tni_id_t      tnis[8];
    size_t              ntni;
    utofu_vcq_hdl_t     vcqh[8];
};

struct tofu_ctx {
    struct fid_ep	ctx_fid;
    struct tofu_sep *	ctx_sep;
    ofi_atomic32_t	ctx_ref;
    uint32_t		ctx_enb;    /* enabled */
    int			ctx_idx;
    fastlock_t		ctx_lck;
/*  struct dlist_entry	ctx_ent; */
    uint64_t		ctx_xop_flg;
    struct dlist_entry	ctx_ent_sep;
    struct dlist_entry	ctx_ent_cq;
    struct dlist_entry	ctx_ent_ctr;
    struct tofu_cq *	ctx_send_cq; /* send completion queue */
    struct tofu_cq *	ctx_recv_cq; /* receive completion queue */
    struct tofu_cntr *	ctx_send_ctr; /* fi_write/fi_read and send operation */
    struct tofu_cntr *	ctx_recv_ctr; /* recv operation */
    int                 enabled;
    int                 index;
};

struct tofu_sep {
    struct fid_ep	sep_fid;
    struct tofu_domain *sep_dom;
    ofi_atomic32_t	sep_ref;
    uint32_t		sep_enb;        /* enabled */
    fastlock_t		sep_lck;
    struct dlist_entry	sep_ent;        /* link to other SEPs */
    struct dlist_entry	sep_htx;        /* head for ep tx ctxs */
    struct dlist_entry	sep_hrx;        /* head for ep rx ctxs */
    struct tofu_av *	sep_av_;
    struct tofu_ctx     sep_sctx[CONF_TOFU_CTXC];
    struct tofu_ctx     sep_rctx[CONF_TOFU_CTXC];
    int                 sep_vcqidx;
    utofu_vcq_hdl_t     sep_vcqh;       /* copy of sep_dom->vcqh[sep_vcqid] */
};

/*
 * circular queue for completion queue (tofu_comp_cirq)
 */
OFI_DECLARE_CIRQUE(struct fi_cq_tagged_entry, tofu_ccirq);
OFI_DECLARE_CIRQUE(struct fi_cq_err_entry, tofu_ccireq);

#define TOFU_CQ_SELECTIVE       1
struct tofu_cq {
    struct fid_cq	cq_fid;
    struct tofu_domain *cq_dom;
    ofi_atomic32_t	cq_ref;
    fastlock_t		cq_lck;
    struct dlist_entry	cq_htx;   /* head for ep tx ctxs */
    struct dlist_entry	cq_hrx;   /* head for ep rx ctxs */
    struct tofu_ccirq  *cq_ccq;   /* circular queue for fi_cq_tagged_entry */
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
    struct fid_av	av_fid;
    struct tofu_domain *av_dom;
    int			av_rxb;    /* rx_ctx_bits */
    ofi_atomic32_t	av_ref;
    fastlock_t		av_lck;
    /* Tofu Internal */
    struct tofu_av_tab {
	size_t		mct;
	size_t		nct;
	void *		tab;    /* 16B x entry */
    }			av_tab;
};

/* Tofu Internal */
struct ulib_sep_name {
    uint8_t  txyz[3];
    uint8_t  a : 1;
    uint8_t  b : 2;
    uint8_t  c : 1;
    uint8_t  p : 3;	/* component id */
    uint8_t  v : 1;	/* valid */
    uint32_t vpid;
    uint8_t  tniq[8];	/* (tni + tcq)[8] */
    /* struct ulib_sep_tniq tniq; */
};

struct tofu_mr {
    struct fid_mr	mr__fid;
    struct tofu_domain *mr__dom;
    ofi_atomic32_t	mr__ref;
    fastlock_t		mr__lck;
/*  struct dlist_entry	mr__ent; */
    struct fi_mr_attr	mr__att;
    uint64_t		mr__flg;
    struct iovec	mr__iov;
};


extern struct fi_provider		tofu_prov;
extern struct fi_ops_mr			tofu_mr__ops;
extern struct fi_ops_cm			tofu_sep_ops_cm;
extern struct fi_ops_cm			tofu_ctx_ops_cm;
extern struct fi_ops_msg		tofu_ctx_ops_msg;
extern struct fi_ops_tagged		tofu_ctx_ops_tag;

extern struct fi_ops_rma		tofu_ctx_ops_rma;
extern struct fi_ops_atomic		tofu_ctx_ops_atomic;

/* === function prototypes ============================================ */
extern int      tofu_impl_isep_open(struct tofu_sep *sep_priv);
extern int	tofu_init_prov_info(const struct fi_info *hints,
                                    struct fi_info **infos);
extern int	tofu_chck_prov_info(uint32_t api_version,
                                    const struct fi_info *hints,
                                    struct fi_info **infos);
extern int	tofu_fabric_open(struct fi_fabric_attr *attr,
                                 struct fid_fabric **fid,
                                 void *context);
extern int	tofu_domain_open(struct fid_fabric *fabric,
                                 struct fi_info *info,
                                 struct fid_domain **fid,
                                 void *context);
extern int	tofu_cq_open(struct fid_domain *fid_dom,
                             struct fi_cq_attr *attr,
                             struct fid_cq **fid_cq_,
                             void *context);
extern int	tofu_cntr_open(struct fid_domain *fid_dom,
                               struct fi_cntr_attr *attr,
                               struct fid_cntr **fid_ctr,
                               void *context);
extern int	tofu_av_open(struct fid_domain *fid_dom,
                             struct fi_av_attr *attr,
                             struct fid_av **fid_av_,
                             void *context);
extern int	tofu_sep_open(struct fid_domain *fid_dom,
                              struct fi_info *info,
                              struct fid_ep **fid_sep,
                              void *context);
extern int	tofu_ctx_tx_context(struct fid_ep *fid_sep,
                                    int index,
                                    struct fi_tx_attr *attr,
                                    struct fid_ep **fid_ctx_tx,
                                    void *context);
extern int	tofu_ctx_rx_context(struct fid_ep *fid_sep,
                                    int index,
                                    struct fi_rx_attr *attr,
                                    struct fid_ep **fid_ctx_rx,
                                    void *context);
extern int	tofu_ctx_ctrl(struct fid *fid, int command, void *arg);

extern int	tofu_chck_fab_attr(const struct fi_fabric_attr *prov_attr,
                                   const struct fi_fabric_attr *user_attr);
extern int	tofu_chck_dom_attr(const struct fi_domain_attr *prov_attr,
                                   const struct fi_info *user_info);
extern int	tofu_chck_av_attr(const struct fi_av_attr *user_attr);
extern int	tofu_chck_cq_attr(const struct fi_cq_attr *user_attr);
extern int	tofu_chck_ctx_tx_attr(const struct fi_tx_attr *prov_attr,
                                      const struct fi_tx_attr *user_attr,
                                      uint64_t user_info_mode);
extern int	tofu_chck_ctx_rx_attr(const struct fi_info *prov_info,
                                      const struct fi_rx_attr *user_attr,
                                      uint64_t user_info_mode);
extern int	tofu_chck_ep_attr(const struct fi_info *prov_info,
                                  const struct fi_info *user_info);
extern void     dbg_show_utof_vcqh(utofu_vcq_hdl_t vcqh);
#endif	/* _TOFU_IMPL_H */
