#include "utf_cqmgr.h"

static inline void
utf_cqselect_add_snic(int tni, size_t sz)
{
    atomic_fetch_add(&utf_cqseltab->snd_len[tni], sz);
}

static inline void
utf_cqselect_add_rnic(int tni, size_t sz)
{
    atomic_fetch_add(&utf_cqseltab->rcv_len[tni], sz);
}
static inline void
utf_cqselect_sub_snic(int tni, size_t sz)
{
    atomic_fetch_sub(&utf_cqseltab->snd_len[tni], sz);
}

static inline void
utf_cqselect_sub_rnic(int tni, size_t sz)
{
    atomic_fetch_sub(&utf_cqseltab->rcv_len[tni], sz);
}

static inline void
utf_cqselect_default_remote(struct tni_info *tinfo, int rank, utofu_vcq_id_t *vcqid, uint64_t *flg)
{
    struct tofu_vname	*vnm = (struct tofu_vname*) tinfo->vnamp;
    utofu_path_id_t	pathid;
    *vcqid = vnm[rank].vcqid;
    pathid = vnm[rank].pathid;
    *flg = UTOFU_ONESIDED_FLAG_PATH(pathid);
}

static inline utofu_vcq_id_t
utf_cqselect_sendone(void *inf, utofu_vcq_hdl_t *vcqh, size_t sz, struct utf_tnimsgc *tnisz)
{
    struct tni_info	*tinfo = (struct tni_info*) inf;
    uint8_t	tni;
    int		sel = 0;

    if (utf_mrail == 0) { /* The first entry is default CQ and TNI */
	*vcqh = tinfo->vcqhdl[0];
	tinfo->usd[0] = 1;
	tni = tinfo->idx[0];
	utf_cqselect_add_snic(tni, sz);
	tnisz->rmsgsz[0] += sz;
	return tinfo->vcqid[0];
    }
    /* at this time */
    *vcqh  = tinfo->vcqhdl[sel];
    tinfo->usd[sel] = 1;
    tni = tinfo->idx[sel];
    utf_cqselect_add_snic(tni, sz);
    tnisz->smsgsz[0] += sz;
    // utf_printf("%s: vcqh(%lx) tni(%d) msglen(%ld)\n", __func__, *vcqh, tni, utf_cqseltab->snd_len[tni]);
    return tinfo->vcqid[sel];
}

/*
 * utf_cqselect_sendmultiple is the default TNI allocation for sender side
 */
static inline int
utf_cqselect_sendmultiple(void *inf, struct utf_vcqhdl_stadd *vs, size_t msgsize, struct utf_tnimsgc *tnisz)
{
    struct tni_info	*tinfo = (struct tni_info*) inf;
    int		tni, ni, st_ni, end_ni, type, i;

    vs->vcqhdl[0] = tinfo->vcqhdl[0];
    tinfo->usd[0] = 1;
    tni = tinfo->idx[0];
    utf_cqselect_add_snic(tni, msgsize);
    tnisz->smsgsz[0] += msgsize;
    ni = 1;
    if (utf_mrail == 0) {
	type = -1;
    } else {
	type = tinfo->ppn;
    }
    switch(type) {
    case 1:
	st_ni = 1; end_ni = tinfo->ntni;
	ni += 5;
	break;
    case 2: /* rank0: tni0, tni2, tni3
	     * rank1: tni1, tni4, tni5 */
	st_ni = 2 + tinfo->nrnk*2;
	end_ni = 4 + tinfo->nrnk*2;
	ni += 2;
	break;
    case 3: /* rank0: tni0, tni3
	     * rank1: tni1, tni4
	     * rank2: tni2, tni5 */
	st_ni = 3 + tinfo->nrnk;
	end_ni = 4 + tinfo->nrnk;
	ni += 1;
	break;
    case 4: /* sharing tni5 by all ranks */
	st_ni = 5; end_ni = 6;
	ni += 1;
	break;
    default:/* The first entry is default CQ and TNI */
	st_ni = end_ni = 0;
	vs->nent = 1;
    }
    for (i = st_ni; i < end_ni; i++) {
	vs->vcqhdl[i] = tinfo->vcqhdl[i];
	tinfo->usd[i] = 1;
	tni = tinfo->idx[i];
	utf_cqselect_add_snic(tni, msgsize);
	tnisz->smsgsz[i] += msgsize;
    }
    return ni;
}

/*
 * utf_cqselect_rget_expose is for the MPI receiver, to find out stadd and vcqid&vcqh
 */
static inline void
utf_cqselect_rget_expose(struct tni_info *tinfo, struct utf_vcqid_stadd *rgetaddr, utofu_vcq_hdl_t *vcqhdl,
			 struct utf_send_msginfo *minfo)
{
    size_t sz = minfo->msghdr.size;
    struct utf_tnimsgc	*tnisz = &minfo->tni_msgs;
    int	i, ntni, tni;
    size_t	frsz;

    ntni = tinfo->ntni;
    frsz = sz/ntni; /* receive size of the last nic is not exact number */
    for (i = 0; i < ntni; i++) {
	rgetaddr->stadd[i] = 0;
	rgetaddr->vcqid[i] = tinfo->vcqid[i];
	vcqhdl[i] = tinfo->vcqhdl[i];
	tni = tinfo->idx[i];
	utf_cqselect_add_rnic(tni, frsz);
	tnisz->rmsgsz[i] = frsz;
    }
    rgetaddr->nent = ntni;
}

/*
 * utf_cqselect_rget_setremote is to setup remote vcqid with path id
 */
static inline void
utf_cqselect_rget_setremote(struct tni_info *tinfo, int rrank,
			    struct utf_vcqid_stadd *rndzdata,
			    struct utf_vcqid_stadd *rgetsender)
{
    int		i, nent = rndzdata->nent;
    struct tofu_vname	*vnm = (struct tofu_vname*) tinfo->vnamp;
    union jtofu_path_coords pcoords;

    pcoords = vnm[rrank].pcoords[0];
    for (i = 0; i < nent; i++) {
	uint64_t	vcqid;
	vcqid = rndzdata->vcqid[i];
	// UTOFU_CALL(1, utofu_set_vcq_id_path, &vcqid, pcoords.a);
	UTOFU_CALL(1, utofu_get_path_id, vcqid, pcoords.a, &rgetsender->pathid[i]);
	rgetsender->vcqid[i] = vcqid;
	rgetsender->stadd[i] = rndzdata->stadd[i];
    }
    rgetsender->nent = nent;
}

