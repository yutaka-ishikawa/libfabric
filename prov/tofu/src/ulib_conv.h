/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_ULIB_CONV_H
#define _ULIB_CONV_H

#include "utofu.h"

#include <assert.h>

/* ======================================================================== */

#include <stdint.h>	    /* for uint64_t */

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
    uint64_t    tux : ULIB_TOFA_BITS_TUX; /* 00: 2^5 = 32 */
    uint64_t    tuy : ULIB_TOFA_BITS_TUY; /* 05: 2^5 = 32 */
    uint64_t    tuz : ULIB_TOFA_BITS_TUZ; /* 10: 2^5 = 32 */
    uint64_t    tua : ULIB_TOFA_BITS_TUA; /* 15: */
    uint64_t    tub : ULIB_TOFA_BITS_TUB; /* 16: */
    uint64_t    tuc : ULIB_TOFA_BITS_TUC; /* 18: */
    uint64_t    tni : ULIB_TOFA_BITS_TNI; /* 19:  6 TNIs */
    uint64_t    tcq : ULIB_TOFA_BITS_TCQ; /* 22: 12 TCQs */
    uint64_t    tag : ULIB_TOFA_BITS_TAG; /* 26: [0-4095] named for MPI */
    uint64_t    off : ULIB_TOFA_BITS_OFF; /* 38: 64 - 38 ; 64MiB */
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

static inline char *
tank2string(char *buf, size_t sz, uint64_t ui64)
{
    union ulib_tofa_u utofa;
    utofa.ui64 = ui64;
    snprintf(buf, sz, "xyzabc(%02x:%02x:%02x:%02x:%02x:%02x), "
             "tni(%d), tcq(%d), pid(%d)",
             utofa.tank.tux, utofa.tank.tuy, utofa.tank.tuz,
             utofa.tank.tua, utofa.tank.tub, utofa.tank.tuc,
             utofa.tank.tni, utofa.tank.tcq, utofa.tank.pid);
    return buf;
}

/* ======================================================================== */

struct ulib_epnt_info {
    uint8_t xyz[8]; /* xyz + abc */
    utofu_tni_id_t tni[1];
    utofu_cq_id_t tcq[1];
    uint16_t cid[1]; /* utofu_cmp_id_t */
};

/* ======================================================================== */

static inline int ulib_cast_epnt_to_tofa(
    const struct ulib_epnt_info *einf,
    struct ulib_tofa *tofa
)
{
    int uc = UTOFU_SUCCESS;
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
    return uc;
}

static inline int ulib_cast_stad_to_tofa(
    unsigned int stag,
    uint64_t offs,
    struct ulib_tofa *tofa
)
{
    int uc = UTOFU_SUCCESS;
    {
	assert(stag < (1UL << ULIB_TOFA_BITS_TAG));
	tofa->tag = stag;
	assert(offs < (1UL << ULIB_TOFA_BITS_OFF));
	tofa->off = offs;
    }
    return uc;
}


#endif	/* _ULIB_CONV_H */
