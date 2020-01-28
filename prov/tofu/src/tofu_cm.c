/*
 * Connection Manager
 */
/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include "tofu_macro.h"

#include <assert.h>	    /* for assert() */
#include <string.h>	    /* for strlen() */

static int
tofu_gnam(struct tofu_sep *sep,  char nam_str[128])
{
    int	 uc, fc = FI_SUCCESS;
    uint8_t	    xyzabc[8];
    utofu_cmp_id_t  cid[1];
    utofu_tni_id_t  tnis[1];
    utofu_cq_id_t   tcqs[1];
    utofu_vcq_hdl_t vcqh;
    utofu_vcq_id_t  vcqi;
    
    vcqh = sep->sep_vcqh;
    if (vcqh != 0) {
	uc = utofu_query_vcq_id(vcqh, &vcqi);
    }
    if (vcqh == 0 || uc != UTOFU_SUCCESS) { fc = -FI_ENODEV; goto bad; }
    uc = utofu_query_vcq_info(vcqi, xyzabc, tnis, tcqs, cid);
    if (uc != UTOFU_SUCCESS) { fc = -FI_ENODEV; goto bad;  }
    cid[0] &= 0x7;
    snprintf(nam_str, 128, "t://%u.%u.%u.%u.%u.%u.%x/;q=%u.%u",
	     xyzabc[0], xyzabc[1], xyzabc[2],
	     xyzabc[3], xyzabc[4], xyzabc[5], cid[0],
	     tnis[0], tcqs[0]);
bad:
    return fc;
}


/*
 * fi_getname
 */
static int
tofu_sep_cm_getname(struct fid *fid, void *addr, size_t *addrlen)
{
    int fc = FI_SUCCESS;
    struct tofu_sep	*sep;
    size_t		blen;
    uint32_t		addr_format;
    char		nam[128];

    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    if (addrlen == 0) {
	fc = -FI_EINVAL; goto bad;
    }
    blen = addrlen[0];
    fprintf(stderr, "%s(,,%ld):%d\n", __func__, blen, __LINE__); fflush(stderr);

    assert(fid != 0);
    switch (fid->fclass) {
    case FI_CLASS_SEP:
	// fprintf(stderr, "YI****** FI_CLASS_SEP\n"); fflush(stderr);
	sep = container_of(fid, struct tofu_sep, sep_fid.fid);
	addr_format = sep->sep_dom->dom_fmt;
	if (addr_format != FI_ADDR_STR) {
	    //fprintf(stderr, "YI****** NOT FI_ADDR_STR(0x%x)\n", addr_format); fflush(stderr);
	    fc = -FI_EOTHER; goto bad;
	}
	nam[0] = 0;
	fc = tofu_gnam(sep, nam);
	if (fc != FI_SUCCESS) { goto bad; }
	/*
	 * man fi_cm(3)
	 *  fi_getname
	 *   On output, addrlen is set to the size of the buffer needed
	 *   to store the address, which may be larger than the input value.
	 */
	if (blen > 0) {
	    int wlen;
	    wlen = snprintf(addr, blen, "%s", nam);
	    if (wlen < 0) {
		fc = -FI_EINVAL; goto bad;
	    }
	    addrlen[0] = 64; /* YYY TOFU_MAX_STR_ADDRLEN */
	    if (wlen >= blen) {
		fc = -FI_ETOOSMALL; goto bad;
	    }
	}
	break;
    case FI_CLASS_EP:
	fprintf(stderr, "YI****** FI_CLASS_EP\n"); fflush(stderr);
	fc = -FI_EINVAL; goto bad;
    default:
	fprintf(stderr, "YI****** FI_CLASS_?? (0x%lx)\n", fid->fclass);
	fflush(stderr);
	fc = -FI_EINVAL; goto bad;
    }
    /*
     * man fi_cm(3)
     *   fi_getname
     *     If the actual address is larger than what can fit into the
     *     buffer, it will be truncated and -FI_ETOOSMALL will be returned.
     */
    if (addr == 0) {
	if (addr_format == FI_ADDR_STR) {
	    addrlen[0] = 64; /* YYY TOFU_MAX_STR_ADDRLEN */
	} else {
	    addrlen[0] = sizeof (uint32_t) * 8;
	}
	fc = -FI_ETOOSMALL; goto bad;
    }
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "addr %p alen %ld\n", addr, blen);
bad:
    return fc;
}

struct fi_ops_cm tofu_sep_ops_cm = {
    .size           = sizeof(struct fi_ops_cm),
    .setname        = fi_no_setname,
    .getname        = tofu_sep_cm_getname,
    .getpeer        = fi_no_getpeer,
    .connect        = fi_no_connect,
    .listen         = fi_no_listen,
    .accept         = fi_no_accept,
    .reject         = fi_no_reject,
    .shutdown       = fi_no_shutdown,
    .join           = fi_no_join,
};

struct fi_ops_cm tofu_ctx_ops_cm = {
    .size	    = sizeof(struct fi_ops_cm),
    .setname	    = fi_no_setname,
    .getname	    = fi_no_getname,
    .getpeer	    = fi_no_getpeer,
    .connect	    = fi_no_connect,
    .listen	    = fi_no_listen,
    .accept	    = fi_no_accept,
    .reject	    = fi_no_reject,
    .shutdown	    = fi_no_shutdown,
    .join	    = fi_no_join,
};

