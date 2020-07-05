#include <stdatomic.h>

#define TOFU_NICSIZE	6	/* number of NIC */
/*
 * 128 B (8 + 3 + 6 + 6 + 9 + 8*6 * 2)
 */
#pragma pack(1)
struct tni_info {
    void		*vnamp;		/* struct tofu_vname */
    uint8_t		ppn;		/* process per node */
    uint8_t		nrnk;		/* rank within node */
    uint8_t		ntni;
    uint8_t		idx[TOFU_NICSIZE]; /* index of snd_len/rcv_len of cqsel_tab */
    uint8_t		usd[TOFU_NICSIZE];
    uint8_t		rsv[9];
    utofu_vcq_hdl_t	vcqhdl[TOFU_NICSIZE];
    utofu_vcq_id_t	vcqid[TOFU_NICSIZE];
};

#pragma pack(1)
struct cqsel_table { /* 6240 B */
    struct tni_info	node[48];	/* 6144 B = 128 * 48 */
    atomic_ulong	snd_len[TOFU_NICSIZE]; /* 48 B */
    atomic_ulong	rcv_len[TOFU_NICSIZE]; /* 48 B */
};

extern struct cqsel_table	*utf_cqseltab;
extern struct tni_info		*utf_tinfo;
extern int	utf_nrnk;
extern int	utf_mrail;
