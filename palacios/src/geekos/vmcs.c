#include <geekos/vmcs.h>
#include <geekos/serial.h>




char *exception_names[] = {
  "#DE (Divide Error)",
  "#DB (Reserved)",
  "NMI",
  "#BP (Breakpoint)",
  "#OF (Overflow)",
  "#BR (BOUND Range Exceeded)",
  "#UD (Invalid Opcode)",
  "#NM (No Math Coprocessor)",
  "#DF (Double Fault)",
  "Coprocessor Segment Overrun",
  "#TS (Invalid TSS)",
  "#NP (Segment Not Present)",
  "#SS (Stack Segment Fault)",
  "#GP (General Protection Fault)",
  "#PF (Page Fault)",
  "(Reserved - 15)",
  "#MF (Math Fault x87)",
  "#AC (Alignment Check)",
  "#MC (Machine Check)",
  "#XF (SIMD FP Exception)",
  "(Reserved - 20)",
  "(Reserved - 21)",
  "(Reserved - 22)",
  "(Reserved - 23)",
  "(Reserved - 24)",
  "(Reserved - 25)",
  "(Reserved - 26)",
  "(Reserved - 27)",
  "(Reserved - 28)",
  "(Reserved - 29)",
  "(Reserved - 30)",
  "(Reserved - 31)",
  "USER 32",
  "USER 33",
  "USER 34",
  "USER 35",
  "USER 36",
  "USER 37",
  "USER 38",
  "USER 39",
  "USER 40",
  "USER 41",
  "USER 42",
  "USER 43",
  "USER 44",
  "USER 45",
  "USER 46",
  "USER 47",
  "USER 48",
  "USER 49",
  "USER 50",
  "USER 51",
  "USER 52",
  "USER 53",
  "USER 54",
  "USER 55",
  "USER 56",
  "USER 57",
  "USER 58",
  "USER 59",
  "USER 60",
  "USER 61",
  "USER 62",
  "USER 63",
  "USER 64",
  "USER 65",
  "USER 66",
  "USER 67",
  "USER 68",
  "USER 69",
  "USER 70",
  "USER 71",
  "USER 72",
  "USER 73",
  "USER 74",
  "USER 75",
  "USER 76",
  "USER 77",
  "USER 78",
  "USER 79",
  "USER 80",
  "USER 81",
  "USER 82",
  "USER 83",
  "USER 84",
  "USER 85",
  "USER 86",
  "USER 87",
  "USER 88",
  "USER 89",
  "USER 90",
  "USER 91",
  "USER 92",
  "USER 93",
  "USER 94",
  "USER 95",
  "USER 96",
  "USER 97",
  "USER 98",
  "USER 99",
  "USER 100",
  "USER 101",
  "USER 102",
  "USER 103",
  "USER 104",
  "USER 105",
  "USER 106",
  "USER 107",
  "USER 108",
  "USER 109",
  "USER 110",
  "USER 111",
  "USER 112",
  "USER 113",
  "USER 114",
  "USER 115",
  "USER 116",
  "USER 117",
  "USER 118",
  "USER 119",
  "USER 120",
  "USER 121",
  "USER 122",
  "USER 123",
  "USER 124",
  "USER 125",
  "USER 126",
  "USER 127",
  "USER 128",
  "USER 129",
  "USER 130",
  "USER 131",
  "USER 132",
  "USER 133",
  "USER 134",
  "USER 135",
  "USER 136",
  "USER 137",
  "USER 138",
  "USER 139",
  "USER 140",
  "USER 141",
  "USER 142",
  "USER 143",
  "USER 144",
  "USER 145",
  "USER 146",
  "USER 147",
  "USER 148",
  "USER 149",
  "USER 150",
  "USER 151",
  "USER 152",
  "USER 153",
  "USER 154",
  "USER 155",
  "USER 156",
  "USER 157",
  "USER 158",
  "USER 159",
  "USER 160",
  "USER 161",
  "USER 162",
  "USER 163",
  "USER 164",
  "USER 165",
  "USER 166",
  "USER 167",
  "USER 168",
  "USER 169",
  "USER 170",
  "USER 171",
  "USER 172",
  "USER 173",
  "USER 174",
  "USER 175",
  "USER 176",
  "USER 177",
  "USER 178",
  "USER 179",
  "USER 180",
  "USER 181",
  "USER 182",
  "USER 183",
  "USER 184",
  "USER 185",
  "USER 186",
  "USER 187",
  "USER 188",
  "USER 189",
  "USER 190",
  "USER 191",
  "USER 192",
  "USER 193",
  "USER 194",
  "USER 195",
  "USER 196",
  "USER 197",
  "USER 198",
  "USER 199",
  "USER 200",
  "USER 201",
  "USER 202",
  "USER 203",
  "USER 204",
  "USER 205",
  "USER 206",
  "USER 207",
  "USER 208",
  "USER 209",
  "USER 210",
  "USER 211",
  "USER 212",
  "USER 213",
  "USER 214",
  "USER 215",
  "USER 216",
  "USER 217",
  "USER 218",
  "USER 219",
  "USER 220",
  "USER 221",
  "USER 222",
  "USER 223",
  "USER 224",
  "USER 225",
  "USER 226",
  "USER 227",
  "USER 228",
  "USER 229",
  "USER 230",
  "USER 231",
  "USER 232",
  "USER 233",
  "USER 234",
  "USER 235",
  "USER 236",
  "USER 237",
  "USER 238",
  "USER 239",
  "USER 240",
  "USER 241",
  "USER 242",
  "USER 243",
  "USER 244",
  "USER 245",
  "USER 246",
  "USER 247",
  "USER 248",
  "USER 249",
  "USER 250",
  "USER 251",
  "USER 252",
  "USER 253",
  "USER 254",
  "USER 255",
};  

char *exception_type_names[] = {
  "External Interrupt",
  "NOT USED",
  "NMI",
  "Hardware Exception",
  "NOT USED",
  "NOT USED",
  "Software Exception",
  "NOT USED"
};

//
// Ignores "HIGH" addresses - 32 bit only for now
//


#define CHK_VMCS_READ(tag, val) {if (VMCS_READ(tag, val) != 0) return -1;}
#define CHK_VMCS_WRITE(tag, val) {if (VMCS_WRITE(tag, val) != 0) return -1;}



int CopyOutVMCSGuestStateArea(struct VMCSGuestStateArea *p) {
  CHK_VMCS_READ(GUEST_CR0, &(p->cr0));
  CHK_VMCS_READ(GUEST_CR3, &(p->cr3));
  CHK_VMCS_READ(GUEST_CR4, &(p->cr4));
  CHK_VMCS_READ(GUEST_DR7, &(p->dr7));
  CHK_VMCS_READ(GUEST_RSP, &(p->rsp));
  CHK_VMCS_READ(GUEST_RIP, &(p->rip));
  CHK_VMCS_READ(GUEST_RFLAGS, &(p->rflags));
  CHK_VMCS_READ(VMCS_GUEST_CS_SELECTOR, &(p->cs.selector));
  CHK_VMCS_READ(VMCS_GUEST_SS_SELECTOR, &(p->ss.selector));
  CHK_VMCS_READ(VMCS_GUEST_DS_SELECTOR, &(p->ds.selector));
  CHK_VMCS_READ(VMCS_GUEST_ES_SELECTOR, &(p->es.selector));
  CHK_VMCS_READ(VMCS_GUEST_FS_SELECTOR, &(p->fs.selector));
  CHK_VMCS_READ(VMCS_GUEST_GS_SELECTOR, &(p->gs.selector));
  CHK_VMCS_READ(VMCS_GUEST_LDTR_SELECTOR, &(p->ldtr.selector));
  CHK_VMCS_READ(VMCS_GUEST_TR_SELECTOR, &(p->tr.selector));
  CHK_VMCS_READ(GUEST_CS_BASE, &(p->cs.baseAddr));
  CHK_VMCS_READ(GUEST_SS_BASE, &(p->ss.baseAddr));
  CHK_VMCS_READ(GUEST_DS_BASE, &(p->ds.baseAddr));
  CHK_VMCS_READ(GUEST_ES_BASE, &(p->es.baseAddr));
  CHK_VMCS_READ(GUEST_FS_BASE, &(p->fs.baseAddr));
  CHK_VMCS_READ(GUEST_GS_BASE, &(p->gs.baseAddr));
  CHK_VMCS_READ(GUEST_LDTR_BASE, &(p->ldtr.baseAddr));
  CHK_VMCS_READ(GUEST_TR_BASE, &(p->tr.baseAddr));
  CHK_VMCS_READ(GUEST_CS_LIMIT, &(p->cs.limit));
  CHK_VMCS_READ(GUEST_SS_LIMIT, &(p->ss.limit));
  CHK_VMCS_READ(GUEST_DS_LIMIT, &(p->ds.limit));
  CHK_VMCS_READ(GUEST_ES_LIMIT, &(p->es.limit));
  CHK_VMCS_READ(GUEST_FS_LIMIT, &(p->fs.limit));
  CHK_VMCS_READ(GUEST_GS_LIMIT, &(p->gs.limit));
  CHK_VMCS_READ(GUEST_LDTR_LIMIT, &(p->ldtr.limit));
  CHK_VMCS_READ(GUEST_TR_LIMIT, &(p->tr.limit));
  CHK_VMCS_READ(GUEST_CS_ACCESS, &(p->cs.access));
  CHK_VMCS_READ(GUEST_SS_ACCESS, &(p->ss.access));
  CHK_VMCS_READ(GUEST_DS_ACCESS, &(p->ds.access));
  CHK_VMCS_READ(GUEST_ES_ACCESS, &(p->es.access));
  CHK_VMCS_READ(GUEST_FS_ACCESS, &(p->fs.access));
  CHK_VMCS_READ(GUEST_GS_ACCESS, &(p->gs.access));
  CHK_VMCS_READ(GUEST_LDTR_ACCESS, &(p->ldtr.access));
  CHK_VMCS_READ(GUEST_TR_ACCESS, &(p->tr.access));
  CHK_VMCS_READ(GUEST_GDTR_BASE, &(p->gdtr.baseAddr));
  CHK_VMCS_READ(GUEST_IDTR_BASE, &(p->idtr.baseAddr));
  CHK_VMCS_READ(GUEST_GDTR_LIMIT, &(p->gdtr.limit));
  CHK_VMCS_READ(GUEST_IDTR_LIMIT, &(p->idtr.limit));
  CHK_VMCS_READ(GUEST_IA32_DEBUGCTL, &(p->dbg_ctrl));
  CHK_VMCS_READ(GUEST_IA32_DEBUGCTL_HIGH, ((char *)&(p->dbg_ctrl)) + 4);
  CHK_VMCS_READ(GUEST_IA32_SYSENTER_CS, &(p->sysenter_cs));
  CHK_VMCS_READ(GUEST_IA32_SYSENTER_ESP, &(p->sysenter_esp));
  CHK_VMCS_READ(GUEST_IA32_SYSENTER_EIP, &(p->sysenter_eip));
  CHK_VMCS_READ(GUEST_SMBASE, &(p->smbase));

  CHK_VMCS_READ(GUEST_ACTIVITY_STATE, &(p->activity));
  CHK_VMCS_READ(GUEST_INT_STATE, &(p->interrupt_state));
  CHK_VMCS_READ(GUEST_PENDING_DEBUG_EXCS, &(p->pending_dbg_exceptions));
  CHK_VMCS_READ(VMCS_LINK_PTR, &(p->vmcs_link));
  CHK_VMCS_READ(VMCS_LINK_PTR_HIGH, ((char *)&(p->vmcs_link)) + 4);
  return 0;
}


int CopyInVMCSGuestStateArea(struct VMCSGuestStateArea *p) {
  CHK_VMCS_WRITE(GUEST_CR0, &(p->cr0));
  CHK_VMCS_WRITE(GUEST_CR3, &(p->cr3));
  CHK_VMCS_WRITE(GUEST_CR4, &(p->cr4));
  CHK_VMCS_WRITE(GUEST_DR7, &(p->dr7));
  CHK_VMCS_WRITE(GUEST_RSP, &(p->rsp));
  CHK_VMCS_WRITE(GUEST_RIP, &(p->rip));
  CHK_VMCS_WRITE(GUEST_RFLAGS, &(p->rflags));
  CHK_VMCS_WRITE(VMCS_GUEST_CS_SELECTOR, &(p->cs.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_SS_SELECTOR, &(p->ss.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_DS_SELECTOR, &(p->ds.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_ES_SELECTOR, &(p->es.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_FS_SELECTOR, &(p->fs.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_GS_SELECTOR, &(p->gs.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_LDTR_SELECTOR, &(p->ldtr.selector));
  CHK_VMCS_WRITE(VMCS_GUEST_TR_SELECTOR, &(p->tr.selector));
  CHK_VMCS_WRITE(GUEST_CS_BASE, &(p->cs.baseAddr));
  CHK_VMCS_WRITE(GUEST_SS_BASE, &(p->ss.baseAddr));
  CHK_VMCS_WRITE(GUEST_DS_BASE, &(p->ds.baseAddr));
  CHK_VMCS_WRITE(GUEST_ES_BASE, &(p->es.baseAddr));
  CHK_VMCS_WRITE(GUEST_FS_BASE, &(p->fs.baseAddr));
  CHK_VMCS_WRITE(GUEST_GS_BASE, &(p->gs.baseAddr));
  CHK_VMCS_WRITE(GUEST_LDTR_BASE, &(p->ldtr.baseAddr));
  CHK_VMCS_WRITE(GUEST_TR_BASE, &(p->tr.baseAddr));
  CHK_VMCS_WRITE(GUEST_CS_LIMIT, &(p->cs.limit));
  CHK_VMCS_WRITE(GUEST_SS_LIMIT, &(p->ss.limit));
  CHK_VMCS_WRITE(GUEST_DS_LIMIT, &(p->ds.limit));
  CHK_VMCS_WRITE(GUEST_ES_LIMIT, &(p->es.limit));
  CHK_VMCS_WRITE(GUEST_FS_LIMIT, &(p->fs.limit));
  CHK_VMCS_WRITE(GUEST_GS_LIMIT, &(p->gs.limit));
  CHK_VMCS_WRITE(GUEST_LDTR_LIMIT, &(p->ldtr.limit));
  CHK_VMCS_WRITE(GUEST_TR_LIMIT, &(p->tr.limit));
  CHK_VMCS_WRITE(GUEST_CS_ACCESS, &(p->cs.access));
  CHK_VMCS_WRITE(GUEST_SS_ACCESS, &(p->ss.access));
  CHK_VMCS_WRITE(GUEST_DS_ACCESS, &(p->ds.access));
  CHK_VMCS_WRITE(GUEST_ES_ACCESS, &(p->es.access));
  CHK_VMCS_WRITE(GUEST_FS_ACCESS, &(p->fs.access));
  CHK_VMCS_WRITE(GUEST_GS_ACCESS, &(p->gs.access));
  CHK_VMCS_WRITE(GUEST_LDTR_ACCESS, &(p->ldtr.access));
  CHK_VMCS_WRITE(GUEST_TR_ACCESS, &(p->tr.access));
  CHK_VMCS_WRITE(GUEST_GDTR_BASE, &(p->gdtr.baseAddr));
  CHK_VMCS_WRITE(GUEST_IDTR_BASE, &(p->idtr.baseAddr));
  CHK_VMCS_WRITE(GUEST_GDTR_LIMIT, &(p->gdtr.limit));
  CHK_VMCS_WRITE(GUEST_IDTR_LIMIT, &(p->idtr.limit));
  CHK_VMCS_WRITE(GUEST_IA32_DEBUGCTL, &(p->dbg_ctrl));
  CHK_VMCS_WRITE(GUEST_IA32_DEBUGCTL_HIGH, ((char *)&(p->dbg_ctrl)) + 4);
  CHK_VMCS_WRITE(GUEST_IA32_SYSENTER_CS, &(p->sysenter_cs));
  CHK_VMCS_WRITE(GUEST_IA32_SYSENTER_ESP, &(p->sysenter_esp));
  CHK_VMCS_WRITE(GUEST_IA32_SYSENTER_EIP, &(p->sysenter_eip));
  CHK_VMCS_WRITE(GUEST_SMBASE, &(p->smbase));

  CHK_VMCS_WRITE(GUEST_ACTIVITY_STATE, &(p->activity));
  CHK_VMCS_WRITE(GUEST_INT_STATE, &(p->interrupt_state));
  CHK_VMCS_WRITE(GUEST_PENDING_DEBUG_EXCS, &(p->pending_dbg_exceptions));
  CHK_VMCS_WRITE(VMCS_LINK_PTR, &(p->vmcs_link));
  CHK_VMCS_WRITE(VMCS_LINK_PTR_HIGH, ((char *)&(p->vmcs_link)) + 4);
  return 0;
}



int CopyOutVMCSHostStateArea(struct VMCSHostStateArea *p) {
  CHK_VMCS_READ(HOST_CR0, &(p->cr0));
  CHK_VMCS_READ(HOST_CR3, &(p->cr3));
  CHK_VMCS_READ(HOST_CR4, &(p->cr4));
  CHK_VMCS_READ(HOST_RSP, &(p->rsp));
  CHK_VMCS_READ(HOST_RIP, &(p->rip));
  CHK_VMCS_READ(VMCS_HOST_CS_SELECTOR, &(p->csSelector));
  CHK_VMCS_READ(VMCS_HOST_SS_SELECTOR, &(p->ssSelector));
  CHK_VMCS_READ(VMCS_HOST_DS_SELECTOR, &(p->dsSelector));
  CHK_VMCS_READ(VMCS_HOST_ES_SELECTOR, &(p->esSelector));
  CHK_VMCS_READ(VMCS_HOST_FS_SELECTOR, &(p->fsSelector));
  CHK_VMCS_READ(VMCS_HOST_GS_SELECTOR, &(p->gsSelector));
  CHK_VMCS_READ(VMCS_HOST_TR_SELECTOR, &(p->trSelector));
  CHK_VMCS_READ(HOST_FS_BASE, &(p->fsBaseAddr));
  CHK_VMCS_READ(HOST_GS_BASE, &(p->gsBaseAddr));
  CHK_VMCS_READ(HOST_TR_BASE, &(p->trBaseAddr));
  CHK_VMCS_READ(HOST_GDTR_BASE, &(p->gdtrBaseAddr));
  CHK_VMCS_READ(HOST_IDTR_BASE, &(p->idtrBaseAddr));
  CHK_VMCS_READ(HOST_IA32_SYSENTER_CS, &(p->sysenter_cs));
  CHK_VMCS_READ(HOST_IA32_SYSENTER_ESP, &(p->sysenter_esp));
  CHK_VMCS_READ(HOST_IA32_SYSENTER_EIP, &(p->sysenter_eip));
  return 0;
}



int CopyInVMCSHostStateArea(struct VMCSHostStateArea *p) {
  CHK_VMCS_WRITE(HOST_CR0, &(p->cr0));
  CHK_VMCS_WRITE(HOST_CR3, &(p->cr3));
  CHK_VMCS_WRITE(HOST_CR4, &(p->cr4));
  CHK_VMCS_WRITE(HOST_RSP, &(p->rsp));
  CHK_VMCS_WRITE(HOST_RIP, &(p->rip));
  CHK_VMCS_WRITE(VMCS_HOST_CS_SELECTOR, &(p->csSelector));
  CHK_VMCS_WRITE(VMCS_HOST_SS_SELECTOR, &(p->ssSelector));
  CHK_VMCS_WRITE(VMCS_HOST_DS_SELECTOR, &(p->dsSelector));
  CHK_VMCS_WRITE(VMCS_HOST_ES_SELECTOR, &(p->esSelector));
  CHK_VMCS_WRITE(VMCS_HOST_FS_SELECTOR, &(p->fsSelector));
  CHK_VMCS_WRITE(VMCS_HOST_GS_SELECTOR, &(p->gsSelector));
  CHK_VMCS_WRITE(VMCS_HOST_TR_SELECTOR, &(p->trSelector));
  CHK_VMCS_WRITE(HOST_FS_BASE, &(p->fsBaseAddr));
  CHK_VMCS_WRITE(HOST_GS_BASE, &(p->gsBaseAddr));
  CHK_VMCS_WRITE(HOST_TR_BASE, &(p->trBaseAddr));
  CHK_VMCS_WRITE(HOST_GDTR_BASE, &(p->gdtrBaseAddr));
  CHK_VMCS_WRITE(HOST_IDTR_BASE, &(p->idtrBaseAddr));
  CHK_VMCS_WRITE(HOST_IA32_SYSENTER_CS, &(p->sysenter_cs));
  CHK_VMCS_WRITE(HOST_IA32_SYSENTER_ESP, &(p->sysenter_esp));
  CHK_VMCS_WRITE(HOST_IA32_SYSENTER_EIP, &(p->sysenter_eip));
  return 0;
}


int CopyOutVMCSExitCtrlFields(struct VMCSExitCtrlFields *p)
{
  CHK_VMCS_READ(VM_EXIT_CTRLS,&(p->exitCtrls));
  CHK_VMCS_READ(VM_EXIT_MSR_STORE_COUNT,&(p->msrStoreCount));
  CHK_VMCS_READ(VM_EXIT_MSR_STORE_ADDR,&(p->msrStoreAddr));
  CHK_VMCS_READ(VM_EXIT_MSR_LOAD_COUNT,&(p->msrLoadCount));
  CHK_VMCS_READ(VM_EXIT_MSR_LOAD_ADDR,&(p->msrLoadAddr));
  return 0;
}

int CopyInVMCSExitCtrlFields(struct VMCSExitCtrlFields *p)
{
  CHK_VMCS_WRITE(VM_EXIT_CTRLS,&(p->exitCtrls));
  CHK_VMCS_WRITE(VM_EXIT_MSR_STORE_COUNT,&(p->msrStoreCount));
  CHK_VMCS_WRITE(VM_EXIT_MSR_STORE_ADDR,&(p->msrStoreAddr));
  CHK_VMCS_WRITE(VM_EXIT_MSR_LOAD_COUNT,&(p->msrLoadCount));
  CHK_VMCS_WRITE(VM_EXIT_MSR_LOAD_ADDR,&(p->msrLoadAddr));
  return 0;
}


int CopyOutVMCSEntryCtrlFields(struct VMCSEntryCtrlFields *p)
{
  CHK_VMCS_READ(VM_ENTRY_CTRLS,&(p->entryCtrls));
  CHK_VMCS_READ(VM_ENTRY_MSR_LOAD_COUNT,&(p->msrLoadCount));
  CHK_VMCS_READ(VM_ENTRY_MSR_LOAD_ADDR,&(p->msrLoadAddr));
  CHK_VMCS_READ(VM_ENTRY_INT_INFO_FIELD,&(p->intInfo));
  CHK_VMCS_READ(VM_ENTRY_EXCEPTION_ERROR,&(p->exceptionErrorCode));
  CHK_VMCS_READ(VM_ENTRY_INSTR_LENGTH,&(p->instrLength));
  return 0;
}

int CopyInVMCSEntryCtrlFields(struct VMCSEntryCtrlFields *p)
{
  CHK_VMCS_WRITE(VM_ENTRY_CTRLS,&(p->entryCtrls));
  CHK_VMCS_WRITE(VM_ENTRY_MSR_LOAD_COUNT,&(p->msrLoadCount));
  CHK_VMCS_WRITE(VM_ENTRY_MSR_LOAD_ADDR,&(p->msrLoadAddr));
  CHK_VMCS_WRITE(VM_ENTRY_INT_INFO_FIELD,&(p->intInfo));
  CHK_VMCS_WRITE(VM_ENTRY_EXCEPTION_ERROR,&(p->exceptionErrorCode));
  CHK_VMCS_WRITE(VM_ENTRY_INSTR_LENGTH,&(p->instrLength));
  return 0;
}

int CopyOutVMCSExitInfoFields(struct VMCSExitInfoFields *p) {
  CHK_VMCS_READ(EXIT_REASON,&(p->reason));
  CHK_VMCS_READ(EXIT_QUALIFICATION,&(p->qualification));
  CHK_VMCS_READ(VM_EXIT_INT_INFO,&(p->intInfo));
  CHK_VMCS_READ(VM_EXIT_INT_ERROR,&(p->intErrorCode));
  CHK_VMCS_READ(IDT_VECTOR_INFO,&(p->idtVectorInfo));
  CHK_VMCS_READ(IDT_VECTOR_ERROR,&(p->idtVectorErrorCode));
  CHK_VMCS_READ(VM_EXIT_INSTR_LENGTH,&(p->instrLength));
  CHK_VMCS_READ(GUEST_LINEAR_ADDR,&(p->guestLinearAddr));
  CHK_VMCS_READ(VMX_INSTR_INFO,&(p->instrInfo));
  CHK_VMCS_READ(IO_RCX,&(p->ioRCX));
  CHK_VMCS_READ(IO_RSI,&(p->ioRSI));
  CHK_VMCS_READ(IO_RDI,&(p->ioRDI));
  CHK_VMCS_READ(IO_RIP,&(p->ioRIP));
  CHK_VMCS_READ(VM_INSTR_ERROR,&(p->instrErrorField));
  return 0;
}


int CopyOutVMCSExecCtrlFields(struct VMCSExecCtrlFields *p)
{
  CHK_VMCS_READ(PIN_VM_EXEC_CTRLS,&(p->pinCtrls));
  CHK_VMCS_READ(PROC_VM_EXEC_CTRLS,&(p->procCtrls));
  CHK_VMCS_READ(EXCEPTION_BITMAP,&(p->execBitmap));
  CHK_VMCS_READ(PAGE_FAULT_ERROR_MASK,&(p->pageFaultErrorMask));
  CHK_VMCS_READ(PAGE_FAULT_ERROR_MATCH,&(p->pageFaultErrorMatch));
  CHK_VMCS_READ(IO_BITMAP_A_ADDR,&(p->ioBitmapA));
  CHK_VMCS_READ(IO_BITMAP_B_ADDR,&(p->ioBitmapB));
  CHK_VMCS_READ(TSC_OFFSET,&(p->tscOffset));
  CHK_VMCS_READ(CR0_GUEST_HOST_MASK,&(p->cr0GuestHostMask));
  CHK_VMCS_READ(CR0_READ_SHADOW,&(p->cr0ReadShadow));
  CHK_VMCS_READ(CR4_GUEST_HOST_MASK,&(p->cr4GuestHostMask));
  CHK_VMCS_READ(CR4_READ_SHADOW,&(p->cr4ReadShadow));
  CHK_VMCS_READ(CR3_TARGET_COUNT, &(p->cr3TargetCount));
  CHK_VMCS_READ(CR3_TARGET_VALUE_0, &(p->cr3TargetValue0));
  CHK_VMCS_READ(CR3_TARGET_VALUE_1, &(p->cr3TargetValue1));
  CHK_VMCS_READ(CR3_TARGET_VALUE_2, &(p->cr3TargetValue2));
  CHK_VMCS_READ(CR3_TARGET_VALUE_3, &(p->cr3TargetValue3));
  CHK_VMCS_READ(VIRT_APIC_PAGE_ADDR, &(p->virtApicPageAddr));
  CHK_VMCS_READ(TPR_THRESHOLD, &(p->tprThreshold));
  CHK_VMCS_READ(MSR_BITMAPS, &(p->MSRBitmapsBaseAddr));
  CHK_VMCS_READ(VMCS_EXEC_PTR,&(p->vmcsExecPtr));
  return 0;
}


int CopyInVMCSExecCtrlFields(struct VMCSExecCtrlFields *p)
{
  CHK_VMCS_WRITE(PIN_VM_EXEC_CTRLS,&(p->pinCtrls));
  CHK_VMCS_WRITE(PROC_VM_EXEC_CTRLS,&(p->procCtrls));
  CHK_VMCS_WRITE(EXCEPTION_BITMAP,&(p->execBitmap));
  CHK_VMCS_WRITE(PAGE_FAULT_ERROR_MASK,&(p->pageFaultErrorMask));
  CHK_VMCS_WRITE(PAGE_FAULT_ERROR_MATCH,&(p->pageFaultErrorMatch));
  CHK_VMCS_WRITE(IO_BITMAP_A_ADDR,&(p->ioBitmapA));
  CHK_VMCS_WRITE(IO_BITMAP_B_ADDR,&(p->ioBitmapB));
  CHK_VMCS_WRITE(TSC_OFFSET,&(p->tscOffset));
  CHK_VMCS_WRITE(CR0_GUEST_HOST_MASK,&(p->cr0GuestHostMask));
  CHK_VMCS_WRITE(CR0_READ_SHADOW,&(p->cr0ReadShadow));
  CHK_VMCS_WRITE(CR4_GUEST_HOST_MASK,&(p->cr4GuestHostMask));
  CHK_VMCS_WRITE(CR4_READ_SHADOW,&(p->cr4ReadShadow));
  CHK_VMCS_WRITE(CR3_TARGET_COUNT, &(p->cr3TargetCount));
  CHK_VMCS_WRITE(CR3_TARGET_VALUE_0, &(p->cr3TargetValue0));
  CHK_VMCS_WRITE(CR3_TARGET_VALUE_1, &(p->cr3TargetValue1));
  CHK_VMCS_WRITE(CR3_TARGET_VALUE_2, &(p->cr3TargetValue2));
  CHK_VMCS_WRITE(CR3_TARGET_VALUE_3, &(p->cr3TargetValue3));
  CHK_VMCS_WRITE(VIRT_APIC_PAGE_ADDR, &(p->virtApicPageAddr));
  CHK_VMCS_WRITE(TPR_THRESHOLD, &(p->tprThreshold));
  CHK_VMCS_WRITE(MSR_BITMAPS, &(p->MSRBitmapsBaseAddr));
  CHK_VMCS_WRITE(VMCS_EXEC_PTR,&(p->vmcsExecPtr));
  return 0;
}


int CopyOutVMCSData(struct VMCSData *p) {
  if (CopyOutVMCSGuestStateArea(&(p->guestStateArea)) != 0) {
    return -1;
  }
  if (CopyOutVMCSHostStateArea(&(p->hostStateArea)) != 0) {
    return -1;
  }
  if (CopyOutVMCSExecCtrlFields(&(p->execCtrlFields)) != 0) {
    return -1;
  }
  if (CopyOutVMCSExitCtrlFields(&(p->exitCtrlFields)) != 0) {
    return -1;
  }
  if (CopyOutVMCSEntryCtrlFields(&(p->entryCtrlFields)) != 0) {
    return -1;
  }
  if (CopyOutVMCSExitInfoFields(&(p->exitInfoFields)) != 0) {
    return -1;
  }
  return 0;
}


int CopyInVMCSData(struct VMCSData *p) {
  if (CopyInVMCSGuestStateArea(&(p->guestStateArea)) != 0) {
    return -1;
  }
  if (CopyInVMCSHostStateArea(&(p->hostStateArea)) != 0) {
    return -1;
  }
  if (CopyInVMCSExecCtrlFields(&(p->execCtrlFields)) != 0) {
    return -1;
  }
  if (CopyInVMCSExitCtrlFields(&(p->exitCtrlFields)) != 0) {
    return -1;
  }
  if (CopyInVMCSEntryCtrlFields(&(p->entryCtrlFields)) != 0) {
    return -1;
  }
  return 0;
}


void SerialPrint_VMX_Regs(struct VMXRegs * regs) {
  SerialPrint("==>VMX Register values:\n");
  SerialPrint("EAX: %x\n", regs->eax);
  SerialPrint("ECX: %x\n", regs->ecx);
  SerialPrint("EDX: %x\n", regs->edx);
  SerialPrint("EBX: %x\n", regs->ebx);
  SerialPrint("ESP: %x\n", regs->esp);
  SerialPrint("EBP: %x\n", regs->ebp);
  SerialPrint("ESI: %x\n", regs->esi);
  SerialPrint("EDI: %x\n", regs->edi);
  SerialPrint("\n");
}


void SerialPrint_VMCSSegment(char * segname, struct VMCSSegment * seg, int abbr) {
  SerialPrint("Segment: %s\n", segname);
  if (abbr == 0) {
    SerialPrint("\tSelector: %x\n", (uint_t)seg->selector);
    SerialPrint("\tAccess: %x\n", *(uint_t*)&(seg->access));
  }
  SerialPrint("\tBase Addr: %x\n", (uint_t)seg->baseAddr);
  SerialPrint("\tLimit: %x\n", (uint_t)seg->limit);

}


void SerialPrint_VMCSGuestStateArea(struct VMCSGuestStateArea * guestState) {
  SerialPrint("==>Guest State Area\n");
  SerialPrint("==>==> Guest Register State\n");
  SerialPrint("GUEST_CR0: %x\n",(uint_t) guestState->cr0);
  SerialPrint("GUEST_CR3: %x\n",(uint_t)guestState->cr3);
  SerialPrint("GUEST_CR4: %x\n",(uint_t)guestState->cr4);
  SerialPrint("GUEST_DR7: %x\n",(uint_t)guestState->dr7);
  SerialPrint("GUEST_RSP: %x\n",(uint_t)guestState->rsp);
  SerialPrint("GUEST_RIP: %x\n",(uint_t)guestState->rip);
  SerialPrint("GUEST_RFLAGS: %x\n",(uint_t)guestState->rflags);

  SerialPrint_VMCSSegment("Guest CS", &(guestState->cs), 0);
  SerialPrint_VMCSSegment("Guest SS", &(guestState->ss), 0);
  SerialPrint_VMCSSegment("Guest DS",&(guestState->ds), 0);
  SerialPrint_VMCSSegment("Guest ES", &(guestState->es), 0);
  SerialPrint_VMCSSegment("Guest FS", &(guestState->fs), 0);
  SerialPrint_VMCSSegment("Guest GS", &(guestState->gs), 0);
  SerialPrint_VMCSSegment("Guest LDTR", &(guestState->ldtr), 0);
  SerialPrint_VMCSSegment("Guest TR", &(guestState->tr), 0);
  SerialPrint_VMCSSegment("Guest GDTR", &(guestState->gdtr), 1);  
  SerialPrint_VMCSSegment("Guest IDTR", &(guestState->idtr), 1);  


  SerialPrint("GUEST_IA32_DEBUGCTL: %x\n",(uint_t)(guestState->dbg_ctrl & 0xffffffff));
  SerialPrint("GUEST_IA32_DEBUGCTL_HIGH: %x\n",(uint_t)(guestState->dbg_ctrl >> 32) & 0xffffffff);
  SerialPrint("GUEST_IA32_SYSENTER_CS: %x\n",guestState->sysenter_cs);
  SerialPrint("GUEST_IA32_SYSENTER_ESP: %x\n",(uint_t)guestState->sysenter_esp);
  SerialPrint("GUEST_IA32_SYSENTER_EIP: %x\n",(uint_t)guestState->sysenter_eip);
  SerialPrint("GUEST_SMBASE: %x\n", (uint_t)guestState->smbase);

  SerialPrint("==>==> Guest Non-Register State\n");
  SerialPrint("GUEST_ACTIVITY_STATE: %x\n", (uint_t)guestState->activity);
  SerialPrint("GUEST_INT_STATE: %x\n", (uint_t)guestState->interrupt_state);
  SerialPrint("GUEST_PENDING_DEBUG_EXCS: %x\n", (uint_t)guestState->pending_dbg_exceptions);
  SerialPrint("VMCS_LINK_PTR: %x\n", (uint_t)guestState->vmcs_link & 0xffffffff);
  SerialPrint("VMCS_LINK_PTR_HIGH: %x\n", (uint_t)(guestState->vmcs_link >> 32) & 0xffffffff);
}


void SerialPrint_VMCSHostStateArea(struct VMCSHostStateArea * hostState) {
  SerialPrint("\n==> Host State Area\n");
  SerialPrint("HOST_CR0: %x\n", (uint_t)hostState->cr0);
  SerialPrint("HOST_CR3: %x\n", (uint_t)hostState->cr3);
  SerialPrint("HOST_CR4: %x\n", (uint_t)hostState->cr4);
  SerialPrint("HOST_RSP: %x\n", (uint_t)hostState->rsp);
  SerialPrint("HOST_RIP: %x\n", (uint_t)hostState->rip);
  SerialPrint("VMCS_HOST_CS_SELECTOR: %x\n", (uint_t)hostState->csSelector);
  SerialPrint("VMCS_HOST_SS_SELECTOR: %x\n", (uint_t)hostState->ssSelector);
  SerialPrint("VMCS_HOST_DS_SELECTOR: %x\n", (uint_t)hostState->dsSelector);
  SerialPrint("VMCS_HOST_ES_SELECTOR: %x\n", (uint_t)hostState->esSelector);
  SerialPrint("VMCS_HOST_FS_SELECTOR: %x\n", (uint_t)hostState->fsSelector);
  SerialPrint("VMCS_HOST_GS_SELECTOR: %x\n", (uint_t)hostState->gsSelector);
  SerialPrint("VMCS_HOST_TR_SELECTOR: %x\n", (uint_t)hostState->trSelector);
  SerialPrint("HOST_FS_BASE: %x\n", (uint_t)hostState->fsBaseAddr);
  SerialPrint("HOST_GS_BASE: %x\n", (uint_t)hostState->gsBaseAddr);
  SerialPrint("HOST_TR_BASE: %x\n", (uint_t)hostState->trBaseAddr);
  SerialPrint("HOST_GDTR_BASE: %x\n", (uint_t)hostState->gdtrBaseAddr);
  SerialPrint("HOST_IDTR_BASE: %x\n", (uint_t)hostState->idtrBaseAddr);
  SerialPrint("HOST_IA32_SYSENTER_CS: %x\n", (uint_t)hostState->sysenter_cs);
  SerialPrint("HOST_IA32_SYSENTER_ESP: %x\n", (uint_t)hostState->sysenter_esp);
  SerialPrint("HOST_IA32_SYSENTER_EIP: %x\n", (uint_t)hostState->sysenter_eip);
}

void SerialPrint_VMCSExecCtrlFields(struct VMCSExecCtrlFields * execCtrls) {
  SerialPrint("\n==> VM-Execution Controls:\n");
  SerialPrint("PIN_VM_EXEC_CTRLS: %x\n", (uint_t) execCtrls->pinCtrls);
  SerialPrint("PROC_VM_EXEC_CTRLS: %x\n", (uint_t) execCtrls->procCtrls);
  SerialPrint("EXCEPTION_BITMAP: %x\n", (uint_t) execCtrls->execBitmap);
  SerialPrint("PAGE_FAULT_ERROR_MASK: %x\n", (uint_t) execCtrls->pageFaultErrorMask);
  SerialPrint("PAGE_FAULT_ERROR_MATCH: %x\n", (uint_t) execCtrls->pageFaultErrorMatch);
  SerialPrint("IO_BITMAP_A_ADDR: %x\n", (uint_t) execCtrls->ioBitmapA);
  //  SerialPrint("IO_BITMAP_A_ADDR_HIGH: %x\n", (uint_t) execCtrls->);
  SerialPrint("IO_BITMAP_B_ADDR: %x\n", (uint_t) execCtrls->ioBitmapB);
  // SerialPrint("IO_BITMAP_B_ADDR_HIGH: %x\n", (uint_t) execCtrls->);
  SerialPrint("TSC_OFFSET: %x\n", (uint_t) execCtrls->tscOffset & 0xffffffff);
  SerialPrint("TSC_OFFSET_HIGH: %x\n", (uint_t) (execCtrls->tscOffset >> 32) & 0xffffffff);
  SerialPrint("CR0_GUEST_HOST_MASK: %x\n", (uint_t) execCtrls->cr0GuestHostMask);
  SerialPrint("CR0_READ_SHADOW: %x\n", (uint_t) execCtrls->cr0ReadShadow);
  SerialPrint("CR4_GUEST_HOST_MASK: %x\n", (uint_t) execCtrls->cr4GuestHostMask);
  SerialPrint("CR4_READ_SHADOW: %x\n", (uint_t) execCtrls->cr4ReadShadow);
  SerialPrint("CR3_TARGET_COUNT: %x\n", (uint_t) execCtrls->cr3TargetCount);
  SerialPrint("CR3_TARGET_VALUE_0: %x\n", (uint_t) execCtrls->cr3TargetValue0);
  SerialPrint("CR3_TARGET_VALUE_1: %x\n", (uint_t) execCtrls->cr3TargetValue1);
  SerialPrint("CR3_TARGET_VALUE_2: %x\n", (uint_t) execCtrls->cr3TargetValue2);
  SerialPrint("CR3_TARGET_VALUE_3: %x\n", (uint_t) execCtrls->cr3TargetValue3);
  SerialPrint("VIRT_APIC_PAGE_ADDR: %x\n", (uint_t) execCtrls->virtApicPageAddr & 0xffffffff);
  SerialPrint("VIRT_APIC_PAGE_ADDR_HIGH: %x\n", (uint_t) (execCtrls->virtApicPageAddr >> 32) & 0xffffffff);
  SerialPrint("TPR_THRESHOLD: %x\n", (uint_t) execCtrls->tprThreshold);
  SerialPrint("MSR_BITMAPS: %x\n", (uint_t) execCtrls->MSRBitmapsBaseAddr & 0xffffffff);
  SerialPrint("MSR_BITMAPS_HIGH: %x\n", (uint_t) (execCtrls->MSRBitmapsBaseAddr >> 32) & 0xffffffff);
  SerialPrint("VMCS_EXEC_PTR: %x\n", (uint_t) execCtrls->vmcsExecPtr & 0xffffffff);
  SerialPrint("VMCS_EXEC_PTR_HIGH: %x\n", (uint_t) (execCtrls->vmcsExecPtr >> 32) & 0xffffffff);
}

void SerialPrint_VMCSExitCtrlFields(struct VMCSExitCtrlFields * exitCtrls) {
  SerialPrint("\n==> VM Exit Controls\n");
  SerialPrint("VM_EXIT_CTRLS: %x\n", (uint_t) exitCtrls->exitCtrls);
  SerialPrint("VM_EXIT_MSR_STORE_COUNT: %x\n", (uint_t) exitCtrls->msrStoreCount);
  SerialPrint("VM_EXIT_MSR_STORE_ADDR: %x\n", (uint_t) exitCtrls->msrStoreAddr & 0xffffffff);
  SerialPrint("VM_EXIT_MSR_STORE_ADDR_HIGH: %x\n", (uint_t) (exitCtrls->msrStoreAddr >> 32) & 0xffffffff);
  SerialPrint("VM_EXIT_MSR_LOAD_COUNT: %x\n", (uint_t) exitCtrls->msrLoadCount);
  SerialPrint("VM_EXIT_MSR_LOAD_ADDR: %x\n", (uint_t) exitCtrls->msrLoadAddr & 0xffffffff);
  SerialPrint("VM_EXIT_MSR_LOAD_ADDR_HIGH: %x\n", (uint_t) (exitCtrls->msrLoadAddr >> 32) & 0xffffffff);
}

void SerialPrint_VMCSEntryCtrlFields(struct VMCSEntryCtrlFields * entryCtrls) {
  SerialPrint("\n==> VM Entry Controls\n");
  SerialPrint("VM_ENTRY_CTRLS: %x\n", (uint_t) entryCtrls->entryCtrls);
  SerialPrint("VM_ENTRY_MSR_LOAD_COUNT: %x\n", (uint_t) entryCtrls->msrLoadCount);
  SerialPrint("VM_ENTRY_MSR_LOAD_ADDR: %x\n", (uint_t) entryCtrls->msrLoadAddr & 0xffffffff);
  SerialPrint("VM_ENTRY_MSR_LOAD_ADDR_HIGH: %x\n", (uint_t) (entryCtrls->msrLoadAddr >> 32) & 0xffffffff);
  SerialPrint("VM_ENTRY_INT_INFO_FIELD: %x\n", (uint_t) entryCtrls->intInfo);
  SerialPrint("VM_ENTRY_EXCEPTION_ERROR: %x\n", (uint_t) entryCtrls->exceptionErrorCode);
  SerialPrint("VM_ENTRY_INSTR_LENGTH: %x\n", (uint_t) entryCtrls->instrLength);
}

void SerialPrint_VMCSExitInfoFields(struct VMCSExitInfoFields * exitInfo) {
  SerialPrint("\n==> VM Exit Info\n");
  SerialPrint("EXIT_REASON: %x\n", (uint_t) exitInfo->reason);
  SerialPrint("EXIT_QUALIFICATION: %x\n", (uint_t) exitInfo->qualification);
  SerialPrint("VM_EXIT_INT_INFO: %x\n", (uint_t) exitInfo->intInfo);
  SerialPrint("VM_EXIT_INT_ERROR: %x\n", (uint_t) exitInfo->intErrorCode);
  SerialPrint("IDT_VECTOR_INFO: %x\n", (uint_t) exitInfo->idtVectorInfo);
  SerialPrint("IDT_VECTOR_ERROR: %x\n", (uint_t) exitInfo->idtVectorErrorCode);
  SerialPrint("VM_EXIT_INSTR_LENGTH: %x\n", (uint_t) exitInfo->instrLength);
  SerialPrint("GUEST_LINEAR_ADDR: %x\n", (uint_t) exitInfo->guestLinearAddr);
  SerialPrint("VMX_INSTR_INFO: %x\n", (uint_t) exitInfo->instrInfo);
  SerialPrint("IO_RCX: %x\n", (uint_t) exitInfo->ioRCX);
  SerialPrint("IO_RSI: %x\n", (uint_t) exitInfo->ioRSI);
  SerialPrint("IO_RDI: %x\n", (uint_t) exitInfo->ioRDI);
  SerialPrint("IO_RIP: %x\n", (uint_t) exitInfo->ioRIP);
  SerialPrint("VM_INSTR_ERROR: %x\n", (uint_t) exitInfo->instrErrorField);
}


void SerialPrint_VMCSData(struct VMCSData * vmcs) {
  SerialPrint("VMCSData Structure\n");

  SerialPrint_VMCSGuestStateArea(&(vmcs->guestStateArea));
  SerialPrint_VMCSHostStateArea(&(vmcs->hostStateArea));
  SerialPrint_VMCSExecCtrlFields(&(vmcs->execCtrlFields));
  SerialPrint_VMCSExitCtrlFields(&(vmcs->exitCtrlFields));
  SerialPrint_VMCSEntryCtrlFields(&(vmcs->entryCtrlFields));
  SerialPrint_VMCSExitInfoFields(&(vmcs->exitInfoFields));
  SerialPrint("\n");
}
