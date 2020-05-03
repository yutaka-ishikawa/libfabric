#include "utf_list.h"
/*
 */
#define EVT_START	0
#define EVT_LCL		1
#define EVT_RMT_RGETDON	2	/* remote armw operation */
#define EVT_RMT_RECVRST	3	/* remote armw operation */
#define EVT_RMT_GET	4	/* remote get operation */
#define EVT_CONT	5
#define EVT_END		6

#ifdef UTF_NATIVE
#pragma pack(1)
struct utf_msghdr { /* 16 Byte */
    uint32_t	src;
    uint32_t	tag:32;
    size_t	size;
};
#else /* for Fabric */
#pragma pack(1)
struct utf_msghdr { /* 40 Byte */
    uint64_t	src;
    uint64_t	tag;
    size_t	size;
    uint64_t	data;
    uint64_t	flgs;
};
#endif
#define MSGHDR_SIZE sizeof(struct utf_msghdr)

/* sizeof(uint16_t) is reserved for entire message size.
 * See struct utf_msgbdy */
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
 * MSG_SIZE 1920
 * MSG_PAYLOAD_SIZE = 1918
 * MSG_EAGER_SIZE = 1878
 */
#pragma pack(1)
struct utf_msgbdy {
    uint16_t	psize:14,	/* 16 kByte max */
    		ptype: 2;	/* 0 : eager
				 * 1 : rendezous
				 * 2 : fi_writmsg
				 * 3 : fi_readmsg */
    union {
	struct utf_hpacket	h_pkt;
	uint8_t		rawdata[MSG_PAYLOAD_SIZE];
    } payload;
};

enum {
    PKT_EAGER = 0,
    PKT_RENDZ = 1,
    PKT_WRITE = 2,
    PKT_READ  = 3
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
	utfslist_entry	slst;
	struct utf_msgbdy	msgbdy;
    };
};

/*
 * req->status
 */
enum utq_reqstatus {
    REQ_NONE	= 0,
    REQ_DONE,
    REQ_OVERRUN,
    REQ_WAIT_RNDZ		/* waiting rendzvous */
};

/*
 * recv_ctr state
 */
enum {
    REQ_RECV_EXPECTED,
    REQ_RECV_UNEXPECTED,
    REQ_RECV_EXPECTED2,
    REQ_SND_REQ,
};

struct utf_msgreq {
    struct utf_msghdr hdr;	/* 16: message header */
    uint8_t	*buf;		/* 24: buffer address */
    utofu_stadd_t bufstadd;	/* 32: stadd of the buffer address */
    utofu_stadd_t rmtstadd;	/* 40: stadd of the remote address */
    uint8_t	ustatus;	/* 41: user-level status */
    uint8_t	fistatus;	/* 42: fabric-level status */
    uint8_t	status;		/* 43: utf-level  status */
    uint8_t	type:4,		/* 44: EXPECTED or UNEXPECTED or SENDREQ */
		ptype:4;	/* PKT_EAGER | PKT_RENDZ | PKT_WRITE | PKT_READ */
    size_t	rsize;		/* 52: utf received size */
    size_t	expsize;	/* 60: expected size in expected queue */
    utfslist_entry slst;	/* 68: list */
    void	(*notify)(struct utf_msgreq*);
    struct utf_recv_cntr *rcntr; /* point to utf_recv_cntr */
#ifndef UTF_NATIVE
    /* for Fabric and expected message */
    void	*fi_ctx;
    uint64_t	fi_ignore;
    uint64_t	fi_flgs;
    void	*fi_ucontext;
    size_t	fi_iov_count;
    struct iovec fi_msg[4];	/* TOFU_IOV_LIMIT */
#endif
};

struct utf_msglst {
    utfslist_entry	slst;	/*  8 B */
    struct utf_msghdr	hdr;	/* 24 B */
    uint32_t		reqidx;	/* 28 B: index of utf_msgreq */
#ifndef UTF_NATIVE
    uint64_t	fi_ignore;	/* ignore */
    uint64_t	fi_flgs;
    void	*fi_context;
#endif
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
#ifndef UTF_NATIVE /* Fabric */
extern utfslist	utf_fitag_explst;	/* expected tagged message list */
extern utfslist	utf_fitag_uexplst;	/* unexpected tagged message list */
extern utfslist	utf_fimsg_explst;	/* expected message list */
extern utfslist	utf_fimsg_uexplst;	/* unexpected message list */
#else
extern utfslist	utf_explst;	/* expected message list */
extern utfslist	utf_uexplst;	/* unexpected message list */
#endif /* ~UTF_NATIVE */

#ifdef UTF_NATIVE
static inline int
utf_uexplst_match(uint32_t src, uint32_t tag, int peek)
{
    struct utf_msglst	*msl;
    uint32_t		idx;
    utfslist_entry	*cur, *prev;

    if (utfslist_isnull(&utf_uexplst)) {
	return -1;
    }
    if (src == -1 && tag == -1) {
	cur = utf_uexplst.head; prev = 0;
	msl = container_of(cur, struct utf_msglst, slst);
	goto find;
    } else if (src == -1 && tag != -1) {
	utfslist_foreach2(&utf_uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (tag == msl->hdr.tag) {
		goto find;
	    } 
	}
    } else if (tag == -1) {
	utfslist_foreach2(&utf_uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (src == msl->hdr.tag) {
		goto find;
	    } 
	}
    } else {
	utfslist_foreach2(&utf_uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (src == msl->hdr.src && tag == msl->hdr.tag) {
		goto find;
	    } 
	}
    }
    return -1;
find:
    idx = msl->reqidx;
    if (peek == 0) {
	utfslist_remove2(&utf_uexplst, cur, prev);
	utf_msglst_free(msl);
    }
    return idx;
}

static inline int
utf_explst_match(uint32_t src, uint32_t tag, int flg)
{
    struct utf_msglst	*msl;
    utfslist_entry	*cur, *prev;
    uint32_t		idx;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("explst_match: utf_explst(%p) src(%d) tag(%d)\n",
		 &utf_explst, src, tag);
    }
    if (utfslist_isnull(&utf_explst)) {
	return -1;
    }
    utfslist_foreach2(&utf_explst, cur, prev) {
	msl = container_of(cur, struct utf_msglst, slst);
	uint32_t exp_src = msl->hdr.src;
	uint32_t exp_tag = msl->hdr.tag;
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("\t exp_src(%d) exp_tag(%x)\n", exp_src, exp_tag);
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
	utfslist_remove2(&utf_explst, cur, prev);
	utf_msglst_free(msl);
    }
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("\t return idx(%d)\n", idx);
    }
    return idx;
}

#else

static inline int
tofu_utf_uexplst_match(utfslist *uexplst, uint64_t src, uint64_t tag, uint64_t ignore, int peek)
{
    struct utf_msglst	*msl;
    utfslist_entry	*cur, *prev;
    uint32_t		idx;

    //utf_printf("tofu_utf_uexplst_match: uexplst(%p) src(%ld) tag(%lx) ignore(%lx) peek(%d)\n",
    //uexplst, src, tag, ignore, peek);
    if (utfslist_isnull(uexplst)) {
	//utf_printf("\t: list is null\n");
	return -1;
    }
    if (src == -1UL) {
	utfslist_foreach2(uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if ((tag & ~ignore) == (msl->hdr.tag & ~ignore)) {
		goto find;
	    }
	}
    } else {
	utfslist_foreach2(uexplst, cur, prev) {
	    msl = container_of(cur, struct utf_msglst, slst);
	    if (src == msl->hdr.src &&
		(tag & ~ignore) == (msl->hdr.tag & ~ignore)) {
		goto find;
	    }
	}
    } /* not found */
    //utf_printf("\t: not found\n");
    return -1;
find:
    idx = msl->reqidx;
    //utf_printf("\t: found idx(%d)\n", idx);
    if (peek == 0) {
	utfslist_remove2(uexplst, cur, prev);
	utf_msglst_free(msl);
    }
    return idx;
}

static inline int
tofu_utf_explst_match(utfslist *explst, uint32_t src, uint64_t tag,  int peek)
{
    struct utf_msglst	*mlst;
    utfslist_entry	*cur, *prev;
    uint32_t		idx;

    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("tofu_utf_explst_match: explst(%p) src(%d) tag(%d)\n",
		 explst, src, tag);
    }
    if (utfslist_isnull(explst)) {
	return -1;
    }
    utfslist_foreach2(explst, cur, prev) {
	mlst = container_of(cur, struct utf_msglst, slst);
	uint64_t exp_src = mlst->hdr.src;
	uint64_t exp_tag = mlst->hdr.tag;
	uint64_t exp_rvignr = ~mlst->fi_ignore;
	DEBUG(DLEVEL_PROTOCOL) {
	    utf_printf("\t mlst(%p) exp_src(%d) exp_tag(%x) exp_rvignr(%lx)\n", mlst, exp_src, exp_tag, exp_rvignr);
	}
	if (exp_src == -1 && (tag & exp_rvignr) == (exp_tag & exp_rvignr)) {
	    goto find;
	} else if (exp_src == src
		   && (tag & exp_rvignr) == (exp_tag & exp_rvignr)) {
	    goto find;
	}
    }
    return -1;
find:
    idx = mlst->reqidx;
    //utf_printf("\t return idx(%d) mlst(%p) exp_src(%ld) exp_tag(0x%lx) exp_ignore(0x%lx)\n", idx, mlst, mlst->hdr.src, mlst->hdr.tag, mlst->fi_ignore);
    if (peek == 0) {
	utfslist_remove2(explst, cur, prev);
	utf_msglst_free(mlst);
    }
    DEBUG(DLEVEL_PROTOCOL) {
	utf_printf("\t return idx(%d)\n", idx);
    }
    return idx;
}
#endif /* ~UTF_NATIVE */

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
    R_WAIT_RNDZ		= 4,
    R_DO_RNDZ		= 5,
    R_DO_READ		= 6,
    R_DO_WRITE		= 7,
    R_DONE		= 8,
} rstate;
	

/*
 * one utf_recv_cntr is reserved for one sender
 */
struct utf_recv_cntr {
    uint8_t	state;		/* rstate */
    uint8_t	initialized;	/* flag for sender's rendezous info  */
    uint8_t	rst_sent;	/* reset request is sent to the sender */
    uint8_t	mypos;		/* my position of recv_cntr pool */
    uint32_t	recvoff;	/* for eager message */
    struct utf_msghdr	hdr;	/* received header */
    struct utf_msgreq	*req;	/* The current handle message request */
    utofu_vcq_id_t	svcqid;	/* rendezous: sender's vcqid */
    uint64_t		flags;	/* rendezous: sender's flags */
    int		sidx;		/* rendezous: sender's sidx */
    //utfslist	rget_cqlst;	/* CQ for remote get operation in UTF level */
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
    uint32_t		rgetdone:1,	/* remote get done */
			ineager: 1,	/* */
			rgetwait:2,	/* */
			state: 4,	/* upto 15 states */
			ostate: 4,
			mypos: 20;	/* */
					/*  +4 =  4 Byte */
    uint32_t		rcvreset: 1,	/* ready for resetting recv offset */
			recvoff: 31;	/*  +4 =  8 Byte */
    uint32_t		dst;		/*  +4 = 12 Byte */
    uint64_t		flags;		/*  +8 = 20 Byte */
    utofu_vcq_id_t	rvcqid;		/*  +8 = 28 Byte */
    size_t		psize;		/* packet-level sent size  +8=36 Byte */
    size_t		usize;		/* user-level sent size */
    utfslist		smsginfo;	/* +16 = 52 Byte */
    union {
	uint8_t		desc[32];	/* +32 = 84 Byte */
	utfslist_entry	slst;		/* for free list */
    };
};

struct utf_send_msginfo { /* msg info */
    struct utf_msghdr	msghdr;		/* message header     +16 = 16 Byte */
    struct utf_egr_sbuf	*sndbuf;	/* send data for eger  +8 = 24 Byte */
    utofu_stadd_t	sndstadd;	/* stadd of sndbuf     +8 = 40 Byte */
    utofu_stadd_t	usrstadd;	/* stadd of user buf for rdma  +8 = 48 Byte */
    void		*usrbuf;	/* stadd of user buf   +8 = 48 Byte */
    struct utf_msgreq	*mreq;		/* request struct      +8 = 32 Byte */
    utfslist_entry		slst;		/* next pointer        +8 = 56 Byte */
    uint8_t		cntrtype;
#ifndef UTF_NATIVE
    void		*context;	/* for Fabric */
#endif
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
    utfslist_entry		slst;		/*  +8 =124 Byte */
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
