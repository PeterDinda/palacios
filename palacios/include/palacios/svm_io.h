/* Northwestern University */
/* (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> */

#ifndef __SVM_IO_H
#define __SVM_IO_H

#ifdef __V3VEE__

#include <palacios/vm_guest.h>
#include <palacios/vmcb.h>
#include <palacios/vmm.h>


struct svm_io_info {
  uint_t type        : 1       PACKED;  // (0=out, 1=in)
  uint_t rsvd        : 1       PACKED;  // Must be Zero
  uint_t str         : 1       PACKED;  // string based io
  uint_t rep         : 1       PACKED;  // repeated io
  uint_t sz8         : 1       PACKED;  // 8 bit op size
  uint_t sz16        : 1       PACKED;  // 16 bit op size
  uint_t sz32        : 1       PACKED;  // 32 bit op size
  uint_t addr16      : 1       PACKED;  // 16 bit addr
  uint_t addr32      : 1       PACKED;  // 32 bit addr
  uint_t addr64      : 1       PACKED;  // 64 bit addr
  uint_t rsvd2       : 6       PACKED;  // Should be Zero
  ushort_t port                PACKED;  // port number
};


int handle_svm_io_in(struct guest_info * info);
int handle_svm_io_ins(struct guest_info * info);
int handle_svm_io_out(struct guest_info * info);
int handle_svm_io_outs(struct guest_info * info);

#endif // !__V3VEE__


#endif
