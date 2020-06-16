#include <mpi.h>
#include <pmix.h>
#include <pmix_fjext.h>
#include "process_map_info.h"
#include <dlfcn.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
static int	pmix_myrank, pmix_nprocs, pmix_lrank, pmix_lldr;

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
    LIB_DLCALL(myPMIx_Put, dlsym(dlp, "FJPMIx_Put"), err1, errstr, "dlsym FJPMIx_Put");
    LIB_DLCALL(myPMIx_Commit, dlsym(dlp, "FJPMIx_Commit"), err1, errstr, "dlsym FJPMIx_Commit");
    LIB_DLCALL2(rc, dlclose(dlp), err1, errstr, "dlclose");
    return 0;
err1:
    fprintf(stderr, "error %s: %s\n", errstr, dlerror());
    return -1;
}

int
main(int argc, char **argv)
{
    int	rc;
    key_t	key;
    int		shmid;
    int		*ip;
    size_t	sz;
    char	*errstr;
    char	buf[128];
    int		nprocs, myrank;
    int		nhost;
    int		ppn, myhost;	/* process per node */
    struct tlib_process_mapinfo *minfo;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    rc = dlib_init();
    if (rc < -0) {
	exit(-1);
    }
    LIB_CALL(rc, myPMIx_Init(pmix_proc, NULL, 0), err, errstr, "PMIx_Init");
    pmix_myrank = pmix_proc->rank;
    pmix_proc->rank = PMIX_RANK_WILDCARD;
    LIB_CALL(rc, myPMIx_Get(pmix_proc, PMIX_JOB_SIZE, NULL, 0, &pval),
	     err, errstr, "Cannot get PMIX_JOB_SIZE");
    pmix_nprocs = pval->data.uint32;
#if 0
    /* Fugaku does not support PMIX_LOCAL_RANK, PMIX_LOCALLDR */
    LIB_CALL(rc, myPMIx_Get(pmix_proc, PMIX_LOCAL_RANK, NULL, 0, &pval),
	     skip1, errstr, "Cannot get PMIX_LOCAL_RANK");
    pmix_lrank = pval->data.uint32;
skip1:
    if (rc != 0) printf("[%d] %s\n", pmix_myrank, errstr);
    LIB_CALL(rc, myPMIx_Get(pmix_proc, PMIX_LOCALLDR, NULL, 0, &pval),
    	     skip2, errstr, "Cannot get PMIX_LOCALLDR");
    pmix_lldr = pval->data.uint32;
skip2:
    if (rc != 0) printf("[%d] %s\n", pmix_myrank, errstr);
    printf("[%d] pmix_nprocs(%d) lrank(%d) lldr(%d)\n",
    	   pmix_myrank, pmix_nprocs, pmix_lrank, pmix_lldr); fflush(stdout);
#endif

    LIB_CALL(rc, myPMIx_Get(pmix_proc, FJPMIX_RANKMAP, NULL, 0, &pval),
	     err, errstr, "Cannot get FJPMIX_RANKMAP");
    minfo = (struct tlib_process_mapinfo*) pval->data.bo.bytes;
    nhost = ((uint64_t) TLIB_OFFSET2PTR(minfo->offset_logical_node_proc)
	     - (uint64_t) TLIB_OFFSET2PTR(minfo->offset_logical_node_list))/sizeof(struct tlib_ranklist);
    ppn = pmix_nprocs/nhost;
    printf("[%d] pmix_nprocs(%d) nhost(%d) ppn(%d)\n", pmix_myrank, pmix_nprocs, nhost, ppn); fflush(stdout);

    sz = sysconf(_SC_PAGESIZE);
    myhost = (pmix_myrank/ppn)*ppn;
    if (pmix_myrank == myhost) {
	pmix_value_t	pv;

	printf("[%d] issuing PMIx_Put TOFU_SHM\n", pmix_myrank); fflush(stdout);
	key = ftok("/tmp/", 1);
	shmid = shmget(key, sz, IPC_CREAT | 0666);
	if (shmid < 0) {
	    perror("error:");
	    exit(-1);
	}
	pv.type = PMIX_STRING;
	strcpy(buf, "/tmp/");
	pv.data.string = buf;
	pmix_proc->rank = pmix_myrank;
	myPMIx_Put(PMIX_LOCAL, "TOFU_SHM", &pv);
	rc = myPMIx_Commit();

	ip = (int*) shmat(shmid, NULL, 0);
	printf("[%d] ip = %p\n", pmix_myrank, ip);
	printf("[%d] *ip = %d\n",  pmix_myrank, *ip);
	*ip = 1000;
	printf("[%d] *ip = %d\n", pmix_myrank, *ip);
    } else {
	pmix_value_t	*pv;
	pmix_proc->rank = myhost;
	// does not work
	//pmix_proc->rank = PMIX_RANK_LOCAL_NODE;
	printf("[%d] issuing PMIx_Get TOFU_SHM from rank(%d)\n", pmix_myrank, pmix_proc->rank); fflush(stdout);
	do {
	    rc = myPMIx_Get(pmix_proc, "TOFU_SHM", NULL, 0, &pv);
	    usleep(1000);
	} while (rc != PMIX_SUCCESS);
	printf("[%d] rc = %d\n", pmix_myrank, rc); fflush(stdout);
	if (rc != 0) {
	    printf("[%d] ERROR!!\n", pmix_myrank); fflush(stdout);
	    goto err;
	}
	printf("[%d] pv->data.string = %s\n", pmix_myrank, pv->data.string); fflush(stdout);
	key = ftok(pv->data.string, 1);
	shmid = shmget(key, sz, 0);
	printf("[%d] shmid= %d\n", pmix_myrank, shmid); fflush(stdout);
	ip = (int*) shmat(shmid, NULL, 0);
	printf("[%d] ip = %p\n", pmix_myrank, ip); fflush(stdout);
	printf("[%d] *ip = %d\n", pmix_myrank, *ip); fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    printf("[%d] finalizing\n", pmix_myrank); fflush(stdout);
    shmdt(ip);
    MPI_Finalize();
    return 0;
err:
    printf("%s\n", errstr);
    MPI_Barrier(MPI_COMM_WORLD);
    printf("[%d] finalizing\n", pmix_myrank); fflush(stdout);
    MPI_Finalize();
    return 0;
}
