#include <pmix.h>
#include <pmix_fjext.h>
#include <utofu.h>
#include <jtofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_queue.h"
#include "utf_sndmgt.h"

size_t	utf_pig_size;	/* piggyback size is globally defined */
int	utf_dflag;
int	utf_initialized;

int
utf_getenvint(char *envp)
{
    int	val = 0;
    char	*cp = getenv(envp);
    if (cp) {
	if (!strncmp(cp, "0x", 2)) {
	    sscanf(cp, "%x", &val);
	} else {
	    val = atoi(cp);
	}
    }
    return val;
}

int
utf_printf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;
    va_start(ap, fmt);
    fprintf(stderr, "[%d] ", myrank);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
    return rc;
}

int
utf_fprintf(FILE *fp, const char *fmt, ...)
{
    va_list	ap;
    int		rc;
    va_start(ap, fmt);
    fprintf(fp, "[%d] ", myrank);
    rc = vfprintf(fp, fmt, ap);
    va_end(ap);
    fflush(fp);
    return rc;
}

/* needs to fi it */
extern struct utf_tofuctx	utf_sndctx[16];
extern struct utf_tofuctx	utf_rcvctx[16];

/*
 * utf_init_1:  The first step of the Tofu initialization
 *		We do not know myrank during this phase
 */
int
utf_init_1(void *ctx, int class, utofu_vcq_hdl_t vcqh, size_t pigsz)
{
    int	i;
    if (class == UTF_TX_CTX) {
	utf_printf("%s: utf_sndctx[0] = %p\n", __func__, ctx);
	utf_sndctx[0].vcqh = vcqh;
	utf_sndctx[0].fi_ctx = ctx;
    } else {
	utf_printf("%s: utf_rcvctx[0] = %p\n", __func__, ctx);
	utf_rcvctx[0].vcqh = vcqh;
	utf_rcvctx[0].fi_ctx = ctx;
    }
    if (utf_initialized) {
	return 0;
    }
    utf_dflag = utf_getenvint("UTF_DEBUG");
    if (utf_dflag > 0) {
	utf_printf("%s: utf_dflag=%d\n", __func__, utf_dflag);
    }
    DEBUG(DLEVEL_ALL) {
	utf_printf("%s: vcqh(%lx) pigsz(%ld)\n", __func__, vcqh, pigsz);
    }
    utf_initialized = 1;
    utf_pig_size = pigsz;

    i = utf_getenvint("UTF_MSGMODE");
    utf_setmsgmode(i);
    /* sender control structure, eager buffer, and protocol engine */
    utf_egrsbuf_init(vcqh, SND_EGR_BUFENT);
    utf_sndminfo_init(vcqh, SND_EGR_BUFENT);
    utf_engine_init();
    utf_msgreq_init();
    utf_msglst_init();
    utf_rmacq_init();
    // utf_pcmd_init();
    /* receive buffer is allocated here, 2020/05/08 
     * size of sndmgt is 4 * 158976 * 4 = about 2.5 MB
     */
    i = utf_getenvint("UTF_TRANSMODE");
    utf_recvbuf_init(vcqh, MAX_NODE*4, i);
    return 0;
}

/*
 * utf_init_2:  The second step of the Tofu initializatoin
 *		We know myrank now
 */
int
utf_init_2(void *av, utofu_vcq_hdl_t vcqh, int nprocs)
{
    if (utf_dflag > 0) {
	utf_redirect();
    } else {
	if (getenv("TOFULOG_DIR")) {
	    utf_redirect();
	}
    }
    utf_setav(av);
    DEBUG(DLEVEL_ALL) {
	utf_printf("%s: pid(%d) vcqh(%lx) nprocs(%d)\n", __func__, mypid, vcqh, nprocs);
    }
    if (myrank == 0) {
	utf_show_msgmode(stderr);
	utf_show_transmode(stderr);
    }
#if 0 /* moving to the 1st phase */
    /* receive buffer is allocated */
    utf_recvbuf_init(vcqh, nprocs);
#endif
    /* sender control is allocated */
    /* SND_EGR_BUFENT is max peers */
    utf_scntr_init(vcqh, nprocs, SND_EGR_BUFENT + 1, RMA_MDAT_ENTSIZE);
    /*
     * Do we need to synchronize ? 2020/05/08
     * We observed the following error on 64 node
     * [12] utf_mrqprogress: utofu_poll_mrq ERROR rc(-195)
     * Tofu communication error (An error of remote memory access is reported by an MRQ descriptor. The specified remote STADD may be invalid.) [TNI=0 CQ=00 CMD=ArmHw RC=11h] {onesided_poll.c:374}
     */
    return 0;
}

void
utf_finalize(void *av, utofu_vcq_hdl_t vcqh)
{
    utf_printf("%s: vcqh(%lx) initialized(%d)\n", __func__, vcqh, utf_initialized);
    if (utf_initialized == 0) return;
    /* waiting if the TRANSMODE_CHND chain is clean up */
    utf_chnclean(av, vcqh);
    utf_egrsbuf_fin(vcqh);
    utf_stadd_free(vcqh);
    UTOFU_CALL(0, utofu_free_vcq, vcqh);
    utf_initialized = 0;
    /* statistics */
    utf_printf("UTF statiscitcs\n");
    utf_show_recv_cntr(stderr);
    if (myrank == 0) {
	fprintf(stderr, "list of vcqid\n");
	utf_show_vcqid(av, stderr); fflush(stderr);
    }
}

int
bremote_add(utofu_vcq_hdl_t vcqh,
	    utofu_vcq_id_t rvcqid, unsigned long flgs, uint64_t val,
	    utofu_stadd_t rstadd, size_t off, uint64_t *res)
{
    char	desc[128];
    size_t	sz;
    int		rc;
    void	*cbdata1, *cbdata2;
    struct utofu_mrq_notice mrq_notice;
    uint64_t		edata = 123;

    flgs |= UTOFU_ONESIDED_FLAG_TCQ_NOTICE
	| UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE
	| UTOFU_ONESIDED_FLAG_REMOTE_MRQ_NOTICE;
/*	| UTOFU_MRQ_TYPE_LCL_ARMW;*/
    utf_printf("bremote_add: off(%ld) val(%ld)\n", off, val);
    UTOFU_CALL(0, utofu_prepare_armw8,
	       vcqh, rvcqid,
	       UTOFU_ARMW_OP_ADD,
	       val, rstadd + off, edata, flgs, desc, &sz);
    cbdata1 = (void*) __LINE__;
    UTOFU_CALL(0, utofu_post_toq, vcqh, desc, sz, cbdata1);
    do {
	rc = utofu_poll_tcq(vcqh, 0, &cbdata2);
    } while (rc == UTOFU_ERR_NOT_FOUND);
    utf_printf("going through...\n");
    UTOFU_ERRCHECK_EXIT(rc);
    ERRCHECK_RETURN(cbdata1, !=, cbdata2, -1);
    do {
	rc = utofu_poll_mrq(vcqh, 0, &mrq_notice);
    } while (rc == UTOFU_ERR_NOT_FOUND);
    *res = mrq_notice.rmt_value;
    return 0;
}


int
utf_rank(int *rank)
{
    INITCHECK();
    *rank = myrank;
    return 0;
}
