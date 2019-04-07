/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_ULIB_OFIF_H
#define _ULIB_OFIF_H

#include "utofu.h"

#include "ulib_conf.h"

#ifdef	CONF_ULIB_OFI
#include <ofi.h>
#include <ofi_list.h>
#include <ofi_mem.h>
#endif	/* CONF_ULIB_OFI */

#include <stddef.h>	    /* for size_t */
#include <stdint.h>	    /* for uint64_t */

/* ======================================================================== */
#ifdef	CONF_ULIB_OFI

#include "ulib_shea.h"

/*
 * buffer pool for unexpected entries; see also ofi_mem.h
 *   struct ulib_uexp_fs;
 *   void ulib_uexp_fs_init(struct ulib_uexp_fs *fs, size_t size);
 *   struct ulib_uexp_fs *ulib_uexp_fs_create(size_t size);
 *   void ulib_uexp_fs_free(struct ulib_uexp_fs *fs);
 *   int ulib_uexp_fs_index(struct ulib_uexp_fs *fs, struct ulib_shea_rinf *en);
 */
DECLARE_FREESTACK(struct ulib_shea_rinf, ulib_uexp_fs);
#endif	/* CONF_ULIB_OFI */
#ifdef	CONF_ULIB_OFI

/* ------------------------------------------------------------------------ */
#if	defined(CONF_ULIB_FICQ) && (CONF_ULIB_FICQ == CONF_ULIB_FICQ_CIRQ)
#include <ofi_rbuf.h>	    /* ring buffer */
#include <rdma/fi_eq.h>	    /* for struct fi_cq_tagged_entry */

/*
 * circular queue for FI completion queue; see also ofi_rbuf.h
 *   void ulib_ficq_cirq_init(struct ulib_ficq_cirq *cirq, size_t size);
 *   struct ulib_ficq_cirq *ulib_ficq_cirq_create(size_t size);
 *   void ulib_ficq_cirq_free(struct ulib_ficq_cirq *cirq);
 */
OFI_DECLARE_CIRQUE(struct fi_cq_tagged_entry, ulib_ficq_cirq);
#elif	defined(CONF_ULIB_FICQ) && (CONF_ULIB_FICQ == CONF_ULIB_FICQ_POOL)
#include <ofi_mem.h>	    /* linked list */
#include <ofi.h>	    /* for container_of () */
#include <rdma/fi_eq.h>	    /* for struct fi_cq_tagged_entry */

struct ulib_ficq_en {
    struct dlist_entry list;
    union {
	struct fi_cq_tagged_entry tagd;
	/* struct fi_cq_err_entry cerr; */ /* YYY */
    } comp;
};

/*
 * buffer pool for FI completion queue; see also ofi_mem.h
 *   struct ulib_ficq_fs;
 *   void ulib_ficq_fs_init(struct ulib_ficq_fs *fs, size_t size);
 *   struct ulib_ficq_fs *ulib_ficq_fs_create(size_t size);
 *   void ulib_ficq_fs_free(struct ulib_ficq_fs *fs);
 *   int ulib_ficq_fs_index(struct ulib_ficq_fs *fs, struct ulib_ficq_en *en);
 */
DECLARE_FREESTACK(struct ulib_ficq_en, ulib_ficq_fs);
#elif	defined(CONF_ULIB_FICQ)
#error "invalid CONF_ULIB_FICQ"
#endif	/* defined(CONF_ULIB_FICQ) */

#endif	/* CONF_ULIB_OFI */
#ifdef	CONF_ULIB_OFI

/* ------------------------------------------------------------------------ */
#include <ofi_mem.h>	    /* linked list */
#include <ofi.h>	    /* for container_of () */

#include "ulib_shea.h"	    /* for struct ulib_shea_data */

/*
 * buffer pool for user data (send) entries; see also ofi_mem.h
 *   struct ulib_udat_fs;
 *   void ulib_udat_fs_init(struct ulib_udat_fs *fs, size_t size);
 *   struct ulib_udat_fs *ulib_udat_fs_create(size_t size);
 *   void ulib_udat_fs_free(struct ulib_udat_fs *fs);
 *   int ulib_udat_fs_index(struct ulib_udat_fs *fs, struct ulib_shea_data *en);
 */
DECLARE_FREESTACK(struct ulib_shea_data, ulib_udat_fs);
#endif	/* CONF_ULIB_OFI */

/* ======================================================================== */

struct ulib_idom;
struct ulib_imr_;
struct ulib_isep;
struct ulib_icep;
struct ulib_ioav;
#ifndef	notdef_icep_toqc
struct ulib_toqc;
#endif	/* notdef_icep_toqc */

struct ulib_idom {
    struct ulib_imr_ *head_imr_;
    /* struct ulib_isep *head_isep */
};

/* fi completion queue */
struct ulib_icqu {
    struct ulib_idom *idom;
    void *cque;
#ifdef	CONF_ULIB_OFI
#if	defined(CONF_ULIB_FICQ) && (CONF_ULIB_FICQ == CONF_ULIB_FICQ_POOL)
    struct dlist_entry cqu_head;
#endif	/* defined(CONF_ULIB_FICQ) */
#endif	/* CONF_ULIB_OFI */
};

struct ulib_imr_ {
    void *addr;
    size_t size;
    unsigned int stag;
    struct ulib_imr_ *next;
    struct ulib_icep *ceps[8];
    utofu_stadd_t stas[8];
    uint64_t toas[8];
};

struct ulib_isep {
    /* struct ulib_isep *next; */
    utofu_tni_id_t tnis[8];
    size_t ntni;
    struct ulib_icep *head;
};
#ifdef	CONF_ULIB_OFI

struct ulib_uexp_fs;
struct ulib_expd_fs;
struct ulib_udat_fs;
#endif	/* CONF_ULIB_OFI */

struct ulib_icep {
    int index;
    int enabled;
    struct ulib_icep *next;
    struct ulib_isep *isep;
    struct ulib_ioav *ioav; /* ofi address vector */
    utofu_vcq_hdl_t vcqh;
    /* fastlock_t icep_lck; */
#ifndef	notdef_icep_toqc
    struct ulib_toqc *toqc;
    struct ulib_shea_cbuf cbuf;
#endif	/* notdef_icep_toqc */
    struct ulib_icqu *icep_scq; /* send cq */
    struct ulib_icqu *icep_rcq; /* recv cq */
#ifdef	CONF_ULIB_OFI
    struct ulib_uexp_fs *uexp_fs;
    struct dlist_entry uexp_list_trcv;
    struct dlist_entry uexp_list_mrcv;
    struct ulib_expd_fs *expd_fs;
    struct dlist_entry expd_list_trcv; /* fi_msg_tagged */
    struct dlist_entry expd_list_mrcv; /* fi_msg */
    struct ulib_udat_fs *udat_fs;
#endif	/* CONF_ULIB_OFI */
};

extern int  ulib_isep_open_tnis_info(struct ulib_isep *isep);
extern int  ulib_icep_ctrl_enab(struct ulib_icep *icep);
extern int  ulib_icep_close(struct ulib_icep *icep);
extern int  ulib_imr__bind_icep(struct ulib_imr_ *imr_, struct ulib_icep *icep);
extern int  ulib_imr__close(struct ulib_imr_ *imr_);

static inline void ulib_icep_lock(struct ulib_icep *icep)
{
    /* fastlock_acquire( &icep->icep_lck ); */
    return ;
}

static inline void ulib_icep_unlock(struct ulib_icep *icep)
{
    /* fastlock_release( &icep->icep_lck ); */
    return ;
}

/* ======================================================================== */

static inline void ulib_ofif_icep_init(
    struct ulib_icep *icep
)
{
    icep->index = -1; /* III */
    icep->enabled = 0;
    icep->next = 0;
    icep->isep = 0; /* III */
    icep->vcqh = 0;
#ifdef	CONF_ULIB_OFI
    icep->uexp_fs = 0;
    dlist_init(&icep->uexp_list_trcv); /* unexpected queue for tagged msg. */
    dlist_init(&icep->uexp_list_mrcv); /* unexpected queue for msg. */
    icep->expd_fs = 0;
    dlist_init(&icep->expd_list_trcv); /* expeced queue */
    dlist_init(&icep->expd_list_mrcv); /* expeced queue */
    icep->udat_fs = 0;
#endif	/* CONF_ULIB_OFI */
    return ;
}

/* ======================================================================== */
#ifdef	CONF_ULIB_OFI

#include <rdma/fi_tagged.h>
#else	/* CONF_ULIB_OFI */
struct fi_msg_tagged;
#endif	/* CONF_ULIB_OFI */

#include <assert.h>	    /* for assert() */
#include <sys/uio.h>	    /* for struct iovec */

struct ulib_shea_trcv {
#ifdef	CONF_ULIB_OFI
    struct dlist_entry entry;
    struct fi_msg_tagged tmsg;
    uint64_t flgs;
#endif	/* CONF_ULIB_OFI */
    uint32_t mblk;
    uint32_t nblk;
    uint64_t rtag;
    uint32_t wlen;
    uint32_t olen;
    uint32_t niov;
    struct iovec iovs[2];
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tims[16];
#endif	/* CONF_ULIB_PERF_SHEA */
};

/*
 * buffer pool for expected entries; see also ofi_mem.h
 *   struct ulib_expd_fs;
 *   void ulib_expd_fs_init(struct ulib_expd_fs *fs, size_t size);
 *   struct ulib_expd_fs *ulib_expd_fs_create(size_t size);
 *   void ulib_expd_fs_free(struct ulib_expd_fs *fs);
 *   int ulib_expd_fs_index(struct ulib_expd_fs *fs, struct ulib_shea_trcv *en);
 */
DECLARE_FREESTACK(struct ulib_shea_trcv, ulib_expd_fs);


static inline void ulib_shea_trcv_init(
    struct ulib_shea_trcv *trcv,
    const struct fi_msg_tagged *tmsg,
    uint64_t flags
)
{
    trcv->mblk = 0;
    trcv->nblk = 0;
    trcv->rtag = 0;
    trcv->wlen = 0;
    trcv->olen = 0;
    /* trcv->flag = 0; */ /* YYY */
#ifdef	CONF_ULIB_OFI
#ifndef	NDEBUG
    dlist_init(&trcv->entry);
#endif	/* NDEBUG */
    trcv->flgs = flags; /* | FI_TAGGED */
    trcv->tmsg = tmsg[0]; /* structure copy */
    /* (sizeof (trcv->iovs) / sizeof (trcv->iovs[0]) */
    assert(tmsg->iov_count <= 2); /* XXX */
    {
	size_t iv, nv = tmsg->iov_count;
	for (iv = 0; iv < nv; iv++) {
	    trcv->iovs[iv].iov_base = tmsg->msg_iov[iv].iov_base;
	    trcv->iovs[iv].iov_len  = tmsg->msg_iov[iv].iov_len;
	}
	trcv->niov = nv;
    }
#endif	/* CONF_ULIB_OFI */
#ifdef	CONF_ULIB_PERF_SHEA
    {
	size_t it, nt = sizeof (trcv->tims) / sizeof (trcv->tims[0]);
	for (it = 0; it < nt; it++) {
	    trcv->tims[it] = 0ULL;
	}
    }
#endif	/* CONF_ULIB_PERF_SHEA */
    return ;
}
#ifdef	notdef_recv_post

static inline void ulib_shea_trcv_link(
    struct ulib_icep *icep,
    struct ulib_shea_trcv *trcv
)
{
    assert(trcv != 0);
    assert(icep != 0);
#ifdef	CONF_ULIB_OFI
#ifndef	NDEBUG
    assert(dlist_empty(&trcv->entry));
#endif	/* NDEBUG */
    dlist_insert_tail(&trcv->entry, &icep->expd_list_trcv); /* YYY FI_TAGGED */
    /* dlist_insert_tail(&trcv->entry, &icep->expd_list_mrcv); */
#endif	/* CONF_ULIB_OFI */
    return ;
}
#endif	/* notdef_recv_post */

extern int  ulib_icep_recv_call_back(
		void *farg,
		int uc,
		const void *vctx
	    );
extern int  ulib_icep_shea_recv_post(
		struct ulib_icep *icep,
		const struct fi_msg_tagged *tmsg,
		uint64_t flags
	    );
extern int  ulib_icep_shea_recv_prog(struct ulib_icep *icep);
extern int  ulib_icep_shea_send_post(
		struct ulib_icep *icep,
		const struct fi_msg_tagged *tmsg,
		uint64_t flags,
		void **vpp_send_hndl
	    );
extern int  ulib_icep_shea_send_prog(
		struct ulib_icep *icep,
		void **vpp_send_hndl,
		uint64_t tims[16]
	    );
extern int  ulib_icqu_read(
		struct ulib_icqu *icqu,
		void *buf,
		size_t mcnt,
		size_t *ncnt
	    );
#ifdef	CONF_ULIB_OFI

/* ------------------------------------------------------------------------ */
#if	defined(CONF_ULIB_FICQ) && (CONF_ULIB_FICQ == CONF_ULIB_FICQ_CIRQ)

static inline void *ulib_icqu_cque_pop(struct ulib_icqu *icqu)
{
    void *cent; /* completion entry */
    struct ulib_ficq_cirq *cirq;

    assert(icqu != 0);
    cirq = icqu->cque;
    assert(cirq != 0);
    if (ofi_cirque_isfull(cirq)) {
	return 0;
    }
    /* get an entry pointed by w.p. */
    cent = ofi_cirque_tail(cirq);

    return cent;
}

static inline void *ulib_icqu_cent_comp_tagd(void *cent)
{
    assert(cent != 0);
    return cent;
}

static inline void ulib_icqu_cque_enq(struct ulib_icqu *icqu, void *cent)
{
    struct ulib_ficq_cirq *cirq;

    assert(icqu != 0);
    cirq = icqu->cque;
    assert(cirq != 0);
    assert(cent == ofi_cirque_tail(cirq));
    /* advance w.p. by one  */
    ofi_cirque_commit(cirq);

    return ;
}

static inline void *ulib_icqu_cque_peek(struct ulib_icqu *icqu)
{
    void *cent; /* completion entry */
    struct ulib_ficq_cirq *cirq;

    assert(icqu != 0);
    cirq = icqu->cque;
    assert(cirq != 0);

    if (ofi_cirque_isempty(cirq)) {
	return 0;
    }
    /* get an entry pointed by r.p. */
    cent = ofi_cirque_head(cirq);

    return cent;
}

static inline void ulib_icqu_cque_deq_push(struct ulib_icqu *icqu, void *cent)
{
    struct ulib_ficq_cirq *cirq;

    assert(icqu != 0);
    cirq = icqu->cque;
    assert(cirq != 0);
    assert(cent == ofi_cirque_head(cirq));
    /* advance r.p. by one  */
    ofi_cirque_discard(cirq);

    return ;
}
#elif	defined(CONF_ULIB_FICQ) && (CONF_ULIB_FICQ == CONF_ULIB_FICQ_POOL)

static inline void ulib_icqu_cque_prnt(
    struct ulib_icqu *icqu,
    void *cent,
    const char *title
)
{
    struct ulib_ficq_fs *fs = icqu->cque;
    struct dlist_entry *head = &icqu->cqu_head;
    void *vent;
    int nh, ph;

    /* head */
    if (head->next == head) { nh = -3; }
    else {
	vent = container_of(head->next, struct ulib_ficq_en, list);
	nh = ulib_ficq_fs_index(fs, vent);
    }
    if (head->prev == head) { ph = -3; }
    else {
	vent = container_of(head->prev, struct ulib_ficq_en, list);
	ph = ulib_ficq_fs_index(fs, vent);
    }

    /* entry */
    if (cent != 0) {
	struct ulib_ficq_en *cqen = cent;
	int ci, ni, pi;

	ci = ulib_ficq_fs_index(fs, cent);

	if (cqen->list.next == head) { ni = -3; }
	else {
	    vent = container_of(cqen->list.next, struct ulib_ficq_en, list);
	    ni = ulib_ficq_fs_index(fs, vent);
	}
	if (cqen->list.prev == head) { pi = -3; }
	else {
	    vent = container_of(cqen->list.prev, struct ulib_ficq_en, list);
	    pi = ulib_ficq_fs_index(fs, vent);
	}
	printf("\t%s cent %3d (next %3d prev %3d) head (next %3d prev %3d)\n",
	    title, ci, ni, pi, nh, ph);
    }
    else {
	printf("\t%s head (next %3d prev %3d)\n",
	    title, nh, ph);
    }

    return ;
}

static inline void *ulib_icqu_cque_pop(struct ulib_icqu *icqu)
{
    void *cent; /* completion entry */
    struct ulib_ficq_fs *fs;

    assert(icqu != 0);
    fs = icqu->cque;
    assert(fs != 0);
    if (freestack_isempty(fs)) {
	return 0;
    }
    cent = freestack_pop(fs);

    return cent;
}

static inline void *ulib_icqu_cent_comp_tagd(void *cent)
{
    assert(cent != 0);
    return &((struct ulib_ficq_en *)cent)->comp.tagd;
}

static inline void ulib_icqu_cque_enq(struct ulib_icqu *icqu, void *cent)
{
    struct ulib_ficq_en *cqen = cent;

    assert(icqu != 0);
    assert(cent != 0);
    dlist_insert_tail(&cqen->list, &icqu->cqu_head);
    if (0) { ulib_icqu_cque_prnt(icqu, cent, "enq"); }

    return ;
}

static inline void *ulib_icqu_cque_peek(struct ulib_icqu *icqu)
{
    void *cent; /* completion entry */
    struct dlist_entry *dent;

    assert(icqu != 0);
    if (dlist_empty(&icqu->cqu_head)) {
	return 0;
    }

    dent = icqu->cqu_head.next;
    cent = container_of(dent, struct ulib_ficq_en, list);

    return cent;
}

static inline void ulib_icqu_cque_deq_push(struct ulib_icqu *icqu, void *cent)
{
    struct ulib_ficq_en *cqen = cent;
    struct ulib_ficq_fs *fs;

    assert(icqu != 0);
    fs = icqu->cque;
    assert(fs != 0);

    if (0) { ulib_icqu_cque_prnt(icqu, cent, "deq"); }
    assert(&cqen->list == icqu->cqu_head.next);
    assert(&cqen->list != &icqu->cqu_head);
    dlist_remove(&cqen->list);
#ifndef	notdef_shea_fix7
    if (
	(icqu->cqu_head.next == &icqu->cqu_head)
	&& (icqu->cqu_head.prev != &icqu->cqu_head)
    ) {
	ulib_icqu_cque_prnt(icqu, 0, "err");
    }
#endif	/* notdef_shea_fix7 */
    if (0) { ulib_icqu_cque_prnt(icqu, cent, "rem"); }

    freestack_push(fs, cent);

    return ;
}
#endif	/* defined(CONF_ULIB_FICQ) */
#endif	/* CONF_ULIB_OFI */

#endif	/* _ULIB_OFIF_H */
