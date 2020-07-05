#ifdef LIBPROCMAP_TEST
#include <mpi.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pmix.h>
#include <utofu.h>
#include <jtofu.h>
#include <pmix_fjext.h>
#include "process_map_info.h"
#include "utf_conf.h"
#include "utf_externs.h"
#include "utf_queue.h"
#include "utf_errmacros.h"
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
static pmix_status_t	(*myPMIx_Put)(pmix_scope_t scope, const char key[],
				      pmix_value_t *val);
static pmix_status_t	(*myPMIx_Commit)(void);
static pmix_status_t	(*myPMIx_Finalize)(const pmix_info_t info[], size_t ninfo);

static pmix_proc_t	pmix_proc[1];
static pmix_value_t	*pval;
static pmix_value_t	*pval;
static int	pmix_myrank, pmix_nprocs;
static struct tofu_vname *pmix_vnam;
static struct tlib_process_mapinfo	*minfo;
static uint64_t		*my_fiaddr;
static int		my_ppn;


static int
dlib_init()
{
    static int	notfirst = 0;
    int	 flag = RTLD_LAZY | RTLD_GLOBAL;
    int rc;
    char *errstr;
    char *lpath = getenv("LIBFJPMIX_PATH");

    if (notfirst) {
	if (notfirst == -1) return -1;
	else return 0;
	notfirst = 1;
    }
    if (lpath == 0) {
	lpath = PATH_PMIXLIB;
    }
    dlp = dlopen(lpath, flag);
    if (dlp == NULL) {
	fprintf(stderr, "%s: %s\n", __func__, dlerror());
	fprintf(stdout, "%s: %s\n", __func__, dlerror());
	return -1;
    }
#ifdef TSIM
    LIB_DLCALL(myPMIx_Init, dlsym(dlp, "PMIx_Init"), err0, errstr, "dlsym PMIx_Init");
    LIB_DLCALL(myPMIx_Finalize, dlsym(dlp, "PMIx_Finalize"), err0, errstr, "dlsym PMIx_Finalize");
    LIB_DLCALL(myPMIx_Get, dlsym(dlp, "PMIx_Get"), err0, errstr, "dlsym PMIx_Get");
    LIB_DLCALL(myPMIx_Put, dlsym(dlp, "PMIx_Put"), err0, errstr, "dlsym PMIx_Put");
    LIB_DLCALL(myPMIx_Commit, dlsym(dlp, "PMIx_Commit"), err0, errstr, "dlsym PMIx_Commit");
#else
    LIB_DLCALL(myPMIx_Init, dlsym(dlp, "FJPMIx_Init"), err0, errstr, "dlsym FJPMIx_Init");
    LIB_DLCALL(myPMIx_Finalize, dlsym(dlp, "FJPMIx_Finalize"), err0, errstr, "dlsym FJPMIx_Finalize");
    LIB_DLCALL(myPMIx_Get, dlsym(dlp, "FJPMIx_Get"), err0, errstr, "dlsym FJPMIx_Get");
    LIB_DLCALL(myPMIx_Put, dlsym(dlp, "FJPMIx_Put"), err0, errstr, "dlsym FJPMIx_Put");
    LIB_DLCALL(myPMIx_Commit, dlsym(dlp, "FJPMIx_Commit"), err0, errstr, "dlsym FJPMIx_Commit");
#endif
err0:
    LIB_DLCALL2(rc, dlclose(dlp), err1, errstr, "dlclose");
    return 0;
err1:
    fprintf(stderr, "error %s: %s\n", errstr, dlerror());
    fprintf(stdout, "error %s: %s\n", errstr, dlerror());
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
    size_t	nent;
    nent = ((uint64_t) TLIB_OFFSET2PTR(minfo->offset_logical_node_proc)
	    - (uint64_t) TLIB_OFFSET2PTR(minfo->offset_logical_node_list))/sizeof(struct tlib_ranklist);
    printf("offset_logical_node_list=0x%lx entries=%ld\n",
	   minfo->offset_logical_node_list, nent);
    show_ranklist(&minfo->offset_logical_node_list, nent);
}

void
utf_vname_vcqid(struct tofu_vname *vnmp)
{
    utofu_vcq_id_t  vcqid;
    size_t	ncnt;

    UTOFU_CALL(1, utofu_construct_vcq_id,  vnmp->xyzabc,
	       vnmp->tniq[0]>>4, vnmp->tniq[0]&0x0f, vnmp->cid, &vcqid);
    /* getting candidates of path coord, vnmp->pcoords */
    {
	union jtofu_phys_coords jcoords;
	jcoords.s.x = vnmp->xyzabc[0]; jcoords.s.y = vnmp->xyzabc[1];
	jcoords.s.z = vnmp->xyzabc[2]; jcoords.s.a = vnmp->xyzabc[3];
	jcoords.s.b = vnmp->xyzabc[4]; jcoords.s.c = vnmp->xyzabc[5];
	JTOFU_CALL(1, jtofu_query_onesided_paths, &jcoords, 5, vnmp->pcoords, &ncnt);
    }
    vnmp->pent = ncnt;
    /* pathid is set using the first candidate of vnmp->pcoords */
    UTOFU_CALL(1, utofu_get_path_id, vcqid, vnmp->pcoords[0].a, &vnmp->pathid);
    /* The default pathid is also embeded into vcqid */
    UTOFU_CALL(1, utofu_set_vcq_id_path, &vcqid, vnmp->pcoords[0].a);
    vnmp->vcqid = vcqid;
}

static struct tofu_vname *
utf_peers_reg(struct tlib_process_mapinfo *minfo, int nprocs, uint64_t **fi_addr, int *ppnp)
{
    struct tlib_ranklist *rankp;
    size_t	sz, ic;
    struct tofu_vname *vnam;
    uint64_t	*addr;
    int		nhost;
    int		ppn;	/* process per node */

    nhost = ((uint64_t) TLIB_OFFSET2PTR(minfo->offset_logical_node_proc)
	     - (uint64_t) TLIB_OFFSET2PTR(minfo->offset_logical_node_list))/sizeof(struct tlib_ranklist);
    if (nprocs < nhost) {
	fprintf(stderr, "%s: # of process (%d) is smaller than allocated host (%d)\n",
		__func__, nprocs, nhost);
	*ppnp = ppn = 1;
    } else {
	*ppnp = ppn = nprocs/nhost;
    }
    fprintf(stderr, "%s: nprocs(%d) ppn(%d) nhost(%d)\n", __func__, nprocs, ppn, nhost);
    rankp = (struct tlib_ranklist*) TLIB_OFFSET2PTR(*(off_t *)&minfo->offset_logical_node_list);
    {
	int	maxprocs = nhost > nprocs ? nhost: nprocs;
	sz = sizeof(struct tofu_vname)*maxprocs;
	vnam = malloc(sz);
    }
    memset(vnam, 0, sz);
    if (*fi_addr == NULL) {
	sz = sizeof(uint64_t)*nprocs;
	*fi_addr = malloc(sz);
    }
    addr = *fi_addr;

    for (ic = 0; ic < nhost; ic++) {
	uint16_t	cid = CONF_TOFU_CMPID;
	int i;

	for (i = 0; i < ppn; i++) {
	    struct tofu_coord tc = *(struct tofu_coord*) &((rankp + ic)->physical_addr);
	    struct tofu_vname *vnmp = &vnam[ic*ppn + i];
	    utofu_tni_id_t	tni;
	    utofu_cq_id_t	cq;
	    vnmp->xyzabc[0] = tc.x; vnmp->xyzabc[1] = tc.y; vnmp->xyzabc[2] = tc.z;
	    vnmp->xyzabc[3] = tc.a; vnmp->xyzabc[4] = tc.b; vnmp->xyzabc[5] = tc.c;
	    utf_tni_select(ppn, i, &tni, &cq);
	    vnmp->tniq[0] = ((tni << 4) & 0xf0) | (cq & 0x0f);
	    vnmp->vpid = ic;
	    vnmp->cid = CONF_TOFU_CMPID;
	    utf_vname_vcqid(vnmp);
	    vnmp->v = 1;
	    if (addr) addr[ic*ppn + i] = ic*ppn + i;
	    if (pmix_myrank == 0) {
		fprintf(stderr, "[%ld] YI!! ppn(%d) i(%d) tni(%d) cq(%d)\n", ic*ppn + i, ppn, i, tni, cq);
	    }
	}
    }
    return vnam;
}

/*
 * fi_addr - pointer to tofu vcqhid array
 * npp - number of processes
 * ppnp - process per node
 * rankp - my rank
 */
struct tofu_vname *
utf_get_peers(uint64_t **fi_addr, int *npp, int *ppnp, int *rnkp)
{
    static int notfirst = 0;
    char *errstr;
    int	rc;

    if (notfirst) {
	if (notfirst == -1) {
	    *fi_addr = NULL;
	    *npp = 0; *ppnp = 0; *rnkp = -1;
	    return NULL;
	}
	*fi_addr = my_fiaddr;
	*npp = pmix_nprocs;
	*ppnp = my_ppn;
	*rnkp = pmix_myrank;
	return pmix_vnam;
    } else {
	rc = dlib_init();
	if (rc < 0) {
	    notfirst = -1;
	    return NULL;
	}
	notfirst = 1;
    }
    fprintf(stderr, "%s: calling PMIX_Init\n", __func__); fflush(stderr);
    LIB_CALL(rc, myPMIx_Init(pmix_proc, NULL, 0),
	     err, errstr, "PMIx_Init");

    pmix_myrank = pmix_proc->rank;
    fprintf(stderr, "%s: pmix_myrank(%d)\n", __func__, pmix_myrank); fflush(stderr);
    pmix_proc->rank= PMIX_RANK_WILDCARD;
    LIB_CALL(rc, myPMIx_Get(pmix_proc, PMIX_JOB_SIZE, NULL, 0, &pval),
	     err, errstr, "Cannot get PMIX_JOB_SIZE");
    pmix_nprocs = pval->data.uint32;
    // printf("nprocs = %d\n", nprocs); fflush(stdout);

    LIB_CALL(rc, myPMIx_Get(pmix_proc, FJPMIX_RANKMAP, NULL, 0, &pval),
	     err, errstr, "Cannot get FJPMIX_RANKMAP");
    //printf("pval->data.bo.bytes(%p) pval->data.bo.size(%ld)\n",
    //pval->data.bo.bytes, pval->data.bo.size); fflush(stdout);

    minfo = (struct tlib_process_mapinfo*) pval->data.bo.bytes;
    DEBUG(DLEVEL_ALL) {
	if (pmix_myrank == 0) show_procmap(minfo, pmix_nprocs);
    }
    LIB_CALL(rc, myPMIx_Finalize(NULL, 0),
	     err, errstr, "PMIx_Finalize");
    pmix_vnam = utf_peers_reg(minfo, pmix_nprocs, fi_addr, ppnp);
    *npp = pmix_nprocs;
    *rnkp = pmix_myrank;
    my_fiaddr = *fi_addr;
    my_ppn = *ppnp;
    return pmix_vnam;
err:
    notfirst = -1;
    fprintf(stderr, "%s\n", errstr);
    fprintf(stdout, "%s\n", errstr);
    return NULL;
}

#ifdef LIBPROCMAP_TEST
#define PMIX_TOFU_SHMEM	"TOFU_TESTSHM"
#else
#define PMIX_TOFU_SHMEM	"TOFU_SHM"
#endif

// sz = sysconf(_SC_PAGESIZE);
void	*
utf_shm_init(size_t sz, char *mykey)
{
    int	rc;
    char	*errstr;
    int	myhrank = (pmix_myrank/my_ppn)*my_ppn;
    key_t	key;
    int		shmid;
    char	buf[128];
    void	*addr;

    if (pmix_myrank == myhrank) {
	pmix_value_t	pv;
	strcpy(buf, mykey);
	key = ftok(buf, 1);
	shmid = shmget(key, sz, IPC_CREAT | 0666);
	if (shmid < 0) { perror("error:"); exit(-1); }
	addr = shmat(shmid, NULL, 0);
	/* expose SHMEM_KEY_VAL */
	pv.type = PMIX_STRING;
	pv.data.string = buf;
	pmix_proc->rank = pmix_myrank;
	myPMIx_Put(PMIX_LOCAL, PMIX_TOFU_SHMEM, &pv);
	LIB_CALL(rc, myPMIx_Commit(), err, errstr, "PMIx_Commit");
    } else {
	pmix_value_t	*pv;
	pmix_proc->rank = myhrank; // PMIX_RANK_LOCAL_NODE does not work
	do {
	    rc = myPMIx_Get(pmix_proc, PMIX_TOFU_SHMEM, NULL, 0, &pv);
	    usleep(1000);
	} while (rc == PMIX_ERR_NOT_FOUND);
	if (rc != PMIX_SUCCESS) {
	    snprintf(buf, 128, "PMIx_Get error rc(%d)\n", rc); errstr = buf;
	    goto err;
	}
	key = ftok(pv->data.string, 1);
	shmid = shmget(key, sz, 0);
	addr = shmat(shmid, NULL, 0);
    }
    return addr;
err:
    fprintf(stderr, "%s\n", errstr);
    return NULL;
}

int
utf_shm_finalize(void *addr)
{
    return shmdt(addr);
}

/**************************************************************************/
#ifdef LIBPROCMAP_TEST
#define SHMEM_KEY_VAL	"/home/users/ea01/ea0103/MPICH-testshm"
int utf_dflag = 1;
int myrank = 1;
int mypid;

int
utf_printf(const char *fmt, ...)
{
    va_list	ap;
    int		rc;
    va_start(ap, fmt);
    fprintf(stderr, "[%d] ", myrank);
    rc = vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
    return rc;
}

int
main(int argc, char **argv)
{
    int	nprocs, ppn;
    int rc, np, rank;
    char *cp;
    char *errstr;
    struct tofu_vname *vnam;
    uint64_t	*addr = NULL;
    
    mypid = getpid();
    LIB_CALL(rc, MPI_Init(&argc, &argv), err, errstr, "MPI_Init");
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    if (myrank == 0) {
	printf("Proc Size = %d\n", nprocs); fflush(stdout);
    }
    cp = getenv("TOFU_NAMED_AV");
    if (cp && atoi(cp) != 0) {
	printf("TOFU_NAMED_AV is %d\n", atoi(cp));
//	printf("TOFU_NAMED_AV is reset and run again\n");
//	goto err0;
    }
    vnam = utf_get_peers(&addr, &np, &ppn, &rank);
    if (vnam == NULL) {
	printf("utf_get_peers error\n");
	goto err1;
    }
    if (myrank == 0) {
	int	i;
	printf("\t:nproc = %d ppn = %d\n", np, ppn);
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
    if (ppn == 1) {
	printf("No Testing Shared Memory because ppn = 1\n");
	goto end;
    }
    if (myrank == 0) {
	printf("Testing Shared Memory\n");
    }
    MPI_Barrier(MPI_COMM_WORLD);
    {
	int	*ip = utf_shm_init(sysconf(_SC_PAGESIZE), SHMEM_KEY_VAL);
	int	myhrank = (myrank/my_ppn)*my_ppn;
	int	mylrank = myrank % my_ppn;
	int	i;
	if (ip == NULL) {
	    printf("Cannot allocate shared memory\n"); goto err1;
	}
	for (i = 0; i < 10; i++) *(ip+mylrank*10+i) = mylrank+1+i;
	MPI_Barrier(MPI_COMM_WORLD);
	if (myrank == myhrank) {
	    for (i = 0; i < my_ppn*10; i++) {
		if (i%10 == 0)  printf("\n[%d] ", myrank);
		printf("%04d ", *(ip+i));
	    }
	    printf("\n");
	}
	rc = utf_shm_finalize(ip);
	if (rc < 0) perror("utf_shm_finalize:");
    }
end:
err1:
    {
	extern int fi_tofu_cntrl(int, ...);
	fi_tofu_cntrl(4, 0);
    }
    if (myrank == 0) printf("End\n");
    fflush(stdout);
    MPI_Finalize();
    return 0;
err:
    fprintf(stderr, "%s\n", errstr);
    return -1;
}
#endif /* LIBPROCMAP_TEST */
