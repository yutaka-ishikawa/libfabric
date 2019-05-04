/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_debug.h"
#include "tofu_impl.h"

#include <stdlib.h>	    /* for calloc(), free */
#include <assert.h>	    /* for assert() */

#include "ulib_shea.h"	    /* for struct ulib_shea_data */
#include "ulib_ofif.h"	    /* for ulib_icep_shea_send_prog() */

extern int ulib_toqc_prog_ackd(struct ulib_toqc *toqc);
extern int ulib_toqc_prog_tcqd(struct ulib_toqc *toqc);

void
yi_showcntrl(const char *func, int lno, void *ptr)
{
#define MPIDI_OFI_AM_HANDLER_ID_BITS   8
#define MPIDI_OFI_AM_TYPE_BITS         8
#define MPIDI_OFI_AM_HDR_SZ_BITS       8
#define MPIDI_OFI_AM_DATA_SZ_BITS     48
    struct MPIDI_OFI_am_header {
        uint64_t handler_id:MPIDI_OFI_AM_HANDLER_ID_BITS;
        uint64_t am_type:MPIDI_OFI_AM_TYPE_BITS;
        uint64_t am_hdr_sz:MPIDI_OFI_AM_HDR_SZ_BITS;
        uint64_t data_sz:MPIDI_OFI_AM_DATA_SZ_BITS;
        uint64_t payload[0];
    } *head;
    struct mpich_cntl {
        int16_t type;          int16_t seqno;
        int origin_rank;       void *ackreq;
        uintptr_t send_buf;    size_t msgsize;
        int comm_id;           int endpoint_id;
        uint64_t rma_key;      int tag;
    } *ctrl;

    head = (struct MPIDI_OFI_am_header*) ptr;
    ctrl = (struct mpich_cntl*) (head + 1);
    fprintf(stderr, "YIMPICH***: %s:%d ctrl->type(%d) ctrl->tag(0x%x)\n",
            func, lno, ctrl->type, ctrl->tag);
    fflush(stderr);
}

void
yi_debug(const char *func, int lno, struct fi_cq_tagged_entry *comp)
{

    /* flags defined in fabric.h */
    fprintf(stderr, "YIMPICH***: %s:%d completion entry comp(%p) flags(%lx) len(%ld) data(%ld) tag(%ld) buf(%p)\n", func, lno, comp, comp->flags, comp->len, comp->data, comp->tag, comp->buf); fflush(stderr);
    if (comp->buf) {
        yi_showcntrl(func, lno, comp->buf);
    }
}

/*
 * fi_close
 */
static int
tofu_cq_close(struct fid *fid)
{
    int fc = FI_SUCCESS;
    struct tofu_cq *cq__priv;

    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid != 0);
    cq__priv = container_of(fid, struct tofu_cq, cq__fid.fid);

    if (ofi_atomic_get32( &cq__priv->cq__ref ) != 0) {
	fc = -FI_EBUSY; goto bad;
    }
    if (cq__priv->cq__ccq != 0) {
	tofu_ccirq_free(cq__priv->cq__ccq); cq__priv->cq__ccq = 0;
    }
    fastlock_destroy( &cq__priv->cq__lck );

    free(cq__priv);

bad:
    return fc;
}

static struct fi_ops tofu_cq__fi_ops = {
    .size	    = sizeof (struct fi_ops),
    .close	    = tofu_cq_close,
    .bind	    = fi_no_bind,
    .control	    = fi_no_control,
    .ops_open	    = fi_no_ops_open,
};

/*
 * fi_cq_read
 */
static ssize_t
tofu_cq_read(struct fid_cq *fid_cq, void *buf, size_t count)
{
    ssize_t        ret = 0;
    struct tofu_cq *cq__priv;
    ssize_t        ment = (ssize_t)count, ient;

    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid_cq != 0);
    cq__priv = container_of(fid_cq, struct tofu_cq, cq__fid);
    if (cq__priv == 0) { }

    //R_DBG0(RDBG_LEVEL1, "fi_cq_read: CQ(%p)", cq__priv);

    fastlock_acquire( &cq__priv->cq__lck );

    /*
     * Checking CQ
     */
    if (ofi_cirque_isempty( cq__priv->cq__ccq )) {
        struct dlist_entry *head, *curr, *next;
        struct tofu_cep *cep_priv;
        struct ulib_icep *icep;
        int uc;

        head = &cq__priv->cq__htx;
        dlist_foreach_safe(head, curr, next) {
            cep_priv = container_of(curr, struct tofu_cep, cep_ent_cq);
            assert(cep_priv->cep_fid.fid.fclass == FI_CLASS_TX_CTX);
            icep = (struct ulib_icep *)(cep_priv + 1);
            uc = ulib_icep_shea_send_prog(icep, 0, 0 /* tims */);
            if (uc != 0 /* UTOFU_SUCCESS */ ) { }
            /* RMA operations */
            if (icep->nrma > 0) {
                //fprintf(stderr, "%d:YICQREAD***: (%d) nrma(%d)\n", mypid, __LINE__, icep->nrma);
                uc = ulib_toqc_prog_ackd(icep->toqc);
                //fprintf(stderr, "\t%d:YICQREAD***: ulib_toqc_prog_ackd return %d\n", mypid, uc); fflush(stderr);
                uc = ulib_toqc_prog_tcqd(icep->toqc);
                //fprintf(stderr, "\t%d:YICQREAD***: ulib_toqc_prog_tcqd return %d\n", mypid, uc); fflush(stderr);
            }
        }
        head = &cq__priv->cq__hrx;
        dlist_foreach_safe(head, curr, next) {
            cep_priv = container_of(curr, struct tofu_cep, cep_ent_cq);
            assert(cep_priv->cep_fid.fid.fclass == FI_CLASS_RX_CTX);
            icep = (struct ulib_icep *)(cep_priv + 1);
            uc = ulib_icep_shea_recv_prog(icep);
            if (uc != 0 /* UTOFU_SUCCESS */ ) { }
        }
        if (ofi_cirque_isempty(cq__priv->cq__ccq)) {
            ret = -FI_EAGAIN; goto bad;
        }
    }
    /* CQ has entries */
    if (ment > ofi_cirque_usedcnt( cq__priv->cq__ccq )) {
	ment = ofi_cirque_usedcnt( cq__priv->cq__ccq );
    }
    //fprintf(stderr, "%d:YICQREAD: ment(%ld) in %s\n", mypid, ment, __func__);

    for (ient = 0; ient < ment; ient++) {
	struct fi_cq_tagged_entry *comp;

	/* get an entry pointed by r.p. */
	comp = ofi_cirque_head( cq__priv->cq__ccq );
	assert(comp != 0);

	/* copy */
	((struct fi_cq_tagged_entry *)buf)[ient] = comp[0];
        //yi_debug(__func__, __LINE__, comp);
	/* advance r.p. by one  */
	ofi_cirque_discard( cq__priv->cq__ccq );
    }
    ret = ient;
    assert(ret > 0);

bad:
    fastlock_release( &cq__priv->cq__lck );
    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s return %ld\n", __FILE__, ret);
    return ret;
}

static struct fi_ops_cq tofu_cq__ops = {
    .size	    = sizeof (struct fi_ops_cq),
    .read	    = tofu_cq_read,
    .readfrom	    = fi_no_cq_readfrom,
    .readerr	    = fi_no_cq_readerr,
    .sread	    = fi_no_cq_sread,
    .sreadfrom	    = fi_no_cq_sreadfrom,
    .signal	    = fi_no_cq_signal,
    .strerror	    = fi_no_cq_strerror,
};

/*
 * fi_cq_open
 */
int tofu_cq_open(
    struct fid_domain *fid_dom,
    struct fi_cq_attr *attr,
    struct fid_cq **fid_cq_,
    void *context
)
{
    int fc = FI_SUCCESS;
    struct tofu_domain *dom_priv;
    struct tofu_cq *cq__priv = 0;

    FI_INFO( &tofu_prov, FI_LOG_CQ, "in %s\n", __FILE__);
    assert(fid_dom != 0);
    dom_priv = container_of(fid_dom, struct tofu_domain, dom_fid );

    if (attr != 0) {
	FI_INFO( &tofu_prov, FI_LOG_CQ, "Requested: FI_CQ_FORMAT_%s\n",
	    (attr->format == FI_CQ_FORMAT_UNSPEC)?  "UNSPEC":
	    (attr->format == FI_CQ_FORMAT_CONTEXT)? "CONTEXT":
	    (attr->format == FI_CQ_FORMAT_MSG)?     "MSG":
	    (attr->format == FI_CQ_FORMAT_DATA)?    "DATA":
	    (attr->format == FI_CQ_FORMAT_TAGGED)?  "TAGGED": "<unknown>");
	fc = tofu_chck_cq_attr(attr);
	if (fc != 0) {
	    goto bad;
	}
        if (attr->format != FI_CQ_FORMAT_TAGGED) {
            /* FI_CQ_FORMAT_TAGGED is only supported in this version */
            fc = -1;
            goto bad;
        }
        if (attr->size != 0
            && attr->size != sizeof(struct fi_cq_tagged_entry)) {
            fc = -1;
            goto bad;
        }
    }

    cq__priv = calloc(1, sizeof (cq__priv[0]));
    if (cq__priv == 0) {
	fc = -FI_ENOMEM; goto bad;
    }
    /* initialize cq__priv */
    {
	cq__priv->cq__dom = dom_priv;
	ofi_atomic_initialize32( &cq__priv->cq__ref, 0 );
	fastlock_init( &cq__priv->cq__lck );

	cq__priv->cq__fid.fid.fclass    = FI_CLASS_CQ;
	cq__priv->cq__fid.fid.context   = context;
	cq__priv->cq__fid.fid.ops       = &tofu_cq__fi_ops;
	cq__priv->cq__fid.ops           = &tofu_cq__ops;

	/* dlist_init( &cq__priv->cq__ent ); */
	dlist_init( &cq__priv->cq__htx );
	dlist_init( &cq__priv->cq__hrx );
    }

    /* tofu_comp_cirq */
    cq__priv->cq__ccq = tofu_ccirq_create(CONF_TOFU_CQSIZE);
    if (cq__priv->cq__ccq == 0) {
        fc = -FI_ENOMEM; goto bad;
    }

    R_DBG0(RDBG_LEVEL1, "fi_cq_open: cq(%p)", cq__priv);

    /* return fid_dom */
    fid_cq_[0] = &cq__priv->cq__fid;
    cq__priv = 0; /* ZZZ */

bad:
    if (cq__priv != 0) {
	tofu_cq_close( &cq__priv->cq__fid.fid );
    }

    return fc;
}
