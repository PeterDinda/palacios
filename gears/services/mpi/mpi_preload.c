#include <mpi/mpi.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include "mpi_hc.h"

static int (*mpi_init)(int *argc, char ***argv) = NULL;
static int (*mpi_deinit)() = NULL;
static int (*mpi_comm_rank)(MPI_Comm, int *) = NULL;
static int (*mpi_send)(void *, int, MPI_Datatype, int, int, MPI_Comm) = NULL;
static int (*mpi_recv)(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *) = NULL;

static int hcall_enabled=0;

int connect_handler(void)
{
  void * handle;
  char * err;

  handle = dlopen("/usr/local/lib/libmpich.so", RTLD_LAZY);
  if (!handle){
    fputs(dlerror(), stderr);
    return -1;
  } 
  mpi_init = dlsym(handle, "MPI_Init");
  if ((err = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", err);
    return -1;
  }
  mpi_deinit = dlsym(handle, "MPI_Finalize");
  if ((err = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", err);
    return -1;
  }
  mpi_comm_rank = dlsym(handle, "MPI_Comm_rank");
  if ((err = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", err);
    return -1;
  }
  mpi_recv = dlsym(handle, "MPI_Recv");
  if ((err = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", err);
    return -1;
  }
  mpi_send = dlsym(handle, "MPI_Send");
  if ((err = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", err);
    return -1;
  }
  
  return 0;
}


int MPI_Init(int *argc, char ***argv)
{
  int rc;
  volatile char temp;

  if (mpi_init == NULL){
    connect_handler();
  }
  
  // Make sure that ***argv is in memory
  temp = ***argv;

  rc = mpi_init(argc,argv);

  if (rc<0) { 
    return rc;
  }

  fprintf(stderr,"Invoking mpi_init_hcall(%p,%p)\n",argc,argv);
  
  if (mpi_init_hcall(argc,argv)<0) {
    // not connected
    hcall_enabled=0;
    fprintf(stderr,"No connection to V3VEE MPI accelerator\n");
  } else {
    // connected
    hcall_enabled=1;
    fprintf(stderr,"Connected to V3VEE MPI accelerator\n");
  }
  
  return rc;
}

int MPI_Finalize()
{
  if (mpi_deinit == NULL){
    connect_handler();
  }
  
  if (hcall_enabled) { 
    if (mpi_deinit_hcall()<0) {
      fprintf(stderr,"Could not disconnect from V3VEE MPI accelerator\n");
    }
    hcall_enabled=0;
  }
  
  return mpi_deinit();
  
}


int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
  int rc;
  volatile int temp;

  if (mpi_comm_rank == NULL){
    connect_handler();
  }


  rc=mpi_comm_rank(comm,rank);
  
  if (rc<0) {
    return rc;
  }

  // Make sure *rank is in memory
  temp=*rank;

  if (hcall_enabled) { 
    if (mpi_comm_rank_hcall(comm,rank)<0) {
      fprintf(stderr,"Could not invoke mpi_comm_rank on V3VEE MPI accelerator\n");
    }
  }

  return rc;

}

int MPI_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
  if (mpi_send == NULL){
    connect_handler();
  }

  if (hcall_enabled) {
    int i;
    volatile char temp;
    int rc;

    // Force into memory
    for (i=0;i<count;i+=4096) { 
      temp=((char*)buf)[i];
    }

    if ((rc=mpi_send_hcall(buf,count,datatype,dest,tag,comm))<0) { 
      fprintf(stderr, "Could not send using V3VEE MPI accelerator - Trying Slow Path\n");
      return mpi_send(buf, count, datatype, dest, tag, comm);
    } else {
      return rc;
    }
  } else {
    return mpi_send(buf, count, datatype, dest, tag, comm);
  }
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status)
{
  if (mpi_recv == NULL){
    connect_handler();
  }

  if (hcall_enabled) {
    int rc;
    int i;
    volatile char temp=93;

    // Force into memory
    for (i=0;i<count;i+=4096) { 
      ((char*)buf)[i]=temp;
    }
    if ((rc=mpi_recv_hcall(buf,count,datatype,source,tag,comm,status))<0) { 
      fprintf(stderr, "Could not receive using V3VEE MPI accelerator - Trying Slow Path\n");
      return mpi_recv(buf, count, datatype, source, tag, comm, status);
    } else {
      return rc;
    }
  } else {
    return mpi_recv(buf, count, datatype, source, tag, comm, status);
  }
}

