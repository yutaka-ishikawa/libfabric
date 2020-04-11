#include <pmix.h>
#include <pmix_fjext.h>
#include <utofu.h>
#include <jtofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_errmacros.h"
#include "utf_sndmgt.h"
#include "utf_queue.h"

size_t	utf_pig_size;	/* piggyback size is globally defined */
int	utf_dflag;
int	utf_initialized;

int
utf_getenvint(char *envp)
{
    char	*cp = getenv(envp);
    if (cp) {
	return atoi(cp);
    }
    return 0;
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

/*
 * utf_init_1:  The first step of the Tofu initialization
 *		We do not know myrank during this phase
 */
int
utf_init_1(utofu_vcq_hdl_t vcqh, size_t pigsz)
{
    int	i;
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
    return 0;
}

/*
 * utf_init_2:  The second step of the Tofu initializatoin
 *		We know myrank now
 */
int
utf_init_2(utofu_vcq_hdl_t vcqh, int nprocs)
{
    if (utf_dflag > 0) {
	utf_redirect();
    }
    DEBUG(DLEVEL_ALL) {
	utf_printf("%s: vcqh(%lx) nprocs(%d)\n", __func__, vcqh, nprocs);
    }
    if (myrank == 0) {
	utf_show_msgmode(stderr);
    }
    /* receive buffer is allocated */
    utf_recvbuf_init(vcqh, nprocs);
    /* sender control is allocated */
    utf_scntr_init(vcqh, nprocs, SND_EGR_BUFENT);
    return 0;
}

void
utf_finalize(utofu_vcq_hdl_t vcqh)
{
    utf_egrsbuf_fin(vcqh);
    utf_stadd_free(vcqh);
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
    UTOFU_CALL(utofu_prepare_armw8,
	       vcqh, rvcqid,
	       UTOFU_ARMW_OP_ADD,
	       val, rstadd + off, edata, flgs, desc, &sz);
    cbdata1 = (void*) __LINE__;
    UTOFU_CALL(utofu_post_toq, vcqh, desc, sz, cbdata1);
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
