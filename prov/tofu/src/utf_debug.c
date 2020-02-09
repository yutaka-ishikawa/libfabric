#include <utofu.h>
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_queue.h"

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
#define SHOWLEN 16
#define BSIZE (DSIZE*SHOWLEN)

void
utf_showpacket(char *msg, struct utf_msgbdy *mbp)
{
    int	i;
    size_t	sz;
    char	buf[BSIZE], *bp;
    bp = buf;
    for (i = 0; i < mbp->payload.h_pkt.hdr.size && i < SHOWLEN; i++) {
	sz = snprintf(bp, DSIZE, "%02x ", mbp->payload.h_pkt.msgdata[i]);
	bp += sz;
    }
    utf_printf("*** %s ***\n"
	     "psize: %d, src: %d, tag: %d, size: %ld\n" "data: %s\n",
	     msg,
	     mbp->psize,
	     mbp->payload.h_pkt.hdr.src,
	     mbp->payload.h_pkt.hdr.tag,
	     mbp->payload.h_pkt.hdr.size, buf);
#if 0
    utf_printf("\t psize might be %d\n",
		 (uint64_t) ((struct utf_msgbdy*)(0))->payload.h_pkt.msgdata
		 + mbp->payload.h_pkt.hdr.size);
    utf_printf("header pos = %ld msgdatapos = %ld\n",
	     (uint64_t) &((struct utf_msgbdy*)(0))->payload.h_pkt.hdr,
	     (uint64_t) ((struct utf_msgbdy*)(0))->payload.h_pkt.msgdata);
#endif
}
