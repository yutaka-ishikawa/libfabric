/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "ulib_conf.h"
#include "ulib_dlog.h"	    /* for ENTER_RC_C() */

#include "ulib_conv.h"	    /* for struct ulib_epnt_info */
#include "ulib_ofif.h"	    /* for struct ulib_isep */
#ifndef	notdef_icep_toqc
#include "ulib_toqc.h"	    /* for struct ulib_toqc */
#endif	/* notdef_icep_toqc */
#include "ulib_tick.h"	    /* for ulib_tick_time() */

#include "utofu.h"	    /* for UTOFU_SUCCESS */

#include "tofu_prv.h"	    /* for struct tofu_*() XXX */

#include <assert.h>	    /* for assert() */
#include <stdlib.h>	    /* for free() */
#include <stdio.h>	    /* for printf() */


int ulib_isep_open_tnis_info(struct ulib_isep *isep)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    {
	utofu_tni_id_t *tnis = 0;
	size_t ntni = 0, ni, nn;
	const size_t mtni = sizeof (isep->tnis) / sizeof (isep->tnis[0]);

	uc = utofu_get_onesided_tnis( &tnis, &ntni );
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	nn = ntni;
	if (nn > mtni) {
	    nn = mtni;
	}
	/* copy tnis[] and ntni */
	for (ni = 0; ni < nn; ni++) {
	    isep->tnis[ni] = tnis[ni];
	}
	isep->ntni = ntni;
	if (tnis != 0) {
	    assert(ntni > 0);
	    free(tnis); tnis = 0;
	}
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

int
ulib_icep_ctrl_enab(void *ptr, size_t off)
{
    int uc = UTOFU_SUCCESS;
    struct ulib_icep *icep = (struct ulib_icep*) ((char*)ptr + off);
    struct ulib_isep *isep;

    fprintf(stderr, "YI********** %s is CALLED\n", __func__); fflush(stderr);
    ENTER_RC_C(uc);

    if (icep == 0) {
	uc = UTOFU_ERR_INVALID_ARG; RETURN_BAD_C(uc);
    }
    /* XXX #include "tofu_prv.h" */
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
    if (icep->enabled != 0) {
	uc = UTOFU_ERR_BUSY; RETURN_BAD_C(uc);
    }

    fprintf(stderr, "YI*********** HERE in %s\n", __func__); fflush(stderr);
#ifdef	CONF_ULIB_OFI
    /* unexpected entries */
    if (icep->uexp_fs == 0) {
	/* YYY fi_rx_attr . total_buffered_recv and size ? */
	icep->uexp_fs = ulib_uexp_fs_create( 512, 0, 0 /* YYY */ );
	if (icep->uexp_fs == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}
    }
    fprintf(stderr, "YI*********** UEXP_FS(%p) in %s\n", icep->uexp_fs, __func__); fflush(stderr);
    /* expected entries */
    if (icep->expd_fs == 0) {
	/* YYY fi_rx_attr . size ? */
	icep->expd_fs = ulib_expd_fs_create( 512, 0, 0 /* YYY */ );
	if (icep->expd_fs == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}
    }
    if ((icep->icep_scq != 0) && (icep->icep_scq->cque == 0)) {
	/* YYY fi_cq_attr . size */
	icep->icep_scq->cque = ulib_ficq_fs_create( 512, 0, 0 /* YYY */ );
	if (icep->icep_scq->cque == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}
    }
    if ((icep->icep_rcq != 0) && (icep->icep_rcq->cque == 0)) {
	/* YYY fi_cq_attr . size */
	icep->icep_rcq->cque = ulib_ficq_fs_create( 512, 0, 0 /* YYY */ );
	if (icep->icep_rcq->cque == 0) {
	    uc = UTOFU_ERR_OUT_OF_MEMORY; RETURN_BAD_C(uc);
	}
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
#endif	/* CONF_ULIB_OFI */
    /* icep_ctrl_enab */
#ifndef	notdef_icep_toqc
    if (icep->vcqh == 0 /* YYY +_NULL */ )
#endif	/* notdef_icep_toqc */
    {
	utofu_vcq_hdl_t vcqh = 0;
	utofu_tni_id_t tni_id;
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

        fprintf(stderr, "YI******** CHECKCHECK tni_id(%d) %s\n", tni_id, __func__);
	uc = utofu_create_vcq_with_cmp_id(tni_id, c_id, flags, &vcqh);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	assert(vcqh != 0); /* XXX : UTOFU_VCQ_HDL_NULL */
	icep->vcqh = vcqh;
#ifdef	notdef_icep_toqc
	icep->enabled = 1;
#endif	/* notdef_icep_toqc */
    }
#ifndef	notdef_icep_toqc
    if (icep->toqc == 0) {
        uc = ulib_toqc_init(icep->vcqh, &icep->toqc);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
    if (icep->cbuf.csiz == 0) {
	uc = ulib_shea_cbuf_init(&icep->cbuf);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
    {
	const unsigned int ctag = 10, dtag = 11;
	const ulib_shea_ercv_cbak_f func = ulib_icep_recv_call_back;
	void *farg = icep;
	uc = ulib_shea_cbuf_enab(&icep->cbuf, icep->vcqh,
		ctag, dtag, func, farg);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
#endif	/* notdef_icep_toqc */
#ifndef	notdef_icep_toqc
    icep->enabled = 1;
#endif	/* notdef_icep_toqc */

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

void
ulib_ofif_icep_init(void *ptr, size_t off)
{
    struct ulib_icep *icep = (struct ulib_icep*) ((char*)ptr + off);
    icep->index = -1; /* III */
    icep->enabled = 0;
    icep->next = 0;
    icep->isep = 0; /* III */
    icep->ioav = 0;
    icep->vcqh = 0;
    icep->toqc = 0;
    icep->cbuf.csiz = 0;
    DLST_INIT(&icep->busy_esnd);  /* head of busy list */
    icep->icep_scq = 0;
    icep->icep_rcq = 0;
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

    if ((icep == 0) || (icep->isep == 0)) {
	uc = UTOFU_ERR_INVALID_ARG; RETURN_BAD_C(uc);
    }

#ifndef	notdef_icep_toqc
    uc = ulib_shea_cbuf_fini(&icep->cbuf);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    if (icep->toqc != 0) {
	uc = ulib_toqc_fini(icep->toqc);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	icep->toqc = 0;
    }
#endif	/* notdef_icep_toqc */
    if (icep->enabled != 0) {
	assert(icep->vcqh != 0); /* XXX : UTOFU_VCQ_HDL_NULL */
	uc = utofu_free_vcq( icep->vcqh );
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	icep->vcqh = 0; /* XXX */
	icep->enabled = 0;
    }
#ifdef	CONF_ULIB_OFI
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
#endif	/* CONF_ULIB_OFI */

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
#ifdef	CONF_ULIB_OFI

#include <ofi.h>	    /* for container_of() */
#include <ofi_list.h>	    /* for dlist_+() */
#include <ofi_iov.h>	    /* for ofi_copy_to_iov() */
#ifdef	CONF_ULIB_FICQ

#include <rdma/fi_eq.h>	    /* for struct fi_cq_tagged_entry */
#endif	/* CONF_ULIB_FICQ */

#include "ulib_shea.h"

static inline int ulib_match_rinf_entry(
    struct dlist_entry *item,
    const void *farg
)
{
    int ret;
    const struct ulib_shea_expd *trcv = farg;
    struct ulib_shea_uexp *rinf;

    rinf = container_of(item, struct ulib_shea_uexp, entry);
    ret =
	((trcv->tmsg.tag | trcv->tmsg.ignore)
	    == (rinf->utag | trcv->tmsg.ignore))
	;
    return ret;
}

/*
 * Matching send message with a message enqueued in unexpected queue
 */
static inline struct ulib_shea_uexp *
ulib_find_uexp_entry(struct ulib_icep *icep,
                     const struct ulib_shea_expd *trcv)
{
    struct ulib_shea_uexp *rval = 0;
    struct dlist_entry *head;
    struct dlist_entry *match;

    head = (trcv->flgs & FI_TAGGED) ?
        &icep->uexp_list_trcv : &icep->uexp_list_mrcv;
    match = dlist_remove_first_match(head, ulib_match_rinf_entry, trcv);
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
#ifndef	NDEBUG
    assert(dlist_empty(&expd->entry));
#endif	/* NDEBUG */
    dlist_insert_head(&expd->entry, head);

    return ;
}

static inline int ulib_icqu_comp_trcv(
    struct ulib_icqu *icqu,
    const struct ulib_shea_expd *expd
)
{
    int uc = UTOFU_SUCCESS;
#ifdef	CONF_ULIB_FICQ
    void *cent;
    struct fi_cq_tagged_entry *comp;

#ifdef	CONF_ULIB_PERF_SHEA
    if (expd->tmsg.context == (void *)31) { /* YYY */
#ifdef	notdef
	printf("/// perf trcv\n");
	printf("\t--- %s %12.9f\n", "post", ulib_tick_conv( expd->tims[ 0 ]));
	printf("\t--- %s %12.9f\n", "phdr", ulib_tick_conv( expd->tims[ 1 ]));
	printf("\t--- %s %12.9f\n", "expd", ulib_tick_conv( expd->tims[ 2 ]));
	printf("\t--- %s %12.9f\n", "uexp", ulib_tick_conv( expd->tims[ 3 ]));
#else	/* notdef */
	printf("// %9s %12s %12s %12s %12s perf trcv\n",
	    "leng", "post", "phdr", "expd", "uxpd");
	printf("%12u %12.9f %12.9f %12.9f %12.9f\n", expd->wlen,
	    ulib_tick_conv( expd->tims[ 0 ]),
	    ulib_tick_conv( expd->tims[ 1 ]),
	    ulib_tick_conv( expd->tims[ 2 ]),
	    ulib_tick_conv( expd->tims[ 3 ]));
#endif	/* notdef */
    }
#endif	/* CONF_ULIB_PERF_SHEA */
    /* fastlock_acquire( &icqu->cqu_lck ); */ /* YYY */
    cent = ulib_icqu_cque_pop(icqu);
    if (cent == 0) {
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
    comp = ulib_icqu_cent_comp_tagd(cent);

    comp->op_context	= expd->tmsg.context;
    comp->flags		=   FI_RECV
			    | FI_MULTI_RECV
			    | (expd->flgs & FI_TAGGED)
			    ;
    comp->len		= expd->wlen;
    comp->buf		= expd->tmsg.msg_iov[0].iov_base;
    comp->data		= 0;
    comp->tag		= expd->rtag;

    ulib_icqu_cque_enq(icqu, cent);

bad:
    /* fastlock_release( &icqu->cqu_lck ); */ /* YYY */
#endif	/* CONF_ULIB_FICQ */
    return uc;
}

static inline int ulib_icqu_comp_tsnd(
    struct ulib_icqu *icqu,
    const struct ulib_shea_data *udat
)
{
    int uc = UTOFU_SUCCESS;
#ifdef	CONF_ULIB_FICQ
    void *cent;
    struct fi_cq_tagged_entry *comp;
    uint64_t flgs;
    size_t tlen;

    /* fastlock_acquire( &icqu->cqu_lck ); */ /* YYY */
    cent = ulib_icqu_cque_pop(icqu);
    if (cent == 0) {
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
    comp = ulib_icqu_cent_comp_tagd(cent);

    /* FI_TAGGED */
    {
	uint64_t dflg = ulib_shea_data_flag(udat);

	flgs = ((dflg & ULIB_SHEA_DATA_TFLG) != 0)? FI_TAGGED: 0;
    }
    /* tlen */
    {
	uint32_t nblk, llen;

	nblk = ulib_shea_data_nblk(udat);
	llen = ulib_shea_data_nblk(udat);

	assert(nblk > 0);
	tlen = ((nblk - 1) * ULIB_SHEA_DBLK) + llen; /* YYY MACRO ? */
    }

    comp->op_context	= ulib_shea_data_ctxt(udat);
    comp->flags		=   FI_SEND
			    | flgs /* FI_TAGGED */
			    ;
    comp->len		= tlen;
    comp->buf		= 0; /* YYY */
    comp->data		= 0;
    comp->tag		= ulib_shea_data_utag(udat);

    ulib_icqu_cque_enq(icqu, cent);


bad:
    /* fastlock_release( &icqu->cqu_lck ); */ /* YYY */
#endif	/* CONF_ULIB_FICQ */
    return uc;
}
#ifdef	DEBUG

static inline void ulib_icep_list_expd(
    struct ulib_icep *icep,
    uint32_t flag
)
{
    int is_tagged_msg;
    struct dlist_entry *head, *curr;
    size_t nrcv = 0;

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

    assert(icep->cbuf.dptr != 0);
    real_base = icep->cbuf.dptr;

    /* iov_base : offs => base + offs */
    {
	uint32_t iiov;

	for (iiov = 0; iiov < rbuf->niov; iiov++) {
	    uintptr_t offs = (uintptr_t)rbuf->iovs[iiov].iov_base; /* XXX */

	    assert((offs + rbuf->iovs[iiov].iov_len) <= icep->cbuf.dsiz);
	    iovs[iiov].iov_base = real_base + offs;
	    iovs[iiov].iov_len  = rbuf->iovs[iiov].iov_len;
	}
    }

    return ;
}

#ifndef	NOTDEF_RECV_FRAG

static inline void ulib_icep_recv_frag(
    struct ulib_shea_expd *trcv,
    const struct ulib_shea_uexp *rinf
)
{
    if ((rinf->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0) {
	assert(trcv->nblk == 0);
	assert(trcv->mblk == 0);
	trcv->mblk  = rinf->mblk;
	trcv->rtag  = rinf->utag;
    }
    else {
	assert(trcv->nblk != 0);
	assert(trcv->mblk != 0);
	assert(trcv->rtag == rinf->utag);
    }
    assert(trcv->nblk == rinf->boff);
#ifndef	NDEBUG
    if ((rinf->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0) {
	assert((rinf->nblk + rinf->boff) <= rinf->mblk);
    }
    else {
	assert((rinf->nblk + rinf->boff) <= trcv->mblk);
    }
#endif	/* NDEBUG */
    /* copy to the user buffer */
    {
	size_t wlen;

	wlen = ulib_copy_iovs(
		trcv->iovs,
		trcv->niov,
		trcv->wlen,
		rinf->rbuf.iovs,
		rinf->rbuf.niov
		);
	trcv->wlen += wlen;
	assert(wlen <= rinf->rbuf.leng);
	trcv->olen += (rinf->rbuf.leng - wlen);
    }
    /* update nblk */
    if (rinf->rbuf.leng == 0) {
	assert((rinf->flag & ULIB_SHEA_UEXP_FLAG_ZFLG) != 0);
	assert((rinf->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0);
	assert(rinf->mblk == 1);
	assert(rinf->boff == 0);
	assert(rinf->nblk == 0);
	trcv->nblk += 1 /* rinf->nblk */;
    }
    else {
	trcv->nblk += rinf->nblk;
    }
if (0) {
printf("\ttrcv %p nblk %3u mblk %3u\n",
trcv, trcv->nblk, trcv->mblk);
}
if (0) {
printf("\ttrcv %p wlen %5u olen %5u\n",
trcv, trcv->wlen, trcv->olen);
}

    return ;
}
#endif	/* NOTDEF_RECV_FRAG */

static inline int ulib_icep_recv_cbak_uexp( /* unexpected */
    struct ulib_icep *icep,
    const struct ulib_shea_uexp *rinf
)
{
    int uc = UTOFU_SUCCESS;
    struct ulib_shea_uexp *uexp;
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tick[4];
#endif	/* CONF_ULIB_PERF_SHEA */

#ifdef	CONF_ULIB_PERF_SHEA
    tick[0] = ulib_tick_time();
#endif	/* CONF_ULIB_PERF_SHEA */
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
	struct iovec iovs[2];

	rbuf->iovs[0].iov_base = malloc(rbuf->leng);
	if (rbuf->iovs[0].iov_base == 0) {
	    uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
	}
	rbuf->iovs[0].iov_len = rbuf->leng;
	rbuf->niov = 1;

	assert(sizeof (iovs) == sizeof (rinf->rbuf.iovs));
	ulib_icep_recv_rbuf_base(icep, rinf, iovs);

	wlen = ulib_copy_iovs(
		rbuf->iovs, /* dst */
		rbuf->niov, /* dst */
		0, /* off */
		iovs, /* src */
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
#ifdef	CONF_ULIB_PERF_SHEA
	{   uint64_t tnow = ulib_tick_time();
	    uexp->tims[ 0 /* phdr */ ] += (tnow - tick[0]);
	}
#endif	/* CONF_ULIB_PERF_SHEA */
    }

bad:
    return uc;
}

int
ulib_icep_recv_call_back(void *farg, /* icep */
                         int r_uc,
                         const void *vctx /* rinf */)
{
    int uc = 0;
    struct ulib_icep *icep = farg;
    const struct ulib_shea_uexp *rinf = vctx;
    struct ulib_shea_expd *trcv;
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tick[4];
#endif	/* CONF_ULIB_PERF_SHEA */

#ifdef	CONF_ULIB_PERF_SHEA
    tick[0] = ulib_tick_time();
#endif	/* CONF_ULIB_PERF_SHEA */
    trcv = ulib_icep_find_expd(icep, rinf);
    if (trcv == 0) {
if (0) {
printf("%s():%d\tuexp\n", __func__, __LINE__);
}
	uc = ulib_icep_recv_cbak_uexp(icep, rinf);
	if (uc != UTOFU_SUCCESS) { goto bad; }
	goto bad; /* XXX - is not an error */
    }
if (0) {
printf("\ttrcv %p utag %016"PRIx64" %c%c\n",
trcv, rinf->utag,
((rinf->flag & ULIB_SHEA_UEXP_FLAG_TFLG) != 0)? 'T': '-',
((rinf->flag & ULIB_SHEA_UEXP_FLAG_ZFLG) != 0)? 'Z': '-');
}
    if (trcv != 0) {
#ifdef	NOTDEF_RECV_FRAG
	assert(trcv->nblk == rinf->boff);
#ifndef	NDEBUG
	if ((rinf->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0) {
	    assert((rinf->nblk + rinf->boff) <= rinf->mblk);
	}
	else {
	    assert((rinf->nblk + rinf->boff) <= trcv->mblk);
	}
#endif	/* NDEBUG */
	/* copy to the user buffer */
	{
	    size_t wlen;

	    wlen = ulib_copy_iovs(
		    trcv->iovs,
		    trcv->niov,
		    trcv->wlen,
		    rinf->rbuf.iovs,
		    rinf->rbuf.niov
		    );
	    trcv->wlen += wlen;
	    assert(wlen <= rinf->rbuf.leng);
	    trcv->olen += (rinf->rbuf.leng - wlen);
	}
	/* check if the packet is a last fragment */
	{
	    if ((rinf->flag & ULIB_SHEA_UEXP_FLAG_MBLK) != 0) {
		assert(trcv->mblk == 0);
		trcv->mblk  = rinf->mblk;
		assert(trcv->nblk == 0);
		trcv->rtag  = rinf->utag;
	    }
	    else {
		assert(trcv->mblk != 0);
		assert(trcv->rtag == rinf->utag);
	    }
	    trcv->nblk += rinf->nblk;
if (0) {
printf("\ttrcv %p nblk %3u mblk %3u\n",
trcv, trcv->nblk, trcv->mblk);
}
if (0) {
printf("\ttrcv %p wlen %5u olen %5u\n",
trcv, trcv->wlen, trcv->olen);
}
	    if (trcv->nblk < trcv->mblk) {
		ulib_icep_link_expd_head(icep, trcv);
		goto bad; /* XXX - is not an error */
	    }
	}
#else	/* NOTDEF_RECV_FRAG */
	/* XXX iov_base : offs => base + offs */
	ulib_icep_recv_rbuf_base(icep, rinf, (struct iovec *)rinf->rbuf.iovs);

	/* update trcv */
	ulib_icep_recv_frag(trcv, rinf);
if (0) {
printf("%s():%d\ttrcv %3d %s\n", __func__, __LINE__,
ulib_expd_fs_index(icep->expd_fs, trcv),
(trcv->nblk < trcv->mblk)? "frag": "comp");
}
	/* check if the packet is a last fragment */
	if (trcv->nblk < trcv->mblk) {
	    ulib_icep_link_expd_head(icep, trcv);
#ifdef	CONF_ULIB_PERF_SHEA
	    {   uint64_t tnow = ulib_tick_time();
		trcv->tims[ 2 /* expd */ ] += (tnow - tick[0]);
		trcv->tims[ 1 /* phdr */ ] += rinf->tims[0];
	    }
#endif	/* CONF_ULIB_PERF_SHEA */
	    goto bad; /* XXX - is not an error */
	}
#ifdef	CONF_ULIB_PERF_SHEA
	{   uint64_t tnow = ulib_tick_time();
	    trcv->tims[ 2 /* expd */ ] += (tnow - tick[0]);
	    trcv->tims[ 1 /* phdr */ ] += rinf->tims[0];
	}
#endif	/* CONF_ULIB_PERF_SHEA */
	/* notify recv cq */
	if (icep->icep_rcq != 0) {
	    uc = ulib_icqu_comp_trcv(icep->icep_rcq, trcv); /* expd */
	    if (uc != 0) {
		assert(uc == UTOFU_SUCCESS); /* YYY recover ? */
		uc = 0; /* YYY severe error */
	    }
	}
	freestack_push(icep->expd_fs, trcv); /* expd */
#endif	/* NOTDEF_RECV_FRAG */
    }

bad:
    return uc;
}

int ulib_icep_shea_recv_post(
    struct ulib_icep *icep,
    const struct fi_msg_tagged *tmsg,
    uint64_t flags
)
{
    int uc = 0;
    struct ulib_shea_expd *expd;
    struct ulib_shea_uexp *uexp;
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tick[4], uexp_time = 0, phdr_time = 0;
#endif	/* CONF_ULIB_PERF_SHEA */

#ifdef	CONF_ULIB_PERF_SHEA
    tick[0] = ulib_tick_time();
#endif	/* CONF_ULIB_PERF_SHEA */
    if (freestack_isempty(icep->expd_fs)) {
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
    expd = freestack_pop(icep->expd_fs);
    if (expd == 0) {
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }

    ulib_shea_expd_init(expd, tmsg, flags);

    /* check unexpected queue */
    do {
#ifdef	CONF_ULIB_PERF_SHEA
	tick[1] = ulib_tick_time();
#endif	/* CONF_ULIB_PERF_SHEA */
	uexp = ulib_find_uexp_entry(icep, expd);
	if (uexp == 0) { break; }
#ifdef	CONF_ULIB_PERF_SHEA
	phdr_time += uexp->tims[0];
#endif	/* CONF_ULIB_PERF_SHEA */

	/* update expd */
	ulib_icep_recv_frag(expd, uexp /* rinf */ );

	/* free uexp->rbuf */
	{
	    struct ulib_shea_rbuf *rbuf = &uexp->rbuf;
	    uint32_t iiov;

	    for (iiov = 0; iiov < rbuf->niov; iiov++) {
		if (rbuf->iovs[iiov].iov_len == 0) { continue; }
		if (rbuf->iovs[iiov].iov_base != 0) {
		    free(rbuf->iovs[iiov].iov_base); /* YYY */
		    /* rbuf->iovs[iiov].iov_base = 0; */
		}
		/* rbuf->iovs[iiov].iov_len = 0; */
	    }
	    rbuf->niov = 0;
	}
if (0) {
printf("%s():%d\tuexp %3d\n", __func__, __LINE__,
ulib_uexp_fs_index(icep->uexp_fs, uexp));
}
	/* free uexp */
	freestack_push(icep->uexp_fs, uexp);

	/* check if the packet is a last fragment */
	if ((expd->mblk != 0) && (expd->nblk >= expd->mblk)) {
	    assert(expd->nblk == expd->mblk);
#ifdef	CONF_ULIB_PERF_SHEA
	    {   uint64_t tnow = ulib_tick_time(), tdif = tnow - tick[1];
		uexp_time += tdif;
		expd->tims[ 0 /* post */ ] += ((tnow - tick[0]) - uexp_time);
		expd->tims[ 3 /* uexp */ ] += uexp_time;
		expd->tims[ 1 /* phdr */ ] += phdr_time;
	    }
#endif	/* CONF_ULIB_PERF_SHEA */
if (0) {
printf("%s():%d\tdone %3d ctx %ld\n", __func__, __LINE__,
ulib_expd_fs_index(icep->expd_fs, expd),
(long)(uintptr_t)expd->tmsg.context);
}
	    /* notify recv cq */
	    if (icep->icep_rcq != 0) {
		uc = ulib_icqu_comp_trcv(icep->icep_rcq, expd);
		if (uc != 0) {
		    assert(uc == UTOFU_SUCCESS); /* YYY recover ? */
		    uc = 0; /* YYY severe error */
		}
	    }
	    freestack_push(icep->expd_fs, expd);
	    goto bad; /* XXX - is not an error */
	}
#ifdef	CONF_ULIB_PERF_SHEA
	{   uint64_t tnow = ulib_tick_time(), tdif = tnow - tick[1];
	    uexp_time += tdif;
	}
#endif	/* CONF_ULIB_PERF_SHEA */
    } while (1);

if (0) {
printf("%s():%d\tqued %3d\n", __func__, __LINE__,
ulib_expd_fs_index(icep->expd_fs, expd));
}
    /* queue it */
    ulib_icep_link_expd(icep, expd);
#ifdef	CONF_ULIB_PERF_SHEA
    {   uint64_t tnow = ulib_tick_time();
	expd->tims[ 0 /* post */ ] += ((tnow - tick[0]) - uexp_time);
	expd->tims[ 1 /* phdr */ ] += phdr_time;
	expd->tims[ 3 /* uexp */ ] += uexp_time;
    }
#endif	/* CONF_ULIB_PERF_SHEA */

bad:
    return uc;
}

int ulib_icep_shea_recv_prog(
    struct ulib_icep *icep
)
{
    int uc = 0;
    struct ulib_toqc *toqc;
    struct ulib_shea_ercv *ercv;

    assert(icep->toqc != 0);
    toqc = icep->toqc;
    assert(icep->cbuf.cptr != 0);
    assert(icep->cbuf.cptr_ercv != 0);
    ercv = icep->cbuf.cptr_ercv;

    uc = ulib_shea_recv_hndr_prog(toqc, ercv);
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
    struct ulib_icep *icep,
    const struct fi_msg_tagged *tmsg,
    uint64_t flags,
    void **vpp_send_hndl
)
{
    int uc = 0;
    struct ulib_shea_cbuf *cbuf = &icep->cbuf;
    struct ulib_toqc_cash *cash_tmpl = 0;
    struct ulib_shea_data *udat = 0;
    struct ulib_shea_esnd *esnd = 0;

    /* flags */

    /* cash_tmpl */
    uc = ulib_icep_find_desc(icep, tmsg->addr, &cash_tmpl);
    if (uc != UTOFU_SUCCESS) { goto bad; }
    assert(cash_tmpl != 0);

    /* udat */
    udat = ulib_icep_shea_data_qget(icep);
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
	const uint64_t flag = ULIB_SHEA_DATA_TFLG;

	tlen = ofi_total_iov_len(tmsg->msg_iov, tmsg->iov_count);

	vpid = cash_tmpl->vpid;

// printf("tlen %5ld vpid %3d utag %016"PRIx64"\n", tlen, vpid, utag);
	ulib_shea_data_init(udat, ctxt, tlen, vpid, utag, flag);
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
#ifdef	CONF_ULIB_SHEA_DATA
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
printf("%s(,%p,%ld,) = %d\n", "utofu_reg_mem", addr, size, uc);
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
#endif	/* CONF_ULIB_SHEA_DATA */

    esnd = ulib_shea_cbuf_esnd_qget(cbuf, udat);
    if (esnd == 0) {
	ulib_icep_free_cash(icep, cash_tmpl);
	ulib_icep_shea_data_qput(icep, udat);
	uc = UTOFU_ERR_OUT_OF_RESOURCE; goto bad;
    }
    DLST_INST(&icep->busy_esnd, esnd, list);

    /* post */
    if (vpp_send_hndl) {
        vpp_send_hndl[0] = esnd; esnd = 0; /* ZZZ */
    }

bad:
    return uc;
}

static inline void ulib_icep_shea_esnd_free(
    struct ulib_icep *icep,
    struct ulib_shea_esnd *esnd
)
{
    struct ulib_shea_data *udat_esnd = esnd->data; /* XXX */

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
	struct ulib_shea_cbuf *cbuf = &icep->cbuf;

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
    struct ulib_icep *icep,
    void **vpp_send_hndl,
    uint64_t tims[16]
)
{
    int uc = UTOFU_SUCCESS;
    struct ulib_shea_esnd *esnd = (vpp_send_hndl == 0)? 0: vpp_send_hndl[0];

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
#ifdef	CONF_ULIB_UTOF_FIX5
	    uc = ulib_icep_toqc_sync(icep, 1000);
	    if (uc != UTOFU_SUCCESS) { goto bad; }
#endif	/* CONF_ULIB_UTOF_FIX5 */
	    if (icep->icep_scq != 0) {
		uc = ulib_icqu_comp_tsnd(icep->icep_scq, esnd->data);
		if (uc != 0) {
		    assert(uc == UTOFU_SUCCESS); /* YYY recover ? */
		    uc = 0; /* YYY severe error */
		}
	    }
#ifdef	CONF_ULIB_PERF_SHEA
	    if (tims != 0) {
		struct ulib_shea_data *udat_esnd = esnd->data; /* XXX */

		memcpy(tims, udat_esnd->tims, sizeof (udat_esnd->tims));
	    }
#endif	/* CONF_ULIB_PERF_SHEA */
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

int ulib_icqu_read(
    struct ulib_icqu *icqu,
    void *buf,
    size_t mcnt,
    size_t *ncnt
)
{
    int uc = UTOFU_SUCCESS;
    size_t icnt;
    struct fi_cq_tagged_entry *comp_dst = buf;

    ENTER_RC_C(uc);

    if (icqu->cque == 0) { /* ! enabled */
	ncnt[0] = 0;
	uc = UTOFU_ERR_NOT_AVAILABLE; RETURN_IGN_C(uc);
    }
    /* fastlock_acquire( &icqu->cqu_lck ); */ /* YYY */
    for (icnt = 0; icnt < mcnt; icnt++) {
	void *cent;
	const struct fi_cq_tagged_entry *comp_src;

	cent = ulib_icqu_cque_peek(icqu);
	if (cent == 0) { break; }

	comp_src = ulib_icqu_cent_comp_tagd(cent);

	/* copy */
	comp_dst[icnt] = comp_src[0];

	ulib_icqu_cque_deq_push(icqu, cent);
    }
    /* fastlock_release( &icqu->cqu_lck ); */ /* YYY */
    ncnt[0] = icnt;
    if ((mcnt > 0) && (icnt == 0)) {
	uc = UTOFU_ERR_NOT_AVAILABLE; RETURN_IGN_C(uc);
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}
#endif	/* CONF_ULIB_OFI */
