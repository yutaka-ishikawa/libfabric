/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <stddef.h>	/* for NULL */
#include <string.h>	/* for strcasecmp() */
#include <stdio.h>

#define TOFU_CAPS (FI_MSG                       \
                        | FI_RMA                \
                         /* | FI_TAGGED */      \
                        | FI_ATOMIC             \
                         /* | FI_MULTICAST */   \
                        | FI_NAMED_RX_CTX       \
                         /* | FI_DIRECTED_RECV */ \
                        | FI_MULTI_RECV         \
                         /* | FI_SOURCE */      \
                        | FI_READ               \
                        | FI_WRITE              \
                        | FI_SEND               \
                        | FI_RECV               \
                        | FI_REMOTE_READ        \
                        | FI_REMOTE_WRITE       \
                        | FI_RMA_EVENT          \
                         /* | FI_SHARED_AV */   \
                         /* | FI_TRIGGER */     \
                         /* | FI_FENECE */      \
                        | FI_LOCAL_COMM         \
                        | FI_REMOTE_COMM        \
                         /* | FI_SOURCE_ERR */  \
                        /* | FI_RMA_PMEM */ )

/* === struct fi_tx_attr ============================================== */

static struct fi_tx_attr tofu_tx_attr = {
    .caps = 0
	    | FI_MSG
	    | FI_RMA
		/* | FI_TAGGED */
	    | FI_ATOMIC
		/* | FI_MULTICAST */
	    | FI_NAMED_RX_CTX
		/* | FI_DIRECTED_RECV */
		/* | FI_MULTI_RECV */
		/* | FI_SOURCE */
	    | FI_READ
	    | FI_WRITE
	    | FI_SEND
		/* | FI_RECV */
		/* | FI_REMOTE_READ */
		/* | FI_REMOTE_WRITE */
	    | FI_RMA_EVENT
		/* | FI_SHARED_AV */
		/* | FI_TRIGGER */
		/* | FI_FENECE */
	    | FI_LOCAL_COMM
	    | FI_REMOTE_COMM
		/* | FI_SOURCE_ERR */
		/* | FI_RMA_PMEM */
	    ,
    .mode = 0
		/* | FI_CONTEXT */
		/* | FI_CONTEXT2 */
		/* | FI_LOCAL_MR */
		/* | FI_MSG_PREFIX */
		/* | FI_ASYNC_IOV */
		/* | FI_RX_CQ_DATA */
		/* | FI_NOTIFY_FLAGS_ONLY */
		/* | FI_RESTRICTED_COMP */
	    ,
    .op_flags = 0
		/* | FI_INJECT */
		/* | FI_MULTI_RECV */
		/* | FI_COMPLETION */
		/* | FI_INJECT_COMPLETE */
		/* | FI_TRANSMIT_COMPLETE */
		/* | FI_DELIVERY_COMPLETE */
		/* | FI_COMMIT_COMPLETE */
		/* | FI_MULTICAST */
	    ,
    .msg_order = 0
#ifdef	NOTYET
	    | FI_ORDER_NONE
		/* | FI_ORDER_RAR */
		/* | FI_ORDER_RAW */
		/* | FI_ORDER_RAS */
		/* | FI_ORDER_WAR */
		/* | FI_ORDER_WAW */
		/* | FI_ORDER_WAS */
		/* | FI_ORDER_SAR */
		/* | FI_ORDER_SAW */
		/* | FI_ORDER_SAS */
#else	/* NOTYET */
	    | FI_ORDER_SAS
#endif	/* NOTYET */
	    ,
    .comp_order = 0
	    | FI_ORDER_NONE /* YYY */
		/* | FI_ORDER_STRICT */
		/* | FI_ORDER_DATA */
	    ,
    .inject_size = 0, /*1024,*/        /* 2019/04/26 */
    .size = 2048, /* YYY */
    .iov_limit = 1, /* XXX TOFU_IOV_LIMIT */
    .rma_iov_limit = 1, /* XXX TOFU_IOV_LIMIT */
};

/* === struct fi_rx_attr ============================================== */

static struct fi_rx_attr tofu_rx_attr = {
    .caps = 0
	    | FI_MSG
	    | FI_RMA
		/* | FI_TAGGED */
	    | FI_ATOMIC
		/* | FI_MULTICAST */
	    | FI_NAMED_RX_CTX
		/* | FI_DIRECTED_RECV */
	    | FI_MULTI_RECV
		/* | FI_SOURCE */
	    | FI_READ
	    | FI_WRITE
		/* | FI_SEND */
	    | FI_RECV
	    | FI_REMOTE_READ
	    | FI_REMOTE_WRITE
	    | FI_RMA_EVENT
		/* | FI_SHARED_AV */
		/* | FI_TRIGGER */
		/* | FI_FENECE */
	    | FI_LOCAL_COMM
	    | FI_REMOTE_COMM
		/* | FI_SOURCE_ERR */
		/* | FI_RMA_PMEM */
	    ,
    .mode = 0
		/* | FI_CONTEXT */
		/* | FI_CONTEXT2 */
		/* | FI_LOCAL_MR */
		/* | FI_MSG_PREFIX */
		/* | FI_ASYNC_IOV */
		/* | FI_RX_CQ_DATA */
		/* | FI_NOTIFY_FLAGS_ONLY */
		/* | FI_RESTRICTED_COMP */
	    ,
    .op_flags = 0
		/* | FI_INJECT */
		/* | FI_MULTI_RECV */
		/* | FI_COMPLETION */
		/* | FI_INJECT_COMPLETE */
		/* | FI_TRANSMIT_COMPLETE */
		/* | FI_DELIVERY_COMPLETE */
		/* | FI_COMMIT_COMPLETE */
		/* | FI_MULTICAST */
	    ,
    .msg_order = 0
	    | FI_ORDER_NONE
		/* | FI_ORDER_RAR */
		/* | FI_ORDER_RAW */
		/* | FI_ORDER_RAS */
		/* | FI_ORDER_WAR */
		/* | FI_ORDER_WAW */
		/* | FI_ORDER_WAS */
		/* | FI_ORDER_SAR */
		/* | FI_ORDER_SAW */
		/* | FI_ORDER_SAS */
	    ,
    .comp_order = 0
	    | FI_ORDER_NONE /* YYY */
		/* | FI_ORDER_STRICT */
		/* | FI_ORDER_DATA */
	    ,
    .total_buffered_recv = 0,
    .size = 2048, /* YYY */
    .iov_limit = 0, /* XXX TOFU_IOV_LIMIT */
};

/* === struct fi_ep_attr ============================================== */

static struct fi_ep_attr tofu_ep_attr = {
    .type = FI_EP_RDM,
    .protocol = FI_PROTO_UNSPEC,
		/* = FI_PROTO_TOFU */
    .protocol_version = FI_VERSION(0,0),
		/* FX1  (0), MP10 (1), FX10 (2), FX100 (3) */
    .max_msg_size = CONF_TOFU_MSGSIZE,
    .msg_prefix_size = 0, /* YYY */
    .max_order_raw_size = 256, /* YYY */
    .max_order_war_size = 256, /* YYY */
    .max_order_waw_size = 256, /* YYY */
    .mem_tag_format = 0, /* YYY ??? */
    .tx_ctx_cnt = 4,
		/* = 4 */
		/* = 6 */
    .rx_ctx_cnt = 4,
		/* = 4 */
		/* = 6 */
    .auth_key_size = 0,
    .auth_key = 0,
};

/* === struct fi_domain_attr ========================================== */

static struct fi_domain_attr tofu_domain_attr = {
    .domain = NULL,
    .name = "tofu", /* YYY TOFU_DOMAIN_NAME */
    .threading = FI_THREAD_FID,
		/* = FI_THREAD_SAFE */
		/* = FI_THREAD_ENDPOINT */
		/* = FI_THREAD_UNSPEC */
    .control_progress = FI_PROGRESS_MANUAL,
		/* = FI_PROGRESS_AUTO */
		/* = FI_PROGRESS_UNSPEC */
    .data_progress = FI_PROGRESS_MANUAL,
		/* = FI_PROGRESS_AUTO */
		/* = FI_PROGRESS_UNSPEC */
    .resource_mgmt = FI_RM_ENABLED,
    .av_type = FI_AV_UNSPEC,
		/* = FI_AV_MAP */
		/* = FI_AV_TABLE */
#ifdef	NOTYET
    .mr_mode = FI_MR_UNSPEC,			/* YYY */ 
		/* = FI_MR_BASIC , FI_MR_SCALABLE */
		/* =  0 */
		/* |= FI_MR_LOCAL */
		/* |= FI_MR_ALLOCATED */
		/* |= FI_MR_PROV_KEY */
		/* |= FI_MR_ENDPOINT */
#else	/* NOTYET */
    .mr_mode = FI_MR_BASIC,			/* YYY */ 
#endif	/* NOTYET */
    .mr_key_size = sizeof (uint64_t),
    .cq_data_size = 4,
    .cq_cnt = 44 * 2 /* tx+rx */,
		/* = 44 (= (12-1) * 4 */
		/* = 66 (= (12-1) * 6 */
    .ep_cnt = 12 - 1,
    .tx_ctx_cnt = 44,
		/* = 44 (= (12-1) * 4 */
		/* = 66 (= (12-1) * 6 */
    .rx_ctx_cnt = 44,
		/* = 44 (= (12-1) * 4 */
		/* = 66 (= (12-1) * 6 */
    .max_ep_tx_ctx = 4,
		/* = 4 */
		/* = 6 */
    .max_ep_rx_ctx = 4,
		/* = 4 */
		/* = 6 */
    .max_ep_stx_ctx = 0,
    .max_ep_srx_ctx = 0,
    .cntr_cnt = 44 * 2 /* tx+rx */,	    /* YYY */
		/* = 44 (= (12-1) * 4 */
		/* = 66 (= (12-1) * 6 */
    .mr_iov_limit = 1,
    .caps = 0, /* YYY */
    .mode = 0, /* YYY */
    .auth_key = NULL,
    .auth_key_size = 0,
    .max_err_data = 0,
    .mr_cnt = 65535,			    /* YYY */
};

/* === struct fi_fabric_attr ========================================== */

static struct fi_fabric_attr tofu_fabric_attr = {
    .fabric = NULL, /* XXX */
    .name = "tofu", /* YYY TOFU_FABRIC_NAME */
    .prov_name = NULL,
    .prov_version = FI_TOFU_VERSION,
    .api_version = FI_VERSION(1,7),
};

/* === struct fi_info ================================================= */

struct fi_info tofu_prov_info = {
    .next = NULL,
    .caps = TOFU_CAPS,
    .mode = FI_CONTEXT, /* YYY */
    .addr_format = FI_ADDR_STR,
    .src_addrlen = 64, /* YYY TOFU_MAX_STR_ADDRLEN <= FI_NAME_MAX (64) */
    .dest_addrlen = 64, /* YYY TOFU_MAX_STR_ADDRLEN <= FI_NAME_MAX (64) */
    .src_addr = NULL,
    .dest_addr = NULL,
    .handle = NULL,
    .tx_attr = &tofu_tx_attr,
    .rx_attr = &tofu_rx_attr,
    .ep_attr = &tofu_ep_attr,
    .domain_attr = &tofu_domain_attr,
    .fabric_attr = &tofu_fabric_attr,
};


int tofu_init_prov_info(const struct fi_info *hints, struct fi_info **infos)
{
    int fc = FI_SUCCESS;
    const struct fi_fabric_attr *prov_fa = &tofu_fabric_attr;
    const struct fi_domain_attr *prov_da = &tofu_domain_attr;
    struct fi_info *head, *info;

    if (hints == 0) {
	goto skip_checks;
    }
    FI_INFO(&tofu_prov, FI_LOG_DEBUG,
            "hints: 0x%p"
            "\tsrc_addr: 0x%p\n"
            "\tdest_addr: 0x%p\n"
            "\ttx_attr: 0x%p\n"
            "\trx_attr: 0x%p\n"
            "\tep_attr: 0x%p\n"
            "\tep_attr->auth_key: 0x%p\n"
            "\tdomain_attr: 0x%p\n"
            "\tdomain_attr->auth_key: 0x%p\n"
            "\tdomain_attr->name: 0x%p\n"
            "\tfabric_attr: 0x%p\n"
            "\tfabric_attr->name: 0x%p\n"
            "\tfabric_attr->prov_name: 0x%p\n"
            "\tnic: 0x%p\n",
            hints, hints->src_addr, hints->dest_addr, hints->tx_attr,
            hints->rx_attr, hints->ep_attr,
            hints->ep_attr ? hints->ep_attr->auth_key : 0,
            hints->domain_attr,
            hints->domain_attr ? hints->domain_attr->auth_key : 0,
            hints->domain_attr ? hints->domain_attr->name : 0,
            hints->fabric_attr,
            hints->fabric_attr ? hints->fabric_attr->name : 0,
            hints->fabric_attr ? hints->fabric_attr->prov_name : 0,
            hints->nic);

    
    if ((hints->fabric_attr != 0)
	&& (hints->fabric_attr->name != 0)
	&& (strcasecmp(hints->fabric_attr->name, prov_fa->name) == 0)) {
	FI_INFO( &tofu_prov, FI_LOG_CORE, "unknown fabric name %s\n",
	    hints->fabric_attr->name);
	return -FI_ENODATA;
    }

    if ((hints->domain_attr != 0)
	&& (hints->domain_attr->name != 0)
	&& (strcasecmp(hints->domain_attr->name, prov_da->name) == 0)) {
	FI_INFO( &tofu_prov, FI_LOG_CORE, "unknown fabric name %s\n",
	    hints->domain_attr->name);
	return -FI_ENODATA;
    }

skip_checks:

    head = NULL;
    {
	info = fi_dupinfo( &tofu_prov_info );
	if (info == 0) { fc = -FI_ENOMEM; }
	else { info->next = head; head = info; }
    }

    // fc = -FI_ENODATA;
    fc = (head == 0)? -FI_ENODATA: 0;
    *infos = head;
    
    return fc;
}

int tofu_chck_prov_info(
    uint32_t api_version,
    const struct fi_info *hints,
    struct fi_info **infos
)
{
    int fc = 0;
    struct util_prov util_prov = { .prov = &tofu_prov, };
    struct fi_info *next, *curr;

#ifdef	DEBUG
    {
	size_t nh = 0, np = 0;
	const struct fi_info *info;

	info = hints;
	while (info != 0) {
	    nh++;
	    info = info->next;
	}

	info = infos[0];
	while (info != 0) {
	    np++;
	    info = info->next;
	}
	FI_DBG( &tofu_prov, FI_LOG_CORE, "api_version %08x "
	"nhints %ld ninfos %ld\n",
	    api_version, nh, np);
    }
#endif	/* DEBUG */
    curr = infos[0];
    while (curr != 0) {
	next = curr->next;
	fc = ofi_check_info( &util_prov, curr, api_version, hints);
	if (fc != 0) {
	    fc = 0;
	}
	curr = next;
    }

    return fc;
}

int tofu_chck_fab_attr(
    const struct fi_fabric_attr *prov_attr,
    const struct fi_fabric_attr *user_attr
)
{
    int fc = 0;
    struct fi_provider *prov = &tofu_prov;

    FI_DBG( &tofu_prov, FI_LOG_FABRIC, "in %s\n", __FILE__);
    if (prov_attr == 0) {
	prov_attr = &tofu_fabric_attr; /* &tofu_prov_info.fabirc_attr */
    }

    fc = ofi_check_fabric_attr(prov, prov_attr, user_attr);
    if (fc != 0) { goto bad; }

bad:
    return fc;
}

int tofu_chck_dom_attr(
    const struct fi_domain_attr *prov_attr,
    const struct fi_info *user_info
)
{
    int fc = 0;
    struct fi_provider *prov = &tofu_prov;

    FI_DBG( &tofu_prov, FI_LOG_DOMAIN, "in %s\n", __FILE__);
    if (prov_attr == 0) {
	prov_attr = &tofu_domain_attr; /* &tofu_prov_info.domain_attr */
    }
    if (
	(user_info->domain_attr != 0)
	&& (user_info->domain_attr->name != 0)
	&& (strcasecmp(user_info->domain_attr->name, "tofu") != 0)
    ) {
	fc = -FI_EINVAL; goto bad;
    }

    if (user_info->domain_attr != 0) {
	uint32_t api_version = FI_VERSION(0,0); /* YYY */
	fc = ofi_check_domain_attr(prov, api_version, prov_attr, user_info);
	if (fc != 0) { goto bad; }
    }

bad:
    return fc;
}

int tofu_chck_av_attr(
    const struct fi_av_attr *user_attr
)
{
    int fc = FI_SUCCESS;

    FI_DBG(&tofu_prov, FI_LOG_AV, "in %s\n", __FILE__);

    /* 
     * 2019/04/09
     * users_attr-name is specified if shared is supported
     * In MPICH, "FI_NAMED_AV_0\" is passed.
     */
    if (user_attr != 0) {
	if ((user_attr->flags & FI_EVENT) != 0) { fc = -FI_ENOSYS; goto bad; }

        FI_DBG( &tofu_prov, FI_LOG_AV, "\t%s\n", user_attr->name);

	if (user_attr->name != 0) { fc = -FI_ENOSYS; goto bad; }
	if (user_attr->rx_ctx_bits < 0) { fc = -FI_EINVAL; goto bad; }
    }

bad:
    return fc;
}

int tofu_chck_cq_attr(
    const struct fi_cq_attr *user_attr
)
{
    int fc = 0;
    struct fi_provider *prov = &tofu_prov;

    FI_DBG( &tofu_prov, FI_LOG_DOMAIN, "in %s\n", __FILE__);

    if (user_attr != 0) {
	fc = ofi_check_cq_attr(prov, user_attr);
	if (fc != 0) { goto bad; }
    }

bad:
    return fc;
}

int tofu_chck_cep_tx_attr(
    const struct fi_tx_attr *prov_attr,
    const struct fi_tx_attr *user_attr,
    uint64_t user_info_mode
)
{
    int fc = 0;
    struct fi_provider *prov = &tofu_prov;

    FI_DBG( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    if (prov_attr == 0) {
	prov_attr = &tofu_tx_attr; /* &tofu_prov_info.tx_attr */
    }
    if (user_attr != 0) {
	fc = ofi_check_tx_attr(prov, prov_attr, user_attr, user_info_mode);
	if (fc != 0) { goto bad; }
	fc = ofi_check_attr_subset(prov, prov_attr->caps, user_attr->caps);
	if (fc != 0) { goto bad; }
    }

bad:
    return fc;
}

int tofu_chck_cep_rx_attr(
    const struct fi_info *prov_info,
    const struct fi_rx_attr *user_attr,
    uint64_t user_info_mode
)
{
    int fc = 0;
    struct fi_provider *prov = &tofu_prov;

    FI_DBG( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    if (prov_info == 0) { /* || (prov_info->rx_attr == 0) */
	prov_info = &tofu_prov_info;
    }
    if (user_attr != 0) {
	uint64_t base_caps = (&tofu_rx_attr)->caps;
	fc = ofi_check_rx_attr(prov, prov_info, user_attr, user_info_mode);
	if (fc != 0) { goto bad; }

	if ((prov_info != 0) && (prov_info->rx_attr != 0)) {
	    base_caps = prov_info->rx_attr->caps;
	}
	fc = ofi_check_attr_subset(prov, base_caps, user_attr->caps);
	if (fc != 0) { goto bad; }
    }

bad:
    return fc;
}

int tofu_chck_ep_attr(
    const struct fi_info *prov_info,
    const struct fi_info *user_info
)
{
    int fc = 0;
    struct util_prov util_prov = { .prov = &tofu_prov, };

    FI_DBG( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    if (prov_info == 0) { /* || (prov_info->rx_attr == 0) */
	prov_info = &tofu_prov_info;
    }
    if (user_info != 0) {
	uint32_t api_version = FI_VERSION(0,0); /* YYY */

	fc = ofi_check_ep_attr( &util_prov, api_version, prov_info, user_info);
	if (fc != 0) { goto bad; }
    }

bad:
    return fc;
}
