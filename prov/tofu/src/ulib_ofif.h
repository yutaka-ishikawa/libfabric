/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_ULIB_OFIF_H
#define _ULIB_OFIF_H

#include "utofu.h"

#include "ulib_conf.h"

#include <ofi.h>
#include <ofi_list.h>
#include <ofi_mem.h>
#include <stddef.h>	    /* for size_t */
#include <stdint.h>	    /* for uint64_t */

/* ======================================================================== */
#include "ulib_shea.h"

/*
 * buffer pool for unexpected entries; see also ofi_mem.h
 *   struct ulib_uexp_fs;
 *   void ulib_uexp_fs_init(struct ulib_uexp_fs *fs, size_t size);
 *   struct ulib_uexp_fs *ulib_uexp_fs_create(size_t size);
 *   void ulib_uexp_fs_free(struct ulib_uexp_fs *fs);
 *   int ulib_uexp_fs_index(struct ulib_uexp_fs *fs, struct ulib_shea_rinf *en);
 */
DECLARE_FREESTACK(struct ulib_shea_uexp, ulib_uexp_fs);
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

struct ulib_uexp_fs;
struct ulib_expd_fs;
struct ulib_udat_fs;

struct ulib_icep {
    int index;
    int enabled;
    struct ulib_icep *next;
    struct ulib_isep *isep;
    struct ulib_ioav *ioav; /* ofi address vector */
    utofu_vcq_hdl_t             vcqh;
    fastlock_t                  icep_lck;
    struct ulib_toqc            *toqc;
    struct ulib_shea_cbuf       cbuf;
    struct ulib_icqu            *icep_scq; /* send cq */
    struct ulib_icqu            *icep_rcq; /* recv cq */
    /* unexpected queue */
    struct ulib_uexp_fs         *uexp_fs;
    struct dlist_entry          uexp_list_trcv; /* fi_msg_tagged */
    struct dlist_entry          uexp_list_mrcv; /* fi_msg */
    /* expected queue */
    struct ulib_expd_fs         *expd_fs;
    struct dlist_entry          expd_list_trcv; /* fi_msg_tagged */
    struct dlist_entry          expd_list_mrcv; /* fi_msg */
    struct ulib_udat_fs         *udat_fs;
    union ulib_tofa_u           tofa;           /* TOFu network Address */  
};

extern int  ulib_isep_open_tnis_info(struct ulib_isep *isep);
extern int  ulib_icep_ctrl_enab(struct ulib_icep *icep);
extern void ulib_ofif_icep_init(void *ptr, size_t off);
extern int  ulib_icep_close(void *ptr, size_t off);
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


/* ======================================================================== */
#ifdef	CONF_ULIB_OFI
#include <rdma/fi_tagged.h>
#else	/* CONF_ULIB_OFI */
struct fi_msg_tagged;
#endif	/* CONF_ULIB_OFI */

#include <assert.h>	    /* for assert() */
#include <sys/uio.h>	    /* for struct iovec */

struct ulib_shea_expd {
    struct dlist_entry entry;
    struct fi_msg_tagged tmsg;
    uint64_t flgs;
    uint32_t mblk;
    uint32_t nblk;
    uint64_t rtag;
    uint32_t wlen;
    uint32_t olen;
    uint32_t niov;
    struct iovec iovs[2];
    uint64_t tims[16];
};

/*
 * buffer pool for expected entries; see also ofi_mem.h
 *   struct ulib_expd_fs;
 *   void ulib_expd_fs_init(struct ulib_expd_fs *fs, size_t size);
 *   struct ulib_expd_fs *ulib_expd_fs_create(size_t size);
 *   void ulib_expd_fs_free(struct ulib_expd_fs *fs);
 *   int ulib_expd_fs_index(struct ulib_expd_fs *fs, struct ulib_shea_trcv *en);
 */
DECLARE_FREESTACK(struct ulib_shea_expd, ulib_expd_fs);


static inline void 
ulib_shea_expd_init(struct ulib_shea_expd *expd,
                    const struct fi_msg_tagged *tmsg,
                    uint64_t flags)
{
    expd->mblk = 0;
    expd->nblk = 0;
    expd->rtag = 0;
    expd->wlen = 0;
    expd->olen = 0;
    /* expd->flag = 0; */ /* YYY */
    dlist_init(&expd->entry);
    expd->flgs = flags; /* | FI_TAGGED */
    expd->tmsg = tmsg[0]; /* structure copy */
    /* (sizeof (expd->iovs) / sizeof (expd->iovs[0]) */
    assert(tmsg->iov_count <= 2); /* XXX */
    {
	size_t iv, nv = tmsg->iov_count;
	for (iv = 0; iv < nv; iv++) {
	    expd->iovs[iv].iov_base = tmsg->msg_iov[iv].iov_base;
	    expd->iovs[iv].iov_len  = tmsg->msg_iov[iv].iov_len;
	}
	expd->niov = nv;
    }
    {
	size_t it, nt = sizeof (expd->tims) / sizeof (expd->tims[0]);
	for (it = 0; it < nt; it++) {
	    expd->tims[it] = 0ULL;
	}
    }
    return ;
}
#ifdef	notdef_recv_post

static inline void ulib_shea_expd_link(
    struct ulib_icep *icep,
    struct ulib_shea_expd *expd
)
{
    assert(expd != 0);
    assert(icep != 0);
#ifdef	CONF_ULIB_OFI
#ifndef	NDEBUG
    assert(dlist_empty(&expd->entry));
#endif	/* NDEBUG */
    dlist_insert_tail(&expd->entry, &icep->expd_list_expd); /* YYY FI_TAGGED */
    /* dlist_insert_tail(&expd->entry, &icep->expd_list_mrcv); */
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

static inline int
ulib_match_expd(struct dlist_entry *item, const void *farg)
{
    int ret;
    const struct ulib_shea_uexp *uexp = farg;
    struct ulib_shea_expd *expd;

    expd = container_of(item, struct ulib_shea_expd, entry);
    ret = ((expd->tmsg.tag & ~expd->tmsg.ignore)
           == (uexp->utag & ~expd->tmsg.ignore));
    return ret;
}

static inline struct ulib_shea_expd*
ulib_icep_find_expd(struct ulib_icep *icep,
                    const struct ulib_shea_uexp *uexp)
{
    struct ulib_shea_expd *expd = 0;
    struct dlist_entry *head;
    struct dlist_entry *match;

    head = (uexp->flag & ULIB_SHEA_UEXP_FLAG_TFLG) ?
	&icep->expd_list_trcv : &icep->expd_list_mrcv;
    match = dlist_remove_first_match(head, ulib_match_expd, uexp);
    if (match == 0) {
	goto bad; /* XXX - is not an error */
    }
    expd = container_of(match, struct ulib_shea_expd, entry);
bad:
    return expd;
}

static inline int
ulib_match_uexp(struct dlist_entry *item, const void *farg)
{
    int ret;
    const struct ulib_shea_expd *expd = farg;
    struct ulib_shea_uexp *uexp;

    uexp = container_of(item, struct ulib_shea_uexp, entry);
    ret = ((uexp->utag & ~expd->tmsg.ignore)
           == (expd->tmsg.tag & ~expd->tmsg.ignore));
    return ret;
}

static inline struct ulib_shea_uexp*
ulib_icep_find_uexp(struct ulib_icep *icep,
                    const struct ulib_shea_expd *expd)
{
    struct ulib_shea_uexp *uexp = 0;
    struct dlist_entry *head;
    struct dlist_entry *match;

    head = (uexp->flag & ULIB_SHEA_UEXP_FLAG_TFLG) ?
	&icep->uexp_list_trcv : &icep->uexp_list_mrcv;
    match = dlist_remove_first_match(head, ulib_match_uexp, expd);
    if (match == 0) {
	goto bad; /* XXX - is not an error */
    }
    uexp = container_of(match, struct ulib_shea_uexp, entry);
bad:
    return uexp;
}

/* Should be check if the following functions may be externally used */
static inline void
ulib_icep_link_expd(struct ulib_icep *icep,
                    struct ulib_shea_expd *expd)
{
    struct dlist_entry *head;
    head = (expd->flgs & FI_TAGGED) != 0 ?
	&icep->expd_list_trcv : &icep->expd_list_mrcv;
     dlist_insert_tail(&expd->entry, head);
    return ;
}

static inline struct ulib_shea_data 
*ulib_icep_shea_data_qget(struct ulib_icep *icep)
{
    struct ulib_shea_data *data = 0;

    if (freestack_isempty(icep->udat_fs)) {
	goto bad;
    }
    data = freestack_pop(icep->udat_fs);
    assert(data != 0);
    /* initialize vcqh and stad_data */
    {
	struct ulib_toqc_cash *cash_real = &data->real;
	const uint64_t fi_a = -1ULL /* YYY FI_ADDR_NOTAVAIL */ ;

	ulib_toqc_cash_init(cash_real, fi_a);
    }
bad:
    return data;
}

static inline void ulib_icep_shea_data_qput(
    struct ulib_icep *icep,
    struct ulib_shea_data *data
)
{
    assert(data != 0);
    /* unregister stad_data */
    {
	struct ulib_toqc_cash *cash_real = &data->real;
	struct ulib_utof_cash *lcsh;

	lcsh = &cash_real->addr[1];
	if (lcsh->stad_data != (utofu_stadd_t)-1ULL) {
	    const unsigned long int flag = 0
					/* | UTOFU_REG_MEM_FLAG_READ_ONLY */
					;
	    int uc;

	    uc = utofu_dereg_mem(lcsh->vcqh, lcsh->stad_data, flag);
	    if (uc != UTOFU_SUCCESS) {
                fprintf(stderr, "YI******* %s(%"PRIxPTR",%"PRIx64",) = %d\n",
                        "utofu_dereg_mem",
                        lcsh->vcqh, lcsh->stad_data, uc);
                fflush(stdout);
		/* YYY abort */
	    }
	    assert(uc == UTOFU_SUCCESS);
	    lcsh->stad_data = -1ULL;
	}
    }
    freestack_push(icep->udat_fs, data);

    return ;
}

#endif	/* _ULIB_OFIF_H */
