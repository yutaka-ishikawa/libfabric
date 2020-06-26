#include <stdatomic.h>
#define TOFU_NICSIZE	6	/* number of NIC */
/*
 * 128 B (1 + 6 + 6 + 19 + 8*6 * 2)
 */
#pragma pack(1)
struct tni_info {
    uint8_t		ntni;
    uint8_t		idx[TOFU_NICSIZE]; /* index of snd_len/rcv_len of cqsel_tab */
    uint8_t		bsy[TOFU_NICSIZE];
    uint8_t		rsv[19];
    utofu_vcq_hdl_t	vcqh[TOFU_NICSIZE];
    utofu_vcq_id_t	vcqid[TOFU_NICSIZE];
};

#pragma pack(1)
struct cqsel_table { /* 6240 B */
    struct tni_info	node[48];	/* 6144 B = 128 * 48 */
    atomic_ulong	snd_len[TOFU_NICSIZE]; /* 48 B */
    atomic_ulong	rcv_len[TOFU_NICSIZE]; /* 48 B */
};

extern struct cqsel_table	*utf_cqseltab;
extern int	utf_nrnk;
extern int	utf_mrail;

static inline utofu_vcq_id_t
utf_cqselect_send(utofu_vcq_hdl_t *vcqh, size_t sz)
{
    struct tni_info	*tinfo = &utf_cqseltab->node[utf_nrnk];
    uint8_t	tni;
    int		sel = 0;

    if (utf_mrail == 0) { /* The first entry is default CQ and TNI */
	*vcqh  = tinfo->vcqh[0];
	tinfo->bsy[0]++;
	tni = tinfo->idx[0];
	atomic_fetch_add(&utf_cqseltab->snd_len[tni], sz);
	return tinfo->vcqid[0];
    }
    /* at this time */
    // *vcqh  = tinfo->vcqh[sel];
    *vcqh  = sel;
    tinfo->bsy[sel]++;
    tni = tinfo->idx[sel];
    //utf_cqseltab->snd_len[tni] += sz;
    //atomic_fetch_add(&utf_cqseltab->snd_len[tni], sz);
    /* */
    utf_printf("%s: vcqh(%lx) tni(%d) msglen(%ld)\n", __func__, *vcqh, tni, utf_cqseltab->snd_len[tni]);
    return tinfo->vcqid[sel];
}

static inline utofu_vcq_id_t
utf_cqselect_recv()
{
    struct tni_info	*tinfo = &utf_cqseltab->node[utf_nrnk];

    return tinfo->vcqid[0];
}

/*
 *
 * stag:
 *	utf_scntr_init: TAG_SNDCTR
 *		utf_scntrp (sndctstadd)
 *	utf_recvbuf_init: TAG_ERBUF, TAG_EGRMGT
 *		erbuf (erbstadd), egrmgt (egrmgtstadd)
 */
