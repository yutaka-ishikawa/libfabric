/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include <ofi_prov.h>
#include "tofu_impl.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern struct fi_info tofu_prov_info; /* defined in tofu_attr.c */
struct util_prov tofu_util_prov = {
	.prov = &tofu_prov,
	.info = &tofu_prov_info,
	.flags = 0,
};


static int
tofu_getinfo(uint32_t version, const char *node,
             const char *service, uint64_t flags,
             const struct fi_info *hints, struct fi_info **info)
{
    struct fi_info *fiinfo = 0;
    int fc = -FI_ENOMEM;

    // fprintf(stderr, "**** YI %s\n", __func__);
    FI_INFO(&tofu_prov, FI_LOG_CORE, "in %s ; version %08x\n",
            __FILE__, version);

    fc = tofu_init_prov_info(hints, &fiinfo);
    if (fc != FI_SUCCESS) {
        fprintf(stderr, "%s ERROR 1 fc(%d)\n", __func__, fc);
        fc = -FI_ENODATA;
    }
    fc = tofu_chck_prov_info(version, hints, &fiinfo);
    if (fc != 0) {
        fprintf(stderr, "%s ERROR 2 fc(%d)\n", __func__, fc);
        FI_INFO(&tofu_prov, FI_LOG_CORE, "fail.\n");
        return fc;
    }
    fiinfo->src_addr = malloc(fiinfo->src_addrlen);
    fiinfo->dest_addr = malloc(fiinfo->dest_addrlen);
    memset(fiinfo->src_addr, 0, fiinfo->src_addrlen);
    memset(fiinfo->dest_addr, 0, fiinfo->dest_addrlen);
    *info = fiinfo;
    FI_INFO(&tofu_prov, FI_LOG_DEBUG,
            "\n\tinfo(%p)->src_addr: 0x%p\n"
            "\tinfo->dest_addr: 0x%p\n"
            "\tinfo->tx_attr: 0x%p\n"
            "\tinfo->rx_attr: 0x%p\n"
            "\tinfo->ep_attr: 0x%p\n"
            "\t   info->ep_attr->auth_key: 0x%p\n"
            "\tinfo->domain_attr: 0x%p\n"
            "\t   info->domain_attr->auth_key: 0x%p\n"
            "\t   info->domain_attr->name: 0x%p\n"
            "\tinfo->fabric_attr: 0x%p\n"
            "\t   info->fabric_attr->name: 0x%p\n"
            "\t   info->fabric_attr->prov_name: 0x%p\n"
            "\tinfo->nic: 0x%p\n",
            fiinfo, fiinfo->src_addr, fiinfo->dest_addr, fiinfo->tx_attr,
            fiinfo->rx_attr, fiinfo->ep_attr,
            fiinfo->ep_attr ? fiinfo->ep_attr->auth_key : 0,
            fiinfo->domain_attr,
            fiinfo->domain_attr ? fiinfo->domain_attr->auth_key : 0,
            fiinfo->domain_attr ? fiinfo->domain_attr->name : 0,
            fiinfo->fabric_attr,
            fiinfo->fabric_attr ? fiinfo->fabric_attr->name : 0,
            fiinfo->fabric_attr ? fiinfo->fabric_attr->prov_name : 0,
            fiinfo->nic);
    //fprintf(stderr, "**** YI %s return %d\n", __func__, fc);
    return fc;
}

static void tofu_fini(void)
{
    FI_INFO(&tofu_prov, FI_LOG_CORE, "in %s\n", __FILE__);
    /*
     * Do not need to call this function, maybe 2019/04/09 YI
     * fi_log_fini();
     */
    return;
}

struct fi_provider tofu_prov = {
    .name =	    FI_TOFU_FABRIC_NAME,
    .version =      FI_TOFU_VERSION,
    .fi_version =   FI_VERSION(1, 7),
    /* .context */
    .getinfo =	    tofu_getinfo,
    .fabric =	    tofu_fabric_open,
    .cleanup =	    tofu_fini,
};

/* fi_prov_ini (a.k.a. fi_tofu_ini) */
struct fi_provider *fi_prov_ini(void)
{
    struct fi_provider *p = 0;

    fi_log_init();
    FI_INFO(&tofu_prov, FI_LOG_CORE, "in %s\n", __FILE__);
    p = &tofu_prov;

    return p;
}

/**************************************************************************/
extern void utf_redirect();
extern int utf_dbg_progress(int);
int mypid, myrank, nprocs;
int rdbgf;
int rdbgl;

TOFU_INI
{
    FI_INFO(&tofu_prov, FI_LOG_DEBUG, "\n**** TOFU INIT ****\n");
#ifndef NDEBUG
    // fprintf(stderr, "**** Debug option is enable\n"); fflush(stderr);
#endif /* ~NDEBUG */
    mypid = getpid();
    myrank = -1; /* will be initialized later */
    {
        char *cp;
        cp = getenv("TOFU_DEBUG_LVL");
        if (cp) {
            rdbgl = atoi(cp);
        }
        cp = getenv("TOFU_DEBUG_FD");
        if (cp) {
            rdbgf = atoi(cp);
            utf_redirect();
        }
    }
#if 0
#ifndef TSIM  /* This is only for real-machines, not a simulated environment */
    {
	jtofu_job_id_t	j_id;
	j_id = generate_hash_string(pmix_proc->nspace);
	jtofu_initialize(j_id, pmix_proc->rank, v->data.bo.bytes);
    }
#endif
#endif
#ifdef TOFU_SIM_BUG
    {
        extern void wa_init();
        fprintf(stderr, "[%d]\t*** utofu on tlib debug workaround is enable\n", mypid);
        fflush(stderr);
        printf("[%d] *** utofu on tlib debug workaround is enable\n", mypid);
        fflush(stdout);
        wa_init();
    }
#endif
    return &tofu_prov;
}

__attribute__((visibility ("default"), EXTERNALLY_VISIBLE))
void fi_tofu_setdopt(int flg, int lvl)
{
    rdbgf = flg;
    rdbgl = lvl;
    R_DBG("fi_tofu_setdopt is called with %x %x", flg, lvl);
    utf_redirect();
}

__attribute__((visibility ("default"), EXTERNALLY_VISIBLE))
int fi_tofu_cntrl(int cmd, ...)
{
    va_list	ap;
    int		rc = 0;
    va_start(ap, cmd);
    switch (cmd) {
    case 0: {
        int     mode;
        mode = va_arg(ap, int);
        rc = utf_dbg_progress(mode);
        break;  }
    }
    va_end(ap);
    return rc;
}
