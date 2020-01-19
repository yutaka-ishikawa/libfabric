#include "tofu_impl.h"

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
    if (ui64 == ((uint64_t) -1)) { /* FI_ADDR_UNSPEC */
        snprintf(buf, sz, "fi_addr_unspec");
    } else {
        snprintf(buf, sz, "xyzabc(%02x:%02x:%02x:%02x:%02x:%02x), "
                 "tni(%d), tcq(%d), cid(0x%x)",
                 utofa.tank.tux, utofa.tank.tuy, utofa.tank.tuz,
                 utofa.tank.tua, utofa.tank.tub, utofa.tank.tuc,
                 utofa.tank.tni, utofa.tank.tcq, utofa.tank.cid);
    }
    return buf;
}

void
dbg_show_utof_vcqh(utofu_vcq_hdl_t vcqh)
{
    int uc;
    utofu_vcq_id_t vcqi = -1UL;
    uint8_t xyz[8];
    uint16_t tni[1], tcq[1], cid[1];
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
    fprintf(stderr, "%d: vcqh(0x%lx) vcqid(%s) in %s:%d of %s\n", mypid, vcqh,
	    tank2string(buf, 128, tofa.ui64),
	    __func__, __LINE__, __FILE__);
    fflush(stderr);
}


/*
 * Initializing utofu
 */
int
tofu_impl_isep_open(struct tofu_sep *sep)
{
    int fc = FI_SUCCESS;
    int uc;
    utofu_tni_id_t *tnis = 0;
    size_t ntni = 0;
    size_t ni;
    const size_t mtni = sizeof (sep->tnis) / sizeof (sep->tnis[0]);

    /* Initialization of Tofu NIC */
    uc = utofu_get_onesided_tnis(&tnis, &ntni);
    R_DBG0(RDBG_LEVEL1, "uc(%d) ntni(%ld)", uc, ntni);
    if (uc != UTOFU_SUCCESS) { fc = -FI_EOTHER; goto bad; }
    if (ntni > mtni) {
        ntni = mtni;
    }
    /* copy tnis[] and ntni */
    for (ni = 0; ni < ntni; ni++) {
        struct utofu_onesided_caps *cap;
        sep->tnis[ni] = tnis[ni];
        utofu_query_onesided_caps(tnis[ni], &cap);
        R_DBG0(RDBG_LEVEL2, "tnid(%d) num_stags(%d)\n",
	       tnis[ni], cap->num_reserved_stags);
    }
    sep->ntni = ntni;
    /* free tnis[] */
    if (tnis != 0) {
        free(tnis); tnis = 0;
    }
    assert(sep->ntni > 0);
bad:
    return fc;
}

static char *
tofu_impl_str_tniq2long(const char *pv, long lv[2])
{
    const char *pv1, *pv2;
    char *p1, *p2;
    long lv1, lv2;

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

static int
tofu_impl_uri_param2long(const char *pparm, long lv_tniq[CONF_TOFU_CTXC * 2])
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

		new_pv = tofu_impl_str_tniq2long(pv, lv);
		if (new_pv == 0) {
		    ret = -FI_EINVAL; /* -__LINE__ */; goto bad;
		}
		nv++;
		pv = new_pv;
	    }
	    cp = pv;
	} else { /* unknown parameter */
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

static int
tofu_imp_uri2long(const char *uri, long lv_xyzabc[6],
		  uint16_t    *cid, long lv_tniq[CONF_TOFU_CTXC * 2])
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
	if ((lv_xyzabc[0] < 0) || (lv_xyzabc[0] >= 256)
	    || (lv_xyzabc[1] < 0) || (lv_xyzabc[1] >= 256)
	    || (lv_xyzabc[2] < 0) || (lv_xyzabc[2] >= 256)
	    || (lv_xyzabc[3] < 0) || (lv_xyzabc[3] >= 2)
	    || (lv_xyzabc[4] < 0) || (lv_xyzabc[4] >= 3)
	    || (lv_xyzabc[5] < 0) || (lv_xyzabc[5] >= 2)) {
	    ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	}
    }

    /* ;q=<tniq> */
    {
	int nv;

	nv = tofu_impl_uri_param2long(uri, lv_tniq);
	if (nv < 0) {
	    ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	}
	ret = nv;
    }
bad:
    return ret;
}

/*
 * URI --> Tofu address
 */
int
tofu_impl_uri2name(const void *vuri, size_t index,  void *vnam)
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

	nv = tofu_imp_uri2long(cp, lv_xyzabc, &lv_cid, lv_tniq);
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
    R_DBG0(RDBG_LEVEL1, "%u.%u.%u.%u.%u.%u cid(%u) return bad(%d)\n",
            name->txyz[0], name->txyz[1], name->txyz[2],
            name->a, name->b, name->c, name->p, fc);
    return fc;
}
