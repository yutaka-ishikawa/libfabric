/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#include <ofi_prov.h>
#include "tofu_impl.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern struct fi_info tofu_prov_info; /* defined in tofu_attr.c */

static int
tofu_getinfo(uint32_t version, const char *node,
             const char *service, uint64_t flags,
             const struct fi_info *hints, struct fi_info **info)
{
    struct fi_info *fiinfo = 0;
    int fc = -FI_ENOMEM;

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
            "\t   info->fabric_attr->prov_name: 0x%p (%s)\n"
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
            fiinfo->fabric_attr ? fiinfo->fabric_attr->prov_name : "NULL",
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

static int tfi_confirm;
static void
tfi_update_caps(const char *attr, uint64_t flg)
{
    char        *cp = getenv(attr);
    if (cp) {
        int         val = atoi(cp);
        if (val == 0) {
            tofu_prov_info.caps = tofu_prov_info.caps & ~flg;
        } else {
            tofu_prov_info.caps = tofu_prov_info.caps | flg;
        }
        if (tfi_confirm) {
            fprintf(stdout, "%s : %d\n", attr, val == 0 ? 0 : 1);
        }
    }
}

/**************************************************************************/
extern void utf_redirect();
extern int utf_dbg_progress(int);
int rdbgf;
int rdbgl;
int tfi_progress_compl_pending;

TOFU_INI
{
    char *cp;
    FI_INFO(&tofu_prov, FI_LOG_DEBUG, "\n**** TOFU INIT ****\n");
#ifndef NDEBUG
    // fprintf(stderr, "**** Debug option is enable\n"); fflush(stderr);
#endif /* ~NDEBUG */
    {
        int	myrank, nprocs, ppn, rc;

        DEBUG(DLEVEL_INIFIN) {
            utf_printf("%s: YI!!! CALLING utf_init\n", __func__);
        }
        rc = utf_init(0, NULL, &myrank, &nprocs, &ppn);
        if (rc < 0) {
            utf_printf("%s: YI!!! RETURNING from utf_init rc(%d) ERROR\n", __func__, rc);
            abort();
        }
    }
    DEBUG(DLEVEL_INIFIN) {
        extern utofu_stadd_t	utf_egr_rbuf_stadd;
        utf_printf("TOFU: eager receive buffer addresses:\n"
                   " utf_egr_rbuf : 0x%lx (stadd: 0x%lx)"
                   " utf_egr_rbuf.rbuf[0] : 0x%lx (stadd: 0x%lx)"
                   " utf_egr_rbuf.rbuf[%d*126] : 0x%lx (stadd: 0x%lx)"
                   " utf_egr_rbuf.rbuf[%d*%d] : 0x%lx\n",
                   &utf_egr_rbuf, utf_egr_rbuf_stadd,
                   &utf_egr_rbuf.rbuf, utf_egr_rbuf_stadd,
                   COM_RBUF_SIZE, &utf_egr_rbuf.rbuf[COM_RBUF_SIZE*126],
                   utf_egr_rbuf_stadd + (uint64_t)&((struct utf_egr_rbuf*)0)->rbuf[COM_RBUF_SIZE*126],
                   COM_RBUF_SIZE, RCV_CNTRL_MAX, &utf_egr_rbuf.rbuf[COM_RBUF_SIZE*RCV_CNTRL_MAX]);
    }
    cp = getenv("TOFU_DEBUG_LVL");
    if (cp) {
        if (!strncmp(cp, "0x", 2)) {
            sscanf(cp, "%x", &rdbgl);
        } else {
            rdbgl = atoi(cp);
        }
    }
    cp = getenv("TOFU_DEBUG_FD");
    if (cp) {
        rdbgf = atoi(cp);
        utf_redirect();
    }
    cp = getenv("TOFU_NAMED_AV");
    if (cp) {
        extern int tofu_av_named;
        tofu_av_named = atoi(cp);
    }
    cp = getenv("UTF_MRAIL");
    if (cp) {
        utf_mode_mrail = atoi(cp);
    }
    /* might be obsolete 2020/10/10 */
    cp = getenv("TFI_COMPLETION_PENDING");
    if (cp) {
        tfi_progress_compl_pending = atoi(cp);
    } else {
        tfi_progress_compl_pending = CONF_TOFU_FI_COMPL_PENDING;
    }
    {
        extern int utf_getenvint(char*);
        extern void tfi_dbg_init();
        int     i;
        i = utf_getenvint("UTF_DBGTIMER_INTERVAL");
        tfi_dbg_timer = i;
        i = utf_getenvint("UTF_DBGTIMER_ACTION");
        tfi_dbg_timact = i;
        tfi_dbg_init();
        i = utf_getenvint("TOFU_COMDEBUG");
        tfi_dbg_info = i;
    }
    cp = getenv("TFI_CONFIRM");
    if (cp) {
        tfi_confirm = 1;
    }
    tfi_update_caps("TFI_ENABLE_TAGGED", FI_TAGGED);
    tfi_update_caps("TFI_ENABLE_RMA", FI_RMA);
    DEBUG(DLEVEL_INIFIN) {
        fprintf(stderr, "TFI_COMPLETION_PENDING = %d\n", tfi_progress_compl_pending);
    }
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
    int     iarg1, iarg2;
    int		rc = 0;
    va_start(ap, cmd);
    switch (cmd) {
    case 0:
        iarg1 = va_arg(ap, int);
        rc = utf_dbg_progress(iarg1);
        break;
    case 1: 
        utf_dbg_progress(1);
        break;
    case 2:
        iarg1 = va_arg(ap, int);
        utf_recvcntr_show(iarg1 == 1 ? stdout : stderr);
        break;
    case 3:
        iarg1 = va_arg(ap, int);
        iarg2 = va_arg(ap, int);
        if (iarg2 == utf_info.myrank) {
            utf_recvcntr_show(iarg1 == 1 ? stdout : stderr);
        }
        break;
    case 4:
        utf_cqtab_show(stderr);
        break;
    }
    va_end(ap);
    return rc;
}
