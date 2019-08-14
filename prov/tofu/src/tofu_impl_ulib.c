/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include <stdio.h>			/* for snprintf() */
#include "tofu_debug.h"
#include "tofu_conf.h"			/* for CONF_TOFU_+ */
#include <rdma/providers/fi_prov.h>	/* for struct fi_provider */
#include <rdma/providers/fi_log.h>	/* for FI_INFO() */
extern struct fi_provider		tofu_prov;
#include "ulib_shea.h"
#include "ulib_conv.h"
#include "tofu_impl.h"
#include "tofu_impl_ulib.h"
#include "ulib_ofif.h"
#include "ulib_notify.h"

/* the following function was originally static inline function in ulib_ofif.c */
extern int
ulib_icep_recv_frag(struct ulib_shea_expd *trcv,
                    const struct ulib_shea_uexp *rinf, void **sndctxt);

/* The following functions are defined and used in this file */
extern int tofu_imp_ulib_cash_find(struct ulib_icep *icep,
                                   uint64_t tank,
                                   struct ulib_toqc_cash **pp_cash_tmpl);
extern void tofu_imp_ulib_cash_free(struct ulib_icep *icep,
                                    struct ulib_toqc_cash *cash_tmpl);
/* end of definitions */

/* ------------------------------------------------------------------------ */
/* STATIC definitions */
static inline void tofu_imp_ulib_icep_recv_rbuf_base(
    struct ulib_icep *icep,
    const struct ulib_shea_uexp *uexp,
    struct iovec iovs[2]
)
{
    uint8_t *real_base;
    const struct ulib_shea_rbuf *rbuf = &uexp->rbuf;
    uint32_t iiov;

    /* assert(icep->cbuf.dptr != 0); */ /* YYY cbuf */
    real_base = 0 /* icep->cbuf.dptr */; /* YYY cbuf */

    /* iov_base : offs => base + offs */
    for (iiov = 0; iiov < rbuf->niov; iiov++) {
        uintptr_t offs = (uintptr_t)rbuf->iovs[iiov].iov_base; /* XXX */
        iovs[iiov].iov_base = real_base + offs;
        iovs[iiov].iov_len  = rbuf->iovs[iiov].iov_len;
    }

    return ;
}

static inline size_t tofu_imp_ulib_copy_iovs(
    struct iovec *iov_dst,
    size_t iov_dct,
    size_t iov_dof,
    const struct iovec *iov_src,
    size_t iov_sct
)
{
    size_t idx, wlen, iov_off = iov_dof;

    for (idx = 0; idx < iov_sct; idx++) {
        /* see also ofi_copy_to_iov() in include/ofi_iov.h */
        wlen = ofi_copy_to_iov(iov_dst, iov_dct, iov_off,
                        iov_src[idx].iov_base, iov_src[idx].iov_len);
        assert(wlen != -1UL);
        iov_off += wlen;
    }
    return (iov_off - iov_dof);
}


static inline int
tofu_imp_ulib_icep_recv_rbuf_copy(
    struct ulib_icep *icep,
    struct ulib_shea_rbuf *rbuf_dst,
    const struct ulib_shea_uexp *uexp
)
{
    int fc = FI_SUCCESS;
    uint32_t leng = uexp->rbuf.leng;

    assert(rbuf_dst->leng == leng); /* copied */

    if (leng > 0) {
	const struct ulib_shea_rbuf *rbuf_src = &uexp->rbuf;
	size_t wlen;
	struct iovec iovs[2];

	rbuf_dst->iovs[0].iov_base = malloc(leng);
	if (rbuf_dst->iovs[0].iov_base == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
        rbuf_dst->alloced = 1;        /* memory is allocated here */
	rbuf_dst->iovs[0].iov_len = leng;
	rbuf_dst->niov = 1;

	assert(sizeof (iovs) == sizeof (rbuf_src->iovs));
	tofu_imp_ulib_icep_recv_rbuf_base(icep, uexp, iovs);

	wlen = tofu_imp_ulib_copy_iovs(
		rbuf_dst->iovs, /* dst */
		rbuf_dst->niov, /* dst */
		0, /* off */
		iovs, /* src */
		rbuf_src->niov /* src */
		);
	assert (wlen == leng);
	if  (wlen != leng) { /* YYY abort */ }
    }

bad:
    return fc;
}

static inline void
tofu_imp_ulib_expd_recv(struct ulib_shea_expd *expd,
                        const struct ulib_shea_uexp *uexp)
{
    if ((uexp->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0) {
	assert(expd->nblk == 0);
	assert(expd->mblk == 0);
	expd->mblk  = uexp->mblk;
	expd->rtag  = uexp->utag;
    } else {
	assert(expd->nblk != 0);
	assert(expd->mblk != 0);
	assert(expd->rtag == uexp->utag);
    }
    assert(expd->nblk == uexp->boff);
#ifndef	NDEBUG
    if ((uexp->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0) {
	assert((uexp->nblk + uexp->boff) <= uexp->mblk);
    } else {
	assert((uexp->nblk + uexp->boff) <= expd->mblk);
    }
#endif	/* NDEBUG */
    /* copy to the user buffer */
    {
	size_t wlen;

	wlen = tofu_imp_ulib_copy_iovs(
		expd->iovs,
		expd->niov,
		expd->wlen,
		uexp->rbuf.iovs,
		uexp->rbuf.niov
		);
	expd->wlen += wlen;
	assert(wlen <= uexp->rbuf.leng);
	expd->olen += (uexp->rbuf.leng - wlen);
    }
    /* update nblk */
    if (uexp->rbuf.leng == 0) {
	assert((uexp->flag & ULIB_SHEA_UEXP_FLAG_ZFLG) != 0);
	assert((uexp->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0);
	assert(uexp->mblk == 1);
	assert(uexp->boff == 0);
	assert(uexp->nblk == 0);
	expd->nblk += 1 /* uexp->nblk */;
    } else {
	expd->nblk += uexp->nblk;
    }

    return ;
}

static inline int
tofu_imp_ulib_expd_cond_comp(struct ulib_shea_expd *expd)
{
    int rc = 0;
    rc = ((expd->mblk != 0) && (expd->nblk >= expd->mblk));
    if (rc != 0) {
	assert(expd->nblk == expd->mblk);
    }
    return rc;
}

static inline void
tofu_imp_ulib_icep_link_expd_head(struct ulib_icep *icep,
                                  struct ulib_shea_expd *expd)
{
    struct dlist_entry *head;
    head = (expd->flgs & FI_TAGGED) ?
	&icep->expd_list_trcv : &icep->expd_list_mrcv;
    dlist_insert_head(&expd->entry, head);
    return;
}

static inline void
ulib_cast_epnt_to_tank(const struct ulib_epnt_info *einf,
                       struct ulib_tank *tank)
{
    assert(sizeof (tank[0]) == sizeof (uint64_t));
    assert(einf->xyz[0] < (1UL << ULIB_TANK_BITS_TUX));
    tank->tux = einf->xyz[0];
    assert(einf->xyz[1] < (1UL << ULIB_TANK_BITS_TUY));
    tank->tuy = einf->xyz[1];
    assert(einf->xyz[2] < (1UL << ULIB_TANK_BITS_TUZ));
    tank->tuz = einf->xyz[2];

    assert(einf->xyz[3] < (1UL << ULIB_TANK_BITS_TUA));
    tank->tua = einf->xyz[3];
    assert(einf->xyz[4] < (1UL << ULIB_TANK_BITS_TUB));
    tank->tub = einf->xyz[4];
    assert(einf->xyz[5] < (1UL << ULIB_TANK_BITS_TUC));
    tank->tuc = einf->xyz[5];

    assert(einf->tni[0] < (1UL << ULIB_TANK_BITS_TNI));
    tank->tni = einf->tni[0];
    assert(einf->tcq[0] < (1UL << ULIB_TANK_BITS_TCQ));
    tank->tcq = einf->tcq[0];

    assert(einf->cid[0] < (1UL << ULIB_TANK_BITS_CID));
    tank->cid = einf->cid[0];
    tank->vld = 1;
    tank->pid = einf->pid[0];
    return ;
}

static inline void
ulib_cast_epnt_to_tank_ui64(const struct ulib_epnt_info *einf,
                            uint64_t *tank_ui64)
{
    union ulib_tofa_u tank_u = { .ui64 = 0, };
    ulib_cast_epnt_to_tank( einf, &tank_u.tank );
    tank_ui64[0] = tank_u.ui64;
    return ;
}

void
tofu_imp_ulib_uexp_rbuf_free(struct ulib_shea_uexp *uexp)
{
    struct ulib_shea_rbuf *rbuf = &uexp->rbuf;
    uint32_t iiov;

    for (iiov = 0; iiov < rbuf->niov; iiov++) {
        if (rbuf->iovs[iiov].iov_len == 0) { continue; }
        if (rbuf->iovs[iiov].iov_base != 0
            && rbuf->alloced) {
            //fprintf(stderr, "YI**** FREE %p but NOT\n", rbuf->iovs[iiov].iov_base);
            // free(rbuf->iovs[iiov].iov_base);
        }
        rbuf->iovs[iiov].iov_base = 0;
        rbuf->iovs[iiov].iov_len = 0;
    }
    rbuf->niov = 0;
    return ;
}

/* END OF STATIC definitions */
/**************************************************************************/


size_t tofu_imp_ulib_size(void)
{
    return sizeof (struct ulib_icep);
}

void
tofu_imp_ulib_init(void *vptr,
                   size_t offs,
                   const struct fi_rx_attr *rx_attr,
                   const struct fi_tx_attr *tx_attr)
{
    // struct ulib_icep *icep = (void *)((uint8_t *)vptr + offs);

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "cep %p in %s\n", vptr, __FILE__);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "attr.size RX %ld TX %ld\n",
	(rx_attr == 0)? 0: rx_attr->size,
	(tx_attr == 0)? 0: tx_attr->size);

    return ;
}

void tofu_imp_ulib_fini(void *vptr, size_t offs)
{
    return ;
}

/*
 * context endpoint is enabled
 */
int tofu_imp_ulib_enab(void *vptr, size_t offs)
{
    int fc = FI_SUCCESS;
    return fc;
}

/*
 * gname --> getname YI
 *      For VNI support, the following function is for utof on tlib.
 *      For PostK machine, we must rewrite this one.
 */
int tofu_imp_ulib_gnam(void *ceps[CONF_TOFU_CTXC],
                       size_t offs,
                       char nam_str[128])
{
    int fc = FI_SUCCESS;
    int ix, nx = CONF_TOFU_CTXC;
    /* struct ulib_sep_name name[1]; */
    uint8_t xyzabc[8];
    uint16_t /* utofu_cmp_id_t */ cid[1];
    uint16_t /* utofu_tni_id_t */ tnis[ CONF_TOFU_CTXC ];
    uint16_t /* utofu_cq_id_t */  tcqs[ CONF_TOFU_CTXC ];

    xyzabc[0] = 255;

    for (ix = 0; ix < nx; ix++) {
	void *vptr = ceps[ix];
	struct ulib_icep        *icep;
	utofu_vcq_id_t vcqi = -1UL;
	int uc;

	if (vptr == 0) {
	    tnis[ix] = 255; tcqs[ix] = 255; continue;
	}
	icep = (void *)((uint8_t *)vptr + offs);
	if (icep->vcqh == 0) {
	    fc = -FI_ENODEV; goto bad;
	}
	uc = utofu_query_vcq_id(icep->vcqh, &vcqi);
	if (uc != UTOFU_SUCCESS) {
	    tnis[ix] = 255; tcqs[ix] = 255; continue;
	}
	uc = utofu_query_vcq_info(vcqi, xyzabc, &tnis[ix], &tcqs[ix], cid);
	if (uc != UTOFU_SUCCESS) {
	    tnis[ix] = 255; tcqs[ix] = 255; continue;
	}
    }
    if (xyzabc[0] == 255) {
	fc = -FI_ENODEV; goto bad;
    }
    {
	int wlen, nx2;
	size_t cz = 128;
	char *cp = nam_str;
	char *del = "";
        cid[0] &= 0x7;
	wlen = snprintf(cp, cz, "t://%u.%u.%u.%u.%u.%u.%x/;q=",
		xyzabc[0], xyzabc[1], xyzabc[2],
                        xyzabc[3], xyzabc[4], xyzabc[5], cid[0]);
	if ((wlen <= 0) || (wlen >= cz)) {
	    fc = -FI_EPERM; goto bad;
	}
	cp += wlen;
	cz -= wlen;

	nx2 = -1;
	for (ix = nx - 1; ix >= 0; ix--) {
	    if (tnis[ix] != 255) { nx2 = ix + 1; break; }
	}
	assert(nx2 >= 0);
	for (ix = 0; ix < nx2; ix++) {
	    if (tnis[ix] == 255) {
		wlen = snprintf(cp, cz, "%s", del);
	    }
	    else {
		wlen = snprintf(cp, cz, "%s%u.%u", del, tnis[ix], tcqs[ix]);
	    }
	    if ((wlen <= 0) || (wlen >= cz)) {
		fc = -FI_EPERM; goto bad;
	    }
	    del = ",";
	    cp += wlen;
	    cz -= wlen;
	}
    }
bad:
    return fc;
}

/*
 * YI:  CRITICAL REGION !!!!!!!!!!!!
 */
int
tofu_impl_ulib_sendmsg_self(void *vptr, size_t offs,
                            const struct fi_msg_tagged *tmsg,
                            uint64_t flags)
{
    int         fc = FI_SUCCESS;
    struct ulib_icep      *icep; /* icep keeping tofu resources */
    struct tofu_cep       *tcep = (struct tofu_cep*) vptr;
    struct ulib_shea_expd *expd;
    struct ulib_shea_uexp *sndreq; /* message is copied to sndreq */
    void        *match;
    size_t      len = 0;
    int         snddone = 0;

    /* shadow points to the icep keeping tofu resources */
    icep = ((struct ulib_icep*) (tcep + 1))->shadow;
    if (freestack_isempty(icep->uexp_fs)) {
	fc = -FI_EAGAIN; return fc;
    }
    sndreq = freestack_pop(icep->uexp_fs);
    /* setup info for matching */
    sndreq->utag = tmsg->tag;
    /* similar but diff:  ulib_icep_shea_send_post() in ulib_ofif.c
     * ULIB_SHEA_UEXP_FLAG_SFLG means this is a self message */
    sndreq->flag = ULIB_SHEA_UEXP_FLAG_MBLK | ULIB_SHEA_UEXP_FLAG_SFLG;
    sndreq->flag |= (flags & FI_TAGGED) ? ULIB_SHEA_UEXP_FLAG_TFLG : 0;
    sndreq->flag |= (flags & FI_COMPLETION) ? ULIB_SHEA_UEXP_FLAG_CFLG : 0;
    if (flags & FI_REMOTE_CQ_DATA) {
        sndreq->flag |= ULIB_SHEA_UEXP_FLAG_IFLG;
        sndreq->idat = tmsg->data;
    }
    sndreq->srci = icep->myrank;
    sndreq->sndctxt = tmsg->context;
    sndreq->mblk = 1;
    sndreq->nblk = 1;
    sndreq->boff = 0;
    len = ofi_total_iov_len(tmsg->msg_iov, tmsg->iov_count);
    if (flags & FI_INJECT
        || tmsg->iov_count > ULIB_MAX_IOV) {
        /*
         * In FI_INJECT, message must be copied.
         * Though we do not support FI_INJECT feature, MPICH issue
         * an injected send with zero length message.
         * If the vector length is more than ULIB_MAX_IOV,
         * the message is copied because unexpected queue entry only handles
         * ULIB_MAX_IOV size.
         */
        int         i;
        char        *mem;

        if (len > 0) {
            mem = malloc(len);
            if (mem == NULL) {
                fc = -FI_EAGAIN; return fc;
            }
        } else {
            mem = 0;
        }
        sndreq->rbuf.iovs[0].iov_base = mem;
        sndreq->rbuf.iovs[0].iov_len = len;
        sndreq->rbuf.niov = 1;
        sndreq->rbuf.leng = len;
        sndreq->rbuf.alloced = 1;
        len = 0;
        for (i = 0; i < tmsg->iov_count; i++) {
            memcpy((void*) &mem[len], tmsg->msg_iov[i].iov_base,
                   tmsg->msg_iov[i].iov_len);
            len += tmsg->msg_iov[i].iov_len;
        }
        snddone = 1;
    } else {
        int     i;
        for (i = 0; i < tmsg->iov_count; i++) {
            sndreq->rbuf.iovs[i] = tmsg->msg_iov[i];
        }
        sndreq->rbuf.niov = tmsg->iov_count;
        sndreq->rbuf.leng = len;
        snddone = 0;
    }
    match = ulib_icep_find_expd(icep, sndreq);
    if (match == NULL) {
        struct dlist_entry      *dlist, *head;
        /*
         * A receive request has not been issued. Enqueu the unexpected queue
         */
        dlist = &sndreq->entry;
        dlist_init(dlist);
        head = (flags & FI_TAGGED) ?
            &icep->uexp_list_trcv : &icep->uexp_list_mrcv;
        dlist_insert_tail(dlist, head);
    } else {
        /* corresponding posted receive request has been registered */
        expd = container_of(match, struct ulib_shea_expd, entry);
        snddone = ulib_icep_recv_frag(expd, sndreq /* uexp */, 0);
        assert(snddone == 1);
        len = expd->wlen; /* Needs Hatanaka-san' check */
        /* free uexp->rbuf */
        tofu_imp_ulib_uexp_rbuf_free(sndreq);
        /* free unexpected message */
        freestack_push(icep->uexp_fs, sndreq);
        /* notify receive completion, this is for receive operation */
        ulib_notify_recvcmpl_cq(icep->vp_tofu_rcq, expd);
        /* free expected message */
        freestack_push(icep->expd_fs, expd);
    }
    if (~(flags & FI_INJECT) /*FI_INJECT does not generate cmpl notification*/
        && snddone && (flags & FI_COMPLETION)) {
        /* send completion is notified immediately, not receive operation  */
        struct fi_cq_tagged_entry cq_e[1];
        ulib_init_cqe(cq_e, tmsg->context, flags|FI_SEND,
                      len, 0, tmsg->data, tmsg->tag);
        tofu_cq_comp_tagged(tcep->cep_send_cq, cq_e);
    }
    return FI_SUCCESS;
}


int
tofu_imp_ulib_send_post(void *vptr, size_t offs,
                        const struct fi_msg_tagged *tmsg,
                        uint64_t flags,
                        void *farg)
{
    int fc = FI_SUCCESS;
    struct ulib_icep *icep = (void *)((uint8_t *)vptr + offs);

    ulib_icep_shea_send_post(icep, tmsg, flags, NULL);
    return fc;
}


/* ------------------------------------------------------------------------ */

#include <stdio.h>	/* for sscanf() */
#include <string.h>	/* for strchr() */
#include <stdlib.h>	/* for strtol() */

#include <rdma/fi_errno.h>  /* for FI_EINVAL */


static char * tofu_imp_str_tniq_to_long(const char *pv, long lv[2])
{
    const char *pv1, *pv2;
    char *p1, *p2;
    long lv1, lv2;

#ifdef	NOTYET
    if (pv[0] == ',') {
	lv[0] = -1;
	lv[1] = -1;
	return pv + 1;
    }
#endif	/* NOTYET */
    pv1 = pv; p1 = 0;
    lv1 = strtol(pv1, &p1, 10);
    if ((p1 == 0) || (p1 <= pv1) || (lv1 < 0)) {
	return 0;
    }
    if (p1[0] != '.') {
	return 0;
    }
    pv2 = p1 + 1; p2 = 0;
    lv2 = strtol(pv2, &p2, 10);
    if ((p2 == 0) || (p2 <= pv2) || (lv2 < 0)) {
	return 0;
    }
    if (lv != 0) {
	lv[0] = lv1;
	lv[1] = lv2;
    }
    return (p2[0] == ',')? p2 + 1: p2;
}

static int tofu_imp_str_uri_param_to_long(
    const char *pparm,
    long lv_tniq[CONF_TOFU_CTXC * 2]
)
{
    int ret = 0;
    const char *cp = pparm;
    int nv = 0;
    long lv_orun[2];

    do {
	const char *pq;

	pq = strchr(cp, ';');
	if (pq == 0) { break; }

	if (strncmp(pq, ";q=", 3) == 0) {
	    const char *pv = pq + 3;

	    if (nv > 0) { /* duplicate ? */
		ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	    }
	    while ((pv[0] != '\0') && (pv[0] != ';')) {
		long *lv = (nv >= CONF_TOFU_CTXC)? lv_orun: &lv_tniq[nv * 2];
		const char *new_pv;

		new_pv = tofu_imp_str_tniq_to_long(pv, lv);
		if (new_pv == 0) {
		    ret = -FI_EINVAL; /* -__LINE__ */; goto bad;
		}
		nv++;
		pv = new_pv;
	    }
	    cp = pv;
	}
	else { /* unknown parameter */
	    cp = pq + 1;
	}
    } while (1);

    /* check values */
    {
	int iv;

	for (iv = 0; iv < nv; iv++) {
	    long *lv = &lv_tniq[iv * 2];

	    if (iv >= CONF_TOFU_CTXC) { continue; }
	    if ((lv[0] < 0) || (lv[0] >= 8  /* XXX */ )) { /* max # of tni */
		ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	    }
	    if ((lv[1] < 0) || (lv[1] >= 32 /* XXX */ )) { /* max # of tcq */
		ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	    }
	}
    }

    ret = nv;

bad:
    return ret;
}

int tofu_imp_str_uri_to_long(
    const char *uri,
    long lv_xyzabc[6],
    uint16_t    *cid,
    long lv_tniq[CONF_TOFU_CTXC * 2]
)
{
    int ret = 0;

    /* xyzabc */
    {
	int rc;
        uint32_t cid32;

	rc = sscanf(uri, "t://%ld.%ld.%ld.%ld.%ld.%ld.%x/;q=",
		&lv_xyzabc[0], &lv_xyzabc[1], &lv_xyzabc[2],
                    &lv_xyzabc[3], &lv_xyzabc[4], &lv_xyzabc[5], &cid32);
        *cid = cid32;
	if (rc != 7) {
	    ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	}

	if ( 0
	    || (lv_xyzabc[0] < 0) || (lv_xyzabc[0] >= 256)
	    || (lv_xyzabc[1] < 0) || (lv_xyzabc[1] >= 256)
	    || (lv_xyzabc[2] < 0) || (lv_xyzabc[2] >= 256)
	    || (lv_xyzabc[3] < 0) || (lv_xyzabc[3] >= 2)
	    || (lv_xyzabc[4] < 0) || (lv_xyzabc[4] >= 3)
	    || (lv_xyzabc[5] < 0) || (lv_xyzabc[5] >= 2)
	) {
	    ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	}
    }

    /* ;q=<tniq> */
    {
	int nv;

	nv = tofu_imp_str_uri_param_to_long(uri, lv_tniq);
	if (nv < 0) {
	    ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	}
	ret = nv;
    }

bad:
    return ret;
}

int tofu_imp_str_uri_to_name(
    const void *vuri,
    size_t index,
    void *vnam
)
{
    int fc = FI_SUCCESS;
    const char *cp;
    struct ulib_sep_name name[1];

    cp = (const char *)vuri + (index * 64 /* FI_NAME_MAX */ );
    memset(name, 0, sizeof(struct ulib_sep_name));
    {
	int nv, iv, mv;
	long lv_xyzabc[6], lv_tniq[CONF_TOFU_CTXC * 2];
        uint16_t        lv_cid;

	nv = tofu_imp_str_uri_to_long(cp, lv_xyzabc, &lv_cid, lv_tniq);
	if (nv < 0) {
	    fc = nv; goto bad;
	}
	/* assert(nv <= CONF_TOFU_CTXC); */

	assert((lv_xyzabc[0] >= 0) && (lv_xyzabc[0] < (1L << 8)));
	name->txyz[0] = lv_xyzabc[0];
	assert((lv_xyzabc[1] >= 0) && (lv_xyzabc[1] < (1L << 8)));
	name->txyz[1] = lv_xyzabc[1];
	assert((lv_xyzabc[2] >= 0) && (lv_xyzabc[2] < (1L << 8)));
	name->txyz[2] = lv_xyzabc[2];

	assert((lv_xyzabc[3] >= 0) && (lv_xyzabc[3] < 2));
	name->a = lv_xyzabc[3];
	assert((lv_xyzabc[4] >= 0) && (lv_xyzabc[4] < 3));
	name->b = lv_xyzabc[4];
	assert((lv_xyzabc[5] >= 0) && (lv_xyzabc[5] < 2));
	name->c = lv_xyzabc[5];
        name->p = lv_cid;
	name->v = 1;

	mv = sizeof (name->tniq) / sizeof (name->tniq[0]);
	for (iv = 0; iv < mv; iv++) {
	    long *lv = &lv_tniq[iv * 2];

	    if (iv >= CONF_TOFU_CTXC) {
		name->tniq[iv] = 0xff;
		continue;
	    }
	    if (iv >= nv) {
		name->tniq[iv] = 0xff;
		continue;
	    }
	    if ((lv[0] < 0) || (lv[0] >= 6)) { /* max # of tni */
		name->tniq[iv] = 0xff;
		continue;
	    }
	    if ((lv[1] < 0) || (lv[1] >= 16 /* XXX */)) { /* max # of tcq */
		name->tniq[iv] = 0xff;
		continue;
	    }
	    name->tniq[iv] = (lv[0] << 4) | (lv[1] << 0);
	}

	name->vpid = -1U; /* YYY */
    }

    /* return */
    if (vnam != 0) {
	memcpy(vnam, name, sizeof (name));
    }

bad:
#if 0
    fprintf(stderr, "YIII** %s\t%u.%u.%u.%u.%u.%u cid(%u) return bad(%d)\n",
            __func__,
            name->txyz[0], name->txyz[1], name->txyz[2],
            name->a, name->b, name->c, name->p, fc);
#endif /* 0 */
    return fc;
}

int tofu_imp_name_tniq_byi(
    const void *vnam,
    size_t index,
    uint8_t tniq[2]
)
{
    int fc = FI_SUCCESS;
    const struct ulib_sep_name *name = vnam;
    uint8_t tni, tcq;

    /* check valid flag */
    if (name->v == 0) { fc = -FI_EINVAL; goto bad; }

    /* check tni */
    {
	size_t mx = sizeof (name->tniq) / sizeof (name->tniq[0]);

	if (index > mx) { fc = -FI_EINVAL; goto bad; }

	tni = name->tniq[index] >> 4;
	tcq = name->tniq[index] &  0x0f;
	if (tni >= 6) { fc = -FI_EINVAL; goto bad; }
    }

    tniq[0] = tni; tniq[1] = tcq;

bad:
    return fc;
}

int tofu_imp_name_valid(
    const void *vnam,
    size_t index
)
{
    int fc = FI_SUCCESS;
    uint8_t tniq[2];

    fc = tofu_imp_name_tniq_byi(vnam, index, tniq);
    if (fc != FI_SUCCESS) { goto bad; }

bad:
    return fc;
}

int tofu_imp_namS_to_tank(
    const void *vnam,
    size_t index,
    uint64_t *tank_ui64
)
{
    int fc = FI_SUCCESS;
    const struct ulib_sep_name *name = vnam;
    uint8_t tniq[2];
    struct ulib_epnt_info info[1];

    fc = tofu_imp_name_tniq_byi(vnam, index, tniq);
    if (fc != FI_SUCCESS) { goto bad; }

    info->xyz[0] = name->txyz[0];
    info->xyz[1] = name->txyz[1];
    info->xyz[2] = name->txyz[2];
    info->xyz[3] = name->a;
    info->xyz[4] = name->b;
    info->xyz[5] = name->c;
    info->tni[0] = tniq[0];
    info->tcq[0] = tniq[1];
    info->cid[0] = name->p; /* component id */
    info->pid[0] = name->vpid; /* virtual processor id. (rank) */

    ulib_cast_epnt_to_tank_ui64(info, tank_ui64);

bad:
    return fc;
}
#ifdef	NOTYET

struct ulib_ut_a {
    uint64_t	vcqi;
    uint8_t	paid;
    uint32_t	vpid;
};

int tofu_imp_namS_to_ut_a(
    const void *vnam,
    size_t index,
    struct ulib_ut_a *ut_a
)
{
    int fc = FI_SUCCESS;
    const struct ulib_sep_name *name = vnam;
    uint8_t tniq[2];

    fc = tofu_imp_name_tniq_byi(vnam, index, tniq);
    if (fc != FI_SUCCESS) { goto bad; }

    /* ut_a */
    {
	union jtofu_phys_coords acoo[1];
	utofu_vcq_id_t vcqi[1];
	int jc, uc;

	acoo[0].a[0] = name->txyz[0];
	acoo[0].a[1] = name->txyz[1];
	acoo[0].a[2] = name->txyz[2];
	acoo[0].a[3] = name->a;
	acoo[0].a[4] = name->b;
	acoo[0].a[5] = name->c;

	/* vcqi */
	{
	    uint8_t *txyz = acoo[0].a;
	    utofu_cmp_id_t c_id = name->p; /* component id */
	    utofu_tni_id_t tnid = tni;
	    utofu_cq_id_t  cqid = tcq;

	    /* assert(c_id == CONF_ULIB_CMP_ID); */

	    uc = utofu_construct_vcq_id(txyz, tnid, cqid, c_id, vcqi);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }

	    ut_a->vcqi = vcqi[0];
	}
	/* paid */
	{
	    union jtofu_path_coords pcoo[4];
	    size_t mcnt = sizeof (pcoo) / sizeof (pcoo[0]), ncnt = 0;
	    uint8_t *tabc = pcoo[0].a;
	    utofu_path_id_t paid[1];

	    jc = jtofu_query_onesided_paths(acoo, mcnt, pcoo, &ncnt);
	    if (jc != JTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }
	    if (ncnt <= 0) { fc = -FI_EINVAL; goto bad; }

	    uc = utofu_get_path_id(vcqi, tabc, paid);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }

	    ut_a->paid = paid[0];
	}
	/* vpid */
	ut_a->vpid = name->vpid;
    }

bad:
    return fc;
}
#endif	/* NOTYET */
#ifdef	NOTYET

int tofu_imp_conv_tank_to_cash(
    uint64_t tank_ui64,
    utofu_cmp_id_t c_id,
    struct ulib_utof_cash *cash
)
{
    int fc = FI_SUCCESS;

    /* cash */
    {
	union ulib_tofa_u au = { .ui64 = tank_ui64, };
	union jtofu_phys_coords acoo[1];
	utofu_vcq_id_t vcqi[1];
	int jc, uc;

	acoo[0].a[0] = au.tank.tux;
	acoo[0].a[1] = au.tank.tuy;
	acoo[0].a[2] = au.tank.tuz;
	acoo[0].a[3] = au.tank.tua;
	acoo[0].a[4] = au.tank.tub;
	acoo[0].a[5] = au.tank.tuc;

	/* c_id */
	assert(c_id == au.tank.cid);

	/* vcqi */
	{
	    uint8_t *txyz = acoo->a; /* ->a[6] */
	    utofu_tni_id_t tnid = au.tank.tni;
	    utofu_cq_id_t  cqid = au.tank.tcq;

	    uc = utofu_construct_vcq_id(txyz, tnid, cqid, c_id, vcqi);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }

	    cash->vcqi = vcqi[0];
	}
	/* paid */
	{
	    union jtofu_path_coords pcoo[4];
	    size_t mcnt = sizeof (pcoo) / sizeof (pcoo[0]), ncnt = 0;
	    uint8_t *tabc = pcoo[0].a;
	    utofu_path_id_t paid[1];

	    jc = jtofu_query_onesided_paths(acoo, mcnt, pcoo, &ncnt);
	    if (jc != JTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }
	    if (ncnt <= 0) { fc = -FI_EINVAL; goto bad; }

	    uc = utofu_get_path_id(vcqi[0], tabc, paid);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }

	    cash->paid = paid[0];
	}
	/* vcqh */
	cash->vcqh = 0;
    }

bad:
    return fc;
}
#endif	/* NOTYET */
#ifdef	NOTYET

int tofu_imp_conv_vcqh_to_cash(
    utofu_vcq_hdl_t vcqh,
    struct ulib_utof_cash *cash
)
{
    int fc = FI_SUCCESS;

    /* cash */
    {
	union jtofu_phys_coords acoo[1];
	utofu_vcq_id_t vcqi[1];
	int jc, uc;

	/* vcqi from vcqh */
	{
	    uint8_t *xyz = acoo->a; /* ->a[6] */
	    utofu_tni_id_t tni[1];
	    utofu_cq_id_t tcq[1];
	    uint16_t cid[1];

	    uc = utofu_query_vcq_id(vcqh, vcqi);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }

	    uc = utofu_query_vcq_info(vcqi[0], xyz, tni, tcq, cid);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }

	    cash->vcqi = vcqi[0];
	}
	/* paid */
	{
	    union jtofu_path_coords pcoo[4];
	    size_t mcnt = sizeof (pcoo) / sizeof (pcoo[0]), ncnt = 0;
	    uint8_t *tabc = pcoo[0].a;
	    utofu_path_id_t paid[1];

	    jc = jtofu_query_onesided_paths(acoo, mcnt, pcoo, &ncnt);
	    if (jc != JTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }
	    if (ncnt <= 0) { fc = -FI_EINVAL; goto bad; }

	    uc = utofu_get_path_id(vcqi[0], tabc, paid);
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }

	    cash->paid = paid[0];
	}
	/* vcqh */
	cash->vcqh = vcqh;
    }

bad:
    return fc;
}
#endif	/* NOTYET */
#ifdef	NOTYET

int tofu_imp_fill_cash_stad(
    struct ulib_utof_cash *cash,
    unsigned int ctag,
    unsigned int dtag
)
{
    int fc = FI_SUCCESS;

    {
	int uc;

	uc = utofu_query_stadd(cash->vcqi, ctag, &cash->stad);
	if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }

	uc = utofu_query_stadd(cash->vcqi, dtag, &cash->stad_data);
	if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }
    }

bad:
    return fc;
}
#endif	/* NOTYET */

/* ------------------------------------------------------------------------ */

static inline struct ulib_toqc_cash * tofu_imp_ulib_cash_uref_unsafe(
    struct dlist_entry *head
)
{
    struct ulib_toqc_cash *cash_unref = 0;

    /*
     * An unreferenced (refc == 0) and Least Recently Used (LRU) entry
     * will be reused as a new entry.
     */
    {
	struct dlist_entry *curr, *next;

	dlist_foreach_safe(head, curr, next) {
	    struct ulib_toqc_cash *cash;
	    int32_t refc;

	    cash = container_of(curr, struct ulib_toqc_cash, list);

	    refc = ofi_atomic_get32(&cash->refc);
	    if (refc != 0) { assert(refc > 0); continue; }

	    cash_unref = cash;
	}
	if (cash_unref != 0) {
	    dlist_remove(&cash_unref->list);
	}
    }

    return cash_unref;
}

int
tofu_imp_ulib_cash_find(struct ulib_icep *icep,
                        uint64_t tank /* key */,
                        struct ulib_toqc_cash **pp_cash_tmpl)
{
    int fc = FI_SUCCESS;
    fprintf(stderr, "YI******* NEEDS TO IMPLEMENT %s\n", __func__);
#if 0
#ifdef	NOTYET
    struct ulib_utof_cash rcsh, lcsh;
#endif	/* NOTYET */

    /* tofu_imp_icep_lock(icep); */

    /* find a cash by tank */
    {
	struct dlist_entry *head = &icep->cash_hd;
	struct dlist_entry *curr, *next;

	dlist_foreach_safe(head, curr, next) {
	    struct ulib_toqc_cash *cash_tmpl;

	    cash_tmpl = container_of(curr, struct ulib_toqc_cash, list);
	    if (
#ifdef	NOTYET_NAME_CHNG
		cash_tmpl->akey == tank
#else	/* NOTYET_NAME_CHNG */
		cash_tmpl->fi_a == tank
#endif	/* NOTYET_NAME_CHNG */
	    ) {
		dlist_remove(curr);
		ofi_atomic_inc32(&cash_tmpl->refc);
		dlist_insert_head(curr, head);
		pp_cash_tmpl[0] = cash_tmpl;
		goto bad; /* FOUND - is not an error */
	    }
	}
    }
#ifdef	NOTYET

    /* [lr]csh */
    {
	utofu_vcq_hdl_t vcqh = icep->vcqh;
	const utofu_cmp_id_t c_id = CONF_ULIB_CMP_ID;
	const unsigned int ctag = 10; /* YYY */
	const unsigned int dtag = 11; /* YYY */

	fc = tofu_imp_conv_tank_to_cash(tank, c_id, &rcsh);
	if (fc != FI_SUCCESS) { goto bad; }

	fc = tofu_imp_conv_vcqh_to_cash(vcqh, &lcsh);
	if (fc != FI_SUCCESS) { goto bad; }

	fc = tofu_imp_fill_cash_stad(&rcsh, ctag, dtag);
	if (fc != FI_SUCCESS) { goto bad; }

	fc = tofu_imp_fill_cash_stad(&lcsh, ctag, dtag);
	if (fc != FI_SUCCESS) { goto bad; }
    }
#endif	/* NOTYET */

    /* new cash */
    {
	struct ulib_cash_fs *cash_fs = icep->cash_fs;
	struct ulib_toqc_cash *cash_tmpl;

	assert(cash_fs != 0);
	if (freestack_isempty(cash_fs)) {
	    cash_tmpl = tofu_imp_ulib_cash_uref_unsafe(&icep->cash_hd);
	    if (cash_tmpl == 0) {
		fc = -FI_EAGAIN; goto bad;
	    }
	}
	else {
	    cash_tmpl = freestack_pop(cash_fs);
	}

	/* ulib_toqc_cash_init(cash_tmpl, tank); */ /* YYY */
	{   /* YYY */
#ifdef	NOTYET_NAME_CHNG
	    cash_tmpl->akey = tank; /* YYY */
#else	/* NOTYET_NAME_CHNG */
	    cash_tmpl->fi_a = tank; /* YYY */
#endif	/* NOTYET_NAME_CHNG */
	    ofi_atomic_initialize32(&cash_tmpl->refc, 0); /* XXX YYY */
	}
        printf("YI   cash_tmpl->vpid = au.tank.pid; no member\n");
//#if 0
	/* vpid */
	{
	    union ulib_tofa_u au = { .ui64 = tank, };
	    cash_tmpl->vpid = au.tank.pid;
	}
//#endif /* 0 */

	ofi_atomic_inc32(&cash_tmpl->refc);
#ifdef	NOTYET

	/* set parameters */
	{
	    int uc;

	    cash_tmpl->addr[0] = rcsh; /* structure copy */
	    cash_tmpl->addr[1] = lcsh; /* structure copy */

	    uc = ulib_shea_make_tmpl(cash_tmpl, &rcsh, &lcsh); /* foo1 */
	    if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL; goto bad; }
	}
#else	/* NOTYET */
	memset(&cash_tmpl->addr[0], 0, sizeof (cash_tmpl->addr[0]));
	memset(&cash_tmpl->addr[1], 0, sizeof (cash_tmpl->addr[1]));
#endif	/* NOTYET */

	/* insert */
	{
	    struct dlist_entry *head = &icep->cash_hd;
	    struct dlist_entry *dent = &cash_tmpl->list;

	    dlist_insert_head(dent, head);
#ifdef	NOTDEF

	    /* YYY */ dlist_remove(dent);
	    /* YYY */ freestack_push(cash_fs, cash_tmpl);

	    /* YYY */ assert(dlist_empty(head));
#endif	/* NOTDEF */
	}

	/* return */
	pp_cash_tmpl[0] = cash_tmpl;
    }


bad:
#endif /* 0 */
    /* tofu_imp_icep_unlock(icep); */
    return fc;
}

void
tofu_imp_ulib_cash_free(struct ulib_icep *icep,
                        struct ulib_toqc_cash *cash_tmpl)
{
    assert(ofi_atomic_get32(&cash_tmpl->refc) > 0);
    ofi_atomic_dec32(&cash_tmpl->refc);
    return ;
}

/* ------------------------------------------------------------------------ */

int tofu_imp_ulib_immr_stad(
    struct tofu_mr *mr__priv,
    void *vp_icep,
    const struct iovec *iov1,
    uint64_t *stad_ui64
)
{
    int fc = FI_SUCCESS;
    stad_ui64[0] = (uintptr_t)iov1->iov_base; /* YYY */
    return fc;
}

void tofu_imp_ulib_immr_stad_free(
    struct tofu_mr *mr__priv
)
{
    /* YYY */
    return ;
}

int tofu_imp_ulib_immr_stad_temp(
    void *vp_icep,
    const struct iovec *iov1,
    uint64_t *stad_ui64
)
{
    int fc = FI_SUCCESS;
    stad_ui64[0] = (uintptr_t)iov1->iov_base; /* YYY */
    return fc;
}

/* ------------------------------------------------------------------------ */

size_t tofu_imp_ulib_isep_size(void)
{
    size_t isiz = sizeof (struct ulib_isep);
    fprintf(stderr, "YI****** %s:%d\t%ld\n", __func__, __LINE__, isiz);
    return isiz;
}

int tofu_imp_ulib_isep_open(struct tofu_sep *sep_priv)
{
    int fc = FI_SUCCESS;
    int uc;
    struct ulib_isep *isep;
    utofu_tni_id_t *tnis = 0;
    size_t ntni = 0;
    size_t ni;
    const size_t mtni = sizeof (isep->tnis) / sizeof (isep->tnis[0]);

    isep = (struct ulib_isep*) (sep_priv + 1);

    /* Initialization of Tofu NIC */
    uc = utofu_get_onesided_tnis( &tnis, &ntni );
    //fprintf(stderr, "YIRMACHECK***: uc(%d) ntni(%ld)\n", uc, ntni);
    if (uc != UTOFU_SUCCESS) { fc = -FI_EOTHER; goto bad; }
    if (ntni > mtni) {
        ntni = mtni;
    }
    /* copy tnis[] and ntni */
    for (ni = 0; ni < ntni; ni++) {
        struct utofu_onesided_caps *cap;
        isep->tnis[ni] = tnis[ni];
        utofu_query_onesided_caps(tnis[ni], &cap);
        /* fprintf(stderr, "YIRMA***: %s tnid(%d) num_stags(%d)\n",
           __func__, tnis[ni], cap->num_reserved_stags); */
    }
    isep->ntni = ntni;
    /* free tnis[] */
    if (tnis != 0) {
        free(tnis); tnis = 0;
    }
    assert(isep->ntni > 0);
bad:
    return fc;
}

void tofu_imp_ulib_isep_clos(struct tofu_sep *sep_priv)
{
if (0) {
printf("%s:%d\t%p\n", __func__, __LINE__, sep_priv);
}
    return ;
}

int tofu_imp_ulib_isep_qtni(struct tofu_sep *sep_priv,
                            int index,
                            uint64_t *niid_ui64)
{
    int fc = FI_SUCCESS;
    struct ulib_isep *isep;

    assert(sep_priv != 0);
    isep = (struct ulib_isep *)(sep_priv + 1);

    if ((index < 0) || (index >= isep->ntni)) {
	fc = -FI_EINVAL; goto bad;
    }
    niid_ui64[0] = isep->tnis[index];

bad:
    return fc;
}

struct ulib_oav_addr {
    utofu_vcq_id_t  vcqi;
    utofu_path_id_t paid;
    uint32_t	    vpid;
};

void ulib_icep_free_cash(
    struct ulib_icep *icep,
    struct ulib_toqc_cash *cash_tmpl
)
{
    assert(ofi_atomic_get32(&cash_tmpl->refc) != 0);
    ofi_atomic_dec32(&cash_tmpl->refc);
    return ;
}

int ulib_ioav_find_addr(
    void *vp_ioav,
    fi_addr_t fi_a,
    struct ulib_oav_addr *addr
)
{
    int uc = UTOFU_SUCCESS;
    struct tofu_av *av__priv = vp_ioav;
    struct ulib_epnt_info einf[1];
    utofu_vcq_id_t vcqi[1];
    utofu_path_id_t paid[1];
    uint32_t vpid;

    //fprintf(stderr, "YIUTOFU***: %s ioav(%p) fi_a(%lx)\n", __func__, vp_ioav, fi_a);
    if (av__priv == 0) {
	uc = UTOFU_ERR_INVALID_ARG; goto bad;
    }
    /* einf + vpid from fi_a */
    {
	union ulib_tofa_u tank_u;
	int fc;

	/* tank from fi_a */
	fc = tofu_av_lup_tank(av__priv, fi_a, &tank_u.ui64);
	if (fc != FI_SUCCESS) {
	    uc = UTOFU_ERR_INVALID_ARG; goto bad;
	}

	/* einf (epnt_info) from tank */
	assert(tank_u.tank.vld != 0); /* valid flag */
	einf->xyz[0] = tank_u.tank.tux;
	einf->xyz[1] = tank_u.tank.tuy;
	einf->xyz[2] = tank_u.tank.tuz;
	einf->xyz[3] = tank_u.tank.tua;
	einf->xyz[4] = tank_u.tank.tub;
	einf->xyz[5] = tank_u.tank.tuc;
	einf->tni[0] = tank_u.tank.tni;
	einf->tcq[0] = tank_u.tank.tcq;

	/* assert(tank_u.tank.cid == CONF_ULIB_CMP_ID); */ /* YYY */
	einf->cid[0] = tank_u.tank.cid;
	/* assert(tank_u.tank.pid != UINT32_MAX); */ /* YYY */
	vpid = tank_u.tank.pid;

	/* validation check */
	{
	    union ulib_tofa_u r_ta;
	    uc = ulib_cast_epnt_to_tofa(einf, &r_ta.tofa);
	    assert(uc == UTOFU_SUCCESS);
	    uc = 0; /* XXX */
	}
    }
    /* rcqi (vcqi) from einf */
    uc = utofu_construct_vcq_id(einf->xyz,
	    einf->tni[0], einf->tcq[0], einf->cid[0], vcqi);
    if (uc != UTOFU_SUCCESS) { goto bad; }

    /* paid */
    {
	uint8_t *tabc = &einf->xyz[3];

	uc = utofu_get_path_id(vcqi[0], tabc, paid);
	if (uc != UTOFU_SUCCESS) { goto bad; }
    }

    addr->vcqi = vcqi[0];
    addr->paid = paid[0];
    addr->vpid = vpid;

bad:
    return uc;
}

/*
 * Making tofu commands for specified remote
 */
int
ulib_icep_find_desc(struct ulib_icep *icep,
                    fi_addr_t dfia, /* destination fi_addr */
                    struct ulib_toqc_cash **pp_cash_tmpl)
{
    int uc = UTOFU_SUCCESS;
    struct ulib_toqc_cash *cash_tmpl;
    struct ulib_oav_addr rava, lava;
    utofu_stadd_t rsta, lsta;
#ifdef	CONF_ULIB_SHEA_DAT1
    utofu_stadd_t rsta_data, lsta_data;
#endif	/* CONF_ULIB_SHEA_DAT1 */

    //fprintf(stderr, "YIUTOFU***: %s enter\n", __func__);
    /* ulib_icep_lock(icep); */

    /* find a cash by fi_addr */
    {
	DLST_DECH(ulib_head_desc) *head;
        struct ulib_toqc_cash *prev_cash_tmpl = 0;

	head = &icep->cash_list_desc;

	cash_tmpl = DLST_PEEK(head, struct ulib_toqc_cash, list);
	while (cash_tmpl != 0) {
            /* DLST_NEXT maro defined in ulib_list.h has a bug.
               Fixed it, but needs to confirm it. 2019/05/05 */
            if (prev_cash_tmpl != 0
                && (cash_tmpl == prev_cash_tmpl)) {
                R_DBG("**Needs to FIX!!! "
                       "cash_tmpl->list.next should be head ?? "
                       "cash_tmpl(%p) cash_tmpl->list.next(%p) "
                       "head(%p) head->next(%p)", cash_tmpl,
                       cash_tmpl->list.next,  head, head->next);
                cash_tmpl = 0;
                break;
            }
	    if (cash_tmpl->fi_a == dfia) {
		DLST_RMOV(head, cash_tmpl, list);
		ofi_atomic_inc32(&cash_tmpl->refc);
		DLST_INSH(head, cash_tmpl, list);
		/* return */
		pp_cash_tmpl[0] = cash_tmpl;
                //fprintf(stderr, "\t\t YI: %s 1 leave\n", __func__);
		goto bad; /* FOUND - is not an error */
	    }
            prev_cash_tmpl = cash_tmpl; /* for debugging purpose */
            /* 
             *  In some case, cash_tmpl->list.next == head->next 2019/05/04
             */
            cash_tmpl=DLST_NEXT(cash_tmpl, head, struct ulib_toqc_cash, list);
	}
    }
    /* rava and lava */
    {
	uc = ulib_ioav_find_addr(icep->ioav, dfia, &rava);
	if (uc != UTOFU_SUCCESS) {
            //fprintf(stderr, "\t\t YI: %s 2 leave\n", __func__);
            goto bad;
        }
        rava.vcqi |= 0xF0000000000;

	uc = utofu_query_vcq_id(icep->vcqh, &lava.vcqi);
	if (uc != UTOFU_SUCCESS) {
            //fprintf(stderr, "\t\t YI: %s 3 leave\n", __func__);
            goto bad;
        }
        fprintf(stderr, "\t\t YIYI: REMOTE_VCQ(%lx,%x,%x) LOCAL_VCQ(%lx,%x,%x)\n in %s\n", rava.vcqi, rava.paid, rava.vpid,  lava.vcqi, lava.paid, lava.vpid, __func__); fflush(stderr);
	lava.paid = (utofu_path_id_t)-1U;
    }
    /* rsta and lsta */
    {
	unsigned int ctag = 10; /* YYY */
#ifdef	CONF_ULIB_SHEA_DAT1
	unsigned int dtag = 11; /* YYY */
#endif	/* CONF_ULIB_SHEA_DAT1 */

	uc = utofu_query_stadd(rava.vcqi, ctag, &rsta);
	if (uc != UTOFU_SUCCESS) {
            //fprintf(stderr, "\t\t YI: %s 4 leave\n", __func__);
            goto bad;
        }

	uc = utofu_query_stadd(lava.vcqi, ctag, &lsta);
	if (uc != UTOFU_SUCCESS) {
            //fprintf(stderr, "\t\t YI: %s 5 leave\n", __func__);
            goto bad;
        }
#ifdef	CONF_ULIB_SHEA_DAT1

	uc = utofu_query_stadd(rava.vcqi, dtag, &rsta_data);
	if (uc != UTOFU_SUCCESS) {
            //fprintf(stderr, "\t\t YI: %s 6 leave\n", __func__);
            goto bad;
        }

	uc = utofu_query_stadd(lava.vcqi, dtag, &lsta_data);
	if (uc != UTOFU_SUCCESS) {
            //fprintf(stderr, "\t\t YI: %s 7 leave\n", __func__);
            goto bad;
        }
#endif	/* CONF_ULIB_SHEA_DAT1 */
    }
    /*
     * unreferenced (refc == 0) and Least Recently Used (LRU) entry
     * is allocated for new entry if the free list is empty
     */
    {
	struct ulib_desc_fs *cash_fs;

	cash_fs = icep->desc_fs;

	if (freestack_isempty(cash_fs)) {
	    DLST_DECH(ulib_head_desc) *head;

	    head = &icep->cash_list_desc;

	    cash_tmpl = DLST_LAST(head, ulib_head_desc, struct ulib_toqc_cash, list);
	    assert(cash_tmpl == 0);
	    if (ofi_atomic_get32(&cash_tmpl->refc) > 0) {
		assert(ofi_atomic_get32(&cash_tmpl->refc) == 0); /* YYY */
		uc = UTOFU_ERR_OUT_OF_RESOURCE;
                //fprintf(stderr, "\t\t YI: %s 8 leave\n", __func__);
                goto bad;
	    }
	    DLST_RMOV(head, cash_tmpl, list);
	    ulib_toqc_cash_init(cash_tmpl, dfia);
	    cash_tmpl->vpid = rava.vpid;
	    ofi_atomic_set32(&cash_tmpl->refc, 1);
	    DLST_INSH(head, cash_tmpl, list);
	} else {
	    cash_tmpl = freestack_pop(cash_fs);
	    assert(cash_tmpl != 0);
	    ulib_toqc_cash_init(cash_tmpl, dfia);
	    cash_tmpl->vpid = rava.vpid;
	    ofi_atomic_set32(&cash_tmpl->refc, 1);
	    /* cache it */
	    {
		DLST_DECH(ulib_head_desc) *head;
		head = &icep->cash_list_desc;
		DLST_INSH(head, cash_tmpl, list);
	    }
	}
	/* cash_tmpl->addr[2] */
	{
	    struct ulib_utof_cash *rcsh, *lcsh;

	    rcsh = &cash_tmpl->addr[0];
	    lcsh = &cash_tmpl->addr[1];

	    rcsh->vcqi = rava.vcqi;
	    rcsh->paid = rava.paid;
	    rcsh->vcqh = 0;
	    rcsh->stad = rsta;
#ifdef	CONF_ULIB_SHEA_DAT1
	    rcsh->stad_data = rsta_data;
#endif	/* CONF_ULIB_SHEA_DAT1 */

	    lcsh->vcqi = lava.vcqi;
	    lcsh->paid = lava.paid;
	    lcsh->vcqh = icep->vcqh;
	    lcsh->stad = lsta;
#ifdef	CONF_ULIB_SHEA_DAT1
	    lcsh->stad_data = lsta_data;
#endif	/* CONF_ULIB_SHEA_DAT1 */

	    /* uc = ulib_shea_make_tmpl(cash_tmpl, rcsh, lcsh); */
	    uc = ulib_shea_foo1(cash_tmpl, rcsh, lcsh);
	    if (uc != UTOFU_SUCCESS) {
		assert(uc == UTOFU_SUCCESS);
		goto bad;
	    }
	}
    }
    /* return */
    pp_cash_tmpl[0] = cash_tmpl;

bad:
    /* ulib_icep_unlock(icep); */
    return uc;
}
