/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_IMPL_SHEA_H
#define _TOFU_IMPL_SHEA_H

/* ======================================================================== */

/* #include <utofu.h> */ /* YYY */
#include <stdint.h>	    /* for uintptr_t */

// defined in utofu.h
//typedef uintptr_t utofu_vcq_hdl_t;

//#define UTOFU_SUCCESS	    0

/* ------------------------------------------------------------------------ */

#include <stdint.h>	    /* for uint32_t */
#include <sys/uio.h>	    /* for struct iovec */

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
#ifdef	NOTDEF

extern struct ulib_shea_esnd *  ulib_shea_cbuf_esnd_qget(
				    struct ulib_shea_cbuf *cbuf, void *data);
extern void			ulib_shea_cbuf_esnd_qput(
				    struct ulib_shea_cbuf *cbuf,
				    struct ulib_shea_esnd *esnd);
#endif	/* NOTDEF */

/* ------------------------------------------------------------------------ */

struct ulib_toqc;

#endif	/* _ULIB_SHEA_H */
