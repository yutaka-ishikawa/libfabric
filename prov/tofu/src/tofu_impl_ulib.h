/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef	_TOFU_IMP_ULIB_H
#define _TOFU_IMP_ULIB_H

/* internal header */

#include <rdma/fabric.h>	/* for struct fi_[rt]x_attr */
#include <stddef.h>		/* for size_t */
#include <stdint.h>		/* for uint64_t */
#include <sys/uio.h>		/* for struct iovec */

/* #include <rdma/fi_tagged.h> */ /* for struct fi_msg_tagged */
struct fi_msg_tagged;

/* #include <rdma/fi_eq.h> */	/* for struct fi_cq_tagged_entry */
struct fi_cq_tagged_entry;

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
                    struct tofu_recv_en *send_entry);
extern int	tofu_imp_ulib_recv_post(
		    void *vptr,
		    size_t offs,
		    const struct fi_msg_tagged *tmsg,
		    uint64_t flags,
		    tofu_imp_ulib_comp_f func,
		    void *farg
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

