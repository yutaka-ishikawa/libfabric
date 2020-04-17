#include <utofu.h>
#include <limits.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_queue.h"
#ifndef UTF_NATIVE
#include "tofu_debug.h"
#endif

static FILE	*logfp;
static char	logname[PATH_MAX];

void
utf_redirect()
{
    if (stderr != logfp) {
        char    *cp = getenv("TOFULOG_DIR");
        if (cp) {
            sprintf(logname, "%s/tofulog-%d", cp, myrank);
        } else {
            sprintf(logname, "tofulog-%d", myrank);
        }
        if ((logfp = fopen(logname, "w")) == NULL) {
            /* where we have to print ? /dev/console ? */
            fprintf(stderr, "Cannot create the logfile: %s\n", logname);
        } else {
            fclose(stderr);
            stderr = logfp;
        }
    }
}

char *
utf_msghdr_string(struct utf_msghdr *hdrp, void *bp)
{
    static char	buf[128];
    snprintf(buf, 128, "src(%ld) tag(0x%lx) size(%ld) data(%ld) msg(0x%lx)",
	     hdrp->src, hdrp->tag, hdrp->size, hdrp->data,
	     bp != 0 ? *(uint64_t*) bp: 0);
    return buf;
}

void
mrq_notice_show(struct utofu_mrq_notice *ntcp)
{
    utf_printf("mrq_notice.notice_type(%d)\n", ntcp->notice_type);
    utf_printf("mrq_notice.edata(%ld)\n", ntcp->edata);
    utf_printf("mrq_notice.rmt_value(%lx)\n", ntcp->rmt_value);
    utf_printf("mrq_notice.lcl_stadd(%lx)\n", ntcp->lcl_stadd);
    utf_printf("mrq_notice.rmt_stadd(%lx)\n", ntcp->rmt_stadd);
}

#define DSIZE 10
#define SHOWLEN 200
#define BSIZE (DSIZE*SHOWLEN)

void
utf_show_data(char *msg, char *data, size_t len)
{
    size_t	i, sz;
    char	buf[BSIZE], *bp;
    bp = buf;
    *bp = 0;
    for (i = 0; i < len && i < SHOWLEN; i++) {
	sz = snprintf(bp, DSIZE, "%02x ", data[i]);
	bp += sz;
    }
    utf_printf("%s %s\n", msg, buf);
}

void
utf_showpacket(char *msg, struct utf_msgbdy *mbp)
{
    int	i;
    size_t	sz;
    char	buf[BSIZE], *bp;
    bp = buf;
    *bp = 0;
    for (i = 0; i < mbp->payload.h_pkt.hdr.size && i < SHOWLEN; i++) {
	sz = snprintf(bp, DSIZE, "%02x ", mbp->payload.h_pkt.msgdata[i]);
	bp += sz;
    }
#ifdef UTF_NATIVE
    utf_printf("*** %s ***\n"
	       "psize: %d, src: %d, tag: %x, size: %ld\n" "data: %s\n",
	       msg, mbp->psize,
	       mbp->payload.h_pkt.hdr.src,
	       mbp->payload.h_pkt.hdr.tag,
	       mbp->payload.h_pkt.hdr.size, buf);
#else
    utf_printf("*** %s ***\n"
	       "psize: %d, src: %d, tag: %x, size: %ld data: 0x%lx flgs: (0x%lx: %s)\n"
	       "data: %s\n",
	       msg, mbp->psize,
	       mbp->payload.h_pkt.hdr.src,
	       mbp->payload.h_pkt.hdr.tag,
	       mbp->payload.h_pkt.hdr.size,
	       mbp->payload.h_pkt.hdr.data,
	       mbp->payload.h_pkt.hdr.flgs,
	       tofu_fi_flags_string(mbp->payload.h_pkt.hdr.flgs),
	       buf);
#endif /* UTF_NATIVE */
#if 0
    utf_printf("\t psize might be %d\n",
		 (uint64_t) ((struct utf_msgbdy*)(0))->payload.h_pkt.msgdata
		 + mbp->payload.h_pkt.hdr.size);
    utf_printf("header pos = %ld msgdatapos = %ld\n",
	     (uint64_t) &((struct utf_msgbdy*)(0))->payload.h_pkt.hdr,
	     (uint64_t) ((struct utf_msgbdy*)(0))->payload.h_pkt.msgdata);
#endif
}
