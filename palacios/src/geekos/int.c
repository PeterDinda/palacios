/*
 * GeekOS interrupt handling data structures and functions
 * Copyright (c) 2001,2003 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.7 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/idt.h>	 /* x86-specific int handling stuff */
#include <geekos/screen.h>
#include <geekos/kassert.h>
#include <geekos/int.h>
#include <geekos/serial.h>
#include <geekos/debug.h>

#include <geekos/cpu.h>

/*
 * Defined in lowlevel.asm.
 */
ulong_t Get_Current_EFLAGS(void);

/* ----------------------------------------------------------------------
 * Private functions and data
 * ---------------------------------------------------------------------- */












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




/*
 * A dummy interrupt handler function.
 * Ensures that the low-level interrupt code always
 * has a handler to call.
 */
static void Dummy_Interrupt_Handler(struct Interrupt_State* state)
{
  Begin_IRQ(state);
  
  /* A "feature" of some chipsets is that if an interrupt is raised by mistake
   * then its automatically assigned to IRQ 7(Int 39). 
   * Makes perfect sense...
   * See:
   * http://forums12.itrc.hp.com/service/forums/questionanswer.do?admit=109447627+1204759699215+28353475&threadId=1118488
   */
  if (state->intNum != 39) {
    Print("Unexpected Interrupt!  Ignoring!\n");
    SerialPrint("*** Unexpected interrupt! *** Ignoring!\n");
    Dump_Interrupt_State(state);
  } 
  End_IRQ(state);

  //STOP();
}

#if 0
static void Print_Selector(const char* regName, uint_t value)
{
    Print("%s: index=%d, ti=%d, rpl=%d\n",
	regName, value >> 3, (value >> 2) & 1, value & 3);
}
#endif

static void SerialPrint_Selector(const char* regName, uint_t value)
{
    SerialPrint("%s: index=%d, ti=%d, rpl=%d\n",
	regName, value >> 3, (value >> 2) & 1, value & 3);
}

/* ----------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

/*
 * Initialize the interrupt system.
 */
void Init_Interrupts(void)
{
    int i;

    PrintBoth("Initializing Interrupts\n");

    /* Low-level initialization.  Build and initialize the IDT. */
    Init_IDT();

    /*
     * Initialize all entries of the handler table with a dummy handler.
     * This will ensure that we always have a handler function to call.
     */
    for (i = 0; i < NUM_IDT_ENTRIES; ++i) {
	Install_Interrupt_Handler(i, Dummy_Interrupt_Handler);
    }

    /* JRL */
    // Disable_IRQ(7);
    /* ** */

    /* Re-enable interrupts */
    Enable_Interrupts();
}

/*
 * Query whether or not interrupts are currently enabled.
 */
bool Interrupts_Enabled(void)
{
    ulong_t eflags = Get_Current_EFLAGS();
    return (eflags & EFLAGS_IF) != 0;
}

/*
 * Dump interrupt state struct to screen
 */
void Dump_Interrupt_State(struct Interrupt_State* state)
{
    uint_t errorCode = state->errorCode;

   SerialPrint("eax=%08x ebx=%08x ecx=%08x edx=%08x\n"
	   "esi=%08x edi=%08x ebp=%08x\n"
	   "eip=%08x cs=%08x eflags=%08x\n"
	   "Interrupt number=%d (%s), error code=%d\n"
	   "index=%d, TI=%d, IDT=%d, EXT=%d\n",
	state->eax, state->ebx, state->ecx, state->edx,
	state->esi, state->edi, state->ebp,
	state->eip, state->cs, state->eflags,
	state->intNum, exception_names[state->intNum], errorCode,
	errorCode >> 3, (errorCode >> 2) & 1, (errorCode >> 1) & 1, errorCode & 1
    );


    if (Is_User_Interrupt(state)) {
	struct User_Interrupt_State *ustate = (struct User_Interrupt_State*) state;
	SerialPrint("user esp=%08x, user ss=%08x\n", ustate->espUser, ustate->ssUser);
    }
    SerialPrint_Selector("cs", state->cs);
    SerialPrint_Selector("ds", state->ds);
    SerialPrint_Selector("es", state->es);
    SerialPrint_Selector("fs", state->fs);
    SerialPrint_Selector("gs", state->gs);



}

