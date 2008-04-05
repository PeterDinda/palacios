#include <palacios/vmcs_gen.h>




void    Set_VMCS_GUEST_ES_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_GUEST_ES_SELECTOR,val); } 
uint_t  Get_VMCS_GUEST_ES_SELECTOR() { uint_t rc; VMCS_READ(VMCS_GUEST_ES_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_GUEST_ES_SELECTOR() { PrintTrace("VMCS_GUEST_ES_SELECTOR = %x\n", Get_VMCS_GUEST_ES_SELECTOR()); }


void    Set_VMCS_GUEST_CS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_GUEST_CS_SELECTOR,val); } 
uint_t  Get_VMCS_GUEST_CS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_GUEST_CS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_GUEST_CS_SELECTOR() { PrintTrace("VMCS_GUEST_CS_SELECTOR = %x\n", Get_VMCS_GUEST_CS_SELECTOR()); }


void    Set_VMCS_GUEST_SS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_GUEST_SS_SELECTOR,val); } 
uint_t  Get_VMCS_GUEST_SS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_GUEST_SS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_GUEST_SS_SELECTOR() { PrintTrace("VMCS_GUEST_SS_SELECTOR = %x\n", Get_VMCS_GUEST_SS_SELECTOR()); }


void    Set_VMCS_GUEST_DS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_GUEST_DS_SELECTOR,val); } 
uint_t  Get_VMCS_GUEST_DS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_GUEST_DS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_GUEST_DS_SELECTOR() { PrintTrace("VMCS_GUEST_DS_SELECTOR = %x\n", Get_VMCS_GUEST_DS_SELECTOR()); }


void    Set_VMCS_GUEST_FS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_GUEST_FS_SELECTOR,val); } 
uint_t  Get_VMCS_GUEST_FS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_GUEST_FS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_GUEST_FS_SELECTOR() { PrintTrace("VMCS_GUEST_FS_SELECTOR = %x\n", Get_VMCS_GUEST_FS_SELECTOR()); }


void    Set_VMCS_GUEST_GS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_GUEST_GS_SELECTOR,val); } 
uint_t  Get_VMCS_GUEST_GS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_GUEST_GS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_GUEST_GS_SELECTOR() { PrintTrace("VMCS_GUEST_GS_SELECTOR = %x\n", Get_VMCS_GUEST_GS_SELECTOR()); }


void    Set_VMCS_GUEST_LDTR_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_GUEST_LDTR_SELECTOR,val); } 
uint_t  Get_VMCS_GUEST_LDTR_SELECTOR() { uint_t rc; VMCS_READ(VMCS_GUEST_LDTR_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_GUEST_LDTR_SELECTOR() { PrintTrace("VMCS_GUEST_LDTR_SELECTOR = %x\n", Get_VMCS_GUEST_LDTR_SELECTOR()); }


void    Set_VMCS_GUEST_TR_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_GUEST_TR_SELECTOR,val); } 
uint_t  Get_VMCS_GUEST_TR_SELECTOR() { uint_t rc; VMCS_READ(VMCS_GUEST_TR_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_GUEST_TR_SELECTOR() { PrintTrace("VMCS_GUEST_TR_SELECTOR = %x\n", Get_VMCS_GUEST_TR_SELECTOR()); }


void    Set_VMCS_HOST_ES_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_HOST_ES_SELECTOR,val); } 
uint_t  Get_VMCS_HOST_ES_SELECTOR() { uint_t rc; VMCS_READ(VMCS_HOST_ES_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_HOST_ES_SELECTOR() { PrintTrace("VMCS_HOST_ES_SELECTOR = %x\n", Get_VMCS_HOST_ES_SELECTOR()); }


void    Set_VMCS_HOST_CS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_HOST_CS_SELECTOR,val); } 
uint_t  Get_VMCS_HOST_CS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_HOST_CS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_HOST_CS_SELECTOR() { PrintTrace("VMCS_HOST_CS_SELECTOR = %x\n", Get_VMCS_HOST_CS_SELECTOR()); }


void    Set_VMCS_HOST_SS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_HOST_SS_SELECTOR,val); } 
uint_t  Get_VMCS_HOST_SS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_HOST_SS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_HOST_SS_SELECTOR() { PrintTrace("VMCS_HOST_SS_SELECTOR = %x\n", Get_VMCS_HOST_SS_SELECTOR()); }


void    Set_VMCS_HOST_DS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_HOST_DS_SELECTOR,val); } 
uint_t  Get_VMCS_HOST_DS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_HOST_DS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_HOST_DS_SELECTOR() { PrintTrace("VMCS_HOST_DS_SELECTOR = %x\n", Get_VMCS_HOST_DS_SELECTOR()); }


void    Set_VMCS_HOST_FS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_HOST_FS_SELECTOR,val); } 
uint_t  Get_VMCS_HOST_FS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_HOST_FS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_HOST_FS_SELECTOR() { PrintTrace("VMCS_HOST_FS_SELECTOR = %x\n", Get_VMCS_HOST_FS_SELECTOR()); }


void    Set_VMCS_HOST_GS_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_HOST_GS_SELECTOR,val); } 
uint_t  Get_VMCS_HOST_GS_SELECTOR() { uint_t rc; VMCS_READ(VMCS_HOST_GS_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_HOST_GS_SELECTOR() { PrintTrace("VMCS_HOST_GS_SELECTOR = %x\n", Get_VMCS_HOST_GS_SELECTOR()); }


void    Set_VMCS_HOST_TR_SELECTOR(uint_t val) { VMCS_WRITE(VMCS_HOST_TR_SELECTOR,val); } 
uint_t  Get_VMCS_HOST_TR_SELECTOR() { uint_t rc; VMCS_READ(VMCS_HOST_TR_SELECTOR,&rc); return rc; }

void    PrintTrace_VMCS_HOST_TR_SELECTOR() { PrintTrace("VMCS_HOST_TR_SELECTOR = %x\n", Get_VMCS_HOST_TR_SELECTOR()); }


void    Set_IO_BITMAP_A_ADDR(uint_t val) { VMCS_WRITE(IO_BITMAP_A_ADDR,val); } 
uint_t  Get_IO_BITMAP_A_ADDR() { uint_t rc; VMCS_READ(IO_BITMAP_A_ADDR,&rc); return rc; }

void    PrintTrace_IO_BITMAP_A_ADDR() { PrintTrace("IO_BITMAP_A_ADDR = %x\n", Get_IO_BITMAP_A_ADDR()); }


void    Set_IO_BITMAP_A_ADDR_HIGH(uint_t val) { VMCS_WRITE(IO_BITMAP_A_ADDR_HIGH,val); } 
uint_t  Get_IO_BITMAP_A_ADDR_HIGH() { uint_t rc; VMCS_READ(IO_BITMAP_A_ADDR_HIGH,&rc); return rc; }

void    PrintTrace_IO_BITMAP_A_ADDR_HIGH() { PrintTrace("IO_BITMAP_A_ADDR_HIGH = %x\n", Get_IO_BITMAP_A_ADDR_HIGH()); }


void    Set_IO_BITMAP_B_ADDR(uint_t val) { VMCS_WRITE(IO_BITMAP_B_ADDR,val); } 
uint_t  Get_IO_BITMAP_B_ADDR() { uint_t rc; VMCS_READ(IO_BITMAP_B_ADDR,&rc); return rc; }

void    PrintTrace_IO_BITMAP_B_ADDR() { PrintTrace("IO_BITMAP_B_ADDR = %x\n", Get_IO_BITMAP_B_ADDR()); }


void    Set_IO_BITMAP_B_ADDR_HIGH(uint_t val) { VMCS_WRITE(IO_BITMAP_B_ADDR_HIGH,val); } 
uint_t  Get_IO_BITMAP_B_ADDR_HIGH() { uint_t rc; VMCS_READ(IO_BITMAP_B_ADDR_HIGH,&rc); return rc; }

void    PrintTrace_IO_BITMAP_B_ADDR_HIGH() { PrintTrace("IO_BITMAP_B_ADDR_HIGH = %x\n", Get_IO_BITMAP_B_ADDR_HIGH()); }


void    Set_MSR_BITMAPS(uint_t val) { VMCS_WRITE(MSR_BITMAPS,val); } 
uint_t  Get_MSR_BITMAPS() { uint_t rc; VMCS_READ(MSR_BITMAPS,&rc); return rc; }

void    PrintTrace_MSR_BITMAPS() { PrintTrace("MSR_BITMAPS = %x\n", Get_MSR_BITMAPS()); }


void    Set_MSR_BITMAPS_HIGH(uint_t val) { VMCS_WRITE(MSR_BITMAPS_HIGH,val); } 
uint_t  Get_MSR_BITMAPS_HIGH() { uint_t rc; VMCS_READ(MSR_BITMAPS_HIGH,&rc); return rc; }

void    PrintTrace_MSR_BITMAPS_HIGH() { PrintTrace("MSR_BITMAPS_HIGH = %x\n", Get_MSR_BITMAPS_HIGH()); }


void    Set_VM_EXIT_MSR_STORE_ADDR(uint_t val) { VMCS_WRITE(VM_EXIT_MSR_STORE_ADDR,val); } 
uint_t  Get_VM_EXIT_MSR_STORE_ADDR() { uint_t rc; VMCS_READ(VM_EXIT_MSR_STORE_ADDR,&rc); return rc; }

void    PrintTrace_VM_EXIT_MSR_STORE_ADDR() { PrintTrace("VM_EXIT_MSR_STORE_ADDR = %x\n", Get_VM_EXIT_MSR_STORE_ADDR()); }


void    Set_VM_EXIT_MSR_STORE_ADDR_HIGH(uint_t val) { VMCS_WRITE(VM_EXIT_MSR_STORE_ADDR_HIGH,val); } 
uint_t  Get_VM_EXIT_MSR_STORE_ADDR_HIGH() { uint_t rc; VMCS_READ(VM_EXIT_MSR_STORE_ADDR_HIGH,&rc); return rc; }

void    PrintTrace_VM_EXIT_MSR_STORE_ADDR_HIGH() { PrintTrace("VM_EXIT_MSR_STORE_ADDR_HIGH = %x\n", Get_VM_EXIT_MSR_STORE_ADDR_HIGH()); }


void    Set_VM_EXIT_MSR_LOAD_ADDR(uint_t val) { VMCS_WRITE(VM_EXIT_MSR_LOAD_ADDR,val); } 
uint_t  Get_VM_EXIT_MSR_LOAD_ADDR() { uint_t rc; VMCS_READ(VM_EXIT_MSR_LOAD_ADDR,&rc); return rc; }

void    PrintTrace_VM_EXIT_MSR_LOAD_ADDR() { PrintTrace("VM_EXIT_MSR_LOAD_ADDR = %x\n", Get_VM_EXIT_MSR_LOAD_ADDR()); }


void    Set_VM_EXIT_MSR_LOAD_ADDR_HIGH(uint_t val) { VMCS_WRITE(VM_EXIT_MSR_LOAD_ADDR_HIGH,val); } 
uint_t  Get_VM_EXIT_MSR_LOAD_ADDR_HIGH() { uint_t rc; VMCS_READ(VM_EXIT_MSR_LOAD_ADDR_HIGH,&rc); return rc; }

void    PrintTrace_VM_EXIT_MSR_LOAD_ADDR_HIGH() { PrintTrace("VM_EXIT_MSR_LOAD_ADDR_HIGH = %x\n", Get_VM_EXIT_MSR_LOAD_ADDR_HIGH()); }


void    Set_VM_ENTRY_MSR_LOAD_ADDR(uint_t val) { VMCS_WRITE(VM_ENTRY_MSR_LOAD_ADDR,val); } 
uint_t  Get_VM_ENTRY_MSR_LOAD_ADDR() { uint_t rc; VMCS_READ(VM_ENTRY_MSR_LOAD_ADDR,&rc); return rc; }

void    PrintTrace_VM_ENTRY_MSR_LOAD_ADDR() { PrintTrace("VM_ENTRY_MSR_LOAD_ADDR = %x\n", Get_VM_ENTRY_MSR_LOAD_ADDR()); }


void    Set_VM_ENTRY_MSR_LOAD_ADDR_HIGH(uint_t val) { VMCS_WRITE(VM_ENTRY_MSR_LOAD_ADDR_HIGH,val); } 
uint_t  Get_VM_ENTRY_MSR_LOAD_ADDR_HIGH() { uint_t rc; VMCS_READ(VM_ENTRY_MSR_LOAD_ADDR_HIGH,&rc); return rc; }

void    PrintTrace_VM_ENTRY_MSR_LOAD_ADDR_HIGH() { PrintTrace("VM_ENTRY_MSR_LOAD_ADDR_HIGH = %x\n", Get_VM_ENTRY_MSR_LOAD_ADDR_HIGH()); }


void    Set_VMCS_EXEC_PTR(uint_t val) { VMCS_WRITE(VMCS_EXEC_PTR,val); } 
uint_t  Get_VMCS_EXEC_PTR() { uint_t rc; VMCS_READ(VMCS_EXEC_PTR,&rc); return rc; }

void    PrintTrace_VMCS_EXEC_PTR() { PrintTrace("VMCS_EXEC_PTR = %x\n", Get_VMCS_EXEC_PTR()); }


void    Set_VMCS_EXEC_PTR_HIGH(uint_t val) { VMCS_WRITE(VMCS_EXEC_PTR_HIGH,val); } 
uint_t  Get_VMCS_EXEC_PTR_HIGH() { uint_t rc; VMCS_READ(VMCS_EXEC_PTR_HIGH,&rc); return rc; }

void    PrintTrace_VMCS_EXEC_PTR_HIGH() { PrintTrace("VMCS_EXEC_PTR_HIGH = %x\n", Get_VMCS_EXEC_PTR_HIGH()); }


void    Set_TSC_OFFSET(uint_t val) { VMCS_WRITE(TSC_OFFSET,val); } 
uint_t  Get_TSC_OFFSET() { uint_t rc; VMCS_READ(TSC_OFFSET,&rc); return rc; }

void    PrintTrace_TSC_OFFSET() { PrintTrace("TSC_OFFSET = %x\n", Get_TSC_OFFSET()); }


void    Set_TSC_OFFSET_HIGH(uint_t val) { VMCS_WRITE(TSC_OFFSET_HIGH,val); } 
uint_t  Get_TSC_OFFSET_HIGH() { uint_t rc; VMCS_READ(TSC_OFFSET_HIGH,&rc); return rc; }

void    PrintTrace_TSC_OFFSET_HIGH() { PrintTrace("TSC_OFFSET_HIGH = %x\n", Get_TSC_OFFSET_HIGH()); }


void    Set_VIRT_APIC_PAGE_ADDR(uint_t val) { VMCS_WRITE(VIRT_APIC_PAGE_ADDR,val); } 
uint_t  Get_VIRT_APIC_PAGE_ADDR() { uint_t rc; VMCS_READ(VIRT_APIC_PAGE_ADDR,&rc); return rc; }

void    PrintTrace_VIRT_APIC_PAGE_ADDR() { PrintTrace("VIRT_APIC_PAGE_ADDR = %x\n", Get_VIRT_APIC_PAGE_ADDR()); }


void    Set_VIRT_APIC_PAGE_ADDR_HIGH(uint_t val) { VMCS_WRITE(VIRT_APIC_PAGE_ADDR_HIGH,val); } 
uint_t  Get_VIRT_APIC_PAGE_ADDR_HIGH() { uint_t rc; VMCS_READ(VIRT_APIC_PAGE_ADDR_HIGH,&rc); return rc; }

void    PrintTrace_VIRT_APIC_PAGE_ADDR_HIGH() { PrintTrace("VIRT_APIC_PAGE_ADDR_HIGH = %x\n", Get_VIRT_APIC_PAGE_ADDR_HIGH()); }


void    Set_VMCS_LINK_PTR(uint_t val) { VMCS_WRITE(VMCS_LINK_PTR,val); } 
uint_t  Get_VMCS_LINK_PTR() { uint_t rc; VMCS_READ(VMCS_LINK_PTR,&rc); return rc; }

void    PrintTrace_VMCS_LINK_PTR() { PrintTrace("VMCS_LINK_PTR = %x\n", Get_VMCS_LINK_PTR()); }


void    Set_VMCS_LINK_PTR_HIGH(uint_t val) { VMCS_WRITE(VMCS_LINK_PTR_HIGH,val); } 
uint_t  Get_VMCS_LINK_PTR_HIGH() { uint_t rc; VMCS_READ(VMCS_LINK_PTR_HIGH,&rc); return rc; }

void    PrintTrace_VMCS_LINK_PTR_HIGH() { PrintTrace("VMCS_LINK_PTR_HIGH = %x\n", Get_VMCS_LINK_PTR_HIGH()); }


void    Set_GUEST_IA32_DEBUGCTL(uint_t val) { VMCS_WRITE(GUEST_IA32_DEBUGCTL,val); } 
uint_t  Get_GUEST_IA32_DEBUGCTL() { uint_t rc; VMCS_READ(GUEST_IA32_DEBUGCTL,&rc); return rc; }

void    PrintTrace_GUEST_IA32_DEBUGCTL() { PrintTrace("GUEST_IA32_DEBUGCTL = %x\n", Get_GUEST_IA32_DEBUGCTL()); }


void    Set_GUEST_IA32_DEBUGCTL_HIGH(uint_t val) { VMCS_WRITE(GUEST_IA32_DEBUGCTL_HIGH,val); } 
uint_t  Get_GUEST_IA32_DEBUGCTL_HIGH() { uint_t rc; VMCS_READ(GUEST_IA32_DEBUGCTL_HIGH,&rc); return rc; }

void    PrintTrace_GUEST_IA32_DEBUGCTL_HIGH() { PrintTrace("GUEST_IA32_DEBUGCTL_HIGH = %x\n", Get_GUEST_IA32_DEBUGCTL_HIGH()); }


void    Set_PIN_VM_EXEC_CTRLS(uint_t val) { VMCS_WRITE(PIN_VM_EXEC_CTRLS,val); } 
uint_t  Get_PIN_VM_EXEC_CTRLS() { uint_t rc; VMCS_READ(PIN_VM_EXEC_CTRLS,&rc); return rc; }

void    PrintTrace_PIN_VM_EXEC_CTRLS() { PrintTrace("PIN_VM_EXEC_CTRLS = %x\n", Get_PIN_VM_EXEC_CTRLS()); }


void    Set_PROC_VM_EXEC_CTRLS(uint_t val) { VMCS_WRITE(PROC_VM_EXEC_CTRLS,val); } 
uint_t  Get_PROC_VM_EXEC_CTRLS() { uint_t rc; VMCS_READ(PROC_VM_EXEC_CTRLS,&rc); return rc; }

void    PrintTrace_PROC_VM_EXEC_CTRLS() { PrintTrace("PROC_VM_EXEC_CTRLS = %x\n", Get_PROC_VM_EXEC_CTRLS()); }


void    Set_EXCEPTION_BITMAP(uint_t val) { VMCS_WRITE(EXCEPTION_BITMAP,val); } 
uint_t  Get_EXCEPTION_BITMAP() { uint_t rc; VMCS_READ(EXCEPTION_BITMAP,&rc); return rc; }

void    PrintTrace_EXCEPTION_BITMAP() { PrintTrace("EXCEPTION_BITMAP = %x\n", Get_EXCEPTION_BITMAP()); }


void    Set_PAGE_FAULT_ERROR_MASK(uint_t val) { VMCS_WRITE(PAGE_FAULT_ERROR_MASK,val); } 
uint_t  Get_PAGE_FAULT_ERROR_MASK() { uint_t rc; VMCS_READ(PAGE_FAULT_ERROR_MASK,&rc); return rc; }

void    PrintTrace_PAGE_FAULT_ERROR_MASK() { PrintTrace("PAGE_FAULT_ERROR_MASK = %x\n", Get_PAGE_FAULT_ERROR_MASK()); }


void    Set_PAGE_FAULT_ERROR_MATCH(uint_t val) { VMCS_WRITE(PAGE_FAULT_ERROR_MATCH,val); } 
uint_t  Get_PAGE_FAULT_ERROR_MATCH() { uint_t rc; VMCS_READ(PAGE_FAULT_ERROR_MATCH,&rc); return rc; }

void    PrintTrace_PAGE_FAULT_ERROR_MATCH() { PrintTrace("PAGE_FAULT_ERROR_MATCH = %x\n", Get_PAGE_FAULT_ERROR_MATCH()); }


void    Set_CR3_TARGET_COUNT(uint_t val) { VMCS_WRITE(CR3_TARGET_COUNT,val); } 
uint_t  Get_CR3_TARGET_COUNT() { uint_t rc; VMCS_READ(CR3_TARGET_COUNT,&rc); return rc; }

void    PrintTrace_CR3_TARGET_COUNT() { PrintTrace("CR3_TARGET_COUNT = %x\n", Get_CR3_TARGET_COUNT()); }


void    Set_VM_EXIT_CTRLS(uint_t val) { VMCS_WRITE(VM_EXIT_CTRLS,val); } 
uint_t  Get_VM_EXIT_CTRLS() { uint_t rc; VMCS_READ(VM_EXIT_CTRLS,&rc); return rc; }

void    PrintTrace_VM_EXIT_CTRLS() { PrintTrace("VM_EXIT_CTRLS = %x\n", Get_VM_EXIT_CTRLS()); }


void    Set_VM_EXIT_MSR_STORE_COUNT(uint_t val) { VMCS_WRITE(VM_EXIT_MSR_STORE_COUNT,val); } 
uint_t  Get_VM_EXIT_MSR_STORE_COUNT() { uint_t rc; VMCS_READ(VM_EXIT_MSR_STORE_COUNT,&rc); return rc; }

void    PrintTrace_VM_EXIT_MSR_STORE_COUNT() { PrintTrace("VM_EXIT_MSR_STORE_COUNT = %x\n", Get_VM_EXIT_MSR_STORE_COUNT()); }


void    Set_VM_EXIT_MSR_LOAD_COUNT(uint_t val) { VMCS_WRITE(VM_EXIT_MSR_LOAD_COUNT,val); } 
uint_t  Get_VM_EXIT_MSR_LOAD_COUNT() { uint_t rc; VMCS_READ(VM_EXIT_MSR_LOAD_COUNT,&rc); return rc; }

void    PrintTrace_VM_EXIT_MSR_LOAD_COUNT() { PrintTrace("VM_EXIT_MSR_LOAD_COUNT = %x\n", Get_VM_EXIT_MSR_LOAD_COUNT()); }


void    Set_VM_ENTRY_CTRLS(uint_t val) { VMCS_WRITE(VM_ENTRY_CTRLS,val); } 
uint_t  Get_VM_ENTRY_CTRLS() { uint_t rc; VMCS_READ(VM_ENTRY_CTRLS,&rc); return rc; }

void    PrintTrace_VM_ENTRY_CTRLS() { PrintTrace("VM_ENTRY_CTRLS = %x\n", Get_VM_ENTRY_CTRLS()); }


void    Set_VM_ENTRY_MSR_LOAD_COUNT(uint_t val) { VMCS_WRITE(VM_ENTRY_MSR_LOAD_COUNT,val); } 
uint_t  Get_VM_ENTRY_MSR_LOAD_COUNT() { uint_t rc; VMCS_READ(VM_ENTRY_MSR_LOAD_COUNT,&rc); return rc; }

void    PrintTrace_VM_ENTRY_MSR_LOAD_COUNT() { PrintTrace("VM_ENTRY_MSR_LOAD_COUNT = %x\n", Get_VM_ENTRY_MSR_LOAD_COUNT()); }


void    Set_VM_ENTRY_INT_INFO_FIELD(uint_t val) { VMCS_WRITE(VM_ENTRY_INT_INFO_FIELD,val); } 
uint_t  Get_VM_ENTRY_INT_INFO_FIELD() { uint_t rc; VMCS_READ(VM_ENTRY_INT_INFO_FIELD,&rc); return rc; }

void    PrintTrace_VM_ENTRY_INT_INFO_FIELD() { PrintTrace("VM_ENTRY_INT_INFO_FIELD = %x\n", Get_VM_ENTRY_INT_INFO_FIELD()); }


void    Set_VM_ENTRY_EXCEPTION_ERROR(uint_t val) { VMCS_WRITE(VM_ENTRY_EXCEPTION_ERROR,val); } 
uint_t  Get_VM_ENTRY_EXCEPTION_ERROR() { uint_t rc; VMCS_READ(VM_ENTRY_EXCEPTION_ERROR,&rc); return rc; }

void    PrintTrace_VM_ENTRY_EXCEPTION_ERROR() { PrintTrace("VM_ENTRY_EXCEPTION_ERROR = %x\n", Get_VM_ENTRY_EXCEPTION_ERROR()); }


void    Set_VM_ENTRY_INSTR_LENGTH(uint_t val) { VMCS_WRITE(VM_ENTRY_INSTR_LENGTH,val); } 
uint_t  Get_VM_ENTRY_INSTR_LENGTH() { uint_t rc; VMCS_READ(VM_ENTRY_INSTR_LENGTH,&rc); return rc; }

void    PrintTrace_VM_ENTRY_INSTR_LENGTH() { PrintTrace("VM_ENTRY_INSTR_LENGTH = %x\n", Get_VM_ENTRY_INSTR_LENGTH()); }


void    Set_TPR_THRESHOLD(uint_t val) { VMCS_WRITE(TPR_THRESHOLD,val); } 
uint_t  Get_TPR_THRESHOLD() { uint_t rc; VMCS_READ(TPR_THRESHOLD,&rc); return rc; }

void    PrintTrace_TPR_THRESHOLD() { PrintTrace("TPR_THRESHOLD = %x\n", Get_TPR_THRESHOLD()); }


void    Set_VM_INSTR_ERROR(uint_t val) { VMCS_WRITE(VM_INSTR_ERROR,val); } 
uint_t  Get_VM_INSTR_ERROR() { uint_t rc; VMCS_READ(VM_INSTR_ERROR,&rc); return rc; }

void    PrintTrace_VM_INSTR_ERROR() { PrintTrace("VM_INSTR_ERROR = %x\n", Get_VM_INSTR_ERROR()); }


void    Set_EXIT_REASON(uint_t val) { VMCS_WRITE(EXIT_REASON,val); } 
uint_t  Get_EXIT_REASON() { uint_t rc; VMCS_READ(EXIT_REASON,&rc); return rc; }

void    PrintTrace_EXIT_REASON() { PrintTrace("EXIT_REASON = %x\n", Get_EXIT_REASON()); }


void    Set_VM_EXIT_INT_INFO(uint_t val) { VMCS_WRITE(VM_EXIT_INT_INFO,val); } 
uint_t  Get_VM_EXIT_INT_INFO() { uint_t rc; VMCS_READ(VM_EXIT_INT_INFO,&rc); return rc; }

void    PrintTrace_VM_EXIT_INT_INFO() { PrintTrace("VM_EXIT_INT_INFO = %x\n", Get_VM_EXIT_INT_INFO()); }


void    Set_VM_EXIT_INT_ERROR(uint_t val) { VMCS_WRITE(VM_EXIT_INT_ERROR,val); } 
uint_t  Get_VM_EXIT_INT_ERROR() { uint_t rc; VMCS_READ(VM_EXIT_INT_ERROR,&rc); return rc; }

void    PrintTrace_VM_EXIT_INT_ERROR() { PrintTrace("VM_EXIT_INT_ERROR = %x\n", Get_VM_EXIT_INT_ERROR()); }


void    Set_IDT_VECTOR_INFO(uint_t val) { VMCS_WRITE(IDT_VECTOR_INFO,val); } 
uint_t  Get_IDT_VECTOR_INFO() { uint_t rc; VMCS_READ(IDT_VECTOR_INFO,&rc); return rc; }

void    PrintTrace_IDT_VECTOR_INFO() { PrintTrace("IDT_VECTOR_INFO = %x\n", Get_IDT_VECTOR_INFO()); }


void    Set_IDT_VECTOR_ERROR(uint_t val) { VMCS_WRITE(IDT_VECTOR_ERROR,val); } 
uint_t  Get_IDT_VECTOR_ERROR() { uint_t rc; VMCS_READ(IDT_VECTOR_ERROR,&rc); return rc; }

void    PrintTrace_IDT_VECTOR_ERROR() { PrintTrace("IDT_VECTOR_ERROR = %x\n", Get_IDT_VECTOR_ERROR()); }


void    Set_VM_EXIT_INSTR_LENGTH(uint_t val) { VMCS_WRITE(VM_EXIT_INSTR_LENGTH,val); } 
uint_t  Get_VM_EXIT_INSTR_LENGTH() { uint_t rc; VMCS_READ(VM_EXIT_INSTR_LENGTH,&rc); return rc; }

void    PrintTrace_VM_EXIT_INSTR_LENGTH() { PrintTrace("VM_EXIT_INSTR_LENGTH = %x\n", Get_VM_EXIT_INSTR_LENGTH()); }


void    Set_VMX_INSTR_INFO(uint_t val) { VMCS_WRITE(VMX_INSTR_INFO,val); } 
uint_t  Get_VMX_INSTR_INFO() { uint_t rc; VMCS_READ(VMX_INSTR_INFO,&rc); return rc; }

void    PrintTrace_VMX_INSTR_INFO() { PrintTrace("VMX_INSTR_INFO = %x\n", Get_VMX_INSTR_INFO()); }


void    Set_GUEST_ES_LIMIT(uint_t val) { VMCS_WRITE(GUEST_ES_LIMIT,val); } 
uint_t  Get_GUEST_ES_LIMIT() { uint_t rc; VMCS_READ(GUEST_ES_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_ES_LIMIT() { PrintTrace("GUEST_ES_LIMIT = %x\n", Get_GUEST_ES_LIMIT()); }


void    Set_GUEST_CS_LIMIT(uint_t val) { VMCS_WRITE(GUEST_CS_LIMIT,val); } 
uint_t  Get_GUEST_CS_LIMIT() { uint_t rc; VMCS_READ(GUEST_CS_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_CS_LIMIT() { PrintTrace("GUEST_CS_LIMIT = %x\n", Get_GUEST_CS_LIMIT()); }


void    Set_GUEST_SS_LIMIT(uint_t val) { VMCS_WRITE(GUEST_SS_LIMIT,val); } 
uint_t  Get_GUEST_SS_LIMIT() { uint_t rc; VMCS_READ(GUEST_SS_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_SS_LIMIT() { PrintTrace("GUEST_SS_LIMIT = %x\n", Get_GUEST_SS_LIMIT()); }


void    Set_GUEST_DS_LIMIT(uint_t val) { VMCS_WRITE(GUEST_DS_LIMIT,val); } 
uint_t  Get_GUEST_DS_LIMIT() { uint_t rc; VMCS_READ(GUEST_DS_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_DS_LIMIT() { PrintTrace("GUEST_DS_LIMIT = %x\n", Get_GUEST_DS_LIMIT()); }


void    Set_GUEST_FS_LIMIT(uint_t val) { VMCS_WRITE(GUEST_FS_LIMIT,val); } 
uint_t  Get_GUEST_FS_LIMIT() { uint_t rc; VMCS_READ(GUEST_FS_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_FS_LIMIT() { PrintTrace("GUEST_FS_LIMIT = %x\n", Get_GUEST_FS_LIMIT()); }


void    Set_GUEST_GS_LIMIT(uint_t val) { VMCS_WRITE(GUEST_GS_LIMIT,val); } 
uint_t  Get_GUEST_GS_LIMIT() { uint_t rc; VMCS_READ(GUEST_GS_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_GS_LIMIT() { PrintTrace("GUEST_GS_LIMIT = %x\n", Get_GUEST_GS_LIMIT()); }


void    Set_GUEST_LDTR_LIMIT(uint_t val) { VMCS_WRITE(GUEST_LDTR_LIMIT,val); } 
uint_t  Get_GUEST_LDTR_LIMIT() { uint_t rc; VMCS_READ(GUEST_LDTR_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_LDTR_LIMIT() { PrintTrace("GUEST_LDTR_LIMIT = %x\n", Get_GUEST_LDTR_LIMIT()); }


void    Set_GUEST_TR_LIMIT(uint_t val) { VMCS_WRITE(GUEST_TR_LIMIT,val); } 
uint_t  Get_GUEST_TR_LIMIT() { uint_t rc; VMCS_READ(GUEST_TR_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_TR_LIMIT() { PrintTrace("GUEST_TR_LIMIT = %x\n", Get_GUEST_TR_LIMIT()); }


void    Set_GUEST_GDTR_LIMIT(uint_t val) { VMCS_WRITE(GUEST_GDTR_LIMIT,val); } 
uint_t  Get_GUEST_GDTR_LIMIT() { uint_t rc; VMCS_READ(GUEST_GDTR_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_GDTR_LIMIT() { PrintTrace("GUEST_GDTR_LIMIT = %x\n", Get_GUEST_GDTR_LIMIT()); }


void    Set_GUEST_IDTR_LIMIT(uint_t val) { VMCS_WRITE(GUEST_IDTR_LIMIT,val); } 
uint_t  Get_GUEST_IDTR_LIMIT() { uint_t rc; VMCS_READ(GUEST_IDTR_LIMIT,&rc); return rc; }

void    PrintTrace_GUEST_IDTR_LIMIT() { PrintTrace("GUEST_IDTR_LIMIT = %x\n", Get_GUEST_IDTR_LIMIT()); }


void    Set_GUEST_ES_ACCESS(uint_t val) { VMCS_WRITE(GUEST_ES_ACCESS,val); } 
uint_t  Get_GUEST_ES_ACCESS() { uint_t rc; VMCS_READ(GUEST_ES_ACCESS,&rc); return rc; }

void    PrintTrace_GUEST_ES_ACCESS() { PrintTrace("GUEST_ES_ACCESS = %x\n", Get_GUEST_ES_ACCESS()); }


void    Set_GUEST_CS_ACCESS(uint_t val) { VMCS_WRITE(GUEST_CS_ACCESS,val); } 
uint_t  Get_GUEST_CS_ACCESS() { uint_t rc; VMCS_READ(GUEST_CS_ACCESS,&rc); return rc; }

void    PrintTrace_GUEST_CS_ACCESS() { PrintTrace("GUEST_CS_ACCESS = %x\n", Get_GUEST_CS_ACCESS()); }


void    Set_GUEST_SS_ACCESS(uint_t val) { VMCS_WRITE(GUEST_SS_ACCESS,val); } 
uint_t  Get_GUEST_SS_ACCESS() { uint_t rc; VMCS_READ(GUEST_SS_ACCESS,&rc); return rc; }

void    PrintTrace_GUEST_SS_ACCESS() { PrintTrace("GUEST_SS_ACCESS = %x\n", Get_GUEST_SS_ACCESS()); }


void    Set_GUEST_DS_ACCESS(uint_t val) { VMCS_WRITE(GUEST_DS_ACCESS,val); } 
uint_t  Get_GUEST_DS_ACCESS() { uint_t rc; VMCS_READ(GUEST_DS_ACCESS,&rc); return rc; }

void    PrintTrace_GUEST_DS_ACCESS() { PrintTrace("GUEST_DS_ACCESS = %x\n", Get_GUEST_DS_ACCESS()); }


void    Set_GUEST_FS_ACCESS(uint_t val) { VMCS_WRITE(GUEST_FS_ACCESS,val); } 
uint_t  Get_GUEST_FS_ACCESS() { uint_t rc; VMCS_READ(GUEST_FS_ACCESS,&rc); return rc; }

void    PrintTrace_GUEST_FS_ACCESS() { PrintTrace("GUEST_FS_ACCESS = %x\n", Get_GUEST_FS_ACCESS()); }


void    Set_GUEST_GS_ACCESS(uint_t val) { VMCS_WRITE(GUEST_GS_ACCESS,val); } 
uint_t  Get_GUEST_GS_ACCESS() { uint_t rc; VMCS_READ(GUEST_GS_ACCESS,&rc); return rc; }

void    PrintTrace_GUEST_GS_ACCESS() { PrintTrace("GUEST_GS_ACCESS = %x\n", Get_GUEST_GS_ACCESS()); }


void    Set_GUEST_LDTR_ACCESS(uint_t val) { VMCS_WRITE(GUEST_LDTR_ACCESS,val); } 
uint_t  Get_GUEST_LDTR_ACCESS() { uint_t rc; VMCS_READ(GUEST_LDTR_ACCESS,&rc); return rc; }

void    PrintTrace_GUEST_LDTR_ACCESS() { PrintTrace("GUEST_LDTR_ACCESS = %x\n", Get_GUEST_LDTR_ACCESS()); }


void    Set_GUEST_TR_ACCESS(uint_t val) { VMCS_WRITE(GUEST_TR_ACCESS,val); } 
uint_t  Get_GUEST_TR_ACCESS() { uint_t rc; VMCS_READ(GUEST_TR_ACCESS,&rc); return rc; }

void    PrintTrace_GUEST_TR_ACCESS() { PrintTrace("GUEST_TR_ACCESS = %x\n", Get_GUEST_TR_ACCESS()); }


void    Set_GUEST_INT_STATE(uint_t val) { VMCS_WRITE(GUEST_INT_STATE,val); } 
uint_t  Get_GUEST_INT_STATE() { uint_t rc; VMCS_READ(GUEST_INT_STATE,&rc); return rc; }

void    PrintTrace_GUEST_INT_STATE() { PrintTrace("GUEST_INT_STATE = %x\n", Get_GUEST_INT_STATE()); }


void    Set_GUEST_ACTIVITY_STATE(uint_t val) { VMCS_WRITE(GUEST_ACTIVITY_STATE,val); } 
uint_t  Get_GUEST_ACTIVITY_STATE() { uint_t rc; VMCS_READ(GUEST_ACTIVITY_STATE,&rc); return rc; }

void    PrintTrace_GUEST_ACTIVITY_STATE() { PrintTrace("GUEST_ACTIVITY_STATE = %x\n", Get_GUEST_ACTIVITY_STATE()); }


void    Set_GUEST_SMBASE(uint_t val) { VMCS_WRITE(GUEST_SMBASE,val); } 
uint_t  Get_GUEST_SMBASE() { uint_t rc; VMCS_READ(GUEST_SMBASE,&rc); return rc; }

void    PrintTrace_GUEST_SMBASE() { PrintTrace("GUEST_SMBASE = %x\n", Get_GUEST_SMBASE()); }


void    Set_GUEST_IA32_SYSENTER_CS(uint_t val) { VMCS_WRITE(GUEST_IA32_SYSENTER_CS,val); } 
uint_t  Get_GUEST_IA32_SYSENTER_CS() { uint_t rc; VMCS_READ(GUEST_IA32_SYSENTER_CS,&rc); return rc; }

void    PrintTrace_GUEST_IA32_SYSENTER_CS() { PrintTrace("GUEST_IA32_SYSENTER_CS = %x\n", Get_GUEST_IA32_SYSENTER_CS()); }


void    Set_HOST_IA32_SYSENTER_CS(uint_t val) { VMCS_WRITE(HOST_IA32_SYSENTER_CS,val); } 
uint_t  Get_HOST_IA32_SYSENTER_CS() { uint_t rc; VMCS_READ(HOST_IA32_SYSENTER_CS,&rc); return rc; }

void    PrintTrace_HOST_IA32_SYSENTER_CS() { PrintTrace("HOST_IA32_SYSENTER_CS = %x\n", Get_HOST_IA32_SYSENTER_CS()); }


void    Set_CR0_GUEST_HOST_MASK(uint_t val) { VMCS_WRITE(CR0_GUEST_HOST_MASK,val); } 
uint_t  Get_CR0_GUEST_HOST_MASK() { uint_t rc; VMCS_READ(CR0_GUEST_HOST_MASK,&rc); return rc; }

void    PrintTrace_CR0_GUEST_HOST_MASK() { PrintTrace("CR0_GUEST_HOST_MASK = %x\n", Get_CR0_GUEST_HOST_MASK()); }


void    Set_CR4_GUEST_HOST_MASK(uint_t val) { VMCS_WRITE(CR4_GUEST_HOST_MASK,val); } 
uint_t  Get_CR4_GUEST_HOST_MASK() { uint_t rc; VMCS_READ(CR4_GUEST_HOST_MASK,&rc); return rc; }

void    PrintTrace_CR4_GUEST_HOST_MASK() { PrintTrace("CR4_GUEST_HOST_MASK = %x\n", Get_CR4_GUEST_HOST_MASK()); }


void    Set_CR0_READ_SHADOW(uint_t val) { VMCS_WRITE(CR0_READ_SHADOW,val); } 
uint_t  Get_CR0_READ_SHADOW() { uint_t rc; VMCS_READ(CR0_READ_SHADOW,&rc); return rc; }

void    PrintTrace_CR0_READ_SHADOW() { PrintTrace("CR0_READ_SHADOW = %x\n", Get_CR0_READ_SHADOW()); }


void    Set_CR4_READ_SHADOW(uint_t val) { VMCS_WRITE(CR4_READ_SHADOW,val); } 
uint_t  Get_CR4_READ_SHADOW() { uint_t rc; VMCS_READ(CR4_READ_SHADOW,&rc); return rc; }

void    PrintTrace_CR4_READ_SHADOW() { PrintTrace("CR4_READ_SHADOW = %x\n", Get_CR4_READ_SHADOW()); }


void    Set_CR3_TARGET_VALUE_0(uint_t val) { VMCS_WRITE(CR3_TARGET_VALUE_0,val); } 
uint_t  Get_CR3_TARGET_VALUE_0() { uint_t rc; VMCS_READ(CR3_TARGET_VALUE_0,&rc); return rc; }

void    PrintTrace_CR3_TARGET_VALUE_0() { PrintTrace("CR3_TARGET_VALUE_0 = %x\n", Get_CR3_TARGET_VALUE_0()); }


void    Set_CR3_TARGET_VALUE_1(uint_t val) { VMCS_WRITE(CR3_TARGET_VALUE_1,val); } 
uint_t  Get_CR3_TARGET_VALUE_1() { uint_t rc; VMCS_READ(CR3_TARGET_VALUE_1,&rc); return rc; }

void    PrintTrace_CR3_TARGET_VALUE_1() { PrintTrace("CR3_TARGET_VALUE_1 = %x\n", Get_CR3_TARGET_VALUE_1()); }


void    Set_CR3_TARGET_VALUE_2(uint_t val) { VMCS_WRITE(CR3_TARGET_VALUE_2,val); } 
uint_t  Get_CR3_TARGET_VALUE_2() { uint_t rc; VMCS_READ(CR3_TARGET_VALUE_2,&rc); return rc; }

void    PrintTrace_CR3_TARGET_VALUE_2() { PrintTrace("CR3_TARGET_VALUE_2 = %x\n", Get_CR3_TARGET_VALUE_2()); }


void    Set_CR3_TARGET_VALUE_3(uint_t val) { VMCS_WRITE(CR3_TARGET_VALUE_3,val); } 
uint_t  Get_CR3_TARGET_VALUE_3() { uint_t rc; VMCS_READ(CR3_TARGET_VALUE_3,&rc); return rc; }

void    PrintTrace_CR3_TARGET_VALUE_3() { PrintTrace("CR3_TARGET_VALUE_3 = %x\n", Get_CR3_TARGET_VALUE_3()); }


void    Set_EXIT_QUALIFICATION(uint_t val) { VMCS_WRITE(EXIT_QUALIFICATION,val); } 
uint_t  Get_EXIT_QUALIFICATION() { uint_t rc; VMCS_READ(EXIT_QUALIFICATION,&rc); return rc; }

void    PrintTrace_EXIT_QUALIFICATION() { PrintTrace("EXIT_QUALIFICATION = %x\n", Get_EXIT_QUALIFICATION()); }


void    Set_IO_RCX(uint_t val) { VMCS_WRITE(IO_RCX,val); } 
uint_t  Get_IO_RCX() { uint_t rc; VMCS_READ(IO_RCX,&rc); return rc; }

void    PrintTrace_IO_RCX() { PrintTrace("IO_RCX = %x\n", Get_IO_RCX()); }


void    Set_IO_RSI(uint_t val) { VMCS_WRITE(IO_RSI,val); } 
uint_t  Get_IO_RSI() { uint_t rc; VMCS_READ(IO_RSI,&rc); return rc; }

void    PrintTrace_IO_RSI() { PrintTrace("IO_RSI = %x\n", Get_IO_RSI()); }


void    Set_IO_RDI(uint_t val) { VMCS_WRITE(IO_RDI,val); } 
uint_t  Get_IO_RDI() { uint_t rc; VMCS_READ(IO_RDI,&rc); return rc; }

void    PrintTrace_IO_RDI() { PrintTrace("IO_RDI = %x\n", Get_IO_RDI()); }


void    Set_IO_RIP(uint_t val) { VMCS_WRITE(IO_RIP,val); } 
uint_t  Get_IO_RIP() { uint_t rc; VMCS_READ(IO_RIP,&rc); return rc; }

void    PrintTrace_IO_RIP() { PrintTrace("IO_RIP = %x\n", Get_IO_RIP()); }


void    Set_GUEST_LINEAR_ADDR(uint_t val) { VMCS_WRITE(GUEST_LINEAR_ADDR,val); } 
uint_t  Get_GUEST_LINEAR_ADDR() { uint_t rc; VMCS_READ(GUEST_LINEAR_ADDR,&rc); return rc; }

void    PrintTrace_GUEST_LINEAR_ADDR() { PrintTrace("GUEST_LINEAR_ADDR = %x\n", Get_GUEST_LINEAR_ADDR()); }


void    Set_GUEST_CR0(uint_t val) { VMCS_WRITE(GUEST_CR0,val); } 
uint_t  Get_GUEST_CR0() { uint_t rc; VMCS_READ(GUEST_CR0,&rc); return rc; }

void    PrintTrace_GUEST_CR0() { PrintTrace("GUEST_CR0 = %x\n", Get_GUEST_CR0()); }


void    Set_GUEST_CR3(uint_t val) { VMCS_WRITE(GUEST_CR3,val); } 
uint_t  Get_GUEST_CR3() { uint_t rc; VMCS_READ(GUEST_CR3,&rc); return rc; }

void    PrintTrace_GUEST_CR3() { PrintTrace("GUEST_CR3 = %x\n", Get_GUEST_CR3()); }


void    Set_GUEST_CR4(uint_t val) { VMCS_WRITE(GUEST_CR4,val); } 
uint_t  Get_GUEST_CR4() { uint_t rc; VMCS_READ(GUEST_CR4,&rc); return rc; }

void    PrintTrace_GUEST_CR4() { PrintTrace("GUEST_CR4 = %x\n", Get_GUEST_CR4()); }


void    Set_GUEST_ES_BASE(uint_t val) { VMCS_WRITE(GUEST_ES_BASE,val); } 
uint_t  Get_GUEST_ES_BASE() { uint_t rc; VMCS_READ(GUEST_ES_BASE,&rc); return rc; }

void    PrintTrace_GUEST_ES_BASE() { PrintTrace("GUEST_ES_BASE = %x\n", Get_GUEST_ES_BASE()); }


void    Set_GUEST_CS_BASE(uint_t val) { VMCS_WRITE(GUEST_CS_BASE,val); } 
uint_t  Get_GUEST_CS_BASE() { uint_t rc; VMCS_READ(GUEST_CS_BASE,&rc); return rc; }

void    PrintTrace_GUEST_CS_BASE() { PrintTrace("GUEST_CS_BASE = %x\n", Get_GUEST_CS_BASE()); }


void    Set_GUEST_SS_BASE(uint_t val) { VMCS_WRITE(GUEST_SS_BASE,val); } 
uint_t  Get_GUEST_SS_BASE() { uint_t rc; VMCS_READ(GUEST_SS_BASE,&rc); return rc; }

void    PrintTrace_GUEST_SS_BASE() { PrintTrace("GUEST_SS_BASE = %x\n", Get_GUEST_SS_BASE()); }


void    Set_GUEST_DS_BASE(uint_t val) { VMCS_WRITE(GUEST_DS_BASE,val); } 
uint_t  Get_GUEST_DS_BASE() { uint_t rc; VMCS_READ(GUEST_DS_BASE,&rc); return rc; }

void    PrintTrace_GUEST_DS_BASE() { PrintTrace("GUEST_DS_BASE = %x\n", Get_GUEST_DS_BASE()); }


void    Set_GUEST_FS_BASE(uint_t val) { VMCS_WRITE(GUEST_FS_BASE,val); } 
uint_t  Get_GUEST_FS_BASE() { uint_t rc; VMCS_READ(GUEST_FS_BASE,&rc); return rc; }

void    PrintTrace_GUEST_FS_BASE() { PrintTrace("GUEST_FS_BASE = %x\n", Get_GUEST_FS_BASE()); }


void    Set_GUEST_GS_BASE(uint_t val) { VMCS_WRITE(GUEST_GS_BASE,val); } 
uint_t  Get_GUEST_GS_BASE() { uint_t rc; VMCS_READ(GUEST_GS_BASE,&rc); return rc; }

void    PrintTrace_GUEST_GS_BASE() { PrintTrace("GUEST_GS_BASE = %x\n", Get_GUEST_GS_BASE()); }


void    Set_GUEST_LDTR_BASE(uint_t val) { VMCS_WRITE(GUEST_LDTR_BASE,val); } 
uint_t  Get_GUEST_LDTR_BASE() { uint_t rc; VMCS_READ(GUEST_LDTR_BASE,&rc); return rc; }

void    PrintTrace_GUEST_LDTR_BASE() { PrintTrace("GUEST_LDTR_BASE = %x\n", Get_GUEST_LDTR_BASE()); }


void    Set_GUEST_TR_BASE(uint_t val) { VMCS_WRITE(GUEST_TR_BASE,val); } 
uint_t  Get_GUEST_TR_BASE() { uint_t rc; VMCS_READ(GUEST_TR_BASE,&rc); return rc; }

void    PrintTrace_GUEST_TR_BASE() { PrintTrace("GUEST_TR_BASE = %x\n", Get_GUEST_TR_BASE()); }


void    Set_GUEST_GDTR_BASE(uint_t val) { VMCS_WRITE(GUEST_GDTR_BASE,val); } 
uint_t  Get_GUEST_GDTR_BASE() { uint_t rc; VMCS_READ(GUEST_GDTR_BASE,&rc); return rc; }

void    PrintTrace_GUEST_GDTR_BASE() { PrintTrace("GUEST_GDTR_BASE = %x\n", Get_GUEST_GDTR_BASE()); }


void    Set_GUEST_IDTR_BASE(uint_t val) { VMCS_WRITE(GUEST_IDTR_BASE,val); } 
uint_t  Get_GUEST_IDTR_BASE() { uint_t rc; VMCS_READ(GUEST_IDTR_BASE,&rc); return rc; }

void    PrintTrace_GUEST_IDTR_BASE() { PrintTrace("GUEST_IDTR_BASE = %x\n", Get_GUEST_IDTR_BASE()); }


void    Set_GUEST_DR7(uint_t val) { VMCS_WRITE(GUEST_DR7,val); } 
uint_t  Get_GUEST_DR7() { uint_t rc; VMCS_READ(GUEST_DR7,&rc); return rc; }

void    PrintTrace_GUEST_DR7() { PrintTrace("GUEST_DR7 = %x\n", Get_GUEST_DR7()); }


void    Set_GUEST_RSP(uint_t val) { VMCS_WRITE(GUEST_RSP,val); } 
uint_t  Get_GUEST_RSP() { uint_t rc; VMCS_READ(GUEST_RSP,&rc); return rc; }

void    PrintTrace_GUEST_RSP() { PrintTrace("GUEST_RSP = %x\n", Get_GUEST_RSP()); }


void    Set_GUEST_RIP(uint_t val) { VMCS_WRITE(GUEST_RIP,val); } 
uint_t  Get_GUEST_RIP() { uint_t rc; VMCS_READ(GUEST_RIP,&rc); return rc; }

void    PrintTrace_GUEST_RIP() { PrintTrace("GUEST_RIP = %x\n", Get_GUEST_RIP()); }


void    Set_GUEST_RFLAGS(uint_t val) { VMCS_WRITE(GUEST_RFLAGS,val); } 
uint_t  Get_GUEST_RFLAGS() { uint_t rc; VMCS_READ(GUEST_RFLAGS,&rc); return rc; }

void    PrintTrace_GUEST_RFLAGS() { PrintTrace("GUEST_RFLAGS = %x\n", Get_GUEST_RFLAGS()); }


void    Set_GUEST_PENDING_DEBUG_EXCS(uint_t val) { VMCS_WRITE(GUEST_PENDING_DEBUG_EXCS,val); } 
uint_t  Get_GUEST_PENDING_DEBUG_EXCS() { uint_t rc; VMCS_READ(GUEST_PENDING_DEBUG_EXCS,&rc); return rc; }

void    PrintTrace_GUEST_PENDING_DEBUG_EXCS() { PrintTrace("GUEST_PENDING_DEBUG_EXCS = %x\n", Get_GUEST_PENDING_DEBUG_EXCS()); }


void    Set_GUEST_IA32_SYSENTER_ESP(uint_t val) { VMCS_WRITE(GUEST_IA32_SYSENTER_ESP,val); } 
uint_t  Get_GUEST_IA32_SYSENTER_ESP() { uint_t rc; VMCS_READ(GUEST_IA32_SYSENTER_ESP,&rc); return rc; }

void    PrintTrace_GUEST_IA32_SYSENTER_ESP() { PrintTrace("GUEST_IA32_SYSENTER_ESP = %x\n", Get_GUEST_IA32_SYSENTER_ESP()); }


void    Set_GUEST_IA32_SYSENTER_EIP(uint_t val) { VMCS_WRITE(GUEST_IA32_SYSENTER_EIP,val); } 
uint_t  Get_GUEST_IA32_SYSENTER_EIP() { uint_t rc; VMCS_READ(GUEST_IA32_SYSENTER_EIP,&rc); return rc; }

void    PrintTrace_GUEST_IA32_SYSENTER_EIP() { PrintTrace("GUEST_IA32_SYSENTER_EIP = %x\n", Get_GUEST_IA32_SYSENTER_EIP()); }


void    Set_HOST_CR0(uint_t val) { VMCS_WRITE(HOST_CR0,val); } 
uint_t  Get_HOST_CR0() { uint_t rc; VMCS_READ(HOST_CR0,&rc); return rc; }

void    PrintTrace_HOST_CR0() { PrintTrace("HOST_CR0 = %x\n", Get_HOST_CR0()); }


void    Set_HOST_CR3(uint_t val) { VMCS_WRITE(HOST_CR3,val); } 
uint_t  Get_HOST_CR3() { uint_t rc; VMCS_READ(HOST_CR3,&rc); return rc; }

void    PrintTrace_HOST_CR3() { PrintTrace("HOST_CR3 = %x\n", Get_HOST_CR3()); }


void    Set_HOST_CR4(uint_t val) { VMCS_WRITE(HOST_CR4,val); } 
uint_t  Get_HOST_CR4() { uint_t rc; VMCS_READ(HOST_CR4,&rc); return rc; }

void    PrintTrace_HOST_CR4() { PrintTrace("HOST_CR4 = %x\n", Get_HOST_CR4()); }


void    Set_HOST_FS_BASE(uint_t val) { VMCS_WRITE(HOST_FS_BASE,val); } 
uint_t  Get_HOST_FS_BASE() { uint_t rc; VMCS_READ(HOST_FS_BASE,&rc); return rc; }

void    PrintTrace_HOST_FS_BASE() { PrintTrace("HOST_FS_BASE = %x\n", Get_HOST_FS_BASE()); }


void    Set_HOST_GS_BASE(uint_t val) { VMCS_WRITE(HOST_GS_BASE,val); } 
uint_t  Get_HOST_GS_BASE() { uint_t rc; VMCS_READ(HOST_GS_BASE,&rc); return rc; }

void    PrintTrace_HOST_GS_BASE() { PrintTrace("HOST_GS_BASE = %x\n", Get_HOST_GS_BASE()); }


void    Set_HOST_TR_BASE(uint_t val) { VMCS_WRITE(HOST_TR_BASE,val); } 
uint_t  Get_HOST_TR_BASE() { uint_t rc; VMCS_READ(HOST_TR_BASE,&rc); return rc; }

void    PrintTrace_HOST_TR_BASE() { PrintTrace("HOST_TR_BASE = %x\n", Get_HOST_TR_BASE()); }


void    Set_HOST_GDTR_BASE(uint_t val) { VMCS_WRITE(HOST_GDTR_BASE,val); } 
uint_t  Get_HOST_GDTR_BASE() { uint_t rc; VMCS_READ(HOST_GDTR_BASE,&rc); return rc; }

void    PrintTrace_HOST_GDTR_BASE() { PrintTrace("HOST_GDTR_BASE = %x\n", Get_HOST_GDTR_BASE()); }


void    Set_HOST_IDTR_BASE(uint_t val) { VMCS_WRITE(HOST_IDTR_BASE,val); } 
uint_t  Get_HOST_IDTR_BASE() { uint_t rc; VMCS_READ(HOST_IDTR_BASE,&rc); return rc; }

void    PrintTrace_HOST_IDTR_BASE() { PrintTrace("HOST_IDTR_BASE = %x\n", Get_HOST_IDTR_BASE()); }


void    Set_HOST_IA32_SYSENTER_ESP(uint_t val) { VMCS_WRITE(HOST_IA32_SYSENTER_ESP,val); } 
uint_t  Get_HOST_IA32_SYSENTER_ESP() { uint_t rc; VMCS_READ(HOST_IA32_SYSENTER_ESP,&rc); return rc; }

void    PrintTrace_HOST_IA32_SYSENTER_ESP() { PrintTrace("HOST_IA32_SYSENTER_ESP = %x\n", Get_HOST_IA32_SYSENTER_ESP()); }


void    Set_HOST_IA32_SYSENTER_EIP(uint_t val) { VMCS_WRITE(HOST_IA32_SYSENTER_EIP,val); } 
uint_t  Get_HOST_IA32_SYSENTER_EIP() { uint_t rc; VMCS_READ(HOST_IA32_SYSENTER_EIP,&rc); return rc; }

void    PrintTrace_HOST_IA32_SYSENTER_EIP() { PrintTrace("HOST_IA32_SYSENTER_EIP = %x\n", Get_HOST_IA32_SYSENTER_EIP()); }


void    Set_HOST_RSP(uint_t val) { VMCS_WRITE(HOST_RSP,val); } 
uint_t  Get_HOST_RSP() { uint_t rc; VMCS_READ(HOST_RSP,&rc); return rc; }

void    PrintTrace_HOST_RSP() { PrintTrace("HOST_RSP = %x\n", Get_HOST_RSP()); }


void    Set_HOST_RIP(uint_t val) { VMCS_WRITE(HOST_RIP,val); } 
uint_t  Get_HOST_RIP() { uint_t rc; VMCS_READ(HOST_RIP,&rc); return rc; }

void    PrintTrace_HOST_RIP() { PrintTrace("HOST_RIP = %x\n", Get_HOST_RIP()); }

void PrintTrace_VMCS_ALL() {

  PrintTrace("==>Guest State Area\n");
  PrintTrace("==>==> Guest Register State\n");
  PrintTrace_GUEST_CR0();
  PrintTrace_GUEST_CR3();
  PrintTrace_GUEST_CR4();
  PrintTrace_GUEST_DR7();
  PrintTrace_GUEST_RSP();
  PrintTrace_GUEST_RIP();
  PrintTrace_GUEST_RFLAGS();
  PrintTrace_VMCS_GUEST_CS_SELECTOR();
  PrintTrace_VMCS_GUEST_SS_SELECTOR();
  PrintTrace_VMCS_GUEST_DS_SELECTOR();
  PrintTrace_VMCS_GUEST_ES_SELECTOR();
  PrintTrace_VMCS_GUEST_FS_SELECTOR();
  PrintTrace_VMCS_GUEST_GS_SELECTOR();
  PrintTrace_VMCS_GUEST_LDTR_SELECTOR();
  PrintTrace_VMCS_GUEST_TR_SELECTOR();
  PrintTrace_GUEST_CS_BASE();
  PrintTrace_GUEST_SS_BASE();
  PrintTrace_GUEST_DS_BASE();
  PrintTrace_GUEST_ES_BASE();
  PrintTrace_GUEST_FS_BASE();
  PrintTrace_GUEST_GS_BASE();
  PrintTrace_GUEST_LDTR_BASE();
  PrintTrace_GUEST_TR_BASE();
  PrintTrace_GUEST_CS_LIMIT();
  PrintTrace_GUEST_SS_LIMIT();
  PrintTrace_GUEST_DS_LIMIT();
  PrintTrace_GUEST_ES_LIMIT();
  PrintTrace_GUEST_FS_LIMIT();
  PrintTrace_GUEST_GS_LIMIT();
  PrintTrace_GUEST_LDTR_LIMIT();
  PrintTrace_GUEST_TR_LIMIT();
  PrintTrace_GUEST_ES_ACCESS();
  PrintTrace_GUEST_CS_ACCESS();
  PrintTrace_GUEST_SS_ACCESS();
  PrintTrace_GUEST_DS_ACCESS();
  PrintTrace_GUEST_FS_ACCESS();
  PrintTrace_GUEST_GS_ACCESS();
  PrintTrace_GUEST_LDTR_ACCESS();
  PrintTrace_GUEST_TR_ACCESS();
  PrintTrace_GUEST_GDTR_BASE();
  PrintTrace_GUEST_IDTR_BASE();
  PrintTrace_GUEST_GDTR_LIMIT();
  PrintTrace_GUEST_IDTR_LIMIT();
  PrintTrace_GUEST_IA32_DEBUGCTL();
  PrintTrace_GUEST_IA32_DEBUGCTL_HIGH();
  PrintTrace_GUEST_IA32_SYSENTER_CS();
  PrintTrace_GUEST_IA32_SYSENTER_ESP();
  PrintTrace_GUEST_IA32_SYSENTER_EIP();
  PrintTrace_GUEST_SMBASE();

  PrintTrace("==>==> Guest Non-Register State\n");
  PrintTrace_GUEST_ACTIVITY_STATE();
  PrintTrace_GUEST_INT_STATE();
  PrintTrace_GUEST_PENDING_DEBUG_EXCS();
  PrintTrace_VMCS_LINK_PTR();
  PrintTrace_VMCS_LINK_PTR_HIGH();

  PrintTrace("\n==> Host State Area\n");
  PrintTrace_HOST_CR0();
  PrintTrace_HOST_CR3();
  PrintTrace_HOST_CR4();
  PrintTrace_HOST_RSP();
  PrintTrace_HOST_RIP();
  PrintTrace_VMCS_HOST_CS_SELECTOR();
  PrintTrace_VMCS_HOST_SS_SELECTOR();
  PrintTrace_VMCS_HOST_DS_SELECTOR();
  PrintTrace_VMCS_HOST_ES_SELECTOR();
  PrintTrace_VMCS_HOST_FS_SELECTOR();
  PrintTrace_VMCS_HOST_GS_SELECTOR();
  PrintTrace_VMCS_HOST_TR_SELECTOR();
  PrintTrace_HOST_FS_BASE();
  PrintTrace_HOST_GS_BASE();
  PrintTrace_HOST_TR_BASE();
  PrintTrace_HOST_GDTR_BASE();
  PrintTrace_HOST_IDTR_BASE();
  PrintTrace_HOST_IA32_SYSENTER_CS();
  PrintTrace_HOST_IA32_SYSENTER_ESP();
  PrintTrace_HOST_IA32_SYSENTER_EIP();


  PrintTrace("\n==> VM-Execution Controls:\n");
  PrintTrace_PIN_VM_EXEC_CTRLS();
  PrintTrace_PROC_VM_EXEC_CTRLS();
  PrintTrace_EXCEPTION_BITMAP();
  PrintTrace_PAGE_FAULT_ERROR_MASK();
  PrintTrace_PAGE_FAULT_ERROR_MATCH();
  PrintTrace_IO_BITMAP_A_ADDR();
  PrintTrace_IO_BITMAP_A_ADDR_HIGH();
  PrintTrace_IO_BITMAP_B_ADDR();
  PrintTrace_IO_BITMAP_B_ADDR_HIGH();
  PrintTrace_TSC_OFFSET();
  PrintTrace_TSC_OFFSET_HIGH();
  PrintTrace_CR0_GUEST_HOST_MASK();
  PrintTrace_CR0_READ_SHADOW();
  PrintTrace_CR4_GUEST_HOST_MASK();
  PrintTrace_CR4_READ_SHADOW();
  PrintTrace_CR3_TARGET_COUNT();
  PrintTrace_CR3_TARGET_VALUE_0();
  PrintTrace_CR3_TARGET_VALUE_1();
  PrintTrace_CR3_TARGET_VALUE_2();
  PrintTrace_CR3_TARGET_VALUE_3();
  PrintTrace_VIRT_APIC_PAGE_ADDR();
  PrintTrace_VIRT_APIC_PAGE_ADDR_HIGH();
  PrintTrace_TPR_THRESHOLD();
  PrintTrace_MSR_BITMAPS();
  PrintTrace_MSR_BITMAPS_HIGH();
  PrintTrace_VMCS_EXEC_PTR();
  PrintTrace_VMCS_EXEC_PTR_HIGH();

  PrintTrace("\n==> VM Exit Controls\n");
  PrintTrace_VM_EXIT_CTRLS();
  PrintTrace_VM_EXIT_MSR_STORE_COUNT();
  PrintTrace_VM_EXIT_MSR_STORE_ADDR();
  PrintTrace_VM_EXIT_MSR_STORE_ADDR_HIGH();
  PrintTrace_VM_EXIT_MSR_LOAD_COUNT();
  PrintTrace_VM_EXIT_MSR_LOAD_ADDR();
  PrintTrace_VM_EXIT_MSR_LOAD_ADDR_HIGH();

  PrintTrace("\n==> VM Entry Controls\n");
  PrintTrace_VM_ENTRY_CTRLS();
  PrintTrace_VM_ENTRY_MSR_LOAD_COUNT();
  PrintTrace_VM_ENTRY_MSR_LOAD_ADDR();
  PrintTrace_VM_ENTRY_MSR_LOAD_ADDR_HIGH();
  PrintTrace_VM_ENTRY_INT_INFO_FIELD();
  PrintTrace_VM_ENTRY_EXCEPTION_ERROR();
  PrintTrace_VM_ENTRY_INSTR_LENGTH();

  PrintTrace("\n==> VM Exit Info\n");
  PrintTrace_EXIT_REASON();
  PrintTrace_EXIT_QUALIFICATION();
  PrintTrace_VM_EXIT_INT_INFO();
  PrintTrace_VM_EXIT_INT_ERROR();
  PrintTrace_IDT_VECTOR_INFO();
  PrintTrace_IDT_VECTOR_ERROR();
  PrintTrace_VM_EXIT_INSTR_LENGTH();
  PrintTrace_GUEST_LINEAR_ADDR();
  PrintTrace_VMX_INSTR_INFO();
  PrintTrace_IO_RCX();
  PrintTrace_IO_RSI();
  PrintTrace_IO_RDI();
  PrintTrace_IO_RIP();
  PrintTrace_VM_INSTR_ERROR();
  PrintTrace("\n");
}
