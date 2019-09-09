/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_debug.h"
#include "ulib_conf.h"
#include "ulib_dlog.h"	    /* for ENTER_RC_C() */

#include "ulib_conv.h"	    /* for union ulib_tofa_u */
#include "ulib_desc.h"	    /* for struct ulib_utof_cash */

#include "utofu.h"	    /* for UTOFU_SUCCESS */

#include <assert.h>	    /* for assert() */
#include <string.h>	    /* for memset() */


static inline int ulib_conv_vcqi_from_tofa(
    uint64_t taui,	    /* IN */
    utofu_cmp_id_t c_id,    /* IN */
    utofu_vcq_id_t *p_vcqi  /* OUT */
)
{
    int uc = UTOFU_SUCCESS;
    union ulib_tofa_u ra_u = { .ui64 = taui, };
    uint8_t txyz[8]; /* xyz + abc */
    utofu_tni_id_t tnid;
    utofu_cq_id_t cqid;

    ENTER_RC_C(uc);

    txyz[0] = ra_u.tofa.tux;
    txyz[1] = ra_u.tofa.tuy;
    txyz[2] = ra_u.tofa.tuz;
    txyz[3] = ra_u.tofa.tua;
    txyz[4] = ra_u.tofa.tub;
    txyz[5] = ra_u.tofa.tuc;

    tnid    = ra_u.tofa.tni;
    cqid    = ra_u.tofa.tcq;

    uc = utofu_construct_vcq_id(txyz, tnid, cqid, c_id, p_vcqi);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline int ulib_conv_stad_from_tofa(
    uint64_t taui,
    utofu_vcq_id_t vcqi,
    uint64_t offs,
    utofu_stadd_t *p_stad
)
{
    int uc = UTOFU_SUCCESS;
    union ulib_tofa_u ra_u = { .ui64 = taui, };

    ENTER_RC_C(uc);

    {
	unsigned int stag = ra_u.tofa.tag;
	utofu_stadd_t stab;

	uc = utofu_query_stadd(vcqi, stag, &stab);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

	p_stad[0] = stab + ra_u.tofa.off + offs;
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline int ulib_conv_paid(
    utofu_vcq_id_t vcqi,
    utofu_path_id_t *p_paid
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    /* path id. (paid) */
    {
	uint8_t txyz[6], *tabc;

	uc = utofu_query_my_coords(txyz);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	tabc = &txyz[3];

	uc = utofu_get_path_id(vcqi, tabc, p_paid);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }
#ifdef	NOTYET_JTOF
    {
	int jc;
union jtofu_phys_coords {
struct { uint8_t x; uint8_t y; uint8_t z; uint8_t a; uint8_t b; uint8_t c; } s;
uint8_t a[6];
};

union jtofu_path_coords {
struct { uint8_t a; uint8_t b; uint8_t c; } s;
uint8_t a[3];
};

	jc = jtofu_query_onesided_paths(
		union jtofu_phys_coords *jxyz,
		size_t m_pa,
		union jtofu_path_coords *jabc,
		size_t *n_pa
	    );
	if (jc != JTOFU_SUCCESS) {
	    uc = UTOFU_ERROR; RETURN_BAD_C(uc);
	}
    }
#endif	/* NOTYET_JTOF */

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline int ulib_vcqh_from_tofa(
    uint64_t taui,
    void *epnt,
    utofu_vcq_hdl_t *p_vcqh
)
{
    int uc = UTOFU_SUCCESS;
    // union ulib_tofa_u ra_u = { .ui64 = taui, };

    ENTER_RC_C(uc);

    /* vcqh */
    {
	p_vcqh[0] = 0; /* XXX */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

static inline int ulib_utof_cash_local(
    uint64_t laui,
    utofu_cmp_id_t c_id,
    void *epnt,
    struct ulib_utof_cash *cash
)
{
    int uc = UTOFU_SUCCESS;
    utofu_vcq_id_t vcqi = -1UL;
    const uint64_t offs = 0;
    utofu_stadd_t stad = -1UL;
    utofu_vcq_hdl_t vcqh = 0;

    ENTER_RC_C(uc);

    /* vcqi */
    uc = ulib_conv_vcqi_from_tofa( laui, c_id, &vcqi );
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* stad */
    uc = ulib_conv_stad_from_tofa( laui, vcqi, offs, &stad );
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* vcqh */
    uc = ulib_vcqh_from_tofa( laui, epnt, &vcqh );
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* return cache */
    {
	cash->vcqi = vcqi;
	cash->stad = stad;
#ifdef	CONF_ULIB_SHEA_DATA
	cash->stad_data = -1ULL;
#endif	/* CONF_ULIB_SHEA_DATA */
	cash->paid = (utofu_path_id_t)-1U;
	cash->vcqh = vcqh;
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_utof_cash_remote(
    uint64_t raui,
    utofu_cmp_id_t c_id,
    void *epnt,
    struct ulib_utof_cash *cash
)
{
    int uc = UTOFU_SUCCESS;
    utofu_vcq_id_t vcqi = -1UL;
    const uint64_t offs = 0;
    utofu_stadd_t stad = -1UL;
    utofu_path_id_t paid = (utofu_path_id_t)-1U;

    ENTER_RC_C(uc);

    /* vcqi */
    uc = ulib_conv_vcqi_from_tofa( raui, c_id, &vcqi );
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* stad */
    uc = ulib_conv_stad_from_tofa( raui, vcqi, offs, &stad );
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* paid */
    uc = ulib_conv_paid( vcqi, &paid );
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* return cache */
    {
	cash->vcqi = vcqi;
	cash->stad = stad;
#ifdef	CONF_ULIB_SHEA_DATA
	cash->stad_data = -1ULL;
#endif	/* CONF_ULIB_SHEA_DATA */
	cash->paid = paid;
	cash->vcqh = 0;
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ======================================================================== */

static inline int ulib_ackd_get1(
    struct utofu_mrq_notice *ackd,
    utofu_vcq_id_t rcqi,
    utofu_stadd_t rsta,
    utofu_stadd_t lsta,
    uint64_t leng,
    uint64_t edat,
    utofu_vcq_hdl_t vcqh,
    unsigned long flag
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    assert(ackd != 0);
    {
	ackd->notice_type = UTOFU_MRQ_TYPE_LCL_GET;
	/* ackd->padding1[0] = 0; */
	ackd->vcq_id = rcqi;
	ackd->edata = edat;
	ackd->lcl_stadd = lsta + leng;
	ackd->rmt_stadd = rsta + leng;
#ifndef	NDEBUG
	ackd->rmt_value = leng; /* XXX */
	ackd->reserved[0] = vcqh; /* XXX */
#endif	/* NDEBUG */
	ackd->reserved[1] = flag; /* XXX */
    }
    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_get1_from_cash(
    const struct ulib_utof_cash *rcsh,
    const struct ulib_utof_cash *lcsh,
    uint64_t roff,
    uint64_t loff,
    uint64_t leng,
    uint64_t edat,
    unsigned long uflg,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    utofu_stadd_t lsta, rsta;
    unsigned long flag;

    ENTER_RC_C(uc);

    /* lsta */
    lsta = lcsh->stad + loff;
    /* rsta */
    rsta = rcsh->stad + roff;

    /* flag */
    flag =  uflg
	    | UTOFU_ONESIDED_FLAG_PATH(rcsh->paid)
	    ;

    /* desc */
    {
	uint64_t *desc_spc = toqd->desc;

	uc = utofu_prepare_get(lcsh->vcqh, rcsh->vcqi,
		lsta, rsta, leng, edat, flag,
		desc_spc, &toqd->size);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    uc = ulib_ackd_get1(toqd->ackd, rcsh->vcqi, rsta, lsta, leng, edat,
	    lcsh->vcqh, flag);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_get1_from_tofa(
    uint64_t raui,
    uint64_t laui,
    uint64_t roff,
    uint64_t loff,
    uint64_t leng,
    uint64_t edat,
    unsigned long uflg,
    void *epnt,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    const utofu_cmp_id_t c_id = CONF_ULIB_CMP_ID;
    struct ulib_utof_cash lcsh[1], rcsh[1];

    ENTER_RC_C(uc);

    /* lcsh */
    uc = ulib_utof_cash_local( laui, c_id, epnt, lcsh );
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* rcsh */
    uc = ulib_utof_cash_remote( raui, c_id, epnt, rcsh );
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    /* desc and ackd */
    uc = ulib_desc_get1_from_cash(rcsh, lcsh, roff, loff, leng, edat,
	    uflg,
	    toqd);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ======================================================================== */

static inline int ulib_ackd_put1(
    struct utofu_mrq_notice *ackd,
    utofu_vcq_id_t rcqi,
    utofu_stadd_t rsta,
    utofu_stadd_t lsta,
    uint64_t leng,
    uint64_t edat,
    utofu_vcq_hdl_t vcqh,
    unsigned long flag
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    assert(ackd != 0);
    {
	ackd->notice_type = UTOFU_MRQ_TYPE_LCL_PUT;
	/* ackd->padding1[0] = 0; */
	ackd->vcq_id = rcqi;
	ackd->edata = edat;
	/* ackd->lcl_stadd = 0; */
	ackd->rmt_stadd = rsta + leng;
#ifndef	NDEBUG
	ackd->rmt_value = leng; /* XXX */
	ackd->reserved[0] = vcqh; /* XXX */
#endif	/* NDEBUG */
	ackd->reserved[1] = flag; /* XXX */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_put1_from_cash_data(
    const struct ulib_utof_cash *rcsh,
    const struct ulib_utof_cash *lcsh,
    uint64_t roff,
    uint64_t loff,
    uint64_t leng,
    uint64_t edat,
    unsigned long uflg,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    utofu_stadd_t lsta, rsta;
    unsigned long flag;

    ENTER_RC_C(uc);

    /* lsta */
    lsta = lcsh->stad + loff;
    /* rsta */
    rsta = rcsh->stad_data + roff;

    /* flag */
    flag =  uflg
	    | UTOFU_ONESIDED_FLAG_PATH(rcsh->paid)
/*            | UTOFU_ONESIDED_FLAG_CACHE_INJECTION */
	    ;
/*    R_DBG0(RDBG_LEVEL1, "cache injection added flag(0x%lx)", flag);*/
    /*R_DBG0(RDBG_LEVEL1, "PUT: vcqi(%lx) rsta(%lx) roff(%lx) leng(%lx) edat(%lx) flag(%lx)",
      rcsh->vcqi, rsta, roff, leng, edat, flag);*/

    /* desc */
    {
	uint64_t *desc_spc = toqd->desc;

	uc = utofu_prepare_put(lcsh->vcqh, rcsh->vcqi,
		lsta, rsta, leng, edat, flag,
		desc_spc, &toqd->size);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    uc = ulib_ackd_put1(toqd->ackd, rcsh->vcqi, rsta, lsta, leng, edat,
	    lcsh->vcqh, flag);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ======================================================================== */

static inline int ulib_ackd_puti_le08(
    struct utofu_mrq_notice *ackd,
    utofu_vcq_id_t rcqi,
    utofu_stadd_t rsta,
    uint64_t lval,
    uint64_t leng,
    uint64_t edat,
    utofu_vcq_hdl_t vcqh,
    unsigned long flag
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    assert(ackd != 0);
    {
	ackd->notice_type = UTOFU_MRQ_TYPE_LCL_PUT;
	/* ackd->padding1[0] = 0; */
	ackd->vcq_id = rcqi;
	ackd->edata = edat;
	/* ackd->lcl_stadd = 0; */
	ackd->rmt_stadd = rsta + leng;
#ifndef	NDEBUG
	ackd->rmt_value = leng; /* XXX */
	ackd->reserved[0] = vcqh; /* XXX */
#endif	/* NDEBUG */
	ackd->reserved[1] = flag; /* XXX */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_puti_le08_from_cash(
    const struct ulib_utof_cash *rcsh,
    const struct ulib_utof_cash *lcsh,
    uint64_t roff,
    uint64_t lval,
    uint64_t leng,
    uint64_t edat,
    unsigned long uflg,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    utofu_stadd_t rsta;
    unsigned long flag;

    ENTER_RC_C(uc);

    /* rsta */
    rsta = rcsh->stad + roff;

    /* flag */
    flag =  uflg
	    | UTOFU_ONESIDED_FLAG_PATH(rcsh->paid)
	    ;

    /* desc */
    {
	uint64_t *desc_spc = toqd->desc;

	uc = utofu_prepare_put_piggyback8(lcsh->vcqh, rcsh->vcqi,
		lval, rsta, leng, edat, flag,
		desc_spc, &toqd->size);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    uc = ulib_ackd_puti_le08(toqd->ackd, rcsh->vcqi, rsta, lval, leng, edat,
	    lcsh->vcqh, flag);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ======================================================================== */

static inline int ulib_ackd_puti_le32(
    struct utofu_mrq_notice *ackd,
    utofu_vcq_id_t rcqi,
    utofu_stadd_t rsta,
    const void *ldat,
    uint64_t leng,
    uint64_t edat,
    utofu_vcq_hdl_t vcqh,
    unsigned long flag
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    assert(ackd != 0);
    {
	ackd->notice_type = UTOFU_MRQ_TYPE_LCL_PUT;
	/* ackd->padding1[0] = 0; */
	ackd->vcq_id = rcqi;
	ackd->edata = edat;
	/* ackd->lcl_stadd = 0; */
	ackd->rmt_stadd = rsta + leng;
#ifndef	NDEBUG
	ackd->rmt_value = leng; /* XXX */
	ackd->reserved[0] = vcqh; /* XXX */
#endif	/* NDEBUG */
	ackd->reserved[1] = flag; /* XXX */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_puti_le32_from_cash(
    const struct ulib_utof_cash *rcsh,
    const struct ulib_utof_cash *lcsh,
    uint64_t roff,
    const void *ldat,
    uint64_t leng,
    uint64_t edat,
    unsigned long uflg,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    utofu_stadd_t rsta;
    unsigned long flag;

    ENTER_RC_C(uc);

    /* rsta */
    rsta = rcsh->stad + roff;

    /* flag */
    flag =  uflg
	    | UTOFU_ONESIDED_FLAG_PATH(rcsh->paid)
	    ;

    /* desc */
    {
	uint64_t *desc_spc = toqd->desc;

	uc = utofu_prepare_put_piggyback(lcsh->vcqh, rcsh->vcqi,
		(void *)ldat, rsta, leng, edat, flag,
		desc_spc, &toqd->size);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    uc = ulib_ackd_puti_le32(toqd->ackd, rcsh->vcqi, rsta, ldat, leng, edat,
	    lcsh->vcqh, flag);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ======================================================================== */

static inline int ulib_ackd_fadd(
    struct utofu_mrq_notice *ackd,
    utofu_vcq_id_t rcqi,
    utofu_stadd_t rsta,
    uint64_t lval,
    uint64_t leng,
    uint64_t edat,
    utofu_vcq_hdl_t vcqh,
    unsigned long flag
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    assert(ackd != 0);
    {
	ackd->notice_type = UTOFU_MRQ_TYPE_LCL_ARMW;
	/* ackd->padding1[0] = 0; */
	ackd->vcq_id = rcqi;
	ackd->edata = edat;
	/* ackd->lcl_stadd = 0; */
	ackd->rmt_stadd = rsta /* + leng */;
#ifndef	NDEBUG
	ackd->rmt_value = leng; /* XXX */
	ackd->reserved[0] = vcqh; /* XXX */
#endif	/* NDEBUG */
	ackd->reserved[1] = flag; /* XXX */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_fadd_from_cash(
    const struct ulib_utof_cash *rcsh,
    const struct ulib_utof_cash *lcsh,
    uint64_t roff,
    uint64_t lval, /* addv */
    uint64_t leng,
    uint64_t edat,
    unsigned long uflg,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    utofu_stadd_t rsta;
    unsigned long flag;

    ENTER_RC_C(uc);

    /* rsta */
    rsta = rcsh->stad + roff;

    /* flag */
    flag =  uflg
	    | UTOFU_ONESIDED_FLAG_PATH(rcsh->paid)
	    ;

    /* desc */
    {
	uint64_t *desc_spc = toqd->desc;

	if (leng == sizeof (uint64_t)) {
	    assert((roff % sizeof (uint64_t)) == 0);
	    uc = utofu_prepare_armw8(lcsh->vcqh, rcsh->vcqi, UTOFU_ARMW_OP_ADD,
		    lval, rsta, edat, flag,
		    desc_spc, &toqd->size);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	}
	else if (leng == sizeof (uint32_t)) {
	    assert((roff % sizeof (uint32_t)) == 0);
	    uc = utofu_prepare_armw4(lcsh->vcqh, rcsh->vcqi, UTOFU_ARMW_OP_ADD,
		    lval, rsta, edat, flag,
		    desc_spc, &toqd->size);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	}
    }

    uc = ulib_ackd_fadd(toqd->ackd, rcsh->vcqi, rsta, lval, leng, edat,
	    lcsh->vcqh, flag);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ======================================================================== */

static inline int ulib_ackd_swap(
    struct utofu_mrq_notice *ackd,
    utofu_vcq_id_t rcqi,
    utofu_stadd_t rsta,
    uint64_t lval,
    uint64_t leng,
    uint64_t edat,
    utofu_vcq_hdl_t vcqh,
    unsigned long flag
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    assert(ackd != 0);
    {
	ackd->notice_type = UTOFU_MRQ_TYPE_LCL_ARMW;
	/* ackd->padding1[0] = 0; */
	ackd->vcq_id = rcqi;
	ackd->edata = edat;
	/* ackd->lcl_stadd = 0; */
	ackd->rmt_stadd = rsta /* + leng */;
#ifndef	NDEBUG
	ackd->rmt_value = leng; /* XXX */
	ackd->reserved[0] = vcqh; /* XXX */
#endif	/* NDEBUG */
	ackd->reserved[1] = flag; /* XXX */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_swap_from_cash(
    const struct ulib_utof_cash *rcsh,
    const struct ulib_utof_cash *lcsh,
    uint64_t roff,
    uint64_t lval, /* addv */
    uint64_t leng,
    uint64_t edat,
    unsigned long uflg,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    utofu_stadd_t rsta;
    unsigned long flag;

    ENTER_RC_C(uc);

    /* rsta */
    rsta = rcsh->stad + roff;

    /* flag */
    flag =  uflg
	    | UTOFU_ONESIDED_FLAG_PATH(rcsh->paid)
	    ;

    /* desc */
    {
	uint64_t *desc_spc = toqd->desc;

	if (leng == sizeof (uint64_t)) {
	    assert((roff % sizeof (uint64_t)) == 0);
	    uc = utofu_prepare_armw8(lcsh->vcqh, rcsh->vcqi, UTOFU_ARMW_OP_SWAP,
		    lval, rsta, edat, flag,
		    desc_spc, &toqd->size);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	}
	else if (leng == sizeof (uint32_t)) {
	    assert((roff % sizeof (uint32_t)) == 0);
	    uc = utofu_prepare_armw4(lcsh->vcqh, rcsh->vcqi, UTOFU_ARMW_OP_SWAP,
		    lval, rsta, edat, flag,
		    desc_spc, &toqd->size);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	}
    }

    uc = ulib_ackd_swap(toqd->ackd, rcsh->vcqi, rsta, lval, leng, edat,
	    lcsh->vcqh, flag);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* ======================================================================== */

static inline int ulib_ackd_cswp(
    struct utofu_mrq_notice *ackd,
    utofu_vcq_id_t rcqi,
    utofu_stadd_t rsta,
    uint64_t swpv,
    uint64_t cmpv,
    uint64_t leng,
    uint64_t edat,
    utofu_vcq_hdl_t vcqh,
    unsigned long flag
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    assert(ackd != 0);
    {
	ackd->notice_type = UTOFU_MRQ_TYPE_LCL_ARMW;
	/* ackd->padding1[0] = 0; */
	ackd->vcq_id = rcqi;
	ackd->edata = edat;
	/* ackd->lcl_stadd = 0; */
	ackd->rmt_stadd = rsta /* + leng */;
	/* ackd->rmt_value = cmpv; */ /* XXX */
#ifndef	NDEBUG
	ackd->rmt_value = leng; /* XXX */
	ackd->reserved[0] = vcqh; /* XXX */
#endif	/* NDEBUG */
	ackd->reserved[1] = flag; /* XXX */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_cswp_from_cash(
    const struct ulib_utof_cash *rcsh,
    const struct ulib_utof_cash *lcsh,
    uint64_t roff,
    uint64_t swpv, /* lval */
    uint64_t cmpv, /* lval */
    uint64_t leng,
    uint64_t edat,
    unsigned long uflg,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    utofu_stadd_t rsta;
    unsigned long flag;

    ENTER_RC_C(uc);

    /* rsta */
    rsta = rcsh->stad + roff;

    /* flag */
    flag =  uflg
	    | UTOFU_ONESIDED_FLAG_PATH(rcsh->paid)
	    ;

    /* desc */
    {
	uint64_t *desc_spc = toqd->desc;

	if (leng == sizeof (uint64_t)) {
	    assert((roff % sizeof (uint64_t)) == 0);
	    uc = utofu_prepare_cswap8(lcsh->vcqh, rcsh->vcqi,
		    cmpv, swpv, rsta, edat, flag,
		    desc_spc, &toqd->size);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	}
	else if (leng == sizeof (uint32_t)) {
	    assert((roff % sizeof (uint32_t)) == 0);
	    uc = utofu_prepare_cswap4(lcsh->vcqh, rcsh->vcqi,
		    cmpv, swpv, rsta, edat, flag,
		    desc_spc, &toqd->size);
	    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
	}
    }

    /* ackd */
    uc = ulib_ackd_cswp(toqd->ackd, rcsh->vcqi, rsta, swpv, cmpv, leng, edat,
		lcsh->vcqh, flag);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}
#ifdef	CONF_ULIB_UTOF_FIX3

/* ======================================================================== */

int utofu_prepare_nop(
    utofu_vcq_hdl_t	vcq_hdl,
    unsigned long int	flags,
    void *		desc,
    size_t *		desc_size
)
{
    int uc = UTOFU_SUCCESS;

    memset(desc, 0, 32);
    if ((flags & UTOFU_ONESIDED_FLAG_TCQ_NOTICE) != 0) {
        ((uint8_t *)desc)[0] |= 0x02;
    }
    desc_size[0] = 32;

    return uc;
}
#endif	/* CONF_ULIB_UTOF_FIX3 */

/* ======================================================================== */

#ifndef	ULIB_MRQ_TYPE_LCL_NOP
/* extra struct utofu_mrq_notice . notice_type */
#define ULIB_MRQ_TYPE_LCL_NOP   30
#endif	/* ULIB_MRQ_TYPE_LCL_NOP */

static inline int ulib_ackd_noop(
    struct utofu_mrq_notice *ackd,
    utofu_vcq_hdl_t vcqh,
    unsigned long flag
)
{
    int uc = UTOFU_SUCCESS;

    ENTER_RC_C(uc);

    assert(ackd != 0);
    {
	ackd->notice_type = ULIB_MRQ_TYPE_LCL_NOP; /* XXX extension */
	/* ackd->padding1[0] = 0; */
	/* ackd->vcq_id = rcqi; */
	/* ackd->edata = edat; */
	/* ackd->lcl_stadd = 0; */
	/* ackd->rmt_stadd = rsta; */
#ifndef	NDEBUG
	ackd->rmt_value = 0; /* XXX */
	ackd->reserved[0] = vcqh; /* XXX */
#endif	/* NDEBUG */
	ackd->reserved[1] = flag; /* XXX */
    }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}

/* static inline */ int ulib_desc_noop_from_cash(
    const struct ulib_utof_cash *lcsh,
    unsigned long uflg,
    struct ulib_toqd_cash *toqd
)
{
    int uc = UTOFU_SUCCESS;
    unsigned long flag;

    ENTER_RC_C(uc);

    /* flag */
    flag = uflg;

    /* desc */
    {
	uint64_t *desc_spc = toqd->desc;

	uc = utofu_prepare_nop(lcsh->vcqh, flag,
		desc_spc, &toqd->size);
	if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }
    }

    uc = ulib_ackd_noop(toqd->ackd, lcsh->vcqh, flag);
    if (uc != UTOFU_SUCCESS) { RETURN_BAD_C(uc); }

    RETURN_OK_C(uc);

    RETURN_RC_C(uc, /* do nothing */ );
}
