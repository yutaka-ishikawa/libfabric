/*
 * Connection Manager
 */
/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"
#include "tofu_macro.h"

#include <assert.h>	    /* for assert() */
#include <string.h>	    /* for strlen() */

/*
 * gname --> getname YI
 *      For VNI support, the following function is for utof on tlib.
 *      For PostK machine, we must rewrite this one.
 */
static int
tofu_imp_ulib_gnam(void *ceps[CONF_TOFU_CTXC],  size_t offs,
                   char nam_str[128])
{
    int fc = FI_SUCCESS;
    int ix, nx = CONF_TOFU_CTXC;
    /* struct ulib_sep_name name[1]; */
    uint8_t xyzabc[8];
    uint16_t /* utofu_cmp_id_t */ cid[1];
    uint16_t /* utofu_tni_id_t */ tnis[ CONF_TOFU_CTXC ];
    uint16_t /* utofu_cq_id_t */  tcqs[ CONF_TOFU_CTXC ];

    xyzabc[0] = 255;

    for (ix = 0; ix < nx; ix++) {
        struct tofu_cep        *icep = (struct tofu_cep *) ceps[ix];
	utofu_vcq_id_t vcqi = -1UL;
	int uc;

	if (icep == 0) {
	    tnis[ix] = 255; tcqs[ix] = 255; continue;
	}
	if (icep->vcqh == 0) {
	    fc = -FI_ENODEV; goto bad;
	}
	uc = utofu_query_vcq_id(icep->vcqh, &vcqi);
	if (uc != UTOFU_SUCCESS) {
	    tnis[ix] = 255; tcqs[ix] = 255; continue;
	}
	uc = utofu_query_vcq_info(vcqi, xyzabc, &tnis[ix], &tcqs[ix], cid);
	if (uc != UTOFU_SUCCESS) {
	    tnis[ix] = 255; tcqs[ix] = 255; continue;
	}
    }
    if (xyzabc[0] == 255) {
	fc = -FI_ENODEV; goto bad;
    }
    {
	int wlen, nx2;
	size_t cz = 128;
	char *cp = nam_str;
	char *del = "";
        cid[0] &= 0x7;
	wlen = snprintf(cp, cz, "t://%u.%u.%u.%u.%u.%u.%x/;q=",
		xyzabc[0], xyzabc[1], xyzabc[2],
                        xyzabc[3], xyzabc[4], xyzabc[5], cid[0]);
	if ((wlen <= 0) || (wlen >= cz)) {
	    fc = -FI_EPERM; goto bad;
	}
	cp += wlen;
	cz -= wlen;

	nx2 = -1;
	for (ix = nx - 1; ix >= 0; ix--) {
	    if (tnis[ix] != 255) { nx2 = ix + 1; break; }
	}
	assert(nx2 >= 0);
	for (ix = 0; ix < nx2; ix++) {
	    if (tnis[ix] == 255) {
		wlen = snprintf(cp, cz, "%s", del);
	    }
	    else {
		wlen = snprintf(cp, cz, "%s%u.%u", del, tnis[ix], tcqs[ix]);
	    }
	    if ((wlen <= 0) || (wlen >= cz)) {
		fc = -FI_EPERM; goto bad;
	    }
	    del = ",";
	    cp += wlen;
	    cz -= wlen;
	}
    }
bad:
    return fc;
}


static int tofu_sep_cm_getname(struct fid *fid, void *addr, size_t *addrlen)
{
    int fc = FI_SUCCESS;
    FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    {
	struct tofu_sep *sep_priv;
	size_t blen;
	/* struct fi_info . addr_format */
	uint32_t addr_format;

	if (addrlen == 0) {
	    fc = -FI_EINVAL; goto bad;
	}
	blen = addrlen[0];
        fprintf(stderr, "%s(,,%ld):%d\n", __func__, blen, __LINE__); fflush(stderr);

	assert(fid != 0);
	switch (fid->fclass) {
	case FI_CLASS_SEP:
            // fprintf(stderr, "YI****** FI_CLASS_SEP\n"); fflush(stderr);
	    sep_priv = container_of(fid, struct tofu_sep, sep_fid.fid);
	    addr_format = sep_priv->sep_dom->dom_fmt;
	    if (addr_format != FI_ADDR_STR) {
                //fprintf(stderr, "YI****** NOT FI_ADDR_STR(0x%x)\n", addr_format); fflush(stderr);
		fc = -FI_EOTHER; goto bad;
	    }
	    /* fill ceps[] */
	    {
	    int ix, nx = CONF_TOFU_CTXC;
	    void *ceps[CONF_TOFU_CTXC];

	    fastlock_acquire( &sep_priv->sep_lck );
	    for (ix = 0; ix < nx; ix++) {
		struct tofu_cep *cep_priv;
		size_t cl = FI_CLASS_TX_CTX;

		cep_priv = tofu_sep_lup_cep_byi_unsafe(sep_priv, cl, ix);
                //fprintf(stderr, "YI********* cep_priv(%p)\n", cep_priv);
		if (cep_priv == 0) {
		    cl = FI_CLASS_RX_CTX;
		    cep_priv = tofu_sep_lup_cep_byi_unsafe(sep_priv, cl, ix);
		}
		if (cep_priv == 0) {
		    ceps[ix] = 0;
		    continue;
		}
		ceps[ix] = cep_priv;
	    }
	    fastlock_release( &sep_priv->sep_lck );
	    {
		const size_t offs_ulib = sizeof (struct tofu_cep);
		char nam[128];

		nam[0] = 0;
		fc = tofu_imp_ulib_gnam(ceps, offs_ulib, nam);
                //fprintf(stderr, "YI********* fc(%d)\n", fc);
		if (fc != FI_SUCCESS) { goto bad; }
		/*
		 * man fi_cm(3)
		 *   fi_getname
		 *     On output, addrlen is set to the size of the buffer
		 *     needed to store the address, which may be larger than
		 *     the input value.
		 */
		if (blen > 0) {
		    int wlen;

		    assert(addr != 0);
		    wlen = snprintf(addr, blen, "%s", nam);
		    if (wlen < 0) {
			fc = -FI_EINVAL; goto bad;
		    }
		    assert((wlen + 1) <= FI_NAME_MAX);
		    assert((wlen + 1) <= 64); /* YYY TOFU_MAX_STR_ADDRLEN */
		    addrlen[0] = 64; /* YYY TOFU_MAX_STR_ADDRLEN */
		    if (wlen >= blen) {
			fc = -FI_ETOOSMALL; goto bad;
		    }
		}
	    }
	    }
	    break;
	case FI_CLASS_EP:
            fprintf(stderr, "YI****** FI_CLASS_EP\n"); fflush(stderr);
	    fc = -FI_EINVAL; goto bad;
	default:
            fprintf(stderr, "YI****** FI_CLASS_?? (0x%lx)\n", fid->fclass); fflush(stderr);
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
	    }
	    else {
		addrlen[0] = sizeof (uint32_t) * 8;
	    }
	    fc = -FI_ETOOSMALL; goto bad;
	}

	FI_INFO(&tofu_prov, FI_LOG_EP_CTRL, "addr %p alen %ld\n",
	    addr, blen);
    }
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

struct fi_ops_cm tofu_cep_ops_cm = {
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

