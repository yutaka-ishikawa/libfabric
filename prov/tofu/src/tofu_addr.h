static inline int
tofu_av_lookup_vcqid_by_fia(struct tofu_av *av,  fi_addr_t fi_a,
			    utofu_vcq_id_t *vcqid, uint64_t *flgs)
{
    int	uc = UTOFU_SUCCESS;
    int	fc = FI_SUCCESS;
    size_t	rx_idx, av_idx;
    struct tofu_vname *vnam;

    if (fi_a == FI_ADDR_NOTAVAIL) {
	fc = -FI_EINVAL; uc = -FI_EINVAL; goto bad;
    }
    assert(av->av_rxb >= 0);
    /* assert(av->av_rxb <= TOFU_RX_CTX_MAX_BITS); */
    if (av->av_rxb == 0) {
	rx_idx = 0;
	av_idx = fi_a;
    } else {
	rx_idx = ((uint64_t)fi_a) >> (64 - av->av_rxb);
	/* av_idx = fi_a & rx_ctx_mask */
	av_idx = (((uint64_t)fi_a) << av->av_rxb) >> av->av_rxb;
    }
    if (av_idx >= av->av_tab[rx_idx].nct) {
	fc = -FI_EINVAL; goto bad;
    }
    assert(av->av_tab[rx_idx].vnm != 0);
    vnam = &av->av_tab[rx_idx].vnm[av_idx];
#if 0
    /* now vcqid is also registered in vnam 2020/06/13 */
    uc = utofu_construct_vcq_id(vnam->xyzabc,
				vnam->tniq[0]>>4,
				vnam->tniq[0]&0x0f,
				vnam->cid, vcqid);
    if (uc != UTOFU_SUCCESS) {
	R_DBG("utof_construct_vcq_id error: rc(%d)\n", uc);
	goto bad;
    }
#else
    *vcqid = vnam->vcqid;
#endif
    if (flgs) {
	utofu_path_id_t	pathid = 0;
	if ((uc = utofu_get_path_id(*vcqid, &vnam->xyzabc[3], &pathid)) != UTOFU_SUCCESS) {
	    R_DBG("utof_get_path_id error: rc(%d)\n", uc);
	}
	*flgs = UTOFU_ONESIDED_FLAG_PATH(pathid);
    }
bad:
    if (uc != UTOFU_SUCCESS) {
	R_DBG("Something wrong fi_a(%ld) %u.%u.%u.%u.%u.%u cid(%u) return bad(%d)\n",
	      fi_a, vnam->xyzabc[0], vnam->xyzabc[1], vnam->xyzabc[2],
	      vnam->xyzabc[3], vnam->xyzabc[4], vnam->xyzabc[5], vnam->cid, uc);
	fc = -FI_EINVAL;
	abort();
    }
    return fc;
}

static inline int
tofu_av_lookup_rank_by_vcqid(struct tofu_av *av,  utofu_vcq_id_t vcqid)
{
    size_t	rx_idx, av_idx;
    struct tofu_av_tab	*avtp;

    for (rx_idx = 0; rx_idx < CONF_TOFU_ATTR_MAX_EP_TXRX_CTX; rx_idx++) {
	avtp = &av->av_tab[rx_idx];
	for (av_idx = 0; av_idx < avtp->nct; av_idx++) {
	    if (avtp->vnm[av_idx].vcqid == vcqid) {
		goto find;
	    }
	}
    }
    av_idx = -1;
    R_DBG("YI*** Cannot find vcqid(%lx) avtp->nct(%ld)", vcqid, avtp->nct);
find:
    return av_idx;
}

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

struct ulib_epnt_info {
    uint8_t xyz[8]; /* xyz + abc */
    utofu_tni_id_t tni[1];
    utofu_cq_id_t tcq[1];
    uint16_t cid[1]; /* utofu_cmp_id_t */
    uint32_t pid[1]; /* vpid */
};
