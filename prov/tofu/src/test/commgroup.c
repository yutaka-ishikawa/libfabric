/*
 * TODO:
 *	MPI_Group_incl: handling MPI_GROUP_EMPTY
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utf_list.h"

#ifdef DEBUG
#define DBG if (1)
#else
#define DBG if (0)
#endif
#define MPICALL_CHECK(rc, func)				\
{							\
    rc = func;						\
    if (rc != MPI_SUCCESS) return rc;			\
}

#define COMMGROUP_ERRCHECK(ent)						\
    if (ent == NULL) {							\
	fprintf(stderr, "%d:%s internal error\n", __LINE__, __func__);	\
    }

#define NOTYET_SUPPORT fprintf(stderr, "%s is not supported for keeping ranks.\n", __func__)

// typedef int MPI_Comm;
// typedef int MPI_Group;

#define GROUPINFO_SIZE	1024
struct grpinfo_ent {
    utfslist_entry	slst;
    MPI_Group		grp;
    int			size;
    int			*ranks;
};

struct cominfo_ent {
    utfslist_entry	slst;
    MPI_Comm		comm;
    struct grpinfo_ent	*grp;	/* this grpinfo entry should not be in the grpinfo_lst
				 * In other words, this grpinfo entry must be also reclaimed
				 * when this cominfo entry is reclaimed. */
};

static struct utfslist		grpinfo_lst;
static struct utfslist		grpinfo_freelst;
static struct grpinfo_ent	*grpinfo_entarray;
static struct utfslist		cominfo_lst;
static struct utfslist		cominfo_freelst;
static struct grpinfo_ent	*cominfo_entarray;
static int			nprocs, myrank;
static int			dbg_noshow = 1;

static void
comgrpinfo_init(int entsize)
{
    int	i;

    /* group info */
    grpinfo_entarray = malloc(sizeof(struct grpinfo_ent)*entsize);
    memset(grpinfo_entarray, 0, sizeof(struct grpinfo_ent)*entsize);
    utfslist_init(&grpinfo_freelst, 0);
    for (i = 0; i < entsize; i++) {
	utfslist_append(&grpinfo_freelst, &grpinfo_entarray[i].slst);
    }
    /* comm info */
    cominfo_entarray = malloc(sizeof(struct cominfo_ent)*entsize);
    memset(cominfo_entarray, 0, sizeof(struct cominfo_ent)*entsize);
    utfslist_init(&cominfo_freelst, 0);
    for (i = 0; i < entsize; i++) {
	utfslist_append(&cominfo_freelst, &cominfo_entarray[i].slst);
    }
}

static struct grpinfo_ent *
grpinfo_reg(MPI_Group grp, int size, int *ranks)
{
    utfslist_entry *slst;
    struct grpinfo_ent	*ent;

    slst = utfslist_remove(&grpinfo_freelst);
    if (slst == NULL) {
	fprintf(stderr, "Too many MPI_Group created\n");
	abort();
    }
    ent = container_of(slst, struct grpinfo_ent, slst);
    ent->grp = grp;
    ent->size = size;
    ent->ranks = ranks;
    utfslist_append(&grpinfo_lst, &ent->slst);
    return ent;
}

static int
grpinfo_unreg(MPI_Group grp)
{
    utfslist_entry	*cur, *prev;
    struct grpinfo_ent *ent;
    utfslist_foreach2(&grpinfo_lst, cur, prev) {
	ent = container_of(cur, struct grpinfo_ent, slst);
	if (ent->grp == grp) goto find;
    }
    /* not found */
    return -1;
find:
    free(ent->ranks);
    ent->grp = 0;
    utfslist_remove2(&grpinfo_lst, cur, prev);
    utfslist_insert(&grpinfo_freelst, cur);
    return 0;
}

static struct grpinfo_ent *
grpinfo_get(MPI_Group grp)
{
    struct utfslist_entry	*cur;
    utfslist_foreach(&grpinfo_lst, cur) {
	struct grpinfo_ent	*ent = container_of(cur, struct grpinfo_ent, slst);
	if (ent->grp == grp) {
	    return ent;
	}
    }
    return NULL;
}

/*
 * copy except group ID
 */
static struct grpinfo_ent *
grpinfo_dup(struct grpinfo_ent *srcent, MPI_Group newgroup)
{
    utfslist_entry *slst;
    struct grpinfo_ent	*dupent;
    int	*grpidx;
    int	size = srcent->size;

    slst = utfslist_remove(&grpinfo_freelst);
    dupent = container_of(slst, struct grpinfo_ent, slst);
    grpidx = malloc(sizeof(int)*size);
    memcpy(grpidx, srcent->ranks, sizeof(int)*size);
    dupent->grp = newgroup;
    dupent->size = size;
    dupent->ranks = grpidx;
    return dupent;
}

static struct cominfo_ent *
cominfo_reg(MPI_Comm comm, struct grpinfo_ent *grpent)
{
    utfslist_entry *slst;
    struct cominfo_ent	*coment;

    slst = utfslist_remove(&cominfo_freelst);
    if (slst == NULL) {
	fprintf(stderr, "Too many MPI_Comm created\n");
	abort();
    }
    coment = container_of(slst, struct cominfo_ent, slst);
    coment->comm = comm;
    coment->grp = grpent;
    utfslist_append(&cominfo_lst, &coment->slst);
    return coment;
}

static int
cominfo_unreg(MPI_Comm comm)
{
    utfslist_entry	*cur, *prev;
    struct cominfo_ent *ent;
    utfslist_foreach2(&cominfo_lst, cur, prev) {
	ent = container_of(cur, struct cominfo_ent, slst);
	if (ent->comm == comm) goto find;
    }
    /* not found */
    return -1;
find:
    free(ent->grp);
    ent->grp = 0;
    utfslist_remove2(&grpinfo_lst, cur, prev);
    utfslist_insert(&cominfo_freelst, cur);
    return 0;
}

static struct cominfo_ent *
cominfo_get(MPI_Comm comm)
{
    struct utfslist_entry	*cur;
    utfslist_foreach(&cominfo_lst, cur) {
	struct cominfo_ent	*ent = container_of(cur, struct cominfo_ent, slst);
	if (ent->comm == comm) {
	    return ent;
	}
    }
    return NULL;
}

static void
comm_show(FILE *fp, const char *fname, MPI_Comm comm)
{
    struct cominfo_ent	*ent = cominfo_get(comm);

    if (dbg_noshow) return;
    if (ent == NULL) {
	fprintf(stderr, "%s: Communicator(%x) is not found\n", fname, comm);
    } else {
	int	i = 0;
	int	*ranks = ent->grp->ranks;
	int	size = ent->grp->size;
	fprintf(stderr, "%s: Communicator(%x)\n", fname, comm);
	while (i < size) {
	    int j;
	    for (j = 0; j < 20 && i < size; j++, i++) {
		fprintf(stderr, "%05d ", *ranks++);
	    }
	}
	fprintf(stderr, "\n");
    }
}

static void
cominfo_dup(MPI_Comm comm, MPI_Comm newcomm)
{
    struct cominfo_ent	*coment = cominfo_get(comm);
    struct grpinfo_ent	*grpent;
    if (coment == NULL) {
	fprintf(stderr, "%s: Communicator(%x) is not found\n", __func__, comm);
	goto ext;
    }
    grpent = grpinfo_dup(coment->grp, 0);
    cominfo_reg(newcomm, grpent);
    DBG {
	comm_show(stderr, __func__, comm);
	comm_show(stderr, __func__, newcomm);
    }
ext:
    return;
}


static int
mycmp(const void *arg1, const void *arg2)
{
    struct inf2 { int key; int rank; };
    int rc;

    rc = ((struct inf2*)arg1)->key < ((struct inf2*)arg2)->key;
    return rc;
}

int
MPI_Init(int *argc, char ***argv)
{
    int	rc;

    MPICALL_CHECK(rc, PMPI_Init(argc, argv));
    {
	int	i, *grpidx;
	utfslist_entry *slst;
	struct grpinfo_ent	*grpent;

	PMPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	PMPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	comgrpinfo_init(GROUPINFO_SIZE);
	grpidx = malloc(sizeof(int)*nprocs);
	for (i = 0; i < nprocs; i++) {
	    grpidx[i] = i;
	}
	slst = utfslist_remove(&grpinfo_freelst);
	grpent = container_of(slst, struct grpinfo_ent, slst);
	grpent->grp = 0;
	grpent->size = nprocs;
	grpent->ranks = grpidx;
	cominfo_reg(MPI_COMM_WORLD, grpent);
    }
    return rc;
}

int
MPI_Comm_group(MPI_Comm comm, MPI_Group *group)
{
    int rc;

    MPICALL_CHECK(rc, PMPI_Comm_group(comm, group));
    {
	struct cominfo_ent	*coment = cominfo_get(comm);
	struct grpinfo_ent	*grpent;
	COMMGROUP_ERRCHECK(coment);
	grpent = grpinfo_dup(coment->grp, *group);
	utfslist_append(&grpinfo_lst, &grpent->slst);
    }
    return rc;
}

/*
 * union: All elements of the rst group (group1), followed by all elements
 *	  of second group (group2) not in the rst group.
 */
int
MPI_Group_union(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    int rc;
    struct grpinfo_ent	*grpent1 = grpinfo_get(group1);
    struct grpinfo_ent	*grpent2 = grpinfo_get(group2);

    MPICALL_CHECK(rc, PMPI_Group_union(group1, group2, newgroup));
    COMMGROUP_ERRCHECK(grpent1);
    COMMGROUP_ERRCHECK(grpent2);
    {
	int	maxsize = grpent1->size + grpent2->size;
	int	newsize = grpent1->size;
	int	*newranks = malloc(sizeof(int)*maxsize);
	int	i, j, *np;
	memcpy(newranks, grpent1->ranks, sizeof(int)*grpent1->size);
	np = newranks + grpent1->size;
	for (i = 0; i < grpent2->size; i++) {
	    for (j = 0; j < grpent1->size; j++) {
		if (grpent1->ranks[j] == grpent2->ranks[i]) goto cont;
	    }
	    *np++ = grpent2->ranks[i]; newsize++;
	cont: ;
	}
	grpinfo_reg(*newgroup, newsize, newranks);
    }
    return rc;
}

/*
 * intersection: all elements of the first group that are also in the second group,
 *		 ordered as in the first group
 */
int
MPI_Group_intersection(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    int rc;
    struct grpinfo_ent	*grpent1 = grpinfo_get(group1);
    struct grpinfo_ent	*grpent2 = grpinfo_get(group2);

    MPICALL_CHECK(rc, PMPI_Group_intersection(group1, group2, newgroup));
    COMMGROUP_ERRCHECK(grpent1);
    COMMGROUP_ERRCHECK(grpent2);
    {
	int	maxsize = grpent1->size + grpent2->size;
	int	newsize = 0;
	int	*newranks = malloc(sizeof(int)*maxsize);
	int	i, j, *np;
	np = newranks;
	for (i = 0; i < grpent1->size; i++) {
	    for (j = 0; j < grpent2->size; j++) {
		if (grpent1->ranks[i] == grpent2->ranks[j]) {
		    *np++ = grpent1->ranks[i]; newsize++;
		    break;
		}
	    }
	}
	grpinfo_reg(*newgroup, newsize, newranks);
    }
    return rc;
}

/*
 * difference: all elements of the first group that are not in the second group,
 *	       ordered as in the first group.
 */
int
MPI_Group_difference(MPI_Group group1, MPI_Group group2, MPI_Group *newgroup)
{
    int rc;
    struct grpinfo_ent	*grpent1 = grpinfo_get(group1);
    struct grpinfo_ent	*grpent2 = grpinfo_get(group2);

    MPICALL_CHECK(rc, PMPI_Group_difference(group1, group2, newgroup));
    COMMGROUP_ERRCHECK(grpent1);
    COMMGROUP_ERRCHECK(grpent2);
    {
	int	maxsize = grpent1->size + grpent2->size;
	int	newsize = 0;
	int	*newranks = malloc(sizeof(int)*maxsize);
	int	i, j, *np;
	np = newranks;
	for (i = 0; i < grpent1->size; i++) {
	    for (j = 0; j < grpent2->size; j++) {
		if (grpent1->ranks[i] == grpent2->ranks[j]) {
		    goto cont;
		}
	    }
	    *np++ = grpent1->ranks[i]; newsize++;
	cont: ;
	}
	grpinfo_reg(*newgroup, newsize, newranks);
    }
    return rc;
}

/*
 * This function creates a group newgroup that consists of the
 * n processes in group with ranks ranks[0],,, ranks[n-1]; the process with rank i in newgroup
 * is the process with rank ranks[i] in group. Each of the n elements of ranks must be a valid
 * rank in group and all elements must be distinct, or else the program is erroneous. If n = 0,
 * then newgroup is MPI_GROUP_EMPTY. This function can, for instance, be used to reorder
 * the elements of a group.
 */
int
MPI_Group_incl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup)
{
    int rc;

    MPICALL_CHECK(rc, PMPI_Group_incl(group, n, ranks, newgroup));
    {
	struct grpinfo_ent	*grpent = grpinfo_get(group);
	int	*newranks = malloc(sizeof(int)*n);
	int newsize = 0;
	int	*np;
	int	i;
	for (i = 0, np = newranks; i < n; i++) {
	    assert(ranks[i] <= grpent->size);
	    *np++ = grpent->ranks[ranks[i]];
	    newsize++;
	}
	grpinfo_reg(*newgroup, newsize, newranks);
    }
    return rc;
}

/*
 * This function creates a group of processes newgroup that is obtained
 * by deleting from group those processes with ranks ranks[0], .., ranks[n-1]. The ordering of
 * processes in newgroup is identical to the ordering in group. Each of the n elements of ranks
 * must be a valid rank in group and all elements must be distinct; otherwise, the program is
 * erroneous. If n = 0, then newgroup is identical to group a valid rank in group
 */
int
MPI_Group_excl(MPI_Group group, int n, const int ranks[], MPI_Group *newgroup)
{
    int rc;

    MPICALL_CHECK(rc, PMPI_Group_excl(group, n, ranks, newgroup));
    {
	struct grpinfo_ent	*ent = grpinfo_get(group);
	int	*newranks;
	newranks = malloc(sizeof(int)*ent->size);
	memcpy(newranks, ent->ranks, sizeof(int)*ent->size);
	if (n == 0) {
	    grpinfo_reg(*newgroup, ent->size, newranks);
	    // utfslist_append(&grpinfo_lst, &newent->slst);
	} else {
	    int	*cp, i;
	    int	newsize = 0;
	    for (i = 0; i < n; i++) {
		assert(ranks[i] <= ent->size);
		newranks[ranks[i]] = -1;
	    }
	    for (i = 0, cp = newranks; i < ent->size; i++) {
		if (newranks[i] != -1) {
		    *cp++ = newranks[i]; newsize++;
		}
	    }
	    grpinfo_reg(*newgroup, newsize, newranks);
	}
    }
    return rc;
}

int
MPI_Group_range_incl(MPI_Group group, int n, int ranges[][3], MPI_Group *newgroup)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Group_range_incl(group, n, ranges, newgroup));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Group_range_excl(MPI_Group group, int n, int ranges[][3], MPI_Group *newgroup)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Group_range_excl(group, n, ranges, newgroup));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Group_free(MPI_Group *group)
{
    int rc;
    rc = grpinfo_unreg(*group);
    assert(rc == 0);
    MPICALL_CHECK(rc, PMPI_Group_free(group));
    return rc;
}

int
MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm)
{
    int rc;

    MPICALL_CHECK(rc, PMPI_Comm_create(comm, group, newcomm));
    {
	struct cominfo_ent	*coment;
	struct grpinfo_ent	*grpent, *newgrpent;

	if (*newcomm == MPI_COMM_NULL) {
	    /* nothing */ goto ext;
	}
	coment = cominfo_get(comm);
	grpent = grpinfo_get(group);
	assert(coment != NULL);	assert(grpent != NULL);
	newgrpent = grpinfo_dup(coment->grp, grpent->grp);
	cominfo_reg(*newcomm, newgrpent);
    }
ext:
    return rc;
}

int
MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int rc;

    MPICALL_CHECK(rc, PMPI_Comm_dup(comm, newcomm));
    cominfo_dup(comm, *newcomm);
    return rc;
}

int
MPI_Comm_dup_with_info(MPI_Comm comm, MPI_Info info, MPI_Comm *newcomm)
{
    int rc;
    rc = PMPI_Comm_dup_with_info(comm, info, newcomm);
    cominfo_dup(comm, *newcomm);
    return rc;
}

int
MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Comm_split(comm, color, key, newcomm));
    {
	struct inf1 {
	    int color; int key; int rank;
	} *inf1;
	struct inf2 {
	    int key; int rank;
	} *inf2, *ifp;
	int	i, myrank, sz, newsz, *grpidx;
	utfslist_entry *slst;
	struct grpinfo_ent	*grpent;

	PMPI_Comm_size(comm, &sz);
	PMPI_Comm_rank(comm, &myrank);
	inf1 = malloc(sizeof(struct inf1)*sz);
	inf2 = malloc(sizeof(struct inf2)*sz);
	assert(inf1 != NULL); assert(inf2 != NULL);
	inf1[myrank].color = color;
	inf1[myrank].key = key;
	for (i = 0; i < sz; i++) {
	    inf1[i].rank = sz;
	}
	MPI_Allgather(&inf1[myrank], 3, MPI_INT, inf1, 3, MPI_INT, comm);
	/* selection */
	ifp = inf2; newsz = 0;
	for (i = 0; i < sz; i++) {
	    if (inf1[i].color == color) {
		ifp->key = inf1[i].key;
		ifp->rank = inf1[i].rank;
		ifp++; newsz++;
	    }
	}
	/* sort */
	qsort(inf2, newsz, sizeof(struct inf2), mycmp);
	/* */
	grpidx = malloc(sizeof(int)*newsz);
	for (i = 0; i < nprocs; i++) {
	    grpidx[i] = inf2[i].rank;
	}
	slst = utfslist_remove(&grpinfo_freelst);
	grpent = container_of(slst, struct grpinfo_ent, slst);
	grpent->grp = 0;
	grpent->size = nprocs;
	grpent->ranks = grpidx;
	cominfo_reg(*newcomm, grpent);
	free(inf1); free(inf2);
    }
    return rc;
}

int
MPI_Comm_free(MPI_Comm *comm)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Comm_free(comm));
    cominfo_unreg(*comm);
    return rc;
}

int
MPI_Cart_create(MPI_Comm comm_old, int ndims, const int dims[], const int periods[],
		int reorder, MPI_Comm *comm_cart)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Cart_create(comm_old, ndims, dims, periods, reorder, comm_cart));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Graph_create(MPI_Comm comm_old, int nnodes, const int indx[], const int edges[],
		 int reorder, MPI_Comm *comm_graph)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Graph_create(comm_old, nnodes, indx, edges, reorder, comm_graph));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Cart_sub(MPI_Comm comm, const int remain_dims[], MPI_Comm *newcomm)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Cart_sub(comm, remain_dims, newcomm));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Dist_graph_create_adjacent(MPI_Comm comm_old, int indegree, const int sources[],
			       const int sourceweights[], int outdegree,
			       const int destinations[], const int destweights[],
			       MPI_Info info, int reorder, MPI_Comm *comm_dist_graph)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Dist_graph_create_adjacent(comm_old, indegree, sources,
			sourceweights, outdegree, destinations, destweights,
			info, reorder, comm_dist_graph));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Dist_graph_create(MPI_Comm comm_old, int n, const int sources[], const int degrees[],
		      const int destinations[], const int weights[], MPI_Info info,
		      int reorder, MPI_Comm *comm_dist_graph)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Dist_graph_create(comm_old, n, sources, degrees,
			       destinations, weights, info,
					     reorder, comm_dist_graph));
    NOTYET_SUPPORT;
    return rc;
}

int
MPI_Comm_idup(MPI_Comm comm, MPI_Comm *newcomm, MPI_Request *request)
{
    int rc;
    MPICALL_CHECK(rc, PMPI_Comm_idup(comm, newcomm, request));
    NOTYET_SUPPORT;
    return rc;
}

#ifdef COMMGROUP_TEST
int
main(int argc, char **argv)
{
    MPI_Comm	newcomm;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    if (myrank == 0) {
	printf("nprocs(%d)\n", nprocs);
	dbg_noshow = 0;
    }
    MPI_Comm_dup(MPI_COMM_WORLD, &newcomm);
    if (myrank == 0) {
	printf("newcom=%x\n", newcomm);
    }
    MPI_Finalize();
    return 0;
}
#endif
