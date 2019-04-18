/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_IMP_ULIB_H
#define _TOFU_IMP_ULIB_H

/* internal header */

#include <rdma/fabric.h>	/* for struct fi_[rt]x_attr */
#include <stddef.h>		/* for size_t */
#include <stdint.h>		/* for uint64_t */
#include <sys/uio.h>		/* for struct iovec */

struct ulib_sep_tniq {
    uint64_t vers : 3; /* version # */
    uint64_t cmpi : 3; /* component id */
    uint64_t nniq : 3; /* # of niqs */
    uint64_t vald : 1; /* valid flag */
    uint64_t tni0 : 3; /* niq[0] */
    uint64_t tcq0 : 6;
    uint64_t tni1 : 3; /* niq[1] */
    uint64_t tcq1 : 6;
    uint64_t tni2 : 3; /* niq[2] */
    uint64_t tcq2 : 6;
    uint64_t tni3 : 3; /* niq[3] */
    uint64_t tcq3 : 6;
    uint64_t tni4 : 3; /* niq[4] */
    uint64_t tcq4 : 6;
    uint64_t tni5 : 3; /* niq[5] */
    uint64_t tcq5 : 6;
};

struct ulib_sep_name {
    uint8_t  txyz[3];
    uint8_t  a : 1;
    uint8_t  b : 2;
    uint8_t  c : 1;
    uint8_t  p : 3;	/* component id */
    uint8_t  v : 1;	/* valid */
    uint32_t vpid;
    uint8_t  tniq[8];	/* (tni + tcq)[8] */
    /* struct ulib_sep_tniq tniq; */
};

struct tofu_imp_sep_ulib {
    utofu_tni_id_t  tnis[8];
    size_t ntni;
};

struct fi_msg_tagged;   /* defined in <rdma/fi_tagged.h> */
struct fi_cq_tagged_entry; /* defined in <rdma/fi_eq.h> */

typedef int (*tofu_imp_ulib_comp_f)(
    void *farg,
    const struct fi_cq_tagged_entry *comp
);

extern size_t	tofu_imp_ulib_size(void);
extern void	tofu_imp_ulib_init(
		    void *vptr,
		    size_t offs,
		    const struct fi_rx_attr *rx_attr,
		    const struct fi_tx_attr *tx_attr
		);
extern void	tofu_imp_ulib_fini(void *vptr, size_t offs);
extern int	tofu_imp_ulib_enab(void *vptr, size_t offs);
extern int	tofu_imp_ulib_gnam(
		    void *vp_s[CONF_TOFU_CTXC],
		    size_t offs,
		    char nam[128]
		);
extern int      tofu_impl_ulib_sendmsg_self(
                    void *vptr, size_t offs,
                    const struct fi_msg_tagged *tmsg,
                    uint64_t flags);
extern int	tofu_imp_ulib_recv_post(
		    void *vptr,
		    size_t offs,
		    const struct fi_msg_tagged *tmsg,
		    uint64_t flags
                );
extern int	tofu_imp_ulib_send_post(
		    void *vptr,
		    size_t offs,
		    const struct fi_msg_tagged *tmsg,
		    uint64_t flags,
		    tofu_imp_ulib_comp_f func,
		    void *farg
		);
extern int	tofu_imp_ulib_send_post_fast(
		    void *vptr,
		    size_t offs,
		    uint64_t	lsta,
		    size_t	tlen,
		    uint64_t	tank,
		    uint64_t	utag,
		    void *	ctxt,
		    uint64_t	iflg,
		    tofu_imp_ulib_comp_f func,
		    void *farg
		);
extern int	tofu_imp_str_uri_to_long(
		    const char *uri,
		    long lv_xyzabc[6],
		    long lv_tniq[CONF_TOFU_CTXC * 2]
		);
extern int	tofu_imp_str_uri_to_name(
			const void *vuri,
			size_t index,
			void *vnam
		);
extern int	tofu_imp_name_valid(
		    const void *vnam,
		    size_t index
		);
extern int	tofu_imp_namS_to_tank(
		    const void *vnam,
		    size_t index,
		    uint64_t *tank_ui64
		);

/* ------------------------------------------------------------------------ */

struct tofu_mr;

extern int	tofu_imp_ulib_immr_stad(
		    struct tofu_mr *mr__priv,
		    void *vp_icep,
		    const struct iovec *iov1,
		    uint64_t *stad_ui64
		);
extern void	tofu_imp_ulib_immr_stad_free(
		    struct tofu_mr *mr__priv
		);
extern int	tofu_imp_ulib_immr_stad_temp(
		    void *vp_icep,
		    const struct iovec *iov1,
		    uint64_t *stad_ui64
		);

/* ------------------------------------------------------------------------ */

struct tofu_sep;

extern size_t	tofu_imp_ulib_isep_size(void);
extern int	tofu_imp_ulib_isep_open(
		    struct tofu_sep *sep_priv
		);
extern void	tofu_imp_ulib_isep_clos(
		    struct tofu_sep *sep_priv
		);
extern int	tofu_imp_ulib_isep_qtni(
		    struct tofu_sep *sep_priv,
		    int index,
		    uint64_t *niid_ui64
		);

#endif	/* _TOFU_IMP_ULIB_H */

