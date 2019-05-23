/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_ATM_H
#define _TOFU_ATM_H

/*
 * fi_ioc (in fi_domain.h)
 *   struct fi_ioc { void *addr; size_t count; };
 * fi_rma_ioc (in fi_rma.h)
 *   struct fi_rma_ioc { uint64_t addr, size_t count; uint64_t key; };
 *
 * lcl fi_ioc     (+ desc)  const
 * rem fi_rma_ioc           const
 * res fi_ioc     (+ desc) !const
 * cmp fi_ioc     (+ desc)  const
 */

struct tofu_atm_vec {
    union {
	const struct fi_ioc *	    lcl;
	const struct fi_rma_ioc *   rmt;
    } vec;
    void **			    dsc;
    size_t			    ioc;
};

struct tofu_atm_arg {
    const struct fi_msg_atomic *    msg;
    uint64_t			    flg;
    struct tofu_atm_vec		    lcl; /* local  */
    struct tofu_atm_vec		    rmt; /* remote */
    struct tofu_atm_vec		    res; /* result */
    struct tofu_atm_vec		    cmp; /* compare */
};

#endif	/* _TOFU_ATM_H */
