/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_ULIB_DESC_H
#define _ULIB_DESC_H

#include "utofu.h"

#include "ulib_conf.h"	    /* for CONF_ULIB_+ */
#include "ulib_list.h"	    /* for DLST_+() */

#include <stdint.h>

#include <ofi_atom.h>	    /* for ofi_atomic_+() */

/* definitions */

#define ULIB_DESC_UFLG_SAFE ( 0 \
			    | UTOFU_ONESIDED_FLAG_STRONG_ORDER \
			    | UTOFU_ONESIDED_FLAG_TCQ_NOTICE \
			    | UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE \
			    )
#define ULIB_DESC_UFLG_ARMW ( 0 \
			    /* | UTOFU_ONESIDED_FLAG_STRONG_ORDER */ \
			    | UTOFU_ONESIDED_FLAG_TCQ_NOTICE \
			    | UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE \
			    )
#define ULIB_DESC_UFLG_POLL ( 0 \
			    /* | UTOFU_ONESIDED_FLAG_STRONG_ORDER */ \
			    | UTOFU_ONESIDED_FLAG_CACHE_INJECTION \
			    | UTOFU_ONESIDED_FLAG_TCQ_NOTICE \
			    | UTOFU_ONESIDED_FLAG_LOCAL_MRQ_NOTICE \
			    )

/* structures */

struct ulib_utof_cash {   /* cache $$$ */
    utofu_vcq_id_t  vcqi; /* local,remote */
    utofu_stadd_t   stad; /* local,remote */
#ifdef	CONF_ULIB_SHEA_DATA
    utofu_stadd_t   stad_data;
#endif	/* CONF_ULIB_SHEA_DATA */
    utofu_path_id_t paid; /* remote */
    utofu_vcq_hdl_t vcqh; /* local */
};

struct ulib_toqd_cash {
    uint64_t                desc[8];
    size_t                  size;
    struct utofu_mrq_notice ackd[1]; /* expected */
};

struct ulib_toqc_cash {
    struct ulib_utof_cash   addr[2]; /* [0] remote [1] local */
    uint64_t		    fi_a;
    uint32_t		    vpid;
    ofi_atomic32_t	    refc;
    DLST_DECE(ulib_toqc_cash)   list;
    struct ulib_toqd_cash   swap[1];
    struct ulib_toqd_cash   fadd[1];
    struct ulib_toqd_cash   cswp[1];
    struct ulib_toqd_cash   puti[1];
    struct ulib_toqd_cash   phdr[1];
#ifdef	CONF_ULIB_SHEA_DATA
    struct ulib_toqd_cash   putd[2];
#endif	/* CONF_ULIB_SHEA_DATA */
};

DLST_DEFH(ulib_head_desc, ulib_toqc_cash);

/* function prototypes */
struct ulib_utof_cash;

extern int  ulib_desc_get1_from_tofa(
		uint64_t raui, uint64_t laui,
		uint64_t roff, uint64_t loff, uint64_t leng, uint64_t edat,
		unsigned long uflg,
		void *epnt, struct ulib_toqd_cash *toqd);

extern int  ulib_desc_get1_from_cash(
		const struct ulib_utof_cash *rcsh,
		const struct ulib_utof_cash *lcsh,
		uint64_t roff, uint64_t loff, uint64_t leng, uint64_t edat,
		unsigned long uflg,
		struct ulib_toqd_cash *toqd);

extern int  ulib_desc_put1_from_cash_data(
		const struct ulib_utof_cash *rcsh,
		const struct ulib_utof_cash *lcsh,
		uint64_t roff, uint64_t loff, uint64_t leng, uint64_t edat,
		unsigned long uflg,
		struct ulib_toqd_cash *toqd);

extern int  ulib_desc_puti_le08_from_cash(
		const struct ulib_utof_cash *rcsh,
		const struct ulib_utof_cash *lcsh,
		uint64_t roff, uint64_t lval, uint64_t leng, uint64_t edat,
		unsigned long uflg,
		struct ulib_toqd_cash *toqd);

extern int  ulib_desc_puti_le32_from_cash(
		const struct ulib_utof_cash *rcsh,
		const struct ulib_utof_cash *lcsh,
		uint64_t roff, const void *ldat, uint64_t leng, uint64_t edat,
		unsigned long uflg,
		struct ulib_toqd_cash *toqd);

extern int  ulib_desc_fadd_from_cash(
		const struct ulib_utof_cash *rcsh,
		const struct ulib_utof_cash *lcsh,
		uint64_t roff, uint64_t lval, uint64_t leng, uint64_t edat,
		unsigned long uflg,
		struct ulib_toqd_cash *toqd);

extern int  ulib_desc_swap_from_cash(
		const struct ulib_utof_cash *rcsh,
		const struct ulib_utof_cash *lcsh,
		uint64_t roff, uint64_t lval, uint64_t leng, uint64_t edat,
		unsigned long uflg,
		struct ulib_toqd_cash *toqd);

extern int  ulib_desc_cswp_from_cash(
		const struct ulib_utof_cash *rcsh,
		const struct ulib_utof_cash *lcsh,
		uint64_t roff, uint64_t swpv, uint64_t cmpv,
		uint64_t leng, uint64_t edat,
		unsigned long uflg,
		struct ulib_toqd_cash *toqd);

extern int  ulib_desc_noop_from_cash(
		const struct ulib_utof_cash *lcsh,
		unsigned long uflg,
		struct ulib_toqd_cash *toqd);

extern int  ulib_utof_cash_remote(
		uint64_t raui,
		utofu_cmp_id_t c_id,
		void *epnt,
		struct ulib_utof_cash *cash);

/* inline functions */

#include <assert.h>	/* for assert() */
#include <string.h>	/* for memset() */

static inline void ulib_toqc_cash_init_desc(
    struct ulib_toqc_cash *cash
)
{
    cash->swap[0].size = 0;
    cash->fadd[0].size = 0;
    cash->cswp[0].size = 0;
    cash->puti[0].size = 0;
    cash->phdr[0].size = 0;
#ifdef	CONF_ULIB_SHEA_DATA
    cash->putd[0].size = 0;
    cash->putd[1].size = 0;
#endif	/* CONF_ULIB_SHEA_DATA */

    return ;
}

static inline void ulib_toqc_cash_init(
    struct ulib_toqc_cash *cash,
    uint64_t fi_a
)
{
    memset(cash, 0, sizeof (cash[0]));

    cash->addr[0].vcqh = 0; /* remote */
    cash->addr[1].vcqh = 0; /* local */
    cash->addr[1].stad = -1ULL; /* local */
#ifdef	CONF_ULIB_SHEA_DATA
    cash->addr[1].stad_data = -1ULL; /* local */
#endif	/* CONF_ULIB_SHEA_DATA */

    cash->fi_a = fi_a;
    cash->vpid = -1U;
    ofi_atomic_initialize32(&cash->refc, 0);

    ulib_toqc_cash_init_desc(cash);

    return ;
}


#endif	/* _ULIB_DESC_H */

