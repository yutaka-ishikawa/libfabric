#include<mpi.h>
#include<stdio.h>
#include<mpidimpl.h>
#include<mpir_comm.h>
#include<mpir_objects.h>

int nprocs, myrank;

int
main(int argc, char **argv)
{
    MPIR_Comm	*mpir_comm = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    if (myrank == 0) {
	//mpir_comm = MPIR_Process.comm_world;
	//MPIR_Comm_get_ptr(MPI_COMM_WORLD, mpir_comm);
	//printf("mpir_comm(%p)\n", mpir_comm);
	printf("mpir_comm->local_comm = %p\n", &mpir_comm->local_comm);
    }
    return 0;
}
