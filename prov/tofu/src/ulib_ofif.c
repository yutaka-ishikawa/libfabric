/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_debug.h"
#include "ulib_conf.h"
#include "ulib_dlog.h"	    /* for ENTER_RC_C() */

#include "ulib_shea.h"
#include "ulib_conv.h"	    /* for struct ulib_epnt_info */
#include "ulib_ofif.h"	    /* for struct ulib_isep */
#ifndef	notdef_icep_toqc
#include "ulib_toqc.h"	    /* for struct ulib_toqc */
#endif	/* notdef_icep_toqc */
#include "ulib_tick.h"	    /* for ulib_tick_time() */

#include "utofu.h"	    /* for UTOFU_SUCCESS */

#include "tofu_prv.h"	    /* for struct tofu_*() XXX */
#include "tofu_cq.h"	    /* for tofu_cq_comp_tagged() XXX */

#include <assert.h>	    /* for assert() */
#include <stdlib.h>	    /* for free() */
#include <stdio.h>	    /* for printf() */

/* should be moved */
static inline char *
fi_addr2string(char *buf, ssize_t sz, fi_addr_t fi_addr, struct fid_ep *fid_ep)
{
    struct tofu_cep *cep_priv;
    struct tofu_av *av__priv;
    uint64_t ui64;

    cep_priv = container_of(fid_ep, struct tofu_cep, cep_fid);
    av__priv = cep_priv->cep_sep->sep_av_;
    tofu_av_lup_tank(av__priv, fi_addr, &ui64);
    return tank2string(buf, sz, ui64);
}

extern void tofu_imp_ulib_uexp_rbuf_free(struct ulib_shea_uexp *uexp);

void
ulib_show_expd(const char *fnnam, int lno, struct ulib_icep *icep)
{
    struct dlist_entry *head, *item;

    fprintf(stderr, "YI_SHOWEXPD: Tagged at %s:%d\n", fnnam, lno);
    head = &icep->expd_list_trcv;
    dlist_foreach(head, item) {
        struct ulib_shea_expd   *expd;
	expd = container_of(item, struct ulib_shea_expd, entry);
        fprintf(stderr, "\t: expd(%p) expd->iovs[0].iov_base(%p)\n", expd, expd->iovs[0].iov_base); fflush(stderr);
    }
    fprintf(stderr, "YI_SHOWEXPD: No tagged\n");
    head = &icep->expd_list_mrcv;
    dlist_foreach(head, item) {
        struct ulib_shea_expd   *expd;
	expd = container_of(item, struct ulib_shea_expd, entry);
        fprintf(stderr, "\t: expd(%p) expd->iovs[0].iov_base(%p)\n", expd, expd->iovs[0].iov_base); fflush(stderr);
    }
}


const char *
fi_class_string(struct tofu_cep *cep)
{
    char        *rc;
    switch(cep->cep_fid.fid.fclass) {
    case FI_CLASS_DOMAIN: rc ="DOMAIN"; break;
    case FI_CLASS_EP:  rc ="EP"; break;
    case FI_CLASS_SEP:  rc ="SEP"; break;
    case FI_CLASS_RX_CTX:  rc ="RX_CTX"; break;
    case FI_CLASS_SRX_CTX:  rc ="SRC_CTX"; break;
    case FI_CLASS_TX_CTX:  rc ="TX_CTX"; break;
    case FI_CLASS_STX_CTX:  rc ="STX_CTX"; break;
    case FI_CLASS_PEP:  rc ="PEP"; break;
    case FI_CLASS_INTERFACE:  rc ="INTERFACE"; break;
    case FI_CLASS_AV:  rc ="AV"; break;
    case FI_CLASS_MR:  rc ="MR"; break;
    case FI_CLASS_EQ:  rc ="EQ"; break;
    case FI_CLASS_CQ:  rc ="CA"; break;
    case FI_CLASS_CNTR:  rc ="CNTR"; break;
    case FI_CLASS_WAIT:  rc ="WAIT"; break;
    case FI_CLASS_POLL:  rc ="POLL"; break;
    case FI_CLASS_CONNREQ:  rc ="CONNREQ"; break;
    case FI_CLASS_MC:  rc ="MC"; break;
    case FI_CLASS_NIC:  rc ="NIC"; break;
    default: rc = "UNKNOWN"; break;
    }
    return rc;
}

void
showCEP(struct ulib_icep *icep)
{
    struct tofu_cep     *cep = (struct tofu_cep*) (((char*)icep) - sizeof(struct tofu_cep));
    fprintf(stderr, "\t\t%s\n", fi_class_string(cep));
}


int
ulib_icep_ctrl_enab(void *ptr, size_t off)
{
    int uc = UTOFU_SUCCESS;
    struct tofu_cep *cep_priv  = (struct tofu_cep*) ptr;
    struct ulib_icep *icep = (struct ulib_icep*) ((char*)ptr + off);
    struct ulib_isep *isep;

    ENTER_RC_C(uc);

    if (icep == 0) {
	uc = UTOFU_ERR_INVALID_ARG; RETURN_BAD_C(uc);
    }
    /* shadow icep */
    if (icep->shadow == 0) {
        struct tofu_cep *cep_peer = 0;
        struct tofu_sep *sep_priv = cep_priv->cep_sep;
        struct ulib_icep *icep_peer;

	assert(icep->enabled == 0);
	if (cep_priv->cep_trx != 0) {
	    cep_peer = cep_priv->cep_trx;
	} else {
	    switch (cep_priv->cep_fid.fid.fclass) {
	    case FI_CLASS_TX_CTX:
		cep_peer = tofu_sep_lup_cep_byi_unsafe(sep_priv,
				FI_CLASS_RX_CTX, cep_priv->cep_idx);
		break;
	    case FI_CLASS_RX_CTX:
		cep_peer = tofu_sep_lup_cep_byi_unsafe(sep_priv,
				FI_CLASS_TX_CTX, cep_priv->cep_idx);
		break;
	    default:
		fprintf(stderr, "YI*********** ERRRRRRRRRRRRR\n");
		uc = UTOFU_ERR_FATAL; RETURN_BAD_C(uc);
	    }
	}
	if (cep_peer != 0) {
	    icep_peer = (struct ulib_icep *)(cep_peer + 1);

	    if (icep_peer->enabled != 0) {
		assert(icep_peer->index == cep_priv->cep_idx);
		assert(icep_peer->isep == (struct ulib_isep *)(sep_priv + 1));
		/* All ICEP's CQs point to Tofu CEP's one  */
		if (icep_peer->vp_tofu_scq == 0) {
		    icep_peer->vp_tofu_scq = cep_priv->cep_send_cq;
		}
		if (icep_peer->vp_tofu_rcq == 0) {
		    icep_peer->vp_tofu_rcq = cep_priv->cep_recv_cq;
		}
		icep->shadow = icep_peer;
		RETURN_OK_C(uc);
	    }
	}
    }
    if (icep->enabled != 0) {
	assert(icep->shadow != 0);
	uc = UTOFU_ERR_BUSY; RETURN_BAD_C(uc);
    }

    if (icep->index < 0) { /* XXX III */
	struct tofu_cep *cep_priv = ptr;
	icep->index = cep_priv->cep_idx;
    }
    if (icep->isep == 0) { /* XXX III */
	struct tofu_cep *cep_priv = ptr;
	icep->isep = (struct ulib_isep *)(cep_priv->cep_sep + 1);
	assert(icep->isep != 0);
    }
    isep = icep->isep;

    /* unexpected entries */
    if (icep->uexp_fs == 0) {
	/* YYY fi_rx_attr . total_buffered_recv and size ? */
	icep->uexp_fs = ulib_uexp_fs_create( 512, 0, 0 /* YYY */ );
	if (icep->uexp_fs == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}
    }
    /* expected entries */
    if (icep->expd_fs == 0) {
	/* YYY fi_rx_attr . size ? */
	icep->expd_fs = ulib_expd_fs_create( 512, 0, 0 /* YYY */ );
	if (icep->expd_fs == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}
    }
    /* All ICEP's CQs point to Tofu CEP's one  */
    if (icep->vp_tofu_scq == 0) {
	struct tofu_cep *cep_priv = ptr;
	icep->vp_tofu_scq = cep_priv->cep_send_cq;
    }
    if (icep->vp_tofu_rcq == 0) {
	struct tofu_cep *cep_priv = ptr;
	icep->vp_tofu_rcq = cep_priv->cep_recv_cq;
    }
    /* transmit entries */
    if (icep->udat_fs == 0) {
	/* YYY fi_tx_attr . size ? */
	icep->udat_fs = ulib_udat_fs_create( 512, 0, 0 /* YYY */ );
	if (icep->udat_fs == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}
    }
    /* desc_cash */
    if (icep->desc_fs == 0) {
	icep->desc_fs = ulib_desc_fs_create( 512, 0, 0 /* YYY */ );
	if (icep->desc_fs == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}
    }
    /* icep_ctrl_enab */
    if (icep->vcqh == 0 /* YYY +_NULL */ )
    {
	utofu_vcq_hdl_t vcqh = 0;
	utofu_tni_id_t tni_id;
        struct tofu_domain  *dom = cep_priv->cep_sep->sep_dom;
	const utofu_cmp_id_t c_id = CONF_ULIB_CMP_ID;
	const unsigned long flags =	0
				/* | UTOFU_VCQ_FLAG_THREAD_SAFE */
				/* | UTOFU_VCQ_FLAG_EXCLUSIVE */
				/* | UTOFU_VCQ_FLAG_SESSION_MODE */
				;

	if ((icep->index < 0) || (icep->index >= isep->ntni)) {
	    uc = UTOFU_ERR_INVALID_TNI_ID; RETURN_BAD_C(uc);
	}
	tni_id = isep->tnis[icep->index];
	uc = utofu_create_vcq_with_cmp_id(tni_id, c_id, flags, &vcqh);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
#if 0 /* checking vcq_id 2019/05/01 */
        {
            utofu_vcq_id_t vcqi = -1UL;
            uint8_t xyz[8];	uint16_t tni[1], tcq[1], cid[1];
            union ulib_tofa_u tofa;
            char buf[128];
            uc = utofu_query_vcq_id(vcqh, &vcqi);
            uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid);
            tofa.ui64 = 0;
            tofa.tank.tux = xyz[0]; tofa.tank.tuy = xyz[1];
            tofa.tank.tuz = xyz[2]; tofa.tank.tua = xyz[3];
            tofa.tank.tub = xyz[4]; tofa.tank.tuc = xyz[5];
            tofa.tank.tni = tni[0]; tofa.tank.tcq = tcq[0];
            tofa.tank.cid = cid[0];
            printf("%d: vcqh(0x%lx) vcqid(%s) in %s:%d of %s\n", mypid, vcqh,
                   tank2string(buf, 128, tofa.ui64),
                   __func__, __LINE__, __FILE__);
            fflush(stdout);
        }
#endif

	assert(vcqh != 0); /* XXX : UTOFU_VCQ_HDL_NULL */
	icep->vcqh = vcqh;
        /*
         * vcqh is copied to domain
         *   It seems that vcqh should be created at domain creation time,
         *   but vcqh associated with TNI is only created at this time.
         */
        dom->dom_vcqh[dom->dom_nvcq] = icep->vcqh;
        dom->dom_nvcq++;
    }
    if (icep->toqc == 0) {
        uc = ulib_toqc_init(icep->vcqh, &icep->toqc);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
    {
        struct tofu_cep *cep_priv = (struct tofu_cep*) ptr;
        struct tofu_sep *sep_priv = cep_priv->cep_sep;

	if (icep->ioav == 0) {
	    icep->ioav = sep_priv->sep_av_; /* av__priv */
	}
    }
    /* initializing cbuf */
    if (icep->cbufp == NULL) {
	const unsigned int ctag = 10, dtag = 11;
	const ulib_shea_ercv_cbak_f func = ulib_icep_recv_call_back;
	void *farg = icep;

	icep->cbufp = (struct ulib_shea_cbuf*)
	    malloc(sizeof(struct ulib_shea_cbuf));
	uc = ulib_shea_cbuf_init(icep->cbufp);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	uc = ulib_shea_cbuf_enab(icep->cbufp, icep->vcqh,
				 ctag, dtag, func, farg);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
    {
        utofu_vcq_id_t vcqi = -1UL;
	uint8_t xyz[8];	uint16_t tni[1], tcq[1], cid[1];
        int uc;
        char    buf[128];
        uc = utofu_query_vcq_id(icep->vcqh, &vcqi);
        if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
        uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid);
        if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
        icep->tofa.ui64 = 0;
        icep->tofa.tank.tux = xyz[0];
        icep->tofa.tank.tuy = xyz[1];
        icep->tofa.tank.tuz = xyz[2];
        icep->tofa.tank.tua = xyz[3];
        icep->tofa.tank.tub = xyz[4];
        icep->tofa.tank.tuc = xyz[5];
        icep->tofa.tank.tni = tni[0];
        icep->tofa.tank.tcq = tcq[0];
        icep->tofa.tank.cid = cid[0];
        fprintf(stderr, "%d: YI****** self TOFU ADDR ******"
                " vcqh(0x%lx) %s = tofa.ui64=%lx in %s of %s\n",
                mypid, icep->vcqh,
                tank2string(buf, 128, icep->tofa.ui64), icep->tofa.ui64,
                __func__, __FILE__);
    }
    icep->nrma = 0;
    icep->enabled = 1;
    /*
     * Ask Hatanaka-san, what is the purpose of shadow ?
     */
    icep->shadow = icep;

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

void
ulib_ofif_icep_init(void *ptr, size_t off)
{
    struct ulib_icep *icep = (struct ulib_icep*) ((char*)ptr + off);
    icep->shadow = 0;
    icep->index = -1; /* III */
    icep->enabled = 0;
    icep->next = 0;
    icep->isep = 0; /* III */
    icep->ioav = 0;
    icep->vcqh = 0;
    icep->toqc = 0;
    DLST_INIT(&icep->busy_esnd);  /* head of busy list */
    icep->vp_tofu_scq = 0;
    icep->vp_tofu_rcq = 0;
    icep->uexp_fs = 0;
    dlist_init(&icep->uexp_list_trcv); /* unexpected queue for tagged msg. */
    dlist_init(&icep->uexp_list_mrcv); /* unexpected queue for msg. */
    icep->expd_fs = 0;
    dlist_init(&icep->expd_list_trcv); /* expeced queue */
    dlist_init(&icep->expd_list_mrcv); /* expeced queue */
    icep->udat_fs = 0;
    icep->desc_fs = 0;
    DLST_INIT(&icep->cash_list_desc);  /* desc_cash head */
    icep->tofa.ui64 = -1UL;
    return ;
}

int ulib_icep_close(void *ptr, size_t off)
{
    struct ulib_icep *icep = (struct ulib_icep *)((char*)ptr + off);
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    R_DBG0("Finalizing but sleep 200msec now... icep(%p)", icep);
    usleep(200*1000);
    R_DBG0("Wakeup and now going to shutdown the utofu provider(%p).", icep);

    if ((icep == 0) || (icep->isep == 0)) {
	uc = UTOFU_ERR_INVALID_ARG; RETURN_BAD_C(uc);
    }
    if (icep != icep->shadow) { /* YYY -- check if icep_peer is active */
	RETURN_OK_C(uc);
    }

    uc = ulib_shea_cbuf_fini(icep->cbufp);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    if (icep->toqc != 0) {
	uc = ulib_toqc_fini(icep->toqc);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	icep->toqc = 0;
    }
    if (icep->enabled != 0) {
	assert(icep->vcqh != 0); /* XXX : UTOFU_VCQ_HDL_NULL */
	uc = utofu_free_vcq( icep->vcqh );
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	icep->vcqh = 0; /* XXX */
	icep->enabled = 0;
    }
    /* unexpected entries */
    if (icep->uexp_fs != 0) {
	ulib_uexp_fs_free(icep->uexp_fs); icep->uexp_fs = 0;
    }
    /* expected entries */
    if (icep->expd_fs != 0) {
	ulib_expd_fs_free(icep->expd_fs); icep->expd_fs = 0;
    }
    /* transmit entries */
    if (icep->udat_fs != 0) {
	ulib_udat_fs_free(icep->udat_fs); icep->udat_fs = 0;
    }
    /* desc_cash */
    if (icep->desc_fs != 0) {
	ulib_desc_fs_free(icep->desc_fs); icep->desc_fs = 0;
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* FI_MR_ENDPOINT */
int ulib_imr__bind_icep(struct ulib_imr_ *imr_, struct ulib_icep *icep)
{
    int uc = UTOFU_SUCCESS;
    size_t loc = -1UL;

    ENTER_RC_C(uc);

    /* fi_mr_bind() */
    {
	size_t ii, ni = sizeof (imr_->ceps) / sizeof (imr_->ceps[0]);

	for (ii = 0; ii < ni; ii++) {
	    if (imr_->ceps[ii] == icep) {
		break;
	    }
	    if (imr_->ceps[ii] == 0) {
		if (loc == -1UL) { loc = ii; /* break; */ }
	    }
	}
	if (loc == -1UL) { uc = UTOFU_ERR_FULL; RETURN_BAD_C(uc); }

	if (imr_->ceps[loc] == 0) {
	    imr_->ceps[loc] = icep;
	}
    }
    /* fi_mr_enable() */
    {
	utofu_vcq_hdl_t vcqh = icep->vcqh;
	void *addr = imr_->addr;
	size_t size = imr_->size;
	unsigned int stag = imr_->stag;
	unsigned long int flag = 0
			    /* | UTOFU_REG_MEM_FLAG_READ_ONLY */
			    ;
	uint64_t offs;
	utofu_stadd_t stof[1];
	union ulib_tofa_u tofa_u;

	if (icep->enabled == 0) {
	    uc = UTOFU_ERR_NOT_PROCESSED; RETURN_BAD_C(uc);
	}

	offs = (uintptr_t)addr % 256;
	if (offs != 0) {
	    addr = (uint8_t *)addr - offs;
	    size += offs;
	}

	{
	    utofu_vcq_id_t vcqi = -1UL;
	    struct ulib_epnt_info i;

	    uc = utofu_query_vcq_id(vcqh, &vcqi);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	    uc = utofu_query_vcq_info(vcqi, i.xyz, i.tni, i.tcq, i.cid);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	    tofa_u.ui64 = 0;

	    uc = ulib_cast_epnt_to_tofa( &i, &tofa_u.tofa );
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	    uc = ulib_cast_stad_to_tofa( stag, offs, &tofa_u.tofa );
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	}

	uc = utofu_reg_mem_with_stag(vcqh, addr, size, stag, flag, stof);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	imr_->stas[loc] = stof[0];
	imr_->toas[loc] = tofa_u.ui64;
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_imr__close(struct ulib_imr_ *imr_)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    {
	size_t ii, ni = sizeof (imr_->ceps) / sizeof (imr_->ceps[0]);

	for (ii = 0; ii < ni; ii++) {
	    struct ulib_icep *icep;
	    utofu_vcq_hdl_t vcqh;
	    utofu_stadd_t stad;
	    const unsigned long int flag = 0
				/* | UTOFU_REG_MEM_FLAG_READ_ONLY */
				;

	    if ((icep = imr_->ceps[ii]) == 0) { continue; }

	    vcqh = icep->vcqh;
	    stad = imr_->stas[ii];

	    uc = utofu_dereg_mem(vcqh, stad, flag);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	    imr_->ceps[ii] = 0;
	    imr_->stas[ii] = 0;
	    imr_->toas[ii] = 0;
	}
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_icep_chck_caps(utofu_tni_id_t tni_id)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    {
	struct utofu_onesided_caps *caps = 0;
	uc = utofu_query_onesided_caps(tni_id, &caps);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	/* flags */
	if ((caps->flags & UTOFU_ONESIDED_CAP_FLAG_ARMW)
		== UTOFU_ONESIDED_CAP_FLAG_ARMW) {
	    /* (flag & ULIB_CAPS_1SDD_FLGS) == ULIB_CAPS_1SDD_FLGS) */
	}
	/* armw_ops */
	if ((caps->armw_ops & ( 0
	    | UTOFU_ONESIDED_CAP_ARMW_OP_CSWAP
	    | UTOFU_ONESIDED_CAP_ARMW_OP_SWAP
	    | UTOFU_ONESIDED_CAP_ARMW_OP_ADD
	    ))
	    == ( 0
	    | UTOFU_ONESIDED_CAP_ARMW_OP_CSWAP
	    | UTOFU_ONESIDED_CAP_ARMW_OP_SWAP
	    | UTOFU_ONESIDED_CAP_ARMW_OP_ADD
	)) {
	    /* (flag & ULIB_CAPS_ARMW_FLGS) == ULIB_CAPS_ARMW_FLGS) */
	}
	/* num_cmp_ids */
	if (caps->num_cmp_ids < 8 /* ULIB_CAPS_NCMP */) {
	}
	/* num_reserved_stags */
	if (caps->num_reserved_stags < 32 /* ULIB_CAPS_PROV_KEYS */) {
	}
	/* cache_line_size */
	/* stag_address_alignment */
	if (caps->stag_address_alignment > 256 /* ULIB_CAPS_STAG_ALGN */) {
	}
	/* max_toq_desc_size */
	if (caps->max_toq_desc_size > 64 /* ULIB_CAPS_DESC_SIZE */) {
	    /* see struct ulib_toqd_cash . desc[] in ulib_desc.h */
	}
	/* max_putget_size */
	/* max_piggyback_size */
	if (caps->max_piggyback_size < 32 /* ULIB_CAPS_PUTI_SIZE */) {
	}
	/* max_edata_size */
	if (caps->max_edata_size < 1 /* ULIB_CAPS_EDAT_SIZE */) {
	}
	/* max_mtu */
	/* max_gap */

    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int ulib_foo(struct ulib_icep *icep)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ======================================================================== */
#include <ofi.h>	    /* for container_of() */
#include <ofi_list.h>	    /* for dlist_+() */
#include <ofi_iov.h>	    /* for ofi_copy_to_iov() */

#include <rdma/fi_eq.h>	    /* for struct fi_cq_tagged_entry */

#include "ulib_shea.h"

/*
 * ulib_match_unexp:
 *      This function is called in order to find a message whose source and tag
 *      are matched with the tag of taged receive operation posted by the user.
 *
 *      The argument "item" is an unexpected message came from a sender.
 *      The argument "farg" is an entry of expected queue, receive requested.
 *      This function is called in order to find a message whose tag
 *      is matched with the tag of taged receive operation posted by the user.
 *      The receiver side has the ignore field.
 */
static inline int
ulib_match_unexp(struct dlist_entry *item, const void *farg)
{
    int ret;
    const struct ulib_shea_expd *expd = farg;
    struct ulib_shea_uexp *uexp;

    /* uexp: sender's data,  expd: posted entry */
    uexp = container_of(item, struct ulib_shea_uexp, entry);

    R_DBG0("MATCH with Unexpected: "
           "expd->addr(%x) expd->tag(0x%lx) expd->ignore(0x%lx) "
           "uexp->addr(%x) uexp->tag(0x%lx) uexp->data(0x%lx)",
           (uint32_t) expd->tmsg.addr, expd->tmsg.tag, expd->tmsg.ignore,
           uexp->srci, uexp->utag, uexp->idat);

    if (expd->tmsg.addr == -1UL) {
        ret = ((expd->tmsg.tag & ~expd->tmsg.ignore)
               == (uexp->utag & ~expd->tmsg.ignore));
    } else {
        ret = ((uint32_t) expd->tmsg.addr == uexp->idat)
            && ((expd->tmsg.tag & ~expd->tmsg.ignore)
                == (uexp->utag & ~expd->tmsg.ignore));
    }
#if 0
    ret = ((expd->tmsg.tag & ~expd->tmsg.ignore)
           == (uexp->utag & ~expd->tmsg.ignore));
#endif
    return ret;
}

/*
 * Matching send message with a message enqueued in unexpected queue
 */
static inline struct ulib_shea_uexp *
ulib_find_uexp_entry(struct ulib_icep *icep,
                     const struct ulib_shea_expd *expd)
{
    struct ulib_shea_uexp *rval = 0;
    struct dlist_entry *head;
    struct dlist_entry *match;

    assert(icep == icep->shadow);
    head = (expd->flgs & FI_TAGGED) ?
        &icep->uexp_list_trcv : &icep->uexp_list_mrcv;
    match = dlist_remove_first_match(head, ulib_match_unexp, expd);
    R_DBG0("tagmached(%p)", match);
    if (match == 0) {
	goto bad; /* XXX - is not an error */
    }
    rval = container_of(match, struct ulib_shea_uexp, entry);

bad:
    return rval;
}


static inline void ulib_icep_link_expd_head(
    struct ulib_icep *icep,
    struct ulib_shea_expd *expd
)
{
    int is_tagged_msg;
    struct dlist_entry *head;

    is_tagged_msg = ((expd->flgs & FI_TAGGED) != 0);
    if (is_tagged_msg) {
	head = &icep->expd_list_trcv;
    }
    else {
	head = &icep->expd_list_mrcv;
    }
    dlist_insert_head(&expd->entry, head);

    return ;
}

/*
 * Notify receive completion -- Creating completion entry
 *      Handling the FI_MULTI_RECV flag 2019/4/27
 *  called by
 *      ulib_icep_shea_send_prog()
 *      tofu_impl_ulib_sendmsg_self()
 *      ulib_icep_recv_call_back()
 *      ulib_icep_shea_recv_post()
 */
int
ulib_icqu_comp_trcv(void *vp_cq__priv,
                    const struct ulib_shea_expd *expd)
{
    int uc = UTOFU_SUCCESS;
    struct fi_cq_tagged_entry cq_e[1];

    cq_e->op_context	= expd->tmsg.context;
    cq_e->flags		=   FI_RECV
                            | (expd->flgs & FI_MULTI_RECV)
			    | (expd->flgs & FI_TAGGED)
			    | (expd->rflg & FI_REMOTE_CQ_DATA)
			    ;
    cq_e->len		= expd->wlen;
    cq_e->buf		= expd->iovs[0].iov_base;
    cq_e->data		= expd->idat; /* FI_REMOTE_CQ_DATA */
    cq_e->tag		= expd->rtag;

    fprintf(stderr, "YICHECK!!****: %s expd(%p)->flgs(0x%lx) cq_e(%p)->flags(0x%lx) cq_e->buf(%p) expd->iovs[0].iov_base(%p)\n", __func__, expd, expd->flgs, cq_e, cq_e->flags, cq_e->buf,  expd->iovs[0].iov_base); fflush(stderr);
    if (cq_e->buf) {
        R_DBG0("\tReceive Completion: flags(0x%lx) data(%ld) len(%ld) buf[]=%d",
               cq_e->flags, cq_e->data, cq_e->len, *(int*)cq_e->buf);
    } else {
        R_DBG0("\tReceive Completion: flags(0x%lx) data(%ld) len(%ld) buf=nil",
               cq_e->flags, cq_e->data, cq_e->len);
    }

    if (vp_cq__priv != 0) {
	int fc;
        /* enter the entry into CQ */
	fc = tofu_cq_comp_tagged(vp_cq__priv, cq_e);
	if (fc != 0) {
	    uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
	}
    }

bad:
    return uc;
}

/*
 * Notify send completion
 */
static inline int 
ulib_icqu_comp_tsnd(void *vp_cq__priv, const struct ulib_shea_data *udat)
{
    int uc = UTOFU_SUCCESS;
    struct fi_cq_tagged_entry cq_e[1];
    uint64_t dflg;
    uint64_t flgs;
    size_t tlen;

    dflg = ulib_shea_data_flag(udat);
    /* converted to fi_flags */
    flgs = ((dflg & ULIB_SHEA_DATA_TFLG) != 0)? FI_TAGGED: 0;
    flgs |= ((dflg & ULIB_SHEA_DATA_CFLG) != 0)? FI_COMPLETION: 0;
    /* tlen */
    {
	uint32_t nblk, llen;

	nblk = ulib_shea_data_nblk(udat);
	llen = ulib_shea_data_llen(udat);

	assert(nblk > 0);
	tlen = ((nblk - 1) * ULIB_SHEA_DBLK) + llen; /* YYY MACRO ? */
    }

    cq_e->op_context	= ulib_shea_data_ctxt(udat);
    cq_e->flags		=   FI_SEND
			    | flgs
			    ;
    cq_e->len		= tlen;
    cq_e->buf		= 0; /* YYY */
    cq_e->data		= 0;
    cq_e->tag		= ulib_shea_data_utag(udat);

    fprintf(stderr, "YISENDCMPLT****: %s in %s flags(0x%lx)\n", __func__, __FILE__, flgs); fflush(stderr);
    R_DBG0("\tSend Completion: flags(0x%lx)", cq_e->flags);

    if (flgs & FI_COMPLETION) {
        if (vp_cq__priv != 0) {
            int fc;
            fc = tofu_cq_comp_tagged(vp_cq__priv, cq_e);
            if (fc != 0) {
                uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
            }
        }
    }

bad:
    return uc;
}

#ifdef	DEBUG
static inline void ulib_icep_list_expd(
    struct ulib_icep *icep_ctxt,
    uint32_t flag
)
{
    struct ulib_icep *icep;
    int is_tagged_msg;
    struct dlist_entry *head, *curr;
    size_t nrcv = 0;

    assert(icep_ctxt != 0); icep = icep_ctxt->shadow; assert(icep != 0);
    is_tagged_msg = ((flag & ULIB_SHEA_UEXP_FLAG_TFLG) != 0);
    if (is_tagged_msg) {
	head = &icep->expd_list_trcv;
    }
    else {
	head = &icep->expd_list_mrcv;
    }

    dlist_foreach(head, curr) {
	struct ulib_shea_expd *trcv_curr;

	trcv_curr = container_of(curr, struct ulib_shea_expd, entry);
	printf("\t[%03ld] trcv %p %016"PRIx64" %016"PRIx64"\n",
	    nrcv, trcv_curr, trcv_curr->tmsg.tag, trcv_curr->tmsg.ignore);
	nrcv++;
    }

    return ;
}
#endif	/* DEBUG */

static inline size_t ulib_copy_iovs(
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

static inline void ulib_icep_recv_rbuf_base(
    struct ulib_icep *icep,
    const struct ulib_shea_uexp *rinf,
    struct iovec iovs[2]
)
{
    uint8_t *real_base;
    const struct ulib_shea_rbuf *rbuf = &rinf->rbuf;

    assert(icep == icep->shadow);
    assert(icep->cbufp->dptr != 0);
    real_base = icep->cbufp->dptr;

    /* iov_base : offs => base + offs */
    {
	uint32_t iiov;

	for (iiov = 0; iiov < rbuf->niov; iiov++) {
	    uintptr_t offs = (uintptr_t)rbuf->iovs[iiov].iov_base; /* XXX */

	    assert((offs + rbuf->iovs[iiov].iov_len) <= icep->cbufp->dsiz);
	    iovs[iiov].iov_base = real_base + offs;
	    iovs[iiov].iov_len  = rbuf->iovs[iiov].iov_len;
	}
    }

    return ;
}

int
ulib_icep_recv_frag(struct ulib_shea_expd *expd,
                    const struct ulib_shea_uexp *uexp)
{
    size_t      wlen;
    int         done;
    if ((uexp->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0) {
	expd->mblk  = uexp->mblk;
	expd->rtag  = uexp->utag;
	expd->rflg |= ((uexp->flag & ULIB_SHEA_UEXP_FLAG_TFLG) != 0)?
			FI_TAGGED: 0;
	expd->rflg |= ((uexp->flag & ULIB_SHEA_UEXP_FLAG_IFLG) != 0)?
			FI_REMOTE_CQ_DATA: 0;
	expd->idat  = uexp->idat; /* FI_REMOTE_CQ_DATA */
    }
    /* copy to the user buffer */
    wlen = ulib_copy_iovs(expd->iovs, expd->niov,expd->wlen,
                          uexp->rbuf.iovs, uexp->rbuf.niov);
    expd->wlen += wlen;
    expd->olen += (uexp->rbuf.leng - wlen);
    /* update nblk */
    if (uexp->rbuf.leng == 0) {
	expd->nblk += 1;
    } else {
	expd->nblk += uexp->nblk;
    }
    R_DBG0("\tfragment message handling nblk(%d) mblk(%d)", expd->mblk, expd->nblk);
    assert(expd->mblk != 0);
    assert(expd->nblk <= expd->mblk);
    done = (expd->mblk == expd->nblk);
    return done;
}

static inline int ulib_icep_recv_cbak_uexp( /* unexpected */
    struct ulib_icep *icep,
    const struct ulib_shea_uexp *rinf
)
{
    int uc = UTOFU_SUCCESS;
    struct ulib_shea_uexp *uexp;
    uint64_t tick[4];

    assert(icep == icep->shadow);
    tick[0] = ulib_tick_time();
    if (freestack_isempty(icep->uexp_fs)) {
/* printf("%s():%d\tuexp empty boff %u\n", __func__, __LINE__, rinf->boff); */
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
    uexp = freestack_pop(icep->uexp_fs);
    if (uexp == 0) {
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
if (0) {
printf("%s():%d\tuexp %3d\n", __func__, __LINE__,
ulib_uexp_fs_index(icep->uexp_fs, uexp));
}

    /* copy rinf */
    uexp[0] = rinf[0]; /* structure copy */
    /* copy rbuf */
    if (uexp->rbuf.leng > 0) {
	struct ulib_shea_rbuf *rbuf = &uexp->rbuf;
	size_t wlen;
	//struct iovec iovs[2];

	rbuf->iovs[0].iov_base = malloc(rbuf->leng);
	if (rbuf->iovs[0].iov_base == 0) {
	    uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
	}
	rbuf->iovs[0].iov_len = rbuf->leng;
	rbuf->niov = 1;

	//assert(sizeof (iovs) == sizeof (rinf->rbuf.iovs));
	//ulib_icep_recv_rbuf_base(icep, rinf, iovs);

	wlen = ulib_copy_iovs(
		rbuf->iovs, /* dst */
		rbuf->niov, /* dst */
		0, /* off */
		rinf->rbuf.iovs, /*iovs,*/ /* src */
		rinf->rbuf.niov /* src */
		);
	assert (wlen == rbuf->leng);
	if  (wlen != rbuf->leng) { /* YYY abort */ }
    }

    /* queue it */
    {
	struct dlist_entry *uexp_entry = &uexp->entry;

	dlist_init(uexp_entry);

	/* queue it */
	{
	    struct dlist_entry *head;
	    int is_tagged_msg;

	    is_tagged_msg = ((rinf->flag & ULIB_SHEA_UEXP_FLAG_TFLG) != 0);
	    if (is_tagged_msg) {
		head = &icep->uexp_list_trcv;
	    }
	    else {
		head = &icep->uexp_list_mrcv;
	    }
	    dlist_insert_tail(uexp_entry, head);
	}
	{   uint64_t tnow = ulib_tick_time();
	    uexp->tims[ 0 /* phdr */ ] += (tnow - tick[0]);
	}
    }

bad:
    return uc;
}

int
ulib_icep_recv_call_back(void *vptr,
                         int r_uc,
                         const void *vctx /* uexp */)
{
    int uc = 0;
    struct ulib_icep *icep = (struct ulib_icep *) vptr;
    const struct ulib_shea_uexp *uexp = vctx;
    struct ulib_shea_expd *trcv;
    int done;
    uint64_t tick[4];

    assert(icep == icep->shadow);
    tick[0] = ulib_tick_time();
    trcv = ulib_icep_find_expd(icep, uexp);

    if (trcv == 0) {
	uc = ulib_icep_recv_cbak_uexp(icep, uexp);
	if (uc != UTOFU_SUCCESS) { goto bad; }
	goto bad; /* XXX - is not an error */
    }

    assert(trcv->nblk == uexp->boff);
    if ((uexp->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0) {
        assert((uexp->nblk + uexp->boff) <= uexp->mblk);
    } else {
        assert((uexp->nblk + uexp->boff) <= trcv->mblk);
    }
    /*
     * iov_base : offs => base + offs
     */
    /* update trcv */
    done = ulib_icep_recv_frag(trcv, uexp);
    if (done == 0) {
        ulib_icep_link_expd_head(icep, trcv);
        goto bad; /* XXX - is not an error */
    }

    /* notify recv cq */
    if (icep->vp_tofu_rcq != 0) {
        uc = ulib_icqu_comp_trcv(icep->vp_tofu_rcq, trcv); /* expd */
        if (uc != 0) {
            assert(uc == UTOFU_SUCCESS); /* YYY recover ? */
            uc = 0; /* YYY severe error */
        }
    }
    freestack_push(icep->expd_fs, trcv); /* expd */

bad:
    return uc;
}

/*
 * The is used 2019/04/19
 */
int
ulib_icep_shea_recv_post(struct ulib_icep *icep_ctxt,
                         const struct fi_msg_tagged *tmsg,
                         uint64_t flags)
{
    int uc = 0;
    struct ulib_icep *icep;
    struct ulib_shea_expd *expd;
    struct ulib_shea_uexp *uexp;

    assert(icep_ctxt != 0); icep = icep_ctxt->shadow; assert(icep != 0);
    if (freestack_isempty(icep->expd_fs)) {
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
    expd = freestack_pop(icep->expd_fs);
    if (expd == 0) {
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }

    ulib_shea_expd_init(expd, tmsg, flags);
recheck:
    /* check unexpected queue */
    uexp = ulib_find_uexp_entry(icep, expd);
    if (uexp == NULL) {
        /* Not found a corresponding message in unexepcted queue,
         * thus insert this request to expected queue */
        //fprintf(stderr, "YIUTOFU****: %s insert request(%p) into expected queue on icep(%p)\n", __func__, expd, icep);
        //showCEP(icep);
        ulib_icep_link_expd(icep, expd);
    } else {
        int     done;
	/* update expd */
	done = ulib_icep_recv_frag(expd, uexp);
        fprintf(stderr, "YIMPICH***2: EXPD-BUF(%p)\n", expd->iovs[0].iov_base); fflush(stderr);
        /* free uexp->rbuf */
        tofu_imp_ulib_uexp_rbuf_free(uexp);
        /* free unexpected message */
        freestack_push(icep->uexp_fs, uexp);

        if (done == 0) {
            /* not yet received all fragments */
	    goto recheck;
        }
        /* notify recv cq */
        if (icep->vp_tofu_rcq != 0) {
            uc = ulib_icqu_comp_trcv(icep->vp_tofu_rcq, expd);
            if (uc != 0) {
                assert(uc == UTOFU_SUCCESS); /* YYY recover ? */
                uc = 0; /* YYY severe error */
            }
        }
        freestack_push(icep->expd_fs, expd);
    }

bad:
    return uc;
}

int
ulib_icep_shea_recv_prog(struct ulib_icep *icep_ctxt)
{
    int uc = 0;
    struct ulib_icep *icep;
    struct ulib_toqc *toqc;
    volatile struct ulib_shea_ercv *ercv;

    assert(icep_ctxt != 0); icep = icep_ctxt->shadow; assert(icep != 0);
    assert(icep->toqc != 0);
    toqc = icep->toqc;
    assert(icep->cbufp->cptr != 0);
    assert(icep->cbufp->cptr_ercv != 0);
    ercv = icep->cbufp->cptr_ercv;

    uc = ulib_shea_recv_hndr_prog(toqc, ercv, icep);
    if (uc != UTOFU_SUCCESS) {
	goto bad;
    }

bad:
    return uc;
}


#include "ulib_desc.h"	    /* for struct ulib_toqc_cash */

extern int  ulib_icep_find_desc(
		struct ulib_icep *icep,
		fi_addr_t dfia,
		struct ulib_toqc_cash **pp_cash_tmpl
	    );
extern void ulib_icep_free_cash(
		struct ulib_icep *icep,
		struct ulib_toqc_cash *cash_tmpl
	    );

/*
 * flags
 *                   fi_msg fi_tagged op_flags
 * FI_REMOTE_CQ_DATA      o         o
 * FI_COMPLETION          o         o FI_SELECTIVE_COMPLETION
 * FI_MORE                o         o
 * FI_INJECT              o         o
 * FI_MULTI_RECV          o       n/a
 * FI_INJECT_COMPLETE     o         o
 * FI_TRANSMIT_COMPLETE   o         o
 * FI_DELIVERY_COMPLETE   o       n/a
 * FI_FENCE               o         o
 * FI_MULTICAST           o       n/a
 */
int ulib_icep_shea_send_post(
    struct ulib_icep *icep_ctxt,
    const struct fi_msg_tagged *tmsg,
    uint64_t flags,
    void **vpp_send_hndl
)
{
    int uc = 0;
    struct ulib_icep *icep;
    struct ulib_shea_cbuf *cbuf;
    struct ulib_toqc_cash *cash_tmpl = 0;
    struct ulib_shea_data *udat = 0;
    struct ulib_shea_esnd *esnd = 0;

    //fprintf(stderr, "YIUTOFU***: %s enter\n", __func__);
    assert(icep_ctxt != 0); icep = icep_ctxt->shadow; assert(icep != 0);
    cbuf = icep->cbufp;
    /* flags */

    /* cash_tmpl */
    uc = ulib_icep_find_desc(icep, tmsg->addr, &cash_tmpl);
    fprintf(stderr, "\t\t:YI 1 uc(%d)\n", uc);
    if (uc != UTOFU_SUCCESS) { goto bad; }
    assert(cash_tmpl != 0);

    /* udat */
    udat = ulib_icep_shea_data_qget(icep);
    fprintf(stderr, "\t\t:YI 2 udat(%p)\n", udat);
    if (udat == 0) {
	ulib_icep_free_cash(icep, cash_tmpl);
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }

    /* tmsg.msg_iov and iov_count */
    {
	void *ctxt = tmsg->context;
	size_t tlen;
	uint32_t vpid; /* remote network address */
	const uint64_t utag = tmsg->tag;
	uint64_t idat;
	uint64_t flag = 0;

	tlen = ofi_total_iov_len(tmsg->msg_iov, tmsg->iov_count);

	vpid = cash_tmpl->vpid;

        flag |= (flags & FI_TAGGED) ? ULIB_SHEA_DATA_TFLG : 0;
        flag |= (flags & FI_COMPLETION) ? ULIB_SHEA_DATA_CFLG : 0;
	if ((flags & FI_REMOTE_CQ_DATA) != 0) {
	    flag |= ULIB_SHEA_DATA_IFLG;
	    idat = tmsg->data;
	} else {
	    idat = -2UL;
	}
        //printf("tlen %5ld vpid %3d utag %016"PRIx64"\n", tlen, vpid, utag);
	ulib_shea_data_init(udat, ctxt, tlen, vpid, utag, idat, flag);
    }
    /* data_init_cash */
    {
	struct ulib_toqc *toqc = icep->toqc;
	struct ulib_toqc_cash *cash_real = &udat->real;

#ifndef	CONF_ULIB_SHEA_DATA
	ulib_toqc_cash_init(cash_real, cash_tmpl->fi_a);
#else	/* CONF_ULIB_SHEA_DATA */
	/* cash_real lcsh (udat->real.addr[1]) is initialized in qget() */
	cash_real->fi_a = cash_tmpl->fi_a;
#endif	/* CONF_ULIB_SHEA_DATA */
	cash_real->vpid = cash_tmpl->vpid;

	assert(toqc != 0);
	ulib_shea_data_init_cash(udat,
	    toqc,
	    cash_tmpl,
	    cash_real
	);
    }
    /* tmsg.desc */
    if (tmsg->iov_count > 0) {
	utofu_stadd_t lsta = -1ULL;

	assert(tmsg->iov_count == 1); /* XXX at most one */
	if (tmsg->desc != 0) {
	    struct ulib_imr_ *imr_ = tmsg->desc[0]; /* XXX iov_count == 1 */
	    uint64_t rx_index;

	    /* rx_index */
	    {
		int index = icep->index;
		assert(index >= 0);
		assert(index < (sizeof (imr_->stas) / sizeof (imr_->stas[0])));
		rx_index = index;
	    }
	    if (imr_->ceps[rx_index] == 0) { /* YYY */
		/* enable ceps[rx_index] */
	    }
	    assert(imr_->ceps[rx_index] == icep);
	    lsta = imr_->stas[rx_index];
	}
	else if (tmsg->msg_iov[0].iov_len > 0) {
	    void *addr = tmsg->msg_iov[0].iov_base;
	    size_t size = tmsg->msg_iov[0].iov_len;
	    const unsigned long int flag = 0
					/* | UTOFU_REG_MEM_FLAG_READ_ONLY */
					;

	    uc = utofu_reg_mem(icep->vcqh, addr, size, flag, &lsta);
	    if (uc != UTOFU_SUCCESS) {
                fprintf(stderr, "\t\tYI %s(,%p,%ld,) = %d\n", "utofu_reg_mem", addr, size, uc);
fflush(stdout);
		ulib_icep_free_cash(icep, cash_tmpl);
		ulib_icep_shea_data_qput(icep, udat);
		goto bad;
	    }
	}
	/* update lcsh in cash_real */
	{
	    struct ulib_toqc_cash *cash_real = &udat->real;

	    assert(cash_real->addr[ 1 /* local */ ].vcqh == 0);
	    assert(cash_tmpl->addr[ 1 /* local */ ].vcqh != 0);
	    /* real lcsh */
	    cash_real->addr[1] = cash_tmpl->addr[1]; /* structure copy */
	    cash_real->addr[1].stad = lsta; /* overwrite */
	    cash_real->addr[1].stad_data = (tmsg->desc != 0)? -1ULL: lsta;
	}
    }

    esnd = ulib_shea_cbuf_esnd_qget(cbuf, udat);
    //fprintf(stderr, "YIUTOFU***: %s esnd(%p)\n", __func__, esnd);
    if (esnd == 0) {
	ulib_icep_free_cash(icep, cash_tmpl);
	ulib_icep_shea_data_qput(icep, udat);
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
#if 0
    printf("%s:%d\tbusy_esnd %p empt %d\n", __FILE__, __LINE__,
           &icep->busy_esnd, dlist_empty(&icep->busy_esnd));
    fflush(stdout);
#endif /* 0 */
    DLST_INST(&icep->busy_esnd, esnd, list);

    /* post */
    //fprintf(stderr, "YIUTOFU***: %s vpp_send_hdnl(%p)\n", __func__, esnd);
#if 0 /* commented out by YI */
    { /* added by YI */
        fprintf(stderr, "\t\t: goint to ulib_shea_foo0\n");
        uc = ulib_shea_foo0(esnd);
    }
#endif /* 0 */
    if (vpp_send_hndl) {
        vpp_send_hndl[0] = esnd; esnd = 0; /* ZZZ */
    }

bad:
    //fprintf(stderr, "YIUTOFU***: %s leave with uc(%d)\n", __func__, uc);
    return uc;
}

static inline void ulib_icep_shea_esnd_free(
    struct ulib_icep *icep,
    struct ulib_shea_esnd *esnd
)
{
    struct ulib_shea_data *udat_esnd = esnd->data; /* XXX */

    assert(icep == icep->shadow);
    if (udat_esnd != 0) {
	struct ulib_toqc_cash *cash;

	cash = ulib_shea_data_toqc_cash_tmpl(udat_esnd);
	if (cash != 0) {
	    ulib_icep_free_cash(icep, cash); /* TTT */
	}
	ulib_icep_shea_data_qput(icep, udat_esnd);
	/* esnd->data = 0; */
    }
    /* queue esnd to the freelist */
    {
	struct ulib_shea_cbuf *cbuf = icep->cbufp;

	DLST_RMOV(&icep->busy_esnd, esnd, list);
	ulib_shea_cbuf_esnd_qput(cbuf, esnd);
	/* esnd = 0; */
    }

    return ;
}

static inline int ulib_icep_toqc_sync(struct ulib_icep *icep, int ntry)
{
    int uc = UTOFU_SUCCESS;
    int ii, ni = ntry;

    assert(icep == icep->shadow);
    if (ni < 0) {
	ni = 1000;
    }

    for (ii = 0; ii < ni; ii++) {
	size_t pend_tcqd = 0;

	uc = ulib_toqc_chck_tcqd(icep->toqc, &pend_tcqd);
	if (uc != UTOFU_SUCCESS) { goto bad; }
	if (pend_tcqd == 0) { break; }

	uc = ulib_toqc_prog(icep->toqc);
	if (uc != UTOFU_SUCCESS) { goto bad; }
    }
    if (ii >= ni) { printf("%s:%d timed-out %d\n", __func__, __LINE__, ii); }

bad:
    return uc;
}

int ulib_icep_shea_send_prog(
    struct ulib_icep *icep_ctxt,
    void **vpp_send_hndl,
    uint64_t tims[16]
)
{
    int uc = UTOFU_SUCCESS;
    struct ulib_icep *icep;
    struct ulib_shea_esnd *esnd = (vpp_send_hndl == 0)? 0: vpp_send_hndl[0];

    assert(icep_ctxt != 0); icep = icep_ctxt->shadow; assert(icep != 0);
    if (esnd == 0) { /* all */
	DLST_DECH(ulib_head_esnd) *head = &icep->busy_esnd;
	struct dlist_entry *curr, *next;
	dlist_foreach_safe(head, curr, next) {
	    void *vp_esnd;

	    vp_esnd = container_of(curr, struct ulib_shea_esnd, list);
	    assert(vp_esnd != 0);
	    uc = ulib_icep_shea_send_prog(icep, &vp_esnd, tims);
	    if (uc != UTOFU_SUCCESS) { /* goto bad; */ }
	    if (vp_esnd == 0) { /* done */
		/* freed esnd */
	    }
	}
	goto bad; /* XXX - is not an error */
    }
    if (esnd->r_no != UINT64_MAX) {
	uint64_t c_no;

	uc = ulib_toqc_prog(icep->toqc);
	if (uc != UTOFU_SUCCESS) { goto bad; }

	uc = ulib_toqc_read(icep->toqc, &c_no);
	if (uc != UTOFU_SUCCESS) { goto bad; }

	if (c_no <= esnd->r_no) { /* completed ? */
	    goto bad; /* XXX - is not an error */
	}
	esnd->r_no = UINT64_MAX;
    }

    /* uc = ulib_shea_prog_send(esnd); */
    uc = ulib_shea_foo0(esnd);
    if (uc != UTOFU_SUCCESS) { goto bad; }

    if (esnd->r_no == UINT64_MAX) {
	if (esnd->l_st == SEND_DONE) {
	    uc = ulib_icep_toqc_sync(icep, 1000);
	    if (uc != UTOFU_SUCCESS) { goto bad; }
	    if (icep->vp_tofu_scq != 0) {
		uc = ulib_icqu_comp_tsnd(icep->vp_tofu_scq, esnd->data);
		if (uc != 0) {
		    assert(uc == UTOFU_SUCCESS); /* YYY recover ? */
		    uc = 0; /* YYY severe error */
		}
	    }
	    if (tims != 0) {
		struct ulib_shea_data *udat_esnd = esnd->data; /* XXX */

		memcpy(tims, udat_esnd->tims, sizeof (udat_esnd->tims));
	    }
	    ulib_icep_shea_esnd_free(icep, esnd);
	    vpp_send_hndl[0] = 0;
	    goto bad; /* XXX - is not an error */
	}
	switch (esnd->l_st) {
	case SEND_NORM:
	case SEND_EXCL:
	    do {
		if (esnd->d_st != DATA_FULL) {
		    break;
		}
		uc = ulib_icep_shea_recv_prog(icep);
		if (uc != UTOFU_SUCCESS) { goto bad; }
	    } while (0);
	    break;
	case WAIT_HEAD:
	case WAIT_NEXT:
	    break;
	default:
	    assert(esnd->r_no != UINT64_MAX);
	    break;
	}
	uc = ulib_icep_toqc_sync(icep, 32);
	if (uc != UTOFU_SUCCESS) { goto bad; }
    }

bad:
    return uc;
}
