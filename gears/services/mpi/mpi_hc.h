#ifndef __MPI_INJECT__
#define __MPI_INJECT__

#define MPI_INIT 1500
#define MPI_DEINIT 1501
#define MPI_RANK 1502
#define MPI_SEND 1503
#define MPI_RECV 1504

#ifndef __KERNEL__
int mpi_init_hcall(int *argc, char ***argv);
int mpi_deinit_hcall();
int mpi_comm_rank_hcall(void *comm, int *rank);
int mpi_send_hcall(void *buf, int n, void *dtype, int dest,
		   int tag, void *comm);
int mpi_recv_hcall(void *buf, int n, void *dtype, int src, 
		   int tag, void *comm, void *stat);

#endif

#endif
