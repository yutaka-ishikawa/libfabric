/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_conf.h"			/* for CONF_TOFU_+ */

#include <rdma/providers/fi_prov.h>	/* for struct fi_provider */
#include <rdma/providers/fi_log.h>	/* for FI_INFO() */

extern struct fi_provider		tofu_prov;

#include "ulib_shea.h"
#include "ulib_conv.h"
#include "tofu_impl.h"
#include "tofu_impl_ulib.h"
#include "tofu_impl_internal.h"

#include <stdio.h>			/* for snprintf() */

struct ulib_toqc_cash;
extern int  tofu_imp_ulib_cash_find(
		struct tofu_imp_cep_ulib *icep,
		uint64_t tank,
		struct ulib_toqc_cash **pp_cash_tmpl
	    );
extern void tofu_imp_ulib_cash_free(
		struct tofu_imp_cep_ulib *icep,
		struct ulib_toqc_cash *cash_tmpl
	    );
extern int  tofu_imp_ulib_icep_recv_call_back(
		void *farg, /* icep */
		int r_uc,
		const void *vctx /* uexp */
	    );


size_t tofu_imp_ulib_size(void)
{
    return sizeof (struct tofu_imp_cep_ulib);
}

void
tofu_imp_ulib_init(void *vptr,
                   size_t offs,
                   const struct fi_rx_attr *rx_attr,
                   const struct fi_tx_attr *tx_attr)
{
    struct tofu_imp_cep_ulib *icep = (void *)((uint8_t *)vptr + offs);

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "cep %p in %s\n", vptr, __FILE__);
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "attr.size RX %ld TX %ld\n",
	(rx_attr == 0)? 0: rx_attr->size,
	(tx_attr == 0)? 0: tx_attr->size);

    icep->uexp_sz = 0;
    icep->uexp_fs = 0;
    dlist_init(&icep->uexp_head_trcv);
    dlist_init(&icep->uexp_head_mrcv);

    icep->expd_sz = 0;
    icep->expd_fs = 0;
    dlist_init(&icep->expd_head_trcv);
    dlist_init(&icep->expd_head_mrcv);

    icep->udat_sz = 0;
    icep->udat_fs = 0;

    icep->cash_sz = 0;
    icep->cash_fs = 0;
    dlist_init(&icep->cash_hd);
#ifdef	NOTYET
    icep->cash_rb = 0;
#endif	/* NOTYET */

    /* control: */
    icep->vcqh = 0;
    icep->toqc = 0;
    ulib_shea_cbuf_init(&icep->cbuf);

    if (rx_attr != 0) {
	/*
	 * man fi_endpoint(3)
	 *   fi_rx_attr . size
	 *     The size of the context.
	 *     The size is specified as the minimum number of receive
	 *     operations that may be posted to the endpoint
	 *     without the operation returning -FI_EAGAIN.
	 */
	icep->expd_sz = rx_attr->size;
	/*
	 * man fi_endpoint(3)
	 *   fi_rx_attr . total_buffer_recv
	 *     The provider may adjust or ignore this value.
	 *     The allocation of internal network buffering among
	 *     received message is provider specific.
	 */
	icep->uexp_sz = rx_attr->size; /* XXX YYY total_buffered_recv */
    }
    else if (tx_attr != 0) {
	/*
	 * man fi_endpoint(3)
	 *   fi_tx_attr . size
	 *     The size of the context.
	 *     The size is specified as the minimum number of transmit
	 *     operations that may be posted to the endpoint
	 *     without the operation returning -FI_EAGAIN.
	 */
	icep->udat_sz = tx_attr->size;
	icep->cash_sz = 32; /* YYY XXX */
    }

    return ;
}

void tofu_imp_ulib_fini(void *vptr, size_t offs)
{
    struct tofu_imp_cep_ulib *icep = (void *)((uint8_t *)vptr + offs);

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "cep %p in %s\n", vptr, __FILE__);
    if (icep->uexp_fs != 0) {
	ulib_uexp_fs_free(icep->uexp_fs); icep->uexp_fs = 0;
    }
    if (icep->expd_fs != 0) {
	ulib_expd_fs_free(icep->expd_fs); icep->expd_fs = 0;
    }
    if (icep->udat_fs != 0) {
	ulib_udat_fs_free(icep->udat_fs); icep->udat_fs = 0;
    }
    if (icep->cash_fs != 0) {
	ulib_cash_fs_free(icep->cash_fs); icep->cash_fs = 0;
    }
    /* dlist_init(&icep->cash_hd); */
#ifdef	NOTYET
    if (icep->cash_rb != 0) {
	rbtDelete(icep->cash_rb); icep->cash_rb = 0;
    }
#endif	/* NOTYET */
    ulib_shea_cbuf_fini(&icep->cbuf);

    return ;
}

/*
 * context endpoint is enabled
 */
int tofu_imp_ulib_enab(void *vptr, size_t offs)
{
    int fc = FI_SUCCESS;
    struct tofu_imp_cep_ulib *icep = (void *)((uint8_t *)vptr + offs);
    int cbuf_alloced = 0;

    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "cep %p in %s\n", vptr, __FILE__);
    if ((icep->uexp_fs == 0) && (icep->uexp_sz > 0)) {
	icep->uexp_fs = ulib_uexp_fs_create(icep->uexp_sz, 0, 0);
	if (icep->uexp_fs == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
    }
    if ((icep->expd_fs == 0) && (icep->expd_sz > 0)) {
        FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "YI: Should be CHECK if it works!!!!\n");
	icep->expd_fs = ulib_expd_fs_create(icep->expd_sz, 0, 0);
	if (icep->expd_fs == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
    }
    if ((icep->udat_fs == 0) && (icep->udat_sz > 0)) {
        FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "YI: Should be CHECK if it works!!!!\n");
	icep->udat_fs = ulib_udat_fs_create(icep->udat_sz, 0, 0);
	if (icep->udat_fs == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
    }
    if ((icep->cash_fs == 0) && (icep->cash_sz > 0)) {
        FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "YI: Should be CHECK if it works!!!!\n");
	icep->cash_fs = ulib_cash_fs_create(icep->cash_sz, 0, 0);
	if (icep->cash_fs == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
    }
#ifdef	NOTYET
    if ((icep->cash_rb == 0) && (icep->cash_sz > 0)) {
	icep->cash_rb = rbtNew(0);
	if (icep->cash_rb == 0) {
	    fc = -FI_ENOMEM; goto bad;
	}
    }
#endif	/* NOTYET */
    if (icep->vcqh == 0) {/* Is this really null pointer ? */ 
        /* needs to initialize vcqh */
	struct tofu_cep *cep_priv = vptr;
	struct tofu_sep *sep_priv = cep_priv->cep_sep;
	struct tofu_imp_cep_ulib *icep_pair = 0;

	if (cep_priv->cep_trx != 0) {
	    icep_pair = &((struct tofu_imp_cep_ulib_s *)cep_priv->cep_trx)->cep_lib;
	}
	if ((icep_pair != 0) && (icep_pair->vcqh != 0)) {
            /*
             * The peer context has been initialized.
             *  i.e. receiver context against sender context
             */
	    icep->vcqh = icep_pair->vcqh;
            cbuf_alloced = 1;
            //cep_priv->cep_idx,
            //icep->vcqh,
            //(cep_priv->cep_trx == 0)? '-':
            //(cep_priv->cep_trx->cep_fid.fid.fclass == FI_CLASS_TX_CTX)? 'T':
            //(cep_priv->cep_trx->cep_fid.fid.fclass == FI_CLASS_RX_CTX)? 'R': 'x',
            //(cep_priv->cep_fid.fid.fclass == FI_CLASS_TX_CTX)? 'T':
            //(cep_priv->cep_fid.fid.fclass == FI_CLASS_RX_CTX)? 'R': 'x');
            //}
	} else {
	    uint64_t niid = -1ULL;
	    int index = cep_priv->cep_idx;
            utofu_tni_id_t tni_id;
            const utofu_cmp_id_t c_id = CONF_ULIB_CMP_ID;
            const unsigned long flags =	0
                /* | UTOFU_VCQ_FLAG_THREAD_SAFE */
                /* | UTOFU_VCQ_FLAG_EXCLUSIVE */
                /* | UTOFU_VCQ_FLAG_SESSION_MODE */;
            int uc;

	    fc = tofu_imp_ulib_isep_qtni(sep_priv, index, &niid);
            tni_id = (utofu_tni_id_t)niid;
	    if (fc != FI_SUCCESS) { goto bad; }
            fprintf(stderr, "YI******** CHECKCHECK tni_id(%d) %s\n", tni_id, __func__);
            uc = utofu_create_vcq_with_cmp_id(tni_id, c_id, flags,
                                              &icep->vcqh);
            if (uc != UTOFU_SUCCESS) { fc = -FI_EBUSY; goto bad; }
            assert(icep->vcqh != 0); /* XXX */
        }
    }
    if (icep->toqc == 0) {
	int uc;
        const unsigned int ctag = 10 /* YYY */, dtag = 11 /* YYY */;
        const ulib_shea_ercv_cbak_f func = tofu_imp_ulib_icep_recv_call_back;
        void *farg = icep;

	uc = ulib_toqc_init(icep->vcqh, &icep->toqc);
	if (uc != 0) { fc = -FI_EINVAL /* XXX */; goto bad; }
        if (cbuf_alloced == 1) {
            /* the peer's cbuf has been allocated */
            fprintf(stderr, "YI***** %s cbuf has been allocated\n", __func__);
        } else {
            uc = ulib_shea_cbuf_enab(&icep->cbuf, icep->vcqh,
                                     ctag, dtag, func, farg);
            if (uc != 0) { fc = -FI_EINVAL /* XXX */; goto bad; }
        }
    }
    /* YI added 2019/04/16 */
    {
        utofu_vcq_id_t vcqi = -1UL;
	uint8_t xyz[8];	uint16_t tni[1], tcq[1], cid[1];
        int uc;
        uc = utofu_query_vcq_id(icep->vcqh, &vcqi);
        if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL /* XXX */; goto bad; }
        uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid);
        if (uc != UTOFU_SUCCESS) { fc = -FI_EINVAL /* XXX */; goto bad; }
        icep->tofa.ui64 = 0;
        icep->tofa.tofa.tux = xyz[0];
        icep->tofa.tofa.tuy = xyz[1];
        icep->tofa.tofa.tuz = xyz[2];
        icep->tofa.tofa.tua = xyz[3];
        icep->tofa.tofa.tub = xyz[4];
        icep->tofa.tofa.tuc = xyz[5];
        icep->tofa.tofa.tni = tni[0];
        icep->tofa.tofa.tcq = tcq[0];
        fprintf(stderr, "YI****** self TOFU ADDR ******"
                " xyz=%2x%2x%2x%2x%2x%2x tni=%x tcq=%x cid=%x tofa.ui64=%lx\n",
                xyz[0],xyz[1],xyz[2],xyz[3],xyz[4],xyz[5],
                tni[0], tcq[0], cid[0], icep->tofa.ui64);
    }
bad:
    return fc;
}

/*
 * gname --> getname YI
 *      For VNI support, the following function is for utof on tlib.
 *      For PostK machine, we must rewrite this one.
 */
int tofu_imp_ulib_gnam(
    void *ceps[CONF_TOFU_CTXC],
    size_t offs,
    char nam_str[128]
)
{
    int fc = FI_SUCCESS;
    int ix, nx = CONF_TOFU_CTXC;
    /* struct ulib_sep_name name[1]; */
    uint8_t xyzabc[8];
    uint16_t /* utofu_cmp_id_t */ cid[1];
    uint16_t /* utofu_tni_id_t */ tnis[ CONF_TOFU_CTXC ];
    uint16_t /* utofu_cq_id_t */  tcqs[ CONF_TOFU_CTXC ];

    /* name->txyz[0] = 255; */
    /* name->j_id = j_id : from pmix_proc_t . nspace */
    /* name->rank = rank : from pmix_proc_t . rank   */
    xyzabc[0] = 255;

    for (ix = 0; ix < nx; ix++) {
	void *vptr = ceps[ix];
	struct tofu_imp_cep_ulib *icep;
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
	wlen = snprintf(cp, cz, "t://%u.%u.%u.%u.%u.%u/;q=",
		xyzabc[0], xyzabc[1], xyzabc[2],
		xyzabc[3], xyzabc[4], xyzabc[5]);
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
    int         cf = FI_SUCCESS;
    struct tofu_imp_cep_ulib *icep = (void *)((uint8_t *)vptr + offs);
    void        *match;
    struct ulib_shea_expd *expd;
    struct ulib_shea_uexp *sndreq; /* message is copied to sndreq */


    if (freestack_isempty(icep->uexp_fs)) {
	fc = -FI_EAGAIN; goto bad;
    }
    sendreq = freestack_pop(icep->uexp_fs);
    /* copy message to sendreq */
    {
        sendreq->utag = tmsg->tag;
        sendreq->mblk = 1;
        sendreq->nblk = 1;
        sendreq->boff = 0;
        sendreq->flag = tmsg->ignore;
        sendreq->srci = 0; /* ??? Ask Hatanaka-san */
        assert(tmsg->iov_count == 1); /* must be 1 */
        sendreq->rbuf.iovs[0] = tmsg->msg_iov[0];
        sendreq->rbuf.niov = 1;
        sendreq->rbuf.leng = tmsg->msg_iov[0].iov_len;
        sendreq->vspc_list[0] = 0;  /* ??? Ask Hatanaka-san */
    }
    match = tofu_imp_ulib_icep_find_expd(icep, sndreq);
    if (match == NULL) {
        /*
         * A receive request has not been issued and thus not found
         * Enque the unexpected message queue
         */
        fprintf(stderr, "YI*********** How do I do ? Ask Hatanaka-san\n", __func__);
        // dlist_insert_tail(&recv_entry->entry, dep);
        return FI_SUCCESS;
    }
    /* corresponding posted receive request has been registered */
    fprintf(stderr, "YI*********** How do I do ? Ask Hatanaka-san\n", __func__);
    dlist_remove(match);
    recv_entry = container_of(match, struct ulib_shea_expd, entry);

    /* copy user buffer using requested expected queue */
    tofu_imp_ulib_expd_recv(recv_entry, sendreq);
    /* free uexp->rbuf */
    tofu_imp_ulib_uexp_rbuf_free(uexp);
    /* free unexpected message */
    freestack_push(icep->uexp_fs, sendreq);
    /* check if the packet carrys the last fragment */
    fprintf(stderr, "YI****** Ask Hatanaka-san in %s\n", __func__);
    if (tofu_imp_ulib_expd_cond_comp(recv_entry)) {
        /* notify recv cq */
        tofu_imp_ulib_icep_evnt_expd(icep, recv_entry);
        freestack_push(icep->expd_fs, reqv_entry);
    }
    return FI_SUCESS;
}

/*
 * tofu_imp_ulib_recv_post() is a critical region
 *      guaded by the cep_lck variable in struct tofu_cep.
 */
int
tofu_imp_ulib_recv_post(void    *vptr,  size_t   offs,
                        const struct fi_msg_tagged *tmsg,
                        uint64_t flags,
                        tofu_imp_ulib_comp_f func,
                        void *farg)
{
    int fc = FI_SUCCESS;
    struct tofu_imp_cep_ulib *icep = (void *)((uint8_t *)vptr + offs);
    struct ulib_shea_expd *req;
    struct ulib_shea_uexp *uexp;

    /* get an expected message */
    if (freestack_isempty(icep->expd_fs)) {
        fc = -FI_EAGAIN; goto bad;
    }
    req = freestack_pop(icep->expd_fs);
    tofu_imp_ulib_expd_init(req, tmsg, func, farg, flags);
    /* check unexpected queue */
    uexp = tofu_imp_ulib_icep_find_uexp(icep, req);
    if (uexp == NULL) {
        /* Not found a corresponding message in unexepcted queue,
         * thus insert this request to expected queue */
        tofu_imp_ulib_icep_link_expd(icep, req);
    } else {
        /* Found a corresponding message in unexpected queue,
         * copy user buffer using requested expected queue */
        tofu_imp_ulib_expd_recv(req, uexp);
        /* free uexp->rbuf */
        tofu_imp_ulib_uexp_rbuf_free(uexp);
        /* free unexpected message */
        freestack_push(icep->uexp_fs, uexp);
        /* check if the packet carrys the last fragment */
        fprintf(stderr, "YI****** Ask Hatanaka-san in %s\n", __func__);
        if (tofu_imp_ulib_expd_cond_comp(req)) {
            /* notify recv cq */
            tofu_imp_ulib_icep_evnt_expd(icep, req);
            freestack_push(icep->expd_fs, req);
        }
    }
bad:
    return fc;
}

int
tofu_imp_ulib_send_post(void *vptr, size_t offs,
                        const struct fi_msg_tagged *tmsg,
                        uint64_t flags,
                        tofu_imp_ulib_comp_f func,
                        void *farg)
{
    int fc = FI_SUCCESS;
    struct tofu_imp_cep_ulib *icep = (void *)((uint8_t *)vptr + offs);
    struct ulib_shea_data *udat = 0;

    /* udat */
    udat = tofu_imp_ulib_icep_shea_data_qget(icep);
    if (udat == 0) {
        fc = -FI_EAGAIN; goto bad;
    }
    /* struct fi_tx_attr . iov_limit >= tmsg->iov_count */
#ifdef	NOTYET
    /* udat: tmsg.msg_iov and iov_count */
    {
	void *ctxt = tmsg->context;
	size_t tlen;
	uint32_t vpid; /* remote network address */
	const uint64_t utag = tmsg->tag;
	const uint64_t flag = ULIB_SHEA_DATA_TFLG;

	tlen = ofi_total_iov_len(tmsg->msg_iov, tmsg->iov_count);

	vpid = 0 /* cash_tmpl->vpid */; /* YYY */

	ulib_shea_data_init(udat, ctxt, tlen, vpid, utag, flag);
    }
#endif	/* NOTYET */
    {
	struct ulib_toqc_cash *cash_tmpl = 0;
	int fc2;

	fc2 = tofu_imp_ulib_cash_find(icep, tmsg->addr, &cash_tmpl);

#ifndef	NOTYET
	if (fc2 == FI_SUCCESS) {
	    tofu_imp_ulib_cash_free(icep, cash_tmpl); /* YYY */
	}
#endif	/* NOTYET */
    }

    tofu_imp_ulib_icep_shea_data_qput(icep, udat); /* YYY */

bad:
    return fc;
}

int tofu_imp_ulib_send_post_fast(
    void *vptr,
    size_t offs,
    uint64_t	lsta,
    size_t	tlen,
    uint64_t	tank,
    uint64_t	utag,
    void *	ctxt,
    uint64_t	iflg,
    tofu_imp_ulib_comp_f func,
    void *farg
)
{
    int fc = FI_SUCCESS;
    struct tofu_imp_cep_ulib *icep = (void *)((uint8_t *)vptr + offs);
    struct ulib_toqc_cash *cash_tmpl = 0;
    struct ulib_shea_data *udat = 0;

    fc = tofu_imp_ulib_cash_find(icep, tank, &cash_tmpl);
    if (fc != FI_SUCCESS) { goto bad; }

    /* udat */
    udat = tofu_imp_ulib_icep_shea_data_qget(icep);
    if (udat == 0) { fc = -FI_EAGAIN; goto bad; }

    /* data_init() */
    {
	uint32_t vpid;
	uint64_t flag = ((iflg & FI_TAGGED) == 0)? 0: ULIB_SHEA_DATA_TFLG;

	vpid = cash_tmpl->vpid; /* remote vpid */

	ulib_shea_data_init(udat, ctxt, tlen, vpid, utag, flag);
    }
    /* data_init_cash() */
    {
	struct ulib_toqc *toqc = icep->toqc;
	struct ulib_toqc_cash *cash_real = &udat->real;

        fprintf(stderr, "YI***** \t=== %s() incompatible pointer !!!!\n", __func__);

#ifdef	NOTYET_NAME_CHNG
	cash_real->akey = cash_tmpl->akey; /* address key */
#else	/* NOTYET_NAME_CHNG */
	cash_real->fi_a = cash_tmpl->fi_a; /* address key */
#endif	/* NOTYET_NAME_CHNG */
	cash_real->vpid = cash_tmpl->vpid; /* virtual processor id. */

	assert(toqc != 0);
	ulib_shea_data_init_cash(udat, toqc, cash_tmpl, cash_real);
    }
    /* udat . real */
    {
	struct ulib_toqc_cash *cash_real = &udat->real;

	// assert(cash_tmpl->addr[ 1 /* local */ ].vcqh != 0); /* YYY */
	assert(cash_real->addr[ 1 /* local */ ].vcqh == 0);
	cash_real->addr[1] = cash_tmpl->addr[1]; /* structure copy */
	cash_real->addr[1].stad = lsta; /* overwrite */
	cash_real->addr[1].stad_data =
	    ((iflg & FI_MULTICAST) == 0)? -1ULL: lsta;
    }
    /* esnd */
    {
	struct ulib_shea_cbuf *cbuf = &icep->cbuf;
	struct ulib_shea_esnd *esnd;

	esnd = ulib_shea_cbuf_esnd_qget(cbuf, udat);
        /* YI needs to implement ? */
	if (esnd == 0) {
            fc = -FI_EAGAIN;
            goto bad;
        }
        /* now sending */
	/* udat = 0; */ /* ZZZ */ /* YYY */
	/* cash_tmpl = 0; */ /* ZZZ */ /* YYY */
	/* vpp_send_hndl[0] = esnd; esnd = 0; */ /* ZZZ */
    }

bad:
    if (cash_tmpl != 0) {
	tofu_imp_ulib_cash_free(icep, cash_tmpl); /* YYY */
    }
    if (udat != 0) {
	tofu_imp_ulib_icep_shea_data_qput(icep, udat); /* YYY */
    }
    return fc;
}

void tofu_imp_ulib_icep_shea_esnd_free(
    struct tofu_imp_cep_ulib *icep,
    struct ulib_shea_esnd *esnd
)
{
    struct ulib_shea_data *udat = 0 /* esnd->data */ ; /* XXX */ /* YYY */

    if (udat != 0) {
	struct ulib_toqc_cash *cash_tmpl;

	cash_tmpl = 0 /* ulib_shea_data_toqc_cash_tmpl(udat) */ ; /* YYY */
	if (cash_tmpl != 0) {
	    tofu_imp_ulib_cash_free(icep, cash_tmpl); /* TTT */
	}
	tofu_imp_ulib_icep_shea_data_qput(icep, udat);
	/* esnd->data = 0; */ /* XXX */
    }
    /* queue esnd to the freelist */
    {
	struct ulib_shea_cbuf *cbuf = &icep->cbuf;

	ulib_shea_cbuf_esnd_qput(cbuf, esnd);
	/* esnd = 0; */
    }

/* bad: */
    return ;
}

/* ------------------------------------------------------------------------ */

int tofu_imp_ulib_icep_recv_cbak_uexp(
    struct tofu_imp_cep_ulib *icep,
    const struct ulib_shea_uexp *uexp_orig
)
{
    int fc = FI_SUCCESS;
    struct ulib_shea_uexp *uexp;

    if (freestack_isempty(icep->uexp_fs)) {
	fc = -FI_EAGAIN; goto bad;
    }
    uexp = freestack_pop(icep->uexp_fs);
    assert(uexp != 0);

    /* copy uexp */
    uexp[0] = uexp_orig[0]; /* structure copy */

    /* copy rbuf */
    tofu_imp_ulib_icep_recv_rbuf_copy(icep, &uexp->rbuf, uexp_orig);

    /* queue it */
    {
	struct dlist_entry *uexp_entry = (struct dlist_entry *)&uexp->vspc_list;

	assert(sizeof (uexp->vspc_list) <= sizeof (uexp_entry[0]));
	dlist_init(uexp_entry);

	/* queue it */
	{
	    struct dlist_entry *head;
	    int is_tagged_msg;

	    is_tagged_msg = ((uexp_orig->flag & ULIB_SHEA_RINF_FLAG_TFLG) != 0);
	    if (is_tagged_msg) {
		head = &icep->uexp_head_trcv;
	    }
	    else {
		head = &icep->uexp_head_mrcv;
	    }
	    dlist_insert_tail(uexp_entry, head);
	}
    }

bad:
    return fc;
}

int tofu_imp_ulib_icep_recv_call_back(
    void *farg, /* icep */
    int r_uc,
    const void *vctx /* uexp */
)
{
    int fc = FI_SUCCESS;
    struct tofu_imp_cep_ulib *icep = farg;
    const struct ulib_shea_uexp *uexp = vctx;
    struct ulib_shea_expd *expd;

    expd = tofu_imp_ulib_icep_find_expd(icep, uexp);
    if (expd == 0) {
	fc = tofu_imp_ulib_icep_recv_cbak_uexp(icep, uexp);
	if (fc != FI_SUCCESS) { goto bad; }
	goto bad; /* XXX - is not an error */
    }

    /* XXX iov_base : offs => base + offs */
    tofu_imp_ulib_icep_recv_rbuf_base(icep, uexp,
	(struct iovec *)uexp->rbuf.iovs);

    /* update expd */
    tofu_imp_ulib_expd_recv(expd, uexp);

    /* check if the packet is a last fragment */
    if ( ! tofu_imp_ulib_expd_cond_comp(expd) ) {
	/* re-queue it */
	tofu_imp_ulib_icep_link_expd_head(icep, expd);
	goto bad; /* XXX - is not an error */
    }

    /* notify recv cq */
    tofu_imp_ulib_icep_evnt_expd(icep, expd);

    freestack_push(icep->expd_fs, expd);

bad:
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
    long lv_tniq[CONF_TOFU_CTXC * 2]
)
{
    int ret = 0;

    /* xyzabc */
    {
	int rc;

	rc = sscanf(uri, "t://%ld.%ld.%ld.%ld.%ld.%ld/%*c",
		&lv_xyzabc[0], &lv_xyzabc[1], &lv_xyzabc[2],
		&lv_xyzabc[3], &lv_xyzabc[4], &lv_xyzabc[5]);
	if (rc != 6) {
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
    fprintf(stderr, "YI********* %s: index(%ld) cp=%p\n", __func__, index, cp);
    fprintf(stderr, "YI********* %s: index(%ld) cp=%s\n", __func__, index, (char*) &cp);

    {
	int nv, iv, mv;
	long lv_xyzabc[6], lv_tniq[CONF_TOFU_CTXC * 2];

	nv = tofu_imp_str_uri_to_long(cp, lv_xyzabc, lv_tniq);
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

	name->p = 0 /* YYY CONF_ULIB_CMP_ID */ ;
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
if (0) {
printf("\t%u.%u.%u.%u.%u.%u"
"\t%u.%.u %u.%u %u.%u %u.%u"
"\t%u.%.u %u.%u %u.%u %u.%u\n",
name->txyz[0], name->txyz[1], name->txyz[2],
name->a, name->b, name->c,
name->tniq[0] >> 4, name->tniq[0] & 0x0f,
name->tniq[1] >> 4, name->tniq[1] & 0x0f,
name->tniq[2] >> 4, name->tniq[2] & 0x0f,
name->tniq[3] >> 4, name->tniq[3] & 0x0f,
name->tniq[4] >> 4, name->tniq[4] & 0x0f,
name->tniq[5] >> 4, name->tniq[5] & 0x0f,
name->tniq[6] >> 4, name->tniq[6] & 0x0f,
name->tniq[7] >> 4, name->tniq[7] & 0x0f);
}
    }

    /* return */
    if (vnam != 0) {
	memcpy(vnam, name, sizeof (name));
    }

bad:
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
    /* info->pid[0] = name->vpid; */ /* YYY virtual processor id. (rank) */

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

int tofu_imp_ulib_cash_find(
    struct tofu_imp_cep_ulib *icep,
    uint64_t tank /* key */,
    struct ulib_toqc_cash **pp_cash_tmpl
)
{
    int fc = FI_SUCCESS;
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
    /* tofu_imp_icep_unlock(icep); */
    return fc;
}

void tofu_imp_ulib_cash_free(
    struct tofu_imp_cep_ulib *icep,
    struct ulib_toqc_cash *cash_tmpl
)
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
    size_t isiz = sizeof (struct tofu_imp_sep_ulib);
    fprintf(stderr, "YI****** %s:%d\t%ld\n", __func__, __LINE__, isiz);
    return isiz;
}

int tofu_imp_ulib_isep_open(
    struct tofu_sep *sep_priv
)
{
    int fc = FI_SUCCESS;
    int uc;
    struct tofu_imp_sep_ulib *isep;
    utofu_tni_id_t *tnis = 0;
    size_t ntni = 0;
    size_t ni, nn = ntni;
    const size_t mtni = sizeof (isep->tnis) / sizeof (isep->tnis[0]);

    isep = &((struct tofu_imp_sep_ulib_s *)sep_priv)->sep_lib;
    fprintf(stderr, "YI***** %s:%d\t%p %p\n",
            __func__, __LINE__, sep_priv, isep); fflush(stderr);

    uc = utofu_get_onesided_tnis( &tnis, &ntni );
    if (uc != UTOFU_SUCCESS) { fc = -FI_EOTHER; goto bad; }
    /* isep */
    if (nn > mtni) {
        nn = mtni;
    }
    /* copy tnis[] and ntni */
    for (ni = 0; ni < nn; ni++) {
        isep->tnis[ni] = tnis[ni];
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

int tofu_imp_ulib_isep_qtni(
    struct tofu_sep *sep_priv,
    int index,
    uint64_t *niid_ui64
)
{
    int fc = FI_SUCCESS;
    struct tofu_imp_sep_ulib *isep;

    assert(sep_priv != 0);
    isep = &((struct tofu_imp_sep_ulib_s *)sep_priv)->sep_lib;

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
    /* assert(ofi_atomic_get32(&cash_tmpl->refc) != 0); */
    /* ofi_atomic_dec32(&cash_tmpl->refc); */ /* YYY */
    assert(cash_tmpl->refc != 0);
    cash_tmpl->refc--;
    return ;
}

int ulib_ioav_find_addr(
    struct ulib_ioav *ioav,
    fi_addr_t fi_a,
    struct ulib_oav_addr *addr
)
{
    int uc = UTOFU_SUCCESS;
    uint32_t rx_ctx_bits = 3;
    uint64_t rx_ctx_mask = (1ULL << (64 - rx_ctx_bits)) - 1;
    fi_addr_t base;
    uint64_t rx_index; /* fi_rx_addr() */

    if (ioav == 0) {
	uc = UTOFU_ERR_INVALID_ARG; goto bad;
    }
    /*
     * oav_priv
     *   uint64_t rx_ctx_mask
     *   if (oav_priv->rx_ctx_bits == 0) {
     *     oav_priv->rx_ctx_mask = -1ULL;
     *   }
     *   else {
     *     oav_priv->rx_ctx_mask = (1ULL << (64 - oav_priv->rx_ctx_bits)) - 1;
     *   }
     */

    base = fi_a & rx_ctx_mask;
    rx_index = fi_a >> (64 - rx_ctx_bits);

#ifdef	NOTYET
    {
	jtofu_job_id_t j_id = 0; /* YYY */
	jtofu_rank_t vpid = (jtofu_rank_t)base;
	union jtofu_phys_coords acoo[1];
	union jtofu_path_coords pcoo[4];
	size_t mcnt = sizeof (pcoo) / sizeof (pcoo[0]), ncnt;
	int jc;

	jc = jtofu_query_phys_coords(j_id, vpid, acoo);
	if (jc != JTOFU_SUCCESS) {
	    uc = UTOFU_ERR_NOT_FOUND; goto bad;
	}

	jc = jtofu_query_onesided_paths(acoo, mcnt, pcoo, &ncnt);
	if (jc != JTOFU_SUCCESS) {
	    uc = UTOFU_ERR_NOT_FOUND; goto bad;
	}
	if (ncnt <= 0) {
	    uc = UTOFU_ERR_NOT_FOUND; goto bad;
	}

	/* rcqi */
	{
	    uint8_t *txyz = acoo[0].a; /* acoo[0].s.x */
	    utofu_cmp_id_t c_id = 0; /* YYY CONF_ULIB_CMP_ID */
	    utofu_tni_id_t tnid = 0; /* YYY vpid */
	    utofu_cq_id_t  cqid = 0; /* YYY rx_index */
	    utofu_vcq_id_t vcqi[1];

	    uc = utofu_construct_vcq_id(txyz, tnid, cqid, c_id, vcqi);
	    if (uc != UTOFU_SUCCESS) {
		uc = UTOFU_ERR_NOT_FOUND; goto bad;
	    }
	    addr->vcqi = vcqi[0];
	}
	/* paid */
	{
	    uint8_t *tabc = pcoo[0].a; /* pcoo[0].s.a */
	    utofu_path_id_t paid[1];

	    uc = utofu_get_path_id(vcqi, tabc, paid);
	    if (uc != UTOFU_SUCCESS) {
		uc = UTOFU_ERR_NOT_FOUND; goto bad;
	    }
	    addr->paid = paid[0];
	}
	addr->vpid = vpid;
    }
#else	/* NOTYET */
    if (base != 0) { /* YYY */
	uc = UTOFU_ERR_NOT_FOUND; goto bad;
    }
    if (rx_index != 0) { /* YYY */
	uc = UTOFU_ERR_NOT_FOUND; goto bad;
    }
    /* YYY */
    {
	utofu_vcq_hdl_t lcqh;
	utofu_vcq_id_t lcqi;
	utofu_path_id_t lpai;

	/* lcqh */
	{
	    struct ulib_icep *icep = (void *)ioav; /* YYY */

	    lcqh = icep->vcqh;
	}
	/* lcqi */
	{
	    uc = utofu_query_vcq_id(lcqh, &lcqi);
	    if (uc != UTOFU_SUCCESS) { goto bad; }
	}
	/* lpai */
	{
	    struct ulib_epnt_info i;
	    uint8_t *tabc;

	    uc = utofu_query_vcq_info(lcqi, i.xyz, i.tni, i.tcq, i.cid);
	    if (uc != UTOFU_SUCCESS) { goto bad; }
	    tabc = &i.xyz[3];

	    uc = utofu_get_path_id(lcqi, tabc, &lpai);
	    if (uc != UTOFU_SUCCESS) { goto bad; }
	}

	addr->vcqi = lcqi;
	addr->paid = lpai;
	addr->vpid = 0;
    }
#endif	/* NOTYET */

bad:
    return uc;
}

int ulib_icep_find_cash(
    struct ulib_icep *icep,
    fi_addr_t dfia, /* destination fi_addr */
    struct ulib_toqc_cash **pp_cash_tmpl
)
{
    int uc = UTOFU_SUCCESS;
    static struct ulib_cash_fs *free_cash_fs = 0; /* YYY */
    static DLST_DECH(ulib_head_cash) head_cash; /* YYY */
    struct ulib_toqc_cash *cash_tmpl;
    struct ulib_oav_addr rava, lava;
    utofu_stadd_t rsta, lsta;
#ifdef	CONF_ULIB_SHEA_DAT1
    utofu_stadd_t rsta_data, lsta_data;
#endif	/* CONF_ULIB_SHEA_DAT1 */

    ulib_icep_lock(icep);
    /* icep_open() */
    if (free_cash_fs == 0) {
	DLST_INIT(&head_cash); /* YYY */
	free_cash_fs = ulib_cash_fs_create( 512, 0, 0 /* YYY */ ); /* YYY */
	if (free_cash_fs == 0) {
	    uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
	}
    }

    /* find a cash by fi_addr */
    {
	DLST_DECH(ulib_head_cash) *head;

	head = &head_cash /* icep->head_cash */; /* YYY */

	cash_tmpl = DLST_PEEK(head, struct ulib_toqc_cash, list);
	while (cash_tmpl != 0) {
	    if (cash_tmpl->fi_a == dfia) {
		DLST_RMOV(head, cash_tmpl, list);
		/* ofi_atomic_inc32(&cash_tmpl->refc); */ /* YYY */
		cash_tmpl->refc++;
		DLST_INSH(head, cash_tmpl, list);
		/* return */
		pp_cash_tmpl[0] = cash_tmpl;
		goto bad; /* FOUND - is not an error */
	    }
	    cash_tmpl = DLST_NEXT(cash_tmpl, head, struct ulib_toqc_cash, list);
	}
	assert(cash_tmpl == 0);
    }
    /* rava and lava */
    {
	struct ulib_ioav *ioav = icep->ioav;

if (ioav == 0) {
ioav = (void *)icep; /* YYY */
}
	uc = ulib_ioav_find_addr(ioav, dfia, &rava);
	if (uc != UTOFU_SUCCESS) { goto bad; }

	uc = utofu_query_vcq_id(icep->vcqh, &lava.vcqi);
	if (uc != UTOFU_SUCCESS) { goto bad; }
	lava.paid = (utofu_path_id_t)-1U;
    }
    /* rsta and lsta */
    {
	unsigned int ctag = 10; /* YYY */
#ifdef	CONF_ULIB_SHEA_DAT1
	unsigned int dtag = 11; /* YYY */
#endif	/* CONF_ULIB_SHEA_DAT1 */

	uc = utofu_query_stadd(rava.vcqi, ctag, &rsta);
	if (uc != UTOFU_SUCCESS) { goto bad; }

	uc = utofu_query_stadd(lava.vcqi, ctag, &lsta);
	if (uc != UTOFU_SUCCESS) { goto bad; }
#ifdef	CONF_ULIB_SHEA_DAT1

	uc = utofu_query_stadd(rava.vcqi, dtag, &rsta_data);
	if (uc != UTOFU_SUCCESS) { goto bad; }

	uc = utofu_query_stadd(lava.vcqi, dtag, &lsta_data);
	if (uc != UTOFU_SUCCESS) { goto bad; }
#endif	/* CONF_ULIB_SHEA_DAT1 */
    }
    /*
     * unreferenced (refc == 0) and Least Recently Used (LRU) entry
     * is allocated for new entry if the free list is empty
     */
    {
	struct ulib_cash_fs *cash_fs;

	cash_fs = free_cash_fs /* icep->cash_fs */; /* YYY */

	if (freestack_isempty(cash_fs)) {
	    DLST_DECH(ulib_head_cash) *head;

	    head = &head_cash /* icep->head_cash */; /* YYY */

	    cash_tmpl = DLST_LAST(head, ulib_head_cash, struct ulib_toqc_cash, list);
	    assert(cash_tmpl == 0);
	    if (cash_tmpl->refc > 0) {
		assert(cash_tmpl->refc == 0); /* YYY */
		uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
	    }
	    DLST_RMOV(head, cash_tmpl, list);
	    ulib_toqc_cash_init(cash_tmpl, dfia);
	    cash_tmpl->vpid = rava.vpid;
	    /* ofi_atomic_set32(&cash_tmpl->refc, 1); */ /* YYY */
	    cash_tmpl->refc = 1;
	    DLST_INSH(head, cash_tmpl, list);
	}
	else {
	    cash_tmpl = freestack_pop(cash_fs);
	    assert(cash_tmpl != 0);
	    ulib_toqc_cash_init(cash_tmpl, dfia);
	    cash_tmpl->vpid = rava.vpid;
	    /* ofi_atomic_set32(&cash_tmpl->refc, 1); */ /* YYY */
	    cash_tmpl->refc = 1;
	    /* cache it */
	    {
		DLST_DECH(ulib_head_cash) *head;
		head = &head_cash /* icep->head_cash */; /* YYY */
		DLST_INSH(head, cash_tmpl, list);
	    }
#if 0
	    if (0) {
		int c_no = ulib_cash_fs_index(cash_fs, cash_tmpl);
		assert(c_no >= 0);
	    }
	    if (0) {
		ulib_cash_fs_free(cash_fs);
	    }
#endif
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
    ulib_icep_unlock(icep);
    return uc;
}
