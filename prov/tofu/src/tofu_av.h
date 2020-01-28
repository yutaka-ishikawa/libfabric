/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_AV__H
#define _TOFU_AV__H

#include "tofu_impl.h"


static inline int
tofu_av_lup_tank(struct tofu_av *av__priv,  fi_addr_t fi_a,  uint64_t *tank)
{
    int fc = FI_SUCCESS;
    size_t av_idx, rx_idx;
    void *vnam;

    if (fi_a == FI_ADDR_NOTAVAIL) {
	fc = -FI_EINVAL; goto bad;
    }
    assert(av__priv->av__rxb >= 0);
    /* assert(av__priv->av_rxb <= TOFU_RX_CTX_MAX_BITS); */
    if (av__priv->av__rxb == 0) {
	rx_idx = 0;
	av_idx = fi_a;
    }
    else {
	rx_idx = ((uint64_t)fi_a) >> (64 - av__priv->av__rxb);
	/* av_idx = fi_a & rx_ctx_mask */
	av_idx = (((uint64_t)fi_a) << av__priv->av__rxb) >> av__priv->av__rxb;
    }
    if (av_idx >= av__priv->av__tab.nct) {
	fc = -FI_EINVAL; goto bad;
    }
    assert(av__priv->av__tab.tab != 0);
    vnam = (char *)av__priv->av__tab.tab + (av_idx * 16); /* XXX */

    /* get the Tofu network Address + raNK */
    fc = tofu_imp_namS_to_tank(vnam, rx_idx, tank);
    if (fc != FI_SUCCESS) { goto bad; }

bad:
    return fc;
}

#include "ulib_conv.h"	    /* for union ulib_tofa_u XXX */

static inline void
tofu_av_lup_rank(struct tofu_av *av__priv,
    utofu_vcq_hdl_t vcqh, int rx_idx, uint32_t *p_rank)
{
    uint32_t rank = -1U;
    utofu_vcq_id_t vcqi = -1UL;
    uint8_t xyz[8]; uint16_t tni[1], tcq[1], cid[1];
    int uc, fc;
    size_t av_idx, av_max;

    uc = utofu_query_vcq_id(vcqh, &vcqi);
    if (uc != UTOFU_SUCCESS) { goto bad; }

    uc = utofu_query_vcq_id(vcqh, &vcqi);
    if (uc != UTOFU_SUCCESS) { goto bad; }

    uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid);
    if (uc != UTOFU_SUCCESS) { goto bad; }

    if ((rx_idx < 0) && (rx_idx >= (1 << av__priv->av__rxb))) {
	goto bad;
    }
    /* YYY linear serch */
    av_max = av__priv->av__tab.nct;
    for (av_idx = 0; av_idx < av_max; av_idx++) {
	fi_addr_t addr = av_idx, fi_a;
	union ulib_tofa_u tank;

	fi_a = fi_rx_addr(addr, rx_idx, av__priv->av__rxb);
	fc = tofu_av_lup_tank(av__priv, fi_a, &tank.ui64);
	if (fc != FI_SUCCESS) { /* fc = 0; */ continue ; }

	if (
	    (tank.tank.vld == 1)
	    && (tank.tank.tux == xyz[0]) && (tank.tank.tuy == xyz[1])
	    && (tank.tank.tuz == xyz[2])
	    && (tank.tank.tua == xyz[3]) && (tank.tank.tub == xyz[4])
	    && (tank.tank.tuc == xyz[5])
	    && (tank.tank.tni == tni[0]) && (tank.tank.tcq == tcq[0])
	    /* && (tank.tank.cid == (cid[0] & 0x7)) */
	) {
	    assert(tank.tank.pid == av_idx); /* XXX */
	    rank = (uint64_t)tank.tank.pid;
	    break;
	}
    }

bad:
    p_rank[0] = rank;
    return ;
}

#endif	/* _TOFU_AV__H */
