#include<mpi.h>
#include<stdio.h>

int nprocs, myrank;

int MPI_Init(int *argc, char ***argv)
{
    int	rc;

    rc = PMPI_Init(argc, argv);
    return rc;
}

int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm)
{
    int rc;

    rc = PMPI_Comm_create(comm, group, newcomm);
    return rc;
}

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int rc;

    rc = PMPI_Comm_dup(comm, newcomm);
    return rc;
}

int MPI_Comm_group(MPI_Comm comm, MPI_Group *group)
{
    int rc;

    rc = PMPI_Comm_group(comm, group);
    return rc
}

int
main(int argc, char **argv)
{
    MPIR_Comm	*mpir_comm = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    return 0;
}
