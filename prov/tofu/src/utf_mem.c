#include <utofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_queue.h"
#include "utf_sndmgt.h"

/* For Sender Side remote index of receiver buffer */
sndmgt		 *egrmgt;	/* array of sndmgt keeping remote index of eager receiver buffer */
utofu_stadd_t	egrmgtstadd;	/* stadd of sndmgt */

/* For Sender-side Eager Buffer */
static struct utf_egr_sbuf	*utf_egsbuf;
static int	utf_egsbufsize;
static utfslist	utf_egsbuffree;
static utofu_stadd_t	egsbfstadd;

/* For Sender-side message information */
static struct utf_send_msginfo	*utf_sndminfo_pool;
static int	utf_sndminfosize;
static utfslist	utf_sndminfofree;
static utofu_stadd_t	sndminfostadd;

/* For Sender-side control structure */
static struct utf_send_cntr	*utf_scntrp;
utofu_stadd_t	sndctrstadd, sndctrstaddend;
static int	utf_scntrsize;
static utfslist	utf_scntrfree;
static uint16_t	*rank2scntridx; /* destination rank to sender control index */
/* For RMA */
static struct utf_rma_cmplinfo	*utf_rmacmplip;
utofu_stadd_t	rmacmplistadd;

/* For Receiver-side eager message handling */
struct erecv_buf *erbuf;	/* eager receiver buffer */
utofu_stadd_t	erbstadd;	/* stadd of eager receiver buffer */
#ifndef UTF_NATIVE
utfslist	utf_fitag_explst;	/* expected tagged message list */
utfslist	utf_fitag_uexplst;	/* unexpected tagged message list */
utfslist	utf_fimsg_explst;	/* expected message list */
utfslist	utf_fimsg_uexplst;	/* unexpected message list */
#else
utfslist		utf_explst;	/* expected message list */
utfslist		utf_uexplst;	/* unexpected message list */
#endif
#if 0
struct utf_msglst	utf_explst_head;  /* expected message list */
struct utf_msglst	utf_explst_tail;  /* expected message list */
struct utf_msglst	utf_uexplst_head; /* unexpected message list */
struct utf_msglst	utf_uexplst_tail; /* unexpected message list */
#endif
struct utf_msglst	*utf_msglst_pool;  /* shared by explst and unexplst */
uint32_t		utf_msglst_size;  /* */
static utfslist		utf_msglstfree;
//static utf_bits		*utf_msglst_bits; /* */

struct utf_msgreq	*utf_msgreq_pool;
uint32_t		utf_msgreq_size;
static utfslist		utf_msgreqfree;
//static utf_bits		*utf_msgreq_bits;

void *
utf_malloc(size_t sz)
{
    void	*ptr = malloc(sz);
    if (ptr == NULL) {
	utf_printf("%s: cannot allocate memory size(%ld)\n", __func__, sz);
	abort();
    }
    return ptr;
}

void
utf_free(void *ptr)
{
    free(ptr);
}


void
utf_msglst_init()
{
    int	i;
    
    utf_msglst_pool = utf_malloc(sizeof(struct utf_msglst)*REQ_SIZE);
    SYSERRCHECK_EXIT(utf_msglst_pool, ==, NULL, "Not enough memory");
    utfslist_init(&utf_msglstfree, NULL);
    for (i = 0; i < REQ_SIZE; i++) {
	utfslist_append(&utf_msglstfree, &utf_msglst_pool[i].slst);
	utf_msglst_pool[i].reqidx = 0;
    }
    utf_msglst_size = REQ_SIZE;
#ifndef UTF_NATIVE
    utfslist_init(&utf_fitag_explst, NULL);/* expected tagged message list */
    utfslist_init(&utf_fitag_uexplst, NULL);/* unexpected tagged message list*/
    utfslist_init(&utf_fimsg_explst, NULL);/* expected message list */
    utfslist_init(&utf_fimsg_uexplst, NULL);/* unexpected message list */
#else
    utfslist_init(&utf_explst, NULL);
    utfslist_init(&utf_uexplst, NULL);
#endif
}
    
static struct utf_msglst *
utf_msglst_alloc()
{
    utfslist_entry *slst = utfslist_remove(&utf_msglstfree);
    struct utf_msglst	*mp;

    if (slst != NULL) {
	// utf_printf("%s allocate entry: %d\n", __func__, pos);
	mp = container_of(slst, struct utf_msglst, slst);
	return mp;
    } else {
	utf_printf("%s: No more msglst\n", __func__);
	abort();
	return NULL;
    }
}


void
utf_msglst_free(struct utf_msglst *msl)
{
    utfslist_insert(&utf_msglstfree, &msl->slst);
}


struct utf_msglst *
utf_msglst_append(struct utfslist *head, struct utf_msgreq *req)
{
    struct utf_msglst	*mlst;

    mlst = utf_msglst_alloc();
    mlst->hdr = req->hdr;
    mlst->reqidx = req - utf_msgreq_pool;
    utfslist_append(head, &mlst->slst);
    return mlst;
}


void
utf_msgreq_init()
{
    int	i;
    
    utf_msgreq_pool = utf_malloc(sizeof(struct utf_msgreq)*REQ_SIZE);
    SYSERRCHECK_EXIT(utf_msgreq_pool, ==, NULL, "Not enough memory");
    utf_msgreq_size = REQ_SIZE;
    utfslist_init(&utf_msgreqfree, NULL);
    for (i = 0; i < REQ_SIZE; i++) {
	utfslist_append(&utf_msgreqfree, &utf_msgreq_pool[i].slst);
    }
    // utf_msgreq_bits = utf_bits_alloc(REQ_SIZE);
    // utf_bits_setall(REQ_SIZE, utf_msgreq_bits);
    // utf_printf("YI %s: utf_msgreq_bits=%lx\n", __func__, utf_msgreq_bits[0]);
}


struct utf_msgreq *
utf_msgreq_alloc()
{
    utfslist_entry *slst = utfslist_remove(&utf_msgreqfree);
    if (slst != 0) {
	struct utf_msgreq *req;
	// utf_printf("%s allocate entry: %d\n", __func__, pos);
	req = container_of(slst, struct utf_msgreq, slst);
	memset(req, 0, sizeof(*req));
	return req;
    } else {
	utf_printf("%s No more msgreq buffer\n", __func__);
	abort();
	return NULL;
    }
}

void
utf_msgreq_free(struct utf_msgreq *req)
{
    req->status = REQ_NONE;
    utfslist_insert(&utf_msgreqfree, &req->slst);
}


void
utf_egrsbuf_init(utofu_vcq_hdl_t vcqh, int entries)
{
    int	rc, i;
    rc = posix_memalign((void*) &utf_egsbuf, 256,
			sizeof(struct utf_egr_sbuf)*entries);
    SYSERRCHECK_EXIT(rc, !=, 0, "Not enough memory");
    memset(utf_egsbuf, 0, sizeof(struct utf_egr_sbuf)*entries);

    UTOFU_CALL(1, utofu_reg_mem, vcqh, (void *)utf_egsbuf,
	       sizeof(struct utf_egr_sbuf)*entries, 0, &egsbfstadd);
    utf_egsbufsize = entries;
    utfslist_init(&utf_egsbuffree, NULL);
    for (i = 0; i < entries; i++) {
	utfslist_append(&utf_egsbuffree, &utf_egsbuf[i].slst);
    }
}

void
utf_egrsbuf_fin(utofu_vcq_hdl_t vcqh)
{
    UTOFU_CALL(0, utofu_dereg_mem, vcqh, egsbfstadd, 0);
}

void
utf_egrsbuf_free(struct utf_egr_sbuf *p)
{
    utfslist_insert(&utf_egsbuffree, &p->slst);
}

struct utf_egr_sbuf *
utf_egrsbuf_alloc(utofu_stadd_t	*stadd)
{
    utfslist_entry *slst = utfslist_remove(&utf_egsbuffree);
    struct utf_egr_sbuf *uesp;
    int	pos;
    uesp = container_of(slst, struct utf_egr_sbuf, slst);
    if (!uesp) {
	utf_printf("%s No more eager sender buffer\n", __func__);
	return NULL;
    }
    pos = uesp - utf_egsbuf;
    *stadd = egsbfstadd + sizeof(struct utf_egr_sbuf)*pos;
    // utf_printf("%s: YI*** pos(%d) stadd(%lx)\n", __func__, pos, *stadd);
    return uesp;
}


/*
 * The maximum size of utf_send_cntr comes from
 *   - the length of the mypos field of struct utf_send_cntr, now uint16_t.
 *	that keeps the index of the utf_scntrp array.
 *   - the length of the rank2scntridx's type, now uint16_t.
 */
void
utf_scntr_init(utofu_vcq_hdl_t vcqh, int nprocs,
	       int scntr_entries, int rmacntr_entries)
{
    int	rc, i;
    size_t	scntr_sz, rmacntr_sz, tot_sz;

    scntr_sz = sizeof(struct utf_send_cntr)*scntr_entries;
    rmacntr_sz = sizeof(struct utf_rma_cmplinfo)*rmacntr_entries;
    tot_sz = scntr_sz + rmacntr_sz;
    rc = posix_memalign((void*) &utf_scntrp, 256, tot_sz);
    SYSERRCHECK_EXIT(rc, !=, 0, "Not enoguh memory");

    memset(utf_scntrp, 0, tot_sz);
    UTOFU_CALL(1, utofu_reg_mem_with_stag, vcqh, (void*) utf_scntrp,
	       tot_sz, TAG_SNDCTR, 0, &sndctrstadd);
    utf_scntrsize = scntr_entries;
    sndctrstaddend = sndctrstadd + scntr_sz;

    utfslist_init(&utf_scntrfree, NULL);
    for (i = 0; i < scntr_entries; i++) {
	utfslist_append(&utf_scntrfree, &utf_scntrp[i].slst);
	utf_scntrp[i].mypos = i;
    }
    /* rank2scntridx table is allocated */
    rank2scntridx = utf_malloc(sizeof(uint16_t)*nprocs);
    SYSERRCHECK_EXIT(rank2scntridx, ==, NULL, "Not enough memory");
    for(i = 0; i < nprocs; i++) {
	rank2scntridx[i] = -1;
    }

    /* setup RMA control area */
    utf_rmacmplip = (struct utf_rma_cmplinfo*) (((uint64_t) utf_scntrp) + scntr_sz);
    rmacmplistadd = sndctrstadd + scntr_sz;
    memset(utf_rmacmplip, 0, rmacntr_sz);
}

int
is_scntr(utofu_stadd_t val, int *evtype)
{
    uint64_t	entry, off;
    if (val >= sndctrstadd && val <= sndctrstaddend) {
	entry = (val - sndctrstadd) / sizeof(struct utf_send_cntr);
	off = val - sndctrstadd - entry*sizeof(struct utf_send_cntr);
	if (off == SCNTR_RGETDONE_OFFST) {
	    *evtype = EVT_RMT_RGETDON;
	} else if (off == SCNTR_RST_RECVRESET_OFFST) {
	    *evtype = EVT_RMT_RECVRST;
	} else {
	    utf_printf("%s: val(0x%lx) sndctrstadd(0x%lx) off(0x%lx)\n",
		     __func__, val, sndctrstadd, off);
	    abort();
	}
	return 1;
    }
    return 0;
}

struct utf_send_cntr *
utf_idx2scntr(int idx)
{
    if (idx < 0 || idx > utf_scntrsize) {
	utf_printf("%s: Internal error: out of range on send control (%d)\n",
		 __func__, idx);
	abort();
    }
    return &utf_scntrp[idx];
}

void
utf_scntr_free(int idx)
{
    struct utf_send_cntr *head;
    uint16_t	headpos;

    headpos = rank2scntridx[idx];
    if (headpos != (uint16_t) -1) {
	head = &utf_scntrp[headpos];
	utfslist_insert(&utf_scntrfree, &head->slst);
	rank2scntridx[idx] = -1;
    }
}

int
scntr2idx(struct utf_send_cntr *scp)
{
    int	idx;
    idx = scp - utf_scntrp;
    assert(idx < utf_scntrsize);
    return idx;
}

void
utf_sndminfo_init(utofu_vcq_hdl_t vcqh, int entries)
{
    int	rc, i;
    
    rc = posix_memalign((void*) &utf_sndminfo_pool, 256,
			sizeof(struct utf_send_msginfo)*entries);
    SYSERRCHECK_EXIT(rc, !=, 0, "Not enough memory");
    memset(utf_sndminfo_pool, 0, sizeof(struct utf_send_msginfo)*entries);

    UTOFU_CALL(1, utofu_reg_mem, vcqh, (void *)utf_sndminfo_pool,
	       sizeof(struct utf_send_msginfo)*entries, 0, &sndminfostadd);
    utf_sndminfosize = entries;
    utfslist_init(&utf_sndminfofree, NULL);
    for (i = 0; i < entries; i++) {
	utfslist_append(&utf_sndminfofree, &utf_sndminfo_pool[i].slst);
    }
}


struct utf_send_msginfo *
utf_sndminfo_alloc()
{
    utfslist_entry *slst = utfslist_remove(&utf_sndminfofree);
    struct utf_send_msginfo *smp;
    if (slst != NULL) {
	smp = container_of(slst, struct utf_send_msginfo, slst);
	return smp;
    } else {
	utf_printf("%s: No more sender buffer\n", __func__);
	return NULL;
    }
}

void
utf_sndminfo_free(struct utf_send_msginfo *smp)
{
    utfslist_insert(&utf_sndminfofree, &smp->slst);
}

struct utf_send_cntr *
utf_scntr_alloc(int dst, utofu_vcq_id_t rvcqid, uint64_t flgs)
{
    struct utf_send_cntr *scp;
    uint16_t	headpos;
    headpos = rank2scntridx[dst];
    if (headpos != (uint16_t)-1) {
	scp = &utf_scntrp[headpos];
    } else {
	/* No head */
	utfslist_entry *slst = utfslist_remove(&utf_scntrfree);
	if (slst == NULL) {
	    scp = NULL;
	    goto err;
	}
	scp = container_of(slst, struct utf_send_cntr, slst);
	rank2scntridx[dst]  = scp->mypos;
	scp->dst = dst;
	utfslist_init(&scp->smsginfo, NULL);
	utfslist_init(&scp->rmawaitlst, NULL);
	scp->state = S_NONE;
	scp->flags = UTOFU_ONESIDED_FLAG_PATH(flgs);
	scp->rvcqid = rvcqid;
	DEBUG(DLEVEL_UTOFU) {
	    utf_printf("%s: flag_path(%0x)\n", __func__, scp->flags);
	}
    }
err:
    // utf_printf("%s: dst(%d) scp(%p) headpos(0x%x) -1(0x%x)\n", __func__, dst, scp, headpos, (uint16_t)-1);
    return scp;
}

int
is_recvbuf(utofu_stadd_t val) {
    if (val >= erbstadd && val <= (erbstadd+sizeof(struct erecv_buf))) {
	return 1;
    } else {
	return 0;
    }
}

void
utf_recvbuf_init(utofu_vcq_id_t vcqh, int nprocs)
{
    int  rc;
    long algn = sysconf(_SC_PAGESIZE);
    rc = posix_memalign((void*) &erbuf, algn, sizeof(struct erecv_buf));
    SYSERRCHECK_EXIT(rc, !=, 0, "Not enough memory");
    memset(erbuf, 0, sizeof(struct erecv_buf));

    UTOFU_CALL(1, utofu_reg_mem_with_stag,
	       vcqh, (void *)erbuf, sizeof(struct erecv_buf),
	       TAG_ERBUF, 0, &erbstadd);
    erbuf->header.cntr = MSG_PEERS - 1;

    rc = posix_memalign((void*) &egrmgt, 256, sizeof(sndmgt)*nprocs);
    SYSERRCHECK_EXIT(rc, !=, 0, "Not enough memory");
    memset(egrmgt, 0, sizeof(sndmgt)*nprocs);

    UTOFU_CALL(1, utofu_reg_mem_with_stag,
	       vcqh, (void *)egrmgt, sizeof(sndmgt)*nprocs,
	       TAG_EGRMGT, 0, &egrmgtstadd);

    utf_printf("%s: erbuf(%p) erbstadd(%lx)\n", __func__, erbuf, erbstadd);
}
 

void
erecvbuf_printcntr()
{
    utf_printf("erbuf counter(%d)\n", erbuf->header.cntr);
}

void
erecvbuf_dump(int idx, int cnt)
{
    int		i;
    char	*bp =  &erbuf->rbuf[MSGBUF_SIZE*idx];

    utf_printf("erbuf entry(%d)\t", idx);
    for(i = 0; i < cnt; i++) printf(":%d", bp[i]);
    printf("\n"); fflush(stdout);
}

struct utf_msgbdy *
utf_recvbuf_get(int idx)
{
    struct utf_msgbdy *bp = (struct utf_msgbdy *)&erbuf->rbuf[MSGBUF_SIZE*idx];
    return bp;
}

utfslist		utf_wait_rmacq;
static utfslist		utf_free_rmacq;
static struct utf_rma_cq *utf_rmacq_pool;

void
utf_rmacq_init()
{
    int	i;
    utf_rmacq_pool = utf_malloc(sizeof(struct utf_rma_cq)*RMACQ_SIZE);
    utfslist_init(&utf_free_rmacq, NULL);
    for (i = 0; i < RMACQ_SIZE; i++) {
	utfslist_append(&utf_free_rmacq, &utf_rmacq_pool[i].slst);
    }
}

struct utf_rma_cq *
utf_rmacq_alloc()
{
    struct utfslist_entry *slst = utfslist_remove(&utf_free_rmacq);
    struct utf_rma_cq *cq;
    if (slst == NULL) {
	utf_printf("%s: no more RMA CQ entry\n", __func__);
	abort();
    }
    cq = container_of(slst, struct utf_rma_cq, slst);
    return cq;
}

void
utf_rmacq_free(struct utf_rma_cq *cq)
{
    utfslist_insert(&utf_free_rmacq, &cq->slst);
}


utofu_stadd_t
utf_mem_reg(utofu_vcq_hdl_t vcqh, void *buf, size_t size)
{
    utofu_stadd_t	stadd = 0;

    UTOFU_CALL(1, utofu_reg_mem, vcqh, buf, size, 0, &stadd);
    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	utf_printf("%s: size(%ld/0x%lx) vcqh(%lx) buf(%p) stadd(%lx)\n", __func__, size, size, vcqh, buf, stadd);
    }
    return stadd;
}

void
utf_mem_dereg(utofu_vcq_id_t vcqh, utofu_stadd_t stadd)
{
    UTOFU_CALL(1, utofu_dereg_mem, vcqh, stadd, 0);
    DEBUG(DLEVEL_PROTO_RMA|DLEVEL_ADHOC) {
	utf_printf("%s: vcqh(%lx) stadd(%lx)\n", __func__, vcqh, stadd);
    }
    return;
}

void
utf_stadd_free(utofu_vcq_hdl_t vcqh)
{
    UTOFU_CALL(0, utofu_dereg_mem, vcqh, erbstadd, 0);
    UTOFU_CALL(0, utofu_dereg_mem, vcqh, egrmgtstadd, 0);
}

void
utf_showstadd()
{
    utf_printf("***** st-addresses *****\n"
	     "erbstadd (erbuf):        %lx (receiver buffer for eager)\n"
	     "egsbfstadd (utf_egsbuf): %lx (sender buffer for eager)\n"
	     "egrmgtstadd (egrmgt):   %lx (array of sndmgt)\n",
	     erbstadd, egsbfstadd, egrmgtstadd);
}
