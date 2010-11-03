#include <geekos/fmtout.h>
#include <geekos/string.h>
#include <geekos/idt.h>
#include <geekos/vm_cons.h>

#define CONS_PORT 0xc0c0

void VMConsPutChar(unsigned char c) {
  /* send char */
  Out_Byte(CONS_PORT, c);
}




void VMConsPutLineN(char * line, int len) {
  int i;
  for (i = 0; i < len && line[i] != 0; i++) { 
    VMConsPutChar(line[i]); 
  }
}


void VMConsPutLine(char * line) {
  int i;
  for (i = 0; line[i]!= 0; i++) { 
    VMConsPutChar(line[i]); 
  }
}


void VMConsPrintHex(unsigned char x)
{
  unsigned char z;
  
  z = (x >> 4) & 0xf ;
  VMConsPrint("%x", z);
  z = x & 0xf;
  VMConsPrint("%x", z);
}

void VMConsMemDump(unsigned char *start, int n)
{
  int i, j;

  for (i=0;i<n;i+=16) {
    VMConsPrint("%8x", *(uchar_t*)(start+i));
    for (j=i; j<i+16 && j<n; j+=2) {
      VMConsPrint(" ");
      VMConsPrintHex(*((unsigned char *)(start+j)));
      if ((j+1)<n) { 
	VMConsPrintHex(*((unsigned char *)(start+j+1)));
      }
    }
    VMConsPrint(" ");
    for (j=i; j<i+16 && j<n;j++) {
      VMConsPrint("%c", ((start[j]>=32) && (start[j]<=126)) ? start[j] : '.');
    }
    VMConsPrint("\n");
  }
}


static struct Output_Sink vm_cons_output_sink;
static void VMCons_Emit(struct Output_Sink * o, int ch) { 
  VMConsPutChar((unsigned char)ch); 
}
static void VMCons_Finish(struct Output_Sink * o) { return; }


static void __inline__ VMConsPrintInternal(const char * format, va_list ap) {
  Format_Output(&vm_cons_output_sink, format, ap);
}


void VMConsPrint(const char * format, ...) {
  va_list args;
  bool iflag = Begin_Int_Atomic();

  va_start(args, format);
  VMConsPrintInternal(format, args);
  va_end(args);

  End_Int_Atomic(iflag);
}

void VMConsPrintList(const char * format, va_list ap) {
  bool iflag = Begin_Int_Atomic();
  VMConsPrintInternal(format, ap);
  End_Int_Atomic(iflag);

}




void VMConsPrintLevel(int level, const char * format, ...) {
    va_list args;
    bool iflag = Begin_Int_Atomic();
    
    va_start(args, format);
    VMConsPrintInternal(format, args);
    va_end(args);
    
    End_Int_Atomic(iflag);   
}

void Init_VMCons() {

    vm_cons_output_sink.Emit = &VMCons_Emit;
    vm_cons_output_sink.Finish = &VMCons_Finish;

    VMConsPrint("Initializing VM Console\n");

    return;
}
