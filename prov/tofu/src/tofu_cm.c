/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include "tofu_impl.h"

#include <assert.h>	    /* for assert() */
#include <string.h>	    /* for strlen() */


static int tofu_sep_cm_getname(struct fid *fid, void *addr, size_t *addrlen)
{
    int fc = FI_SUCCESS;
    FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "in %s\n", __FILE__);
    {
	struct tofu_sep *sep_priv;
	size_t blen;
	/* struct fi_info . addr_format */
	uint32_t addr_format;

	/*
	 * man fi_cm(3)
	 *   fi_getname
	 *     On input, the addrlen parameter should indicate the size of
	 *     the addr buffer.
	 */
	if (addrlen == 0) {
	    fc = -FI_EINVAL; goto bad;
	}
	blen = addrlen[0];
printf("%s(,,%ld):%d\n", __func__, blen, __LINE__);

	assert(fid != 0);
	switch (fid->fclass) {
	case FI_CLASS_SEP:
	    sep_priv = container_of(fid, struct tofu_sep, sep_fid.fid);
	    if (sep_priv == 0) { }
	    addr_format = sep_priv->sep_dom->dom_fmt;
	    if (addr_format != FI_ADDR_STR) {
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
	    /* break; */
	default:
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

	FI_INFO( &tofu_prov, FI_LOG_EP_CTRL, "addr %p alen %ld\n",
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

