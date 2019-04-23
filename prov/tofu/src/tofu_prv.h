/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_PRV_H
#define _TOFU_PRV_H

/* private structure */

#include "tofu_conf.h"

#include <stdint.h>	    /* for uint64_t */
#include <ofi_lock.h>	    /* for fastlock_t */
#include <ofi_atom.h>	    /* for ofi_atomic32_t */
#include <ofi_list.h>	    /* for struct dlist_entry */
#include <sys/uio.h>	    /* for struct iovec */
#include <rdma/fi_domain.h> /* for struct fid_mr */
#include <rdma/fi_endpoint.h>	/* for struct fid_ep */

/* -------------------------------------------------------------------- */

struct tofu_domain;

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

/* -------------------------------------------------------------------- */

struct tofu_av;

struct tofu_sep {
    struct fid_ep	sep_fid;
    struct tofu_domain *sep_dom;
    ofi_atomic32_t	sep_ref;
    uint32_t		sep_enb;    /* enabled */
    fastlock_t		sep_lck;
/*  struct dlist_entry	sep_ent; */
    struct dlist_entry	sep_htx;    /* head for ep tx ctxs */
    struct dlist_entry	sep_hrx;    /* head for ep rx ctxs */
    struct tofu_av *	sep_av_;
};

/* -------------------------------------------------------------------- */

/* struct tofu_sep; */
struct tofu_cq;
struct tofu_cntr;

struct tofu_cep {
    struct fid_ep	cep_fid;
    struct tofu_sep *	cep_sep;
    ofi_atomic32_t	cep_ref;
    uint32_t		cep_enb;    /* enabled */
    int			cep_idx;
    fastlock_t		cep_lck;
/*  struct dlist_entry	cep_ent; */
    uint64_t		cep_xop_flg;
    struct tofu_cep *	cep_trx;      /* Name should be changed ? cep_peer ?*/
                                      /* cep_trx does not mean point to Sender side, right ? */
    struct dlist_entry	cep_ent_sep;
    struct dlist_entry	cep_ent_cq;
    struct dlist_entry	cep_ent_ctr;
    struct tofu_cq *	cep_send_cq; /* send completion queue */
    struct tofu_cq *	cep_recv_cq; /* receive completion queue */
    struct tofu_cntr *	cep_wop_ctr; /* write rma operation */
    struct tofu_cntr *	cep_rop_ctr; /* read  rma operation */
    /*
     * see struct ulib_icep for more internal structures in ulib_ofif.h
     */
};

#endif	/* _TOFU_PRV_H */
