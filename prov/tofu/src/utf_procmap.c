#ifdef LIBPROCMAP_TEST
#include <mpi.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <pmix.h>
#include <utofu.h>
#include <jtofu.h>
#include <pmix_fjext.h>
#include "process_map_info.h"
#include "tofu_impl.h"

#define LIB_DLCALL(var, funcall, lbl, evar, str) do { \
	var = funcall;			\
	if (var == NULL) {		\
		evar = str; goto lbl;	\
	}				\
} while(0);

#define LIB_DLCALL2(rc, funcall, lbl, evar, str) do {	\
	rc = funcall;			\
	if (rc != 0) {			\
		evar = str; goto lbl;	\
	}				\
} while(0);

#define LIB_CALL(rc, funcall, lbl, evar, str) do { \
	rc = funcall;		      \
	if (rc != 0) {			\
		evar = str; goto lbl;	\
	}				\
} while(0);

/*
 * 31             24 23           16 15            8 7 6 5  2 1 0
 * +---------------+---------------+---------------+---+----+---+
 * |              x|              y|              z|  a|   b|  c|
 * +---------------+---------------+---------------+---+----+---+
 */
struct tofu_coord {
    uint32_t	c: 2, b: 4, a: 2, z: 8, y: 8, x: 8;
};

#define PATH_PMIXLIB	"/usr/lib/FJSVtcs/ple/lib64/libpmix.so"

static void	*dlp;
static pmix_status_t	(*myPMIx_Init)(pmix_proc_t *proc, pmix_info_t info[], size_t ninfo);
static pmix_status_t	(*myPMIx_Get)(const pmix_proc_t *proc, const char key[],
				      const pmix_info_t info[], size_t ninfo,
				      pmix_value_t **val);
static pmix_status_t	(*myPMIx_Finalize)(const pmix_info_t info[], size_t ninfo);
static pmix_proc_t	pmix_proc[1];
static pmix_value_t	*pval;
static pmix_value_t	*pval;

static int
dlib_init()
{
    int	 flag = RTLD_LAZY | RTLD_GLOBAL;
    int rc;
    char *errstr;
    char *lpath = getenv("LIBFJPMIX_PATH");

    if (lpath == 0) {
	lpath = PATH_PMIXLIB;
    }
    dlp = dlopen(lpath, flag);
    if (dlp == NULL) {
	fprintf(stderr, "%s: %s\n", __func__, dlerror());
	return -1;
    }
    LIB_DLCALL(myPMIx_Init, dlsym(dlp, "FJPMIx_Init"), err1, errstr, "dlsym FJPMIx_Init");
    LIB_DLCALL(myPMIx_Finalize, dlsym(dlp, "FJPMIx_Finalize"), err1, errstr, "dlsym FJPMIx_Finalize");
    LIB_DLCALL(myPMIx_Get, dlsym(dlp, "FJPMIx_Get"), err1, errstr, "dlsym FJPMIx_Get");
    LIB_DLCALL2(rc, dlclose(dlp), err1, errstr, "dlclose");
    return 0;
err1:
    fprintf(stderr, "error %s: %s\n", errstr, dlerror());
    return -1;
}

static char *
string_tofu6d(uint32_t coord, char *buf)
{
    struct tofu_coord tc = *(struct tofu_coord*)(&coord);
    snprintf(buf, 128, "x(%02d) y(%02d) z(%02d) a(%02d) b(%02d) c(%02d)",
	     tc.x, tc.y, tc.z, tc.a, tc.b, tc.c);
    return buf;
}

static char *
string_tofu3d(tlib_tofu3d host, char *buf)
{
    snprintf(buf, 128, "x(%02d) y(%02d) z(%02d)", host.x, host.y, host.z);
    return buf;
}

static void
show_ranklist(off_t *offset, int nprocs)
{
    static char	buf1[128], buf2[128];
    struct tlib_ranklist	*rankp = (struct tlib_ranklist*) TLIB_OFFSET2PTR(*offset);
    int	i;
    for(i = 0; i < nprocs; i++) {
	printf("\tphys=%s virt=%s\n",
	       string_tofu6d((rankp + i)->physical_addr, buf1),
	       string_tofu3d((rankp + i)->virtual_addr, buf2));
    }
}

void
show_procmap(struct tlib_process_mapinfo *minfo, int nprocs)
{
    printf("offset_logical_node_list=0x%lx entries=%ld\n",
	   minfo->offset_logical_node_list,
	   ((uint64_t) TLIB_OFFSET2PTR(minfo->offset_logical_node_proc)
	    - (uint64_t) TLIB_OFFSET2PTR(minfo->offset_logical_node_list))/sizeof(struct tlib_ranklist));
    show_ranklist(&minfo->offset_logical_node_list, nprocs);
}

struct tofu_vname *
utf_peers_reg(struct tlib_process_mapinfo *minfo, int nprocs, uint64_t **fi_addr)
{
    struct tlib_ranklist *rankp;
    size_t	sz, ic;
    struct tofu_vname *vnm;
    uint64_t	*addr;

    rankp = (struct tlib_ranklist*) TLIB_OFFSET2PTR(*(off_t *)&minfo->offset_logical_node_list);
    sz = sizeof(struct tofu_vname)*nprocs;
    vnm = malloc(sz);
    memset(vnm, 0, sz);
    if (*fi_addr == NULL) {
	sz = sizeof(uint64_t)*nprocs;
	*fi_addr = malloc(sz);
    }
    addr = *fi_addr;

    for (ic = 0; ic < nprocs; ic++) {
	struct tofu_coord tc;
	utofu_tni_id_t	tni = 0;
	utofu_cq_id_t	cq = 0;
	uint16_t	cid = CONF_TOFU_CMPID;
	utofu_vcq_id_t  vcqid;

	tc = *(struct tofu_coord*) &((rankp + ic)->physical_addr);
	vnm[ic].xyzabc[0] = tc.x; vnm[ic].xyzabc[1] = tc.y; vnm[ic].xyzabc[2] = tc.z; vnm[ic].xyzabc[3] = tc.a;
	vnm[ic].xyzabc[4] = tc.b;  vnm[ic].xyzabc[5] = tc.c;
	vnm[ic].cid = CONF_TOFU_CMPID;
	vnm[ic].v = 1;
	vnm[ic].vpid = ic;
	utofu_construct_vcq_id(vnm[ic].xyzabc, tni, cq, cid, &vcqid);
        vnm[ic].vcqid = vcqid;
	if (addr) addr[ic] = ic;
    }
    return vnm;
}

struct tofu_vname *
utf_get_peers(uint64_t **fi_addr, int *npp, int *rnkp)
{
    static int first = 0;
    int	myrank, nprocs;
    struct tofu_vname *vnam;
    struct tlib_process_mapinfo	*minfo;
    char *errstr;
    int	rc;

    if (first == 0) {
	rc = dlib_init();
	if (rc < 0) return NULL;
	first = 1;
    }
    LIB_CALL(rc, myPMIx_Init(pmix_proc, NULL, 0),
	     err, errstr, "PMIx_Init");

    myrank = pmix_proc->rank;
    pmix_proc->rank= PMIX_RANK_WILDCARD;
    LIB_CALL(rc, myPMIx_Get(pmix_proc, PMIX_JOB_SIZE, NULL, 0, &pval),
	     err, errstr, "Cannot get PMIX_JOB_SIZE");
    nprocs = pval->data.uint32;
    // printf("nprocs = %d\n", nprocs); fflush(stdout);

    LIB_CALL(rc, myPMIx_Get(pmix_proc, FJPMIX_RANKMAP, NULL, 0, &pval),
	     err, errstr, "Cannot get FJPMIX_RANKMAP");
    //printf("pval->data.bo.bytes(%p) pval->data.bo.size(%ld)\n",
    //pval->data.bo.bytes, pval->data.bo.size); fflush(stdout);

    minfo = (struct tlib_process_mapinfo*) pval->data.bo.bytes;
    if (myrank == 0) show_procmap(minfo, nprocs);
    LIB_CALL(rc, myPMIx_Finalize(NULL, 0),
	     err, errstr, "PMIx_Finalize");

    vnam = utf_peers_reg(minfo, nprocs, fi_addr);
    *npp = nprocs;
    *rnkp = myrank;
    return vnam;
err:
    fprintf(stderr, "%s\n", errstr);
    return NULL;
}

#ifdef LIBPROCMAP_TEST
int
main(int argc, char **argv)
{
    int	nprocs, myrank;
    int rc, np, rank;
    char *cp;
    char *errstr;
    struct tofu_vname *vnam;
    uint64_t	*addr = NULL;
    
    LIB_CALL(rc, MPI_Init(&argc, &argv), err, errstr, "MPI_Init");
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    if (myrank == 0) {
	printf("Proc Size = %d\n", nprocs); fflush(stdout);
    }
    cp = getenv("TOFU_NAMED_AV");
    if (cp && atoi(cp) != 0) {
	printf("TOFU_NAMED_AV is reset and run again\n");
	goto err0;
    }
    vnam = utf_get_peers(&addr, &np, &rank);
    if (vnam == NULL) {
	printf("utf_get_peers error\n");
	goto err0;
    }
    if (myrank == 0) {
	int	i;
	printf("\t:nproc = %d\n", np);
	for (i = 0; i < nprocs; i++) {
	    utofu_vcq_id_t vcqid;
	    uint8_t coords[8];
	    uint16_t tni, tcq, cid;
	    vcqid = vnam[i].vcqid;
	    rc = utofu_query_vcq_info(vcqid, coords, &tni, &tcq, &cid);
	    printf("\t: [%ld] vcqid(%lx), "
		   "x(%02d) y(%02d) z(%02d) a(%02d) b(%02d) c(%02d) tni(%02d) tcq(%02d) cid(%02d)\n",
		   addr == NULL ? -1 : addr[i],
		   vcqid, coords[0], coords[1], coords[2], coords[3], coords[4], coords[5], tni, tcq, cid);
	}
    }
err0:
    fflush(stdout);
    MPI_Finalize();
    return 0;
err:
    fprintf(stderr, "%s\n", errstr);
    return -1;
}
#endif /* LIBPROCMAP_TEST */
