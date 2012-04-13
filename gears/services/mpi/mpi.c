/* 
   MPI module
  
   (c) 2012 Peter Dinda

 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <palacios/vmm.h>
#include <palacios/vm_guest.h>
#include <interfaces/vmm_host_hypercall.h>

#include "mpi_hc.h"

#define DEEP_DEBUG    0
#define SHALLOW_DEBUG 0

#if DEEP_DEBUG
#define DEEP_DEBUG_PRINT(fmt, args...) printk((fmt), ##args)
#else
#define DEEP_DEBUG_PRINT(fmt, args...) 
#endif

#if SHALLOW_DEBUG
#define SHALLOW_DEBUG_PRINT(fmt, args...) printk((fmt), ##args)
#else
#define SHALLOW_DEBUG_PRINT(fmt, args...) 
#endif


#define ERROR(fmt, args...) printk((fmt), ##args)
#define INFO(fmt, args...) printk((fmt), ##args)

#define RENDEZVOUS_TABLE_MAX 32
#define EXEC_NAME_MAX 128

struct rendezvous_table_row {
  enum {
    FREE=0,
    INITED,
    RANKED,
  }  state;
  
  char exec[EXEC_NAME_MAX];
  uint64_t rank;
  struct guest_info *core;
  struct guest_accessors *acc;
  uint64_t cr3;
  wait_queue_head_t send_wait_queue;
  int send_pending;
  uint64_t send_vaddr;
  uint64_t send_size;
  uint64_t send_dest;
  uint64_t send_tag;
  uint64_t send_rc;
  wait_queue_head_t recv_wait_queue;
  int recv_pending;
  uint64_t recv_vaddr;
  uint64_t recv_size;
  uint64_t recv_src;
  uint64_t recv_tag;
  uint64_t recv_stat_vaddr;
  uint64_t recv_rc;
};

static struct rendezvous_table_row *rtab;


static int mpi_init_hcall(struct guest_info *core,
			  struct guest_accessors *acc,
			  int *argc, 
			  char ***argv)
{
  int i;
  struct rendezvous_table_row *r;
  uint32_t va;

  SHALLOW_DEBUG_PRINT("mpi: mpi_init_hcall(%p,%p)\n",(void*)argc,(void*)argv);
  
  if (!rtab) { 
    ERROR("mpi: no rtab!\n");
    return -1;
  } 

  for (i=0;i<RENDEZVOUS_TABLE_MAX;i++) { 
    if (rtab[i].state==FREE) {
      break;
    }
  }
  
  if (i==RENDEZVOUS_TABLE_MAX) { 
    ERROR("mpi: no room in rtab\n");
    return -1;
  }

  r=&(rtab[i]);
  r->rank=0;
  r->core=core;
  r->acc=acc;
  r->cr3=acc->get_cr3(core);
  r->send_pending=0;
  r->recv_pending=0;

  // The following hideously assumes that FIX FIX FIX
  // the guest app is 32 bit!  FIX FIX FIX
  // THIS IS COMMON ASSUMPTION THROUGHOUT FIX FIX FIX
  if (acc->read_gva(core,(uint64_t)argv,4,&va)<0) { 
    ERROR("mpi: init cannot copy argv (first deref)\n");
    return -1;
  } else {
    //now we have *argv
    // we want **argv
    if (acc->read_gva(core,(uint64_t)va,4,&va)<0) { 
      ERROR("mpi: init cannot copy argv (second deref)\n");
      return -1;
    } else {
      // now we have **argv, and we want the array it points to
      if (acc->read_gva(core,(uint64_t)va,EXEC_NAME_MAX,r->exec)<0) { 
	ERROR("mpi: init cannot copy exec name (third deref)\n");
	return -1;
      }
      // for good measure
      r->exec[EXEC_NAME_MAX-1]=0;
    }
  }
  
  init_waitqueue_head(&(r->send_wait_queue));
  init_waitqueue_head(&(r->recv_wait_queue));

  r->state=INITED;
  
  DEEP_DEBUG_PRINT("mpi: inited entry %d to '%s' core=%p cr3=%p\n",
		   i,r->exec,r->core,(void*)(r->cr3));

  return 0;
}

static int mpi_deinit_hcall(struct guest_info *core,
			    struct guest_accessors *acc)
{
  int i;
  uint64_t cr3;

  SHALLOW_DEBUG_PRINT("mpi: mpi_deinit_hcall()\n");

  cr3=acc->get_cr3(core);

  for (i=0;i<RENDEZVOUS_TABLE_MAX;i++) { 
    if (rtab[i].state!=FREE && 
	rtab[i].core==core &&
	rtab[i].cr3==cr3) {
      break;
    }
  }
  
  if (i==RENDEZVOUS_TABLE_MAX) { 
    ERROR("mpi: could not find matching row in rtab to delete\n");
    return -1;
  }

  if (rtab[i].send_pending) { 
    ERROR("mpi: warning: deleting matching row with send pending\n");
  }

  if (rtab[i].recv_pending) { 
    ERROR("mpi: warning: deleting matching row with recv pending\n");
  }

  DEEP_DEBUG_PRINT("mpi: removing row for core %p, cr3 %p, exec '%s'\n",
		   core, (void*)cr3, rtab[i].exec);

  
  memset(&(rtab[i]),0,sizeof(struct rendezvous_table_row));
  
  return 0;
}

static int mpi_comm_rank_hcall(struct guest_info *core,
			       struct guest_accessors *acc,
			       void *comm_va, 
			       int *rank_va)
{
  int i;
  uint64_t cr3;

  SHALLOW_DEBUG_PRINT("mpi_comm_rank_hcall(%p,%p)\n",(void*)comm_va,(void*)rank_va);

  cr3=acc->get_cr3(core);

  for (i=0;i<RENDEZVOUS_TABLE_MAX;i++) { 
    if (rtab[i].state==INITED && 
	rtab[i].core==core &&
	rtab[i].cr3==cr3) {
      break;
    }
  }
  
  if (i==RENDEZVOUS_TABLE_MAX) { 
    ERROR("mpi: no matching row found\n");
    return -1;
  }
  
  //
  // The following completely ignores the communicator
  // Throughout we assume everyone is in MPI_COMM_WORLD
  // FIX FIX FIX FIX
  //

  if (acc->read_gva(core,(uint64_t)rank_va,4,&(rtab[i].rank))<0) { 
    ERROR("mpi: rank cannot copy rank\n");
    return -1;
  } 

  rtab[i].state=RANKED;
  
  SHALLOW_DEBUG_PRINT("mpi: ranking rcore %p, cr3 %p, exec '%s' as %llu\n",
		      core, (void*)cr3, rtab[i].exec, rtab[i].rank);

  return 0;
}

#define PAGE_ADDR(x) ((x)&~((uint64_t)0xfff))
#define PAGE_NEXT_ADDR(x) (PAGE_ADDR(x)+0x1000)



static uint64_t fast_inter_vm_copy(struct guest_info      *dest_core,
				   struct guest_accessors *dest_acc,
				   uint64_t                dest_va,
				   struct guest_info      *src_core,
				   struct guest_accessors *src_acc,
				   uint64_t                src_va,
				   uint64_t                count)
{

  uint64_t left, chunk;
  uint64_t src_page_left, dest_page_left;
  uint64_t src_host_va, dest_host_va;
  
  left = count;

  while (left) { 
    src_page_left = PAGE_NEXT_ADDR(src_va) - src_va;
    dest_page_left = PAGE_NEXT_ADDR(dest_va) - dest_va;
    
    chunk = src_page_left < dest_page_left ? src_page_left : dest_page_left;
    chunk = chunk < left ? chunk : left;

    DEEP_DEBUG_PRINT("mpi: copy chunk=%d, src_va=%p, dest_va=%p\n", 
		     chunk, src_va, dest_va);

    if (src_acc->gva_to_hva(src_core,src_va,&src_host_va)<0) { 
      ERROR("mpi: cannot translate src address %p in VM core %p\n",src_va,src_core);
      return count-left;
    }
    if (dest_acc->gva_to_hva(dest_core,dest_va,&dest_host_va)<0) { 
      ERROR("mpi: cannot translate dest address %p in VM core %p\n",dest_va,dest_core);
      return count-left;
    }

    DEEP_DEBUG_PRINT("mpi: copy chunk=%d, src_host_va=%p, dest_host_va=%p\n",
		     chunk, src_host_va, dest_host_va);

    memcpy((void*)dest_host_va,(void*)src_host_va,chunk);
 
    src_va += chunk;
    dest_va += chunk;
    left -= chunk;
  }

  return count;

}
				 


static int mpi_send_hcall(struct guest_info *core,
			  struct guest_accessors *acc,
			  void *buf, 
			  int n, 
			  int dtype, 
			  int dest, 
			  int tag, 
			  int comm)
{
  uint64_t cr3;
  int i;
  struct rendezvous_table_row *sender, *receiver;

  SHALLOW_DEBUG_PRINT("mpi: mpi_send_hcall(%p,%p,%p,%p,%p,%p)\n",(void*)buf,(void*)n,(void*)dtype,(void*)dest,(void*)tag,(void*)comm);

  cr3=acc->get_cr3(core);

  // First find me
  for (i=0;i<RENDEZVOUS_TABLE_MAX;i++) { 
    if (rtab[i].state==RANKED && 
	rtab[i].core==core &&
	rtab[i].cr3==cr3) {
      break;
    }
  }

  if (i==RENDEZVOUS_TABLE_MAX) { 
    ERROR("mpi: existential panic in send\n");
    return -1;
  }

  sender=&(rtab[i]);

  // Next try to find a matching receive

  for (i=0;i<RENDEZVOUS_TABLE_MAX;i++) { 
    if (&(rtab[i])!=sender &&
	rtab[i].state==RANKED && 
	strncmp(rtab[i].exec,sender->exec,EXEC_NAME_MAX)==0) {
      break;
    }
  }

  if (i==RENDEZVOUS_TABLE_MAX) { 
    DEEP_DEBUG_PRINT("mpi: receiver does not exist yet - pending ourselves\n");
    goto pending;
  } else {
    receiver=&(rtab[i]);
    if (!(receiver->recv_pending)) { 
      DEEP_DEBUG_PRINT("mpi: receiver has no pending receive - pending ourselves\n");
      goto pending;
    } 
    // totally ignores communicator!!!  FIX FIX FIX
    // simplistic fully qualified matching FIX FIX FIX
    if (receiver->recv_tag==tag &&
	receiver->recv_src==sender->rank) { 
      // fast path
      // totally ignores types and assumes byte xfer FIX FIX FIX
      uint64_t size = n < receiver->recv_size ? n : receiver->recv_size;

      SHALLOW_DEBUG_PRINT("mpi: mpi_send: copying %llu bytes\n", size);
      
      if (fast_inter_vm_copy(receiver->core,
			     receiver->acc,
			     receiver->recv_vaddr,
			     core,
			     acc,
			     buf,
			     size) != size) { 
	ERROR("mpi: fast_inter_vm_copy failed in mpi_send: destvm=%p, destacc=%p, dest_va=%p, srcvm=%p, srcacc=%p, src_va=%p, size=%llu\n",receiver->core,receiver->acc,receiver->recv_vaddr,core,acc,buf,size);
	return -1;
      }
			     

      SHALLOW_DEBUG_PRINT("mpi: mpi_send: finished copying\n");
      

      // Now we release the receiver
      receiver->recv_rc = 0;
      receiver->recv_pending = 0;
  
      wake_up_interruptible(&(receiver->recv_wait_queue));

      // And we are also done

      return 0;

    } else {
      DEEP_DEBUG_PRINT("mpi: receiver's pending receive does not match - pending ourselves\n");
      goto pending;
    }
  }
      


 pending:
  
  // we store our state
  sender->send_vaddr=buf;
  sender->send_size=n;
  sender->send_dest=dest;
  sender->send_tag=tag;
  sender->send_rc=-1;

  // And now we wait for the receive to do the job
  sender->send_pending=1;
  while (wait_event_interruptible(sender->send_wait_queue,
				  !(sender->send_pending)) !=0) {
    // wait wait wait
  }
  
  // released

  return sender->send_rc;
}

static int mpi_recv_hcall(struct guest_info *core,
			  struct guest_accessors *acc,
			  void *buf, 
			  int n, 
			  int dtype, 
			  int src, 
			  int tag, 
			  int comm, 
			  void *stat) 
{
  uint64_t cr3;
  int i;
  struct rendezvous_table_row *sender, *receiver;

  SHALLOW_DEBUG_PRINT("mpi_recv_hcall(%p,%p,%p,%p,%p,%p,%p)\n",(void*)buf,(void*)n,(void*)dtype,(void*)src,(void*)tag,(void*)comm,(void*)stat);

  cr3=acc->get_cr3(core);

  // First find me
  for (i=0;i<RENDEZVOUS_TABLE_MAX;i++) { 
    if (rtab[i].state==RANKED && 
	rtab[i].core==core &&
	rtab[i].cr3==cr3) {
      break;
    }
  }

  if (i==RENDEZVOUS_TABLE_MAX) { 
    ERROR("mpi: existential panic in receive\n");
    return -1;
  }

  receiver=&(rtab[i]);

  // Next try to find a matching send

  for (i=0;i<RENDEZVOUS_TABLE_MAX;i++) { 
    if (&(rtab[i])!=receiver &&
	rtab[i].state==RANKED && 
	strncmp(rtab[i].exec,receiver->exec,EXEC_NAME_MAX)==0) {
      break;
    }
  }

  if (i==RENDEZVOUS_TABLE_MAX) { 
    DEEP_DEBUG_PRINT("mpi: sender does not exist yet - pending ourselves\n");
    goto pending;
  } else {
    sender=&(rtab[i]);
    if (!(sender->send_pending)) { 
      DEEP_DEBUG_PRINT("mpi: sender has no pending receive - pending ourselves\n");
      goto pending;
    } 
    // totally ignores communicator!!!  FIX FIX FIX
    // simplistic fully qualified matching FIX FIX FIX
    if (sender->send_tag==tag &&
	sender->send_dest==receiver->rank) { 

      uint64_t size = n < sender->send_size ? n : sender->send_size;
      
      SHALLOW_DEBUG_PRINT("mpi: mpi_recv: copying %llu bytes\n", size);

      if (fast_inter_vm_copy(core,
			     acc,
			     buf,
			     sender->core,
			     sender->acc,
			     sender->send_vaddr,
			     size) != size) { 
	ERROR("mpi: fast_inter_vm_copy failed in mpi_recv: destvm=%p, destacc=%p, dest_va=%p, srcvm=%p, srcacc=%p, src_va=%p, size=%llu\n",core,acc,buf,sender->core,sender->acc,sender->send_vaddr,size);
	return -1;
      }
      
      SHALLOW_DEBUG_PRINT("mpi: mpi_recv: finished copying\n");

      // Now we release the sender
      sender->send_rc = 0;
      sender->send_pending = 0;

      wake_up_interruptible(&(sender->send_wait_queue));

      // And we are also done

      return 0;

    } else {
      DEEP_DEBUG_PRINT("mpi: sender's pending send does not match - pending ourselves\n");
      goto pending;
    }
  }
      


 pending:
  
  // we store our state
  receiver->recv_vaddr=buf;
  receiver->recv_size=n;
  receiver->recv_src=src;
  receiver->recv_tag=tag;
  receiver->recv_rc=-1;

  // And now we wait for the send to do the job
  receiver->recv_pending=1;
  while (wait_event_interruptible(receiver->recv_wait_queue,
				  !(receiver->recv_pending)) !=0) {
    // wait wait wait
  }

  // released

  return receiver->recv_rc;
}


static void get_args_64(palacios_core_t core,
			struct guest_accessors *acc,
			uint64_t *a1,
			uint64_t *a2,
			uint64_t *a3,
			uint64_t *a4,
			uint64_t *a5,
			uint64_t *a6,
			uint64_t *a7,
			uint64_t *a8)
{
  *a1 = acc->get_rcx(core);
  *a2 = acc->get_rdx(core);
  *a3 = acc->get_rsi(core);
  *a4 = acc->get_rdi(core);
  *a5 = acc->get_r8(core);
  *a6 = acc->get_r9(core);
  *a7 = acc->get_r10(core);
  *a8 = acc->get_r11(core);
}

static void get_args_32(palacios_core_t core,
			struct guest_accessors *acc,
			uint64_t *a1,
			uint64_t *a2,
			uint64_t *a3,
			uint64_t *a4,
			uint64_t *a5,
			uint64_t *a6,
			uint64_t *a7,
			uint64_t *a8)
{
  uint64_t rsp;
  uint32_t temp;


  rsp = acc->get_rsp(core);

  acc->read_gva(core,rsp,4,&temp); *a1=temp;
  acc->read_gva(core,rsp+4,4,&temp); *a2=temp;
  acc->read_gva(core,rsp+8,4,&temp); *a3=temp;
  acc->read_gva(core,rsp+12,4,&temp); *a4=temp;
  acc->read_gva(core,rsp+16,4,&temp); *a5=temp;
  acc->read_gva(core,rsp+20,4,&temp); *a6=temp;
  acc->read_gva(core,rsp+24,4,&temp); *a7=temp;
  acc->read_gva(core,rsp+28,4,&temp); *a8=temp;
  
}

static void get_args(palacios_core_t core,
		     struct guest_accessors *acc,
		     uint64_t *a1,
		     uint64_t *a2,
		     uint64_t *a3,
		     uint64_t *a4,
		     uint64_t *a5,
		     uint64_t *a6,
		     uint64_t *a7,
		     uint64_t *a8)
{
  uint64_t rbx;
  uint32_t ebx;

  rbx=acc->get_rbx(core);
  ebx=rbx&0xffffffff;
  
  switch (ebx) {
  case 0x64646464:
    DEEP_DEBUG_PRINT("64 bit hcall\n");
    return get_args_64(core,acc,a1,a2,a3,a4,a5,a6,a7,a8);
    break;
  case 0x32323232:
    DEEP_DEBUG_PRINT("32 bit hcall\n");
    return get_args_32(core,acc,a1,a2,a3,a4,a5,a6,a7,a8);
    break;
  default:
    ERROR("UNKNOWN hcall calling convention\n");
    break;
  }
}

static void put_return(palacios_core_t core, 
		       struct guest_accessors *acc,
		       uint64_t rc)
{
  acc->set_rax(core,rc);
}
		     

int mpi_hypercall(palacios_core_t *core,
		  unsigned int hid,
		  struct guest_accessors *acc,
		  void *p)
{
  uint64_t a1,a2,a3,a4,a5,a6,a7,a8;
  uint64_t rc;

  DEEP_DEBUG_PRINT("palacios: mpi_hypercall(%p,0x%x,%p,%p)\n",
		  core,hid,acc,p);

  get_args(core,acc,&a1,&a2,&a3,&a4,&a5,&a6,&a7,&a8);

  DEEP_DEBUG_PRINT("palacios: arguments: %p, %p, %p, %p, %p, %p, %p, %p\n",
		    a1,a2,a3,a4,a5,a6,a7,a8);

  switch (hid) { 
  case MPI_INIT:
    rc = mpi_init_hcall(core,acc,(int*)a1,(char ***)a2);
    break;
  case MPI_DEINIT:
    rc = mpi_deinit_hcall(core,acc);
    break;
  case MPI_RANK:
    rc = mpi_comm_rank_hcall(core,acc,(void*)a1,(int*)a2);
    break;
  case MPI_SEND:
    rc = mpi_send_hcall(core,acc,(void*)a1,(int)a2,(int)a3,(int)a4,(int)a5,(int)a6);
    break;
  case MPI_RECV:
    rc = mpi_recv_hcall(core,acc,(void*)a1,(int)a2,(int)a3,(int)a4,(int)a5,(int)a6,(void*)a7);
    break;
  default:
    ERROR("palacios: mpi: unknown hcall number\n");
    rc = -1;
  }

  put_return(core,acc,rc);

  return 0;

} 



EXPORT_SYMBOL(mpi_hypercall);


int init_module(void) 
{

  rtab = kmalloc(sizeof(struct rendezvous_table_row)*RENDEZVOUS_TABLE_MAX,GFP_KERNEL);
  if (!rtab) { 
    ERROR("mpi: could not allocate memory\n");
    return -1;
  } else {
    memset(rtab,0,sizeof(struct rendezvous_table_row)*RENDEZVOUS_TABLE_MAX);
    INFO("mpi: inited\n");
    return 0;
  }
  
}


void cleanup_module(void) 
{
  if (rtab) { 
    kfree(rtab);
    rtab=0;
  }

  INFO("mpi: deinited\n");
 
}


