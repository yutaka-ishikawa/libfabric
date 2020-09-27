#include <stdint.h>
#include <utofu.h>
#include <jtofu.h>
#include "tofu_impl.h"
#include "tofu_addr.h"

/*
 * FI_MSG           : 0x000002
 * FI_RMA	    : 0x000004
 * FI_TAGGED	    : 0x000008
 * FI_ATOMIC	    : 0x000010
 * FI_MULTICAST	    : 0x000020
 * FI_RECV          : 0x000400
 * FI_SEND          : 0x000800
 * FI_REMOTE_READ   : 0x001000
 * FI_REMOTE_WRITE  : 0x002000
 * FI_MULTI_RECV    : 0x010000
 * FI_REMOTE_CQ_DATA: 0x020000
 * FI_MORE	    : 0x040000
 * FI_PEEK	    : 0x080000
 * FI_TRIGGER	    : 0x100000
 * FI_FENCE	    : 0x200000
 * FI_COMPLETION    :0x1000000
 * FI_INJECT	    :0x2000000
 */

#define B_SIZE	1024
char *
tofu_fi_flags_string(uint64_t flags)
{
    static char	buf[B_SIZE + 1];
    buf[0] = 0;
    if (flags & FI_MSG) strncat(buf, "FI_MSG", B_SIZE);
    if (flags & FI_RMA) strncat(buf, ", FI_RMA", B_SIZE);
    if (flags & FI_TAGGED) strncat(buf, ", FI_TAGGED", B_SIZE);
    if (flags & FI_ATOMIC) strncat(buf, ", FI_ATOMIC", B_SIZE);
    if (flags & FI_ATOMICS) strncat(buf, ", FI_ATOMICS", B_SIZE);
    if (flags & FI_MULTICAST) strncat(buf, ", FI_MULTICAST", B_SIZE);
    if (flags & FI_READ) strncat(buf, ", FI_READ", B_SIZE);
    if (flags & FI_WRITE) strncat(buf, ", FI_WRITE", B_SIZE);
    if (flags & FI_RECV) strncat(buf, ", FI_RECV", B_SIZE);
    if (flags & FI_SEND) strncat(buf, ", FI_SEND", B_SIZE);
    if (flags & FI_REMOTE_READ) strncat(buf, ", FI_REMOTE_READ", B_SIZE);
    if (flags & FI_REMOTE_WRITE) strncat(buf, ", FI_REMOTE_WRITE", B_SIZE);
    if (flags & FI_MULTI_RECV) strncat(buf, ", FI_MULTI_RECV", B_SIZE);
    if (flags & FI_REMOTE_CQ_DATA) strncat(buf, ", FI_REMOTE_CQ_DATA", B_SIZE);
    if (flags & FI_MORE) strncat(buf, ", FI_MORE", B_SIZE);
    if (flags & FI_PEEK) strncat(buf, ", FI_PEEK", B_SIZE);
    if (flags & FI_TRIGGER) strncat(buf, ", FI_TRIGGER", B_SIZE);
    if (flags & FI_FENCE) strncat(buf, ", FI_FENCE", B_SIZE);
    if (flags & FI_COMPLETION) strncat(buf, ", FI_COMPLETION", B_SIZE);
    if (flags & FI_INJECT) strncat(buf, ", FI_INJECT", B_SIZE);
    if (flags & FI_INJECT_COMPLETE) strncat(buf, ", FI_INJECT_COMPLETE", B_SIZE);
    if (flags & FI_TRANSMIT_COMPLETE) strncat(buf, ", FI_TRANSMIT_COMPLETE", B_SIZE);
    if (flags & FI_DELIVERY_COMPLETE) strncat(buf, ", FI_DELIVERY_COMPLETE", B_SIZE);
    if (flags & FI_MATCH_COMPLETE) strncat(buf, ", FI_MATCH_COMPLETE", B_SIZE);
    if (flags & FI_VARIABLE_MSG) strncat(buf, ", FI_VARIABLE_MSG", B_SIZE);
    if (flags & FI_RMA_PMEM) strncat(buf, ", FI_RMA_PMEM", B_SIZE);
    if (flags & FI_SOURCE_ERR) strncat(buf, ", FI_FOURCE_ERR", B_SIZE);
    if (flags & FI_LOCAL_COMM) strncat(buf, ", FI_LOCAL_COMM", B_SIZE);
    if (flags & FI_REMOTE_COMM) strncat(buf, ", FI_REMOTE_COMM", B_SIZE);
    if (flags & FI_SHARED_AV) strncat(buf, ", FI_SHARED_AV", B_SIZE);
    if (flags & FI_PROV_ATTR_ONLY) strncat(buf, ", FI_PROV_ATTR_ONLY", B_SIZE);
    if (flags & FI_NUMERICHOST) strncat(buf, ", FI_NUMERICHOST", B_SIZE);
    if (flags & FI_RMA_EVENT) strncat(buf, ", FI_RMA_EVENT", B_SIZE);
    if (flags & FI_SOURCE) strncat(buf, ", FI_SOURCE", B_SIZE);
    if (flags & FI_NAMED_RX_CTX) strncat(buf, ", FI_NAMED_RX_CTX", B_SIZE);
    if (flags & FI_DIRECTED_RECV) strncat(buf, ", FI_DIRECTED_RECV", B_SIZE);
    if (flags & FI_CLAIM) strncat(buf, ", FI_CLAIM", B_SIZE);
    if (flags & FI_DISCARD) strncat(buf, ", FI_DISCARD", B_SIZE);
    return buf;
}

char *
tofu_data_dump(void *data, int len)
{
    static char	buf[B_SIZE];
    char *cp = data;
    int	i, j, k;
    j = 0;
    if (data == NULL) {
	strcpy(buf, "<NULL>");
	return buf;
    }
    k = len > 80 ? 80 : len;
    for (i = 0; i < k; i++) {
	snprintf(&buf[j], B_SIZE, "%02x:", *cp);
	j += 3; cp++;
    }
    return buf;
}

char *
tofu_fi_msg_string(const struct fi_msg_tagged *msgp)
{
    static char	buf[B_SIZE];
    snprintf(buf, B_SIZE,
	     "iov_count(%ld) len(%ld) src(%ld) tag(0x%lx) ignore(0x%lx) "
	     "context(%p) data(0x%lx)", msgp->iov_count,
	     ofi_total_iov_len(msgp->msg_iov, msgp->iov_count),
	     msgp->addr, msgp->tag, msgp->ignore, msgp->context, msgp->data);
    return buf;
}

char *
tofu_fi_msg_data(const struct fi_msg_tagged *msgp)
{
    static char	buf[B_SIZE];
    int	i, j, k;

    k = 0;
    buf[0] = 0;
    for (i = 0; i < msgp->iov_count; i++) {
	char *cp = msgp->msg_iov[i].iov_base;
	int cnt = msgp->msg_iov[i].iov_len > 32 ? 32 : msgp->msg_iov[i].iov_len;
	for (j = 0; j < cnt && k < B_SIZE - 4; j++) {
	    snprintf(&buf[k], B_SIZE, "%02x:", *cp);
	    k += 3; cp++;
	}
	if (k >= B_SIZE) break;
	buf[k++] = '+';	buf[k] = 0;
    }
    return buf;
}

char *
tofu_fi_err_string(int cc)
{
    static char	buf[B_SIZE];
    switch(cc) {
    case -FI_EIO:
	strcpy(buf, "-FI_EIO");
	break;
    case -FI_EAGAIN:
	strcpy(buf, "-FI_EAGAIN");
	break;
    case -FI_ENOMEM:
	strcpy(buf, "-FI_ENOMEM");
	break;
    case FI_SUCCESS:
	strcpy(buf, "FI_SUCCESS");
	break;
    default:
	strcpy(buf, "???");
	break;
    }
    return buf;
}

void
rdbg_mpich_cntrl(const char *func, int lno, void *ptr)
{
    struct MPIDI_OFI_am_header *head;
    head = (struct MPIDI_OFI_am_header*) ptr;
    fprintf(stderr, "[%d][%d]:%s:%d "
	    "MPICH Header: handler_id(%d) am_type(0x%x) "
	    "am_hdr_sz(0x%x) data_sz(0x%lx) seqno(%d)\n",
	    utf_info.myrank, utf_info.mypid, func, lno,
	    head->handler_id, head->am_type, head->am_hdr_sz,
	    (long unsigned) head->data_sz, head->seqno);
    fflush(stderr);
}

void
rdbg_iovec(const char *func, int lno, size_t count, const void *ptr)
{
    struct iovec *iovp = (struct iovec*) ptr;
    ssize_t	i;

    fprintf(stderr, "[%d][%d]:%s:%d IOVEC: count(%ld)",
	    utf_info.myrank, utf_info.mypid, func, lno, count);
    for (i = 0; i < count; i++) {
	fprintf(stderr, " base(%p) len(%ld)", iovp[i].iov_base,
		iovp[i].iov_len);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

char *
tank2string(char *buf, size_t sz, uint64_t ui64)
{
    union ulib_tofa_u utofa;
    utofa.ui64 = ui64;
    if (ui64 == ((uint64_t) -1)) { /* FI_ADDR_UNSPEC */
        snprintf(buf, sz, "fi_addr_unspec");
    } else {
        snprintf(buf, sz, "xyzabc(%02x:%02x:%02x:%02x:%02x:%02x), "
                 "tni(%d), tcq(%d), cid(0x%x)",
                 utofa.tank.tux, utofa.tank.tuy, utofa.tank.tuz,
                 utofa.tank.tua, utofa.tank.tub, utofa.tank.tuc,
                 utofa.tank.tni, utofa.tank.tcq, utofa.tank.cid);
    }
    return buf;
}

char *
vcqid2string(char *buf, size_t sz, utofu_vcq_id_t vcqi)
{
    uint8_t	abcxyz[8];
    uint16_t	tni[1], cqid[1], cid[1];
    int	uc;

    uc = utofu_query_vcq_info(vcqi, abcxyz, tni, cqid, cid);
    if (uc == UTOFU_SUCCESS) {
	snprintf(buf, sz, "xyzabc(%02x:%02x:%02x:%02x:%02x:%02x), "
		 "tni(%d), cqid(%d), cid(0x%x)",
		 abcxyz[0], abcxyz[1], abcxyz[2], abcxyz[3], abcxyz[4], abcxyz[5],
		 tni[0], cqid[0], cid[0]);
    } else {
	R_DBG("Cannot parse vcqid(%lx)", vcqi);
	snprintf(buf, sz, "Cannot parse vcqid(%lx)", vcqi);
    } 
    
    return buf;
}

void
dbg_show_utof_vcqh(utofu_vcq_hdl_t vcqh)
{
    int uc;
    utofu_vcq_id_t vcqi = -1UL;
    uint8_t xyz[8];
    uint16_t tni[1], tcq[1], cid[1];
    union ulib_tofa_u tofa;
    char buf[128];
    
    uc = utofu_query_vcq_id(vcqh, &vcqi);
    if (uc != UTOFU_SUCCESS) {
	fprintf(stderr, "%s: utofu_query_vcq_id error (%d)\n", __func__, uc);
	goto bad;
    }
    uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid);
    if (uc != UTOFU_SUCCESS) {
	fprintf(stderr, "%s: utofu_query_vcq_id error (%d)\n", __func__, uc);
	goto bad;
    }
    tofa.ui64 = 0;
    tofa.tank.tux = xyz[0]; tofa.tank.tuy = xyz[1];
    tofa.tank.tuz = xyz[2]; tofa.tank.tua = xyz[3];
    tofa.tank.tub = xyz[4]; tofa.tank.tuc = xyz[5];
    tofa.tank.tni = tni[0]; tofa.tank.tcq = tcq[0];
    tofa.tank.cid = cid[0];
    fprintf(stderr, "%d: vcqh(0x%lx) vcqid(%s) in %s:%d of %s\n", utf_info.mypid, vcqh,
	    tank2string(buf, 128, tofa.ui64),
	    __func__, __LINE__, __FILE__);
    fflush(stderr);
bad:
    return;
}

void
dbg_show_utof_myvcqh(size_t sz, utofu_vcq_hdl_t *vcqh)
{
    int uc;
    utofu_vcq_id_t vcqi = -1UL;
    uint8_t xyz[8];
    uint16_t tni[1], tcq[1], cid[1];
    size_t	ent;

    fprintf(stderr, "[%d]:%d MY vcqhs (%ld): \n", utf_info.myrank, utf_info.mypid, sz);
    for (ent = 0; ent < sz; ent++) {
	if (vcqh[ent] == 0) break;
	uc = utofu_query_vcq_id(vcqh[ent], &vcqi);
	if (uc != UTOFU_SUCCESS) {
	    fprintf(stderr, "%s: utofu_query_vcq_id error (%d)\n", __func__, uc);
	    goto bad;
	}
	uc = utofu_query_vcq_info(vcqi, xyz, tni, tcq, cid);
	if (uc != UTOFU_SUCCESS) {
	    fprintf(stderr, "%s: utofu_query_vcq_id error (%d)\n", __func__, uc);
	    goto bad;
	}
	fprintf(stderr, "\tvcqh(0x%lx) vcqid(0x%lx) "
		"vcqid(xyzabc(%02d:%02d:%02d:%02d:%02d:%02d), tni(%02d), tcq(%02d), cid(%02d))\n",
		vcqh[ent], vcqi, xyz[0], xyz[1], xyz[2], xyz[3], xyz[4], xyz[5],
		tni[0], tcq[0], cid[0]);
    }
    fflush(stderr);
bad:
    return;
}

char *tofu_fi_class_string[] = {
	"FI_CLASS_UNSPEC", "FI_CLASS_FABRIC", "FI_CLASS_DOMAIN", "FI_CLASS_EP",
	"FI_CLASS_SEP", "FI_CLASS_RX_CTX", "FI_CLASS_SRX_CTX", "FI_CLASS_TX_CTX",
	"FI_CLASS_STX_CTX", "FI_CLASS_PEP", "FI_CLASS_INTERFACE", "FI_CLASS_AV",
	"FI_CLASS_MR", "FI_CLASS_EQ", "FI_CLASS_CQ", "FI_CLASS_CNTR", "FI_CLASS_WAIT",
	"FI_CLASS_POLL", "FI_CLASS_CONNREQ", "FI_CLASS_MC", "FI_CLASS_NIC",
};

/*
  struct iovec  {
	void *iov_base;
	__kernel_size_t iov_len;
  };
  struct fi_msg_tagged {
	const struct iovec	*msg_iov;
	void			**desc;
	size_t			iov_count;
	fi_addr_t		addr;
	uint64_t		tag;
	uint64_t		ignore;
	void			*context;
	uint64_t		data;
  };
*/
