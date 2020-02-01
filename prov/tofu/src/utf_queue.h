#include "utf_list.h"
/*
 * In case of MPICH
 *  #define MPI_TAG_UB           0x64400001	32 bit
 */

/* The following lines must be deleted because edata is only 1B available */
//#define EDATA_ASSEMBLE(sidx, ridx)	(((sidx) << 16) | ((ridx) & 0xffff))
//#define EDATA_SEND_CNTR_IDX(edata)	((edata)>>16)
//#define EDATA_EGBUF_IDX(edata)		((edata)&0xffff)
//union edata {
//    struct {
//	uint16_t sidx;	/* 4B: index of eager send control array */
//	uint16_t ridx;	/* 6B: index of receiver buffer array */
//	uint16_t size;	/* 8B: size of buffer */
//	uint16_t type;	/* 2B */
//    } encoded;
//    uint64_t	u64;
//};
#define EVT_START	0
#define EVT_LCL		1
#define EVT_RMT_RGETDON	2	/* remote armw operation */
#define EVT_RMT_RECVRST	3	/* remote armw operation */
#define EVT_RMT_GET	4	/* remote get operation */
#define EVT_CONT	5
#define EVT_END		6

#pragma pack(1)
struct utf_msghdr { /* 16 Byte */
    uint32_t	src;
    uint32_t	tag:32;
    size_t	size;
};
#define MSGHDR_SIZE sizeof(struct utf_msghdr)

#define MSG_EAGER_SIZE	(MSG_SIZE - MSGHDR_SIZE - sizeof(uint16_t))
#define MSG_PAYLOAD_SIZE (MSG_SIZE - sizeof(uint16_t))
#define MSG_MAKE_PSIZE(sz) ((sz) + MSGHDR_SIZE + sizeof(uint16_t))
#define MSG_CALC_EAGER_USIZE(ssz) ((ssz) - MSGHDR_SIZE - sizeof(uint16_t))
#define MSG_EAGERONLY	0
#define MSG_RENDEZOUS	1

#pragma pack(1)
struct utf_hpacket {
    struct utf_msghdr	hdr;
    uint8_t		msgdata[MSG_EAGER_SIZE];
};

/*
 * 1920 = 18 + 1902
 */
#pragma pack(1)
struct utf_msgbdy {
    uint16_t	psize:15,	/* 32 kByte max */
    		rndz: 1;	/* 1 for rendezous */
    union {
	struct utf_hpacket	h_pkt;
	uint8_t		rawdata[MSG_PAYLOAD_SIZE];
    } payload;
};

#define EMSG_HDR(msgp) ((msgp)->payload.h_pkt.hdr)
#define EMSG_DATA(msgp) ((msgp)->payload.h_pkt.msgdata)
#define EMSG_SIZE(msgp) ((msgp)->psize - sizeof(uint16_t) - MSGHDR_SIZE)
#define EMSG_DATAONLY(msgp) ((msgp)->payload.rawdata)

/*
 * 1920 = 18 + 1902
 */
struct utf_egr_sbuf {
    union {
	slist_entry	slst;
	struct utf_msgbdy	msgbdy;
    };
};

enum utq_reqstatus {
    REQ_NONE	= 0,
    REQ_DONE,
};

enum {
    REQ_RECV_EXPECTED,
    REQ_RECV_UNEXPECTED,
    REQ_SND_REQ,
};

struct utf_msgreq {
    struct utf_msghdr hdr;	/* 16: message header */
    uint8_t	*buf;		/* 24: buffer address */
    utofu_stadd_t bufstadd;	/*   : stadd of the buffer address */
    utofu_stadd_t rmtstadd;	/**/
    uint16_t	ustatus;	/* 26: user-level status */
    uint8_t	status;		/* 27: utf-level  status */
    uint8_t	type:4,		/* 28: EXPECTED or UNEXPECTED or SENDREQ */
		rndz:4;		/* RENDEZOUS or not */
    size_t	rsize;		/* 36: utf received size */
    slist_entry	slst;		/* 44: list */
};

struct utf_msglst {
    slist_entry		slst;	/*  8 B */
    struct utf_msghdr	hdr;	/* 24 B */
    uint32_t		reqidx;	/* 28 B: index of utf_msgreq */
};

extern struct utf_msgreq	*utf_msgreq_pool;

static inline struct utf_msgreq *
utf_idx2msgreq(uint32_t idx)
{
    return &utf_msgreq_pool[idx];
}

static inline int
utf_msgreq2idx(struct utf_msgreq *req)
{
    return req - utf_msgreq_pool;
}

struct utf_msglst *msl;
extern void	utf_msglst_free(struct utf_msglst *msl);
extern slist	utf_explst;	/* expected message list */
extern slist	utf_uexplst;	/* unexpected message list */

static inline int
utf_uexplst_match(uint32_t src, uint32_t tag, int flg)
{
    struct utf_msglst	*msl;
    uint32_t		idx;

    if (slist_isnull(&utf_uexplst)) {
	return -1;
    }
    if (src == -1 && tag == -1) {
	slist_entry *slst = slist_remove(&utf_uexplst);
	msl = container_of(slst, struct utf_msglst, slst);
	goto find;
    } else if (src == -1 && tag != -1) {
	slist_entry	*cur;
	slist_foreach(&utf_uexplst, cur) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (tag == msl->hdr.tag) {
		goto find;
	    } 
	}
    } else if (tag == -1) {
	slist_entry	*cur;
	slist_foreach(&utf_uexplst, cur) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (src == msl->hdr.tag) {
		goto find;
	    } 
	}
    } else {
	slist_entry	*cur;
	slist_foreach(&utf_uexplst, cur) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (src == msl->hdr.tag && tag == msl->hdr.tag) {
		goto find;
	    } 
	}
    }
    return -1;
find:
    idx = msl->reqidx;
    if (flg) {
	utf_msglst_free(msl);
    }
    return idx;
}

static inline int
utf_explst_match(uint32_t src, uint32_t tag, int flg)
{
    struct utf_msglst	*msl;
    slist_entry		*cur;
    uint32_t		idx;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("explst_match: utf_explst(%p) src(%d) tag(%d)\n",
		 &utf_explst, src, tag);
    }
    if (slist_isnull(&utf_explst)) {
	return -1;
    }
    slist_foreach(&utf_explst, cur) {
	msl = container_of(cur, struct utf_msglst, slst);
	uint32_t exp_src = msl->hdr.src;
	uint32_t exp_tag = msl->hdr.tag;
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("\t exp_src(%d) exp_tag(%d)\n", exp_src, exp_tag);
	}
	if (exp_src == -1 && exp_tag != -1) {
	    goto find;
	} else if (exp_src == -1 && exp_tag == tag) {
	    goto find;
	} else if (exp_tag == -1 && exp_src == src) {
	    goto find;
	} else if (exp_src == src && exp_tag == tag) {
	    goto find;
	}
    }
    return -1;
find:
    idx = msl->reqidx;
    if (flg) {
	utf_msglst_free(msl);
    }
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("\t return idx(%d)\n", idx);
    }
    return idx;
}


/* 24 BYTE, 10 entries in one cache line */
struct uexp_entry {
    struct utf_msghdr	hdr;
    void		*udat;
};

typedef enum rstate {
    R_FREE		= 0,
    R_NONE		= 1,
    R_HEAD		= 2,
    R_BODY		= 3,
    R_DO_RNDZ		= 4,
    R_DONE		= 5,
} rstate;
	

struct utf_recv_cntr {
    uint8_t	state;
    uint8_t	initialized;
    uint8_t	rst_sent;	/* reset request is sent to the sender */
    uint8_t	mypos;
    uint32_t	recvoff;
    struct utf_msghdr	hdr;
    struct utf_msgreq	*req;
    utofu_vcq_id_t	svcqid;
    uint64_t		flags;
};

typedef enum sstate {
    S_FREE		= 0,
    S_NONE		= 1,
    S_REQ_ROOM		= 2,
    S_HAS_ROOM		= 3,
    S_DO_EGR		= 4,
    S_DO_EGR_WAITCMPL	= 5,
    S_DONE_EGR		= 6,
    S_REQ_RDVR		= 7,
    S_RDVDONE		= 8,
    S_DONE		= 9,
    S_WAIT_BUFREADY	= 10
} sstate;

enum {
    SNDCNTR_BUFFERED_EAGER,
    SNDCNTR_INPLACE_EAGER,
    SNDCNTR_RENDEZOUS
};

#define SCNTR_NOROOM	-1
#define SCNTR_HEAD	0
#define SCNTR_TAIL	1
#define SCNTR_OK	1
#define SCNTR_RGETDONE_OFFST		0x0
#define SCNTR_RST_RECVRESET_OFFST	0x4

#define SCNTR_ADDR_CNTR_FIELD(sidx)	\
    (utf_sndctr_stadd() + sizeof(struct utf_send_cntr)*(sidx))

#pragma pack(1)
struct utf_send_cntr {	/* 92 Byte */
    uint32_t		rgetdone:1,	/* ready for resetting recv offset */
			ineager: 1,	/* */
			rgetwait:2,	/* */
			state: 8,	/* upto 15 states */
			mypos: 20;	/* */
					/*  +4 =  4 Byte */
    uint32_t		rcvreset: 1,
			recvoff: 31;	/*  +4 =  8 Byte */
    uint32_t		dst;		/*  +4 = 12 Byte */
    uint64_t		flags;		/*  +8 = 20 Byte */
    utofu_vcq_id_t	rvcqid;		/*  +8 = 28 Byte */
    size_t		psize;		/* packet-level sent size  +8=36 Byte */
    size_t		usize;		/* user-level sent size */
    slist		smsginfo;	/* +16 = 52 Byte */
    union {
	uint8_t		desc[32];	/* +32 = 84 Byte */
	slist_entry	slst;		/* for free list */
    };
};

struct utf_send_msginfo { /* msg info */
    struct utf_msghdr	msghdr;		/* message header     +16 = 16 Byte */
    struct utf_egr_sbuf	*sndbuf;	/* send data for eger  +8 = 24 Byte */
    utofu_stadd_t	sndstadd;	/* stadd of sndbuf     +8 = 40 Byte */
    utofu_stadd_t	usrstadd;	/* stadd of user buf   +8 = 48 Byte */
    void		*usrbuf;	/* stadd of user buf   +8 = 48 Byte */
    struct utf_msgreq	*mreq;		/* request struct      +8 = 32 Byte */
    slist_entry		slst;		/* next pointer        +8 = 56 Byte */
    uint8_t		cntrtype;
};

#if 0
struct utf_send_cntr {	/* 128 Byte */
    uint32_t		rcvreset: 1,
			cntrtype: 7,
			state: 8,
			mypos: 16;	/*  +4 =  4 Byte */
    uint32_t		recvoff;	/*  +4 =  8 Byte */
    struct utf_msghdr	msghdr;		/* +16 = 24 Byte */
    uint32_t		dst;		/*  +4 = 28 Byte */
    uint8_t		desc[32];	/* +32 = 60 Byte */
    uint64_t		flags;		/*  +8 = 68 Byte */
    utofu_vcq_id_t	rvcqid;		/*  +8 = 76 Byte */
    size_t		ssize;		/*  +8 = 84 Byte */
    struct utf_egr_sbuf	*sndbuf;	/*  +8 = 92 Byte */
    utofu_stadd_t	sndstadd;	/*  +8 =100 Byte */
    utofu_stadd_t	usrstadd;	/*  +8 =108 Byte */
    struct utf_msgreq	*mreq;		/*  +8 =116 Byte */
    slist_entry		slst;		/*  +8 =124 Byte */
    uint32_t		rsrv;		/*  +4 =128 Byte */
};
#endif /* 0 */

#define MSGBUF_SIZE	(MSG_SIZE*10)	/* 1920 * 10 */
#define MSGBUF_THR	(MSG_SIZE*5)
union recv_head {
    uint64_t cntr;
    char	 pad[256];
};
struct erecv_buf {
    union recv_head header;
    char	rbuf[MSGBUF_SIZE*MSG_PEERS];
};
#define ERECV_INDEX(addr)	\
   ((addr - erbstadd - sizeof(union recv_head) - 1)/MSGBUF_SIZE)
#define ERECV_LENGTH(addr)	\
   (addr - (erbstadd + sizeof(union recv_head) + ERECV_INDEX(addr)*MSGBUF_SIZE))

#define ERB_CNTR	0
