/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef _ULIB_CONF_H
#define _ULIB_CONF_H

#include <stdio.h>
#include <inttypes.h>

/* #define CONF_ULIB_LOG_VERBOSE */
#define CONF_ULIB_LOG_FAIL

#define CONF_ULIB_CMP_ID    0x07

#define CONF_ULIB_UTOF_FIX1 /* vcq_id in utofu_mrq_notice */
#define CONF_ULIB_UTOF_FIX2 /* cbdata in utofu_poll_tcq() */
#define CONF_ULIB_UTOF_FIX3 /* utofu_prepare_nop() */
//#define CONF_ULIB_UTOF_FIX5 /* completion with same edata */

/* #define CONF_ULIB_SHEA_PAGE (256 * 32) */

#define CONF_ULIB_PERF_SHEA

#define CONF_ULIB_OFI

#define CONF_ULIB_FICQ_CIRQ 110
#define CONF_ULIB_FICQ_POOL 111

#define CONF_ULIB_SHEA_DATA
#define CONF_ULIB_SHEA_DAT1

/* #define CONF_ULIB_FICQ CONF_ULIB_FICQ_CIRQ */
#define CONF_ULIB_FICQ CONF_ULIB_FICQ_POOL

#define CONF_ULIB_LIST_TAILQ
/* #define CONF_ULIB_LIST_DLIST */

#endif	/* _ULIB_CONF_H */
