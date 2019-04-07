/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_IMPL_SHEA_H
#define _TOFU_IMPL_SHEA_H

/* ======================================================================== */

/* #include <utofu.h> */ /* YYY */
#include <stdint.h>	    /* for uintptr_t */


typedef uintptr_t utofu_vcq_hdl_t;

//#define UTOFU_SUCCESS	    0

/* ------------------------------------------------------------------------ */

#include <stdint.h>	    /* for uint32_t */
#include <sys/uio.h>	    /* for struct iovec */

#if 0
struct ulib_shea_rbuf {
    struct iovec iovs[2];
    uint32_t niov;
    uint32_t leng;
};
#endif

struct ulib_shea_uexp {
    uint64_t utag;
    uint32_t mblk;
    uint32_t nblk;
    uint32_t boff;
    uint32_t flag;
    uint32_t srci;
    struct ulib_shea_rbuf rbuf;
    void * vspc_list[2];
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tims[1];
#endif	/* CONF_ULIB_PERF_SHEA */
};

#define ULIB_SHEA_RINF_FLAG_MBLK    0x01
#define ULIB_SHEA_RINF_FLAG_TFLG    0x02
#define ULIB_SHEA_RINF_FLAG_ZFLG    0x04

/* ------------------------------------------------------------------------ */

#include <ofi_list.h>		/* for struct dlist_entry */
#include <ofi_atom.h>		/* for ofi_atomic32_t */

#if 0
struct ulib_utof_cash {
    uint64_t		    vcqi;
    uint64_t		    stad;
    uint64_t		    stad_data;
    uint8_t		    paid;
    uintptr_t		    vcqh;
};
struct ulib_toqd_cash {
    uint64_t		    desc[8];
    size_t		    size;
    uint64_t		    ackd[8];
};

struct ulib_desc_cash {
    struct ulib_utof_cash   addr[2];
    uint64_t		    akey;    /* XXX tank */
    uint32_t		    vpid;
    ofi_atomic32_t	    refc;
    struct dlist_entry	    list;
    struct ulib_toqd_cash   swap[1];
    struct ulib_toqd_cash   fadd[1];
    struct ulib_toqd_cash   cswp[1];
    struct ulib_toqd_cash   puti[1];
    struct ulib_toqd_cash   phdr[1];
    struct ulib_toqd_cash   putd[2];
};

/* ------------------------------------------------------------------------ */

struct ulib_shea_data {
    uint32_t nblk;
    uint32_t boff;
    uint32_t llen; /* last length */
    uint32_t rank;
    uint64_t utag;
    uint64_t flag;
    void *ctxt;
    void *toqc;
    void *desc_cash_tmpl; /* YYY toqc_cash_tmpl */
    void *desc_cash_real; /* YYY toqc_cash_real */
    struct ulib_desc_cash real;
    int (*func)(void *farg, int r_uc, const void *vctx);
    void *farg;
#ifdef	CONF_ULIB_PERF_SHEA
    uint64_t tims[16];
#endif	/* CONF_ULIB_PERF_SHEA */
};

#define ULIB_SHEA_DATA_TFLG	(1ULL << 0)
#define ULIB_SHEA_DATA_ZFLG	(1ULL << 1)

/* ------------------------------------------------------------------------ */

struct ulib_shea_cbuf { /* YYY */
    uint64_t ui64[16];
};

struct ulib_shea_esnd { /* YYY */
    uint64_t ui64[16];
};

typedef int (*ulib_shea_ercv_cbak_f)(void *farg, int r_uc, const void *vctx);

#endif /* 0 */
/* ======================================================================== */
#if 0
#include <stdint.h>         /* for uint64_t */

#define ULIB_TOFA_BITS_TUX  5
#define ULIB_TOFA_BITS_TUY  5
#define ULIB_TOFA_BITS_TUZ  5
#define ULIB_TOFA_BITS_TUA  1
#define ULIB_TOFA_BITS_TUB  2
#define ULIB_TOFA_BITS_TUC  1
#define ULIB_TOFA_BITS_TNI  3
#define ULIB_TOFA_BITS_TCQ  4   /* <  6 */
#define ULIB_TOFA_BITS_TAG  12  /* < 18 */
#define ULIB_TOFA_BITS_OFF  26  /* < 40 (= 32 + 8) */

struct ulib_tofa { /* compact format : network address + memory address */
    uint64_t	tux : ULIB_TOFA_BITS_TUX; /* 00: 2^5 = 32 */
    uint64_t	tuy : ULIB_TOFA_BITS_TUY; /* 05: 2^5 = 32 */
    uint64_t	tuz : ULIB_TOFA_BITS_TUZ; /* 10: 2^5 = 32 */
    uint64_t	tua : ULIB_TOFA_BITS_TUA; /* 15: */
    uint64_t	tub : ULIB_TOFA_BITS_TUB; /* 16: */
    uint64_t	tuc : ULIB_TOFA_BITS_TUC; /* 18: */
    uint64_t	tni : ULIB_TOFA_BITS_TNI; /* 19:  6 TNIs */
    uint64_t	tcq : ULIB_TOFA_BITS_TCQ; /* 22: 12 TCQs */
    uint64_t	tag : ULIB_TOFA_BITS_TAG; /* 26: [0-4095] named for MPI */
    uint64_t	off : ULIB_TOFA_BITS_OFF; /* 38: 64 - 38 ; 64MiB */
};

#define ULIB_TANK_BITS_TUX  ULIB_TOFA_BITS_TUX
#define ULIB_TANK_BITS_TUY  ULIB_TOFA_BITS_TUY
#define ULIB_TANK_BITS_TUZ  ULIB_TOFA_BITS_TUZ
#define ULIB_TANK_BITS_TUA  ULIB_TOFA_BITS_TUA
#define ULIB_TANK_BITS_TUB  ULIB_TOFA_BITS_TUB
#define ULIB_TANK_BITS_TUC  ULIB_TOFA_BITS_TUC
#define ULIB_TANK_BITS_TNI  ULIB_TOFA_BITS_TNI
#define ULIB_TANK_BITS_TCQ  6	/* 2^6 = 64 */
#define ULIB_TANK_BITS_CID  3	/* component id. */
#define ULIB_TANK_BITS_VLD  1	/* valid flag */
#define ULIB_TANK_BITS_PID  32	/* vpid : virtual processor id. */

struct ulib_tank { /* compact format : network address + rank (vpid) */
    uint64_t	tux : ULIB_TANK_BITS_TUX; /* 00: 2^5 = 32 */
    uint64_t	tuy : ULIB_TANK_BITS_TUY; /* 05: 2^5 = 32 */
    uint64_t	tuz : ULIB_TANK_BITS_TUZ; /* 10: 2^5 = 32 */
    uint64_t	tua : ULIB_TANK_BITS_TUA; /* 15: 2^1 =  2 */
    uint64_t	tub : ULIB_TANK_BITS_TUB; /* 16: 2^2 =  4 */
    uint64_t	tuc : ULIB_TANK_BITS_TUC; /* 18: 2^1 =  2 */
    uint64_t	tni : ULIB_TANK_BITS_TNI; /* 19: 2^3 =  8 */
    uint64_t	tcq : ULIB_TANK_BITS_TCQ; /* 22: 2^6 = 64 */
    uint64_t	cid : ULIB_TANK_BITS_CID; /* 28: 2^3 =  8 */
    uint64_t	vld : ULIB_TANK_BITS_VLD; /* 31: 2^1 =  2 */
    uint64_t	pid : ULIB_TANK_BITS_PID; /* 32: 2^32     */
};

union ulib_tofa_u {
    uint64_t		ui64;
    struct ulib_tofa	tofa; /* TOFu network Address */
    struct ulib_tank	tank; /* Tofu network Address + raNK (vpid) */
};

/* ------------------------------------------------------------------------ */

#include <stdint.h>	    /* for uint8_t */
/* #include <utofu.h> */    /* YYY */
/* typedef uint16_t utofu_tni_id_t; */
/* typedef uint16_t utofu_cq_id_t;  */
/* typedef uint16_t utofu_cmp_id_t; */

struct ulib_epnt_info {
    uint8_t xyz[8]; /* xyz + abc */
    uint16_t tni[1]; /* utofu_tni_id_t */
    uint16_t tcq[1]; /* utofu_cq_id_t  */
    uint16_t cid[1]; /* utofu_cmp_id_t */
/*  uint32_t pid[1]; */ /* YYY vpid */
};

/* ======================================================================== */

#include <assert.h>	    /* for assert() */
#endif /* if 0 */

#if 0
extern void ulib_shea_cbuf_init(struct ulib_shea_cbuf *cbuf);
extern void ulib_shea_cbuf_fini(struct ulib_shea_cbuf *cbuf);
static inline void ulib_cast_epnt_to_tofa(
    const struct ulib_epnt_info *einf,
    struct ulib_tofa *tofa
)
{
    assert(sizeof (tofa[0]) == sizeof (uint64_t));
    {
        assert(einf->xyz[0] < (1UL << ULIB_TOFA_BITS_TUX));
        tofa->tux = einf->xyz[0];
        assert(einf->xyz[1] < (1UL << ULIB_TOFA_BITS_TUY));
        tofa->tuy = einf->xyz[1];
        assert(einf->xyz[2] < (1UL << ULIB_TOFA_BITS_TUZ));
        tofa->tuz = einf->xyz[2];

        assert(einf->xyz[3] < (1UL << ULIB_TOFA_BITS_TUA));
        tofa->tua = einf->xyz[3];
        assert(einf->xyz[4] < (1UL << ULIB_TOFA_BITS_TUB));
        tofa->tub = einf->xyz[4];
        assert(einf->xyz[5] < (1UL << ULIB_TOFA_BITS_TUC));
        tofa->tuc = einf->xyz[5];

        assert(einf->tni[0] < (1UL << ULIB_TOFA_BITS_TNI));
        tofa->tni = einf->tni[0];
        assert(einf->tcq[0] < (1UL << ULIB_TOFA_BITS_TCQ));
        tofa->tcq = einf->tcq[0];
    }

    return ;
}

static inline void ulib_cast_epnt_to_tofa_ui64(
    const struct ulib_epnt_info *einf,
    uint64_t *tofa_ui64
)
{
    {
        union ulib_tofa_u tofa_u = { .ui64 = 0, };

        ulib_cast_epnt_to_tofa( einf, &tofa_u.tofa );

        tofa_ui64[0] = tofa_u.ui64;
    }

    return ;
}
#endif /* 0 */

static inline void ulib_cast_epnt_to_tank(
    const struct ulib_epnt_info *einf,
    struct ulib_tank *tank
)
{
    assert(sizeof (tank[0]) == sizeof (uint64_t));
    {
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
        tank->pid = 0 /* einf->pid[0] */ ; /* YYY */
    }

    return ;
}

static inline void ulib_cast_epnt_to_tank_ui64(
    const struct ulib_epnt_info *einf,
    uint64_t *tank_ui64
)
{
    {
        union ulib_tofa_u tank_u = { .ui64 = 0, };

        ulib_cast_epnt_to_tank( einf, &tank_u.tank );

        tank_ui64[0] = tank_u.ui64;
    }

    return ;
}
/* ------------------------------------------------------------------------ */
#if 0
static inline void ulib_shea_data_init(
    struct ulib_shea_data *data,
    void *ctxt,
    size_t tlen,
    uint32_t vpid,
    uint64_t utag,
    uint64_t flag
)
{
    data->rank = vpid;
    data->utag = utag;
    data->ctxt = ctxt;

    /* nblk from tlen */ /* YYY */
    /* llen from tlen */ /* YYY */

    if (tlen == 0) { /* zero length */
	flag |= ULIB_SHEA_DATA_ZFLG;
    }
    data->flag = flag;

    /* data->toqc = 0; */
    /* data->desc_cash_tmpl  = 0; */ /* YYY toqc_cash_tmpl */
    /* data->desc_cash_real  = 0; */ /* YYY toqc_cash_real */
    /* data->func  = 0; */
    /* data->farg  = 0; */

    return ;
}

static inline void ulib_shea_data_init_cash(
    struct ulib_shea_data *data,
    void *toqc,
    void *cash_tmpl,
    void *cash_real
)
{
    data->toqc = toqc;
    data->desc_cash_tmpl = cash_tmpl;
    data->desc_cash_real = cash_real;

    return ;
}
#endif

/* ------------------------------------------------------------------------ */
#ifdef	NOTDEF

extern struct ulib_shea_esnd *  ulib_shea_cbuf_esnd_qget(
				    struct ulib_shea_cbuf *cbuf, void *data);
extern void			ulib_shea_cbuf_esnd_qput(
				    struct ulib_shea_cbuf *cbuf,
				    struct ulib_shea_esnd *esnd);
#endif	/* NOTDEF */

#if 0
static inline struct ulib_shea_esnd * ulib_shea_cbuf_esnd_qget(
    struct ulib_shea_cbuf *cbuf,
    void *data
)
{
    struct ulib_shea_esnd *esnd = 0;
    /* YYY */
    return esnd;
}

static inline void ulib_shea_cbuf_esnd_qput(
    struct ulib_shea_cbuf *cbuf,
    struct ulib_shea_esnd *esnd
)
{
    /* YYY */
    return ;
}

int ulib_shea_cbuf_enab(
    struct ulib_shea_cbuf *cbuf,
    utofu_vcq_hdl_t vcqh,
    unsigned int ctag,
    unsigned int dtag,
    ulib_shea_ercv_cbak_f func,
    void *farg
)
{
    int uc = UTOFU_SUCCESS;
    return uc;
}

#endif

/* ------------------------------------------------------------------------ */

struct ulib_toqc;

#if 0
static inline int ulib_toqc_init( /* YYY */
    utofu_vcq_hdl_t vcqh,
    struct ulib_toqc **pp_toqc
)
{
    int uc = UTOFU_SUCCESS;
    pp_toqc[0] = (void *)(uintptr_t)0x5f5e5d5c;
    return uc;
}
#endif

#endif	/* _ULIB_SHEA_H */
