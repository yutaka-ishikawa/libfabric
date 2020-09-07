/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_CONF_H
#define _TOFU_CONF_H

#define FI_TOFU_FABRIC_NAME "tofu"
#define FI_TOFU_VERSION_MINOR 0
#define FI_TOFU_VERSION_MAJOR 1
#define FI_TOFU_VERSION (FI_VERSION(FI_TOFU_VERSION_MAJOR, FI_TOFU_VERSION_MINOR))

#define CONF_TOFU_RECV
#define CONF_TOFU_RMA

//#define CONF_TOFU_CMPID         0x7
//#define CONF_TOFU_CMPID         0x6
#define CONF_TOFU_CTXC  	8   /* fi_ep_attr . [rt]x_ctx_cnt */
#define CONF_TOFU_CQSIZE        256

#define CONF_TOFU_INJECTSIZE    228    /* MUST BE the same size of TOFU_INJECTSIZE in utf_conf.h */
//#define CONF_TOFU_INJECTSIZE    1856    /* 1878 */
//#define CONF_TOFU_MSGSIZE     ((16 * 1024 * 1024) - 1) // MAX in TOFU
//#define CONF_TOFU_MSGSIZE       (32 * 1024) // See also MPIDI_OFI_DEFAULT_SHORT_SEND_SIZE in MPICH
#define CONF_TOFU_MSGSIZE       ((0x1ULL << 32) - 1)    // utf provides rendezvous protocol

/* fi_domain_attr */
#define CONF_TOFU_ATTR_CQ_DATA_SIZE     4
#define CONF_TOFU_ATTR_CQ_CNT           (44 * 2) /* tx + rx */
                        		/* = 44 (= (12-1) * 4 */
                        		/* = 66 (= (12-1) * 6 */
#define CONF_TOFU_ATTR_EP_CNT           (12 - 1)
#define CONF_TOFU_ATTR_TXRX_CTX_CNT     44
                        		/* = 44 (= (12-1) * 4 */
                        		/* = 66 (= (12-1) * 6 */
#define CONF_TOFU_ATTR_MAX_EP_TXRX_CTX  4 /* 4 or 6 */
#define CONF_TOFU_ATTR_CNTR_CNT         (44 * 2) /* tx+rx */


#endif	/* _TOFU_CONF_H */
