#include "hcall.h"
#include "mpi_hc.h"

int mpi_init_hcall(int *argc, char ***argv)
{
  long long rc;
  long long zero=0;
  long long cmd=MPI_INIT;

  HCALL(rc,cmd,argc,argv,zero,zero,zero,zero,zero,zero);
  
  return rc;
}

int mpi_deinit_hcall()
{
  long long rc;
  long long zero=0;
  long long cmd=MPI_DEINIT;

  HCALL(rc,cmd,zero,zero,zero,zero,zero,zero,zero,zero);
  
  return rc;
}

int mpi_comm_rank_hcall(void *comm, int *rank)
{
  long long rc;
  long long zero=0;
  long long cmd=MPI_RANK;
  
  HCALL(rc,cmd,comm,rank,zero,zero,zero,zero,zero,zero);
  
  return rc;
}

int mpi_send_hcall(void *buf, int n, void* dtype, int dest, int tag, void *comm)
{
  long long rc;
  long long zero=0;
  long long cmd=MPI_SEND;

  HCALL(rc,cmd,buf,n,dtype,dest,tag,comm,zero,zero);

  return rc;
}

int mpi_recv_hcall(void *buf, int n, void *dtype, int src, int tag, 
		   void * comm, void *stat) 
{
  long long rc;
  long long zero=0;
  long long cmd=MPI_RECV;
  
  HCALL(rc,cmd,buf,n,dtype,src,tag,comm,stat,zero);

  return rc;
}

