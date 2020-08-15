#include "tofu_impl.h"
#include "tofu_addr.h"

static char *
tofu_impl_str_tniq2long(const char *pv, long lv[2])
{
    const char *pv1, *pv2;
    char *p1, *p2;
    long lv1, lv2;

    pv1 = pv; p1 = 0;
    lv1 = strtol(pv1, &p1, 10);
    if ((p1 == 0) || (p1 <= pv1) || (lv1 < 0)) {
	return 0;
    }
    if (p1[0] != '.') {
	return 0;
    }
    pv2 = p1 + 1; p2 = 0;
    lv2 = strtol(pv2, &p2, 10);
    if ((p2 == 0) || (p2 <= pv2) || (lv2 < 0)) {
	return 0;
    }
    if (lv != 0) {
	lv[0] = lv1;
	lv[1] = lv2;
    }
    return (p2[0] == ',')? p2 + 1: p2;
}

static int
tofu_impl_uri_param2long(const char *pparm, long *tniq)
{
    int ret = 0;
    const char *cp = pparm;
    int nv = 0;
    long lv_orun[2];

    do {
	const char *pq;

	pq = strchr(cp, ';');
	if (pq == 0) { break; }

	if (strncmp(pq, ";q=", 3) == 0) {
	    const char *pv = pq + 3;

	    if (nv > 0) { /* duplicate ? */
		R_DBG("violarion nv(%d) pparm=%s\n",  nv, pparm);
		ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	    }
	    while ((pv[0] != '\0') && (pv[0] != ';')) {
		long *lv = (nv >= CONF_TOFU_CTXC)? lv_orun: &tniq[nv * 2];
		const char *new_pv;

		new_pv = tofu_impl_str_tniq2long(pv, lv);
		if (new_pv == 0) {
		    R_DBG("violation pv=%s lv=%ld\n", pv, *lv);
		    ret = -FI_EINVAL; /* -__LINE__ */; goto bad;
		}
		nv++;
		pv = new_pv;
	    }
	    cp = pv;
	} else { /* unknown parameter */
	    cp = pq + 1;
	}
    } while (1);

    /* check values */
    {
	int iv;

	for (iv = 0; iv < nv; iv++) {
	    long *lv = &tniq[iv * 2];

	    if (iv >= CONF_TOFU_CTXC) { continue; }
	    if ((lv[0] < 0) || (lv[0] >= 8  /* XXX */ )) { /* max # of tni */
		R_DBG("violation lv[0]=(%ld) must be 0-7\n", lv[0]);
		ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	    }
	    if ((lv[1] < 0) || (lv[1] >= 32 /* XXX */ )) { /* max # of tcq */
		R_DBG("violation lv[1]=(%ld) must be 0-7\n", lv[1]);
		ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	    }
	}
    }
    ret = nv;
bad:
    return ret;
}

static int
tofu_imp_uri2long(const char *uri, union jtofu_phys_coords *xyzabc,  uint16_t *cid, long *tniq)
{
    int ret = 0;

    /* xyzabc */
    {
	int rc;
        uint32_t cid32;

	rc = sscanf(uri, "t://%hhd.%hhd.%hhd.%hhd.%hhd.%hhd.%x/;q=",
		    &xyzabc->s.x, &xyzabc->s.y, &xyzabc->s.z,
                    &xyzabc->s.a, &xyzabc->s.b, &xyzabc->s.z, &cid32);
        *cid = cid32;
	if (rc != 7) {
	    R_DBG("# of entries != 7 uri=%s\n", uri);
	    ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	}
	if (xyzabc->s.a >= 2 || xyzabc->s.b >= 3  || xyzabc->s.c >= 2) {
	    R_DBG("xyzabc violarion uri=%s\n", uri);
	    ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	}
    }

    /* ;q=<tniq> */
    {
	int nv;

	nv = tofu_impl_uri_param2long(uri, tniq);
	if (nv < 0) {
	    ret = -FI_EINVAL /* -__LINE__ */; goto bad;
	}
	ret = nv;
    }
bad:
    return ret;
}

/*
 * URI --> Tofu address
 */
int
tofu_impl_uri2name(const void *vuri, size_t index,  struct tofu_vname *vnam)
{
    int fc = FI_SUCCESS;
    const char *cp;
    int		nv, iv, mv;
    long	tniq[CONF_TOFU_CTXC*2];
    uint16_t    cid;

   
    cp = (const char *)vuri + (index * 64 /* FI_NAME_MAX */ );
    nv = tofu_imp_uri2long(cp, &vnam->xyzabc, &cid, tniq);
    if (nv < 0) {
	fc = nv; goto bad;
    }
    vnam->cid = cid;
    vnam->v = 1;

    mv = sizeof (vnam->tniq) / sizeof (vnam->tniq[0]);
    for (iv = 0; iv < mv; iv++) {
	long *lv = &tniq[iv * 2];
	if (iv >= CONF_TOFU_CTXC || iv >= nv
	    || (lv[0] < 0) || (lv[0] >= 6) /* max # of tni */
	    || (lv[1] < 0) || (lv[1] >= 16)) /* max # of tcq */ {
	    vnam->tniq[iv] = 0xff;
	    continue;
	}
	/* tniid << 4 | cqid */
	vnam->tniq[iv] = (lv[0] << 4) | (lv[1] << 0);
    }
    vnam->vpid = -1U; /* YYY */
bad:
#if 0
    R_DBG0(RDBG_LEVEL1, "%u.%u.%u.%u.%u.%u cid(%u) return fc=%d",
            vnam->xyzabc[0], vnam->xyzabc[1], vnam->xyzabc[2],
            vnam->xyzabc[3], vnam->xyzabc[4], vnam->xyzabc[5], vnam->cid, fc);
#endif
    return fc;
}
