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

#define CONF_TOFU_SHEA

#define CONF_TOFU_CTXC	8   /* fi_ep_attr . [rt]x_ctx_cnt */

#endif	/* _TOFU_CONF_H */
