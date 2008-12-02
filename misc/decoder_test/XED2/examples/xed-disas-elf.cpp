/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2007 Intel Corporation 
All rights reserved. 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/// @file disas-elf.cpp
/// @author Mark Charney   <mark.charney@intel.com>

#include "xed-disas-elf.H"

#if defined(XED_ELF_READER)

////////////////////////////////////////////////////////////////////////////
#include <elf.h>

extern "C" {
#include "xed-interface.h"
#include "xed-portability.h"
#include "xed-examples-util.h"
}

#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <map>
using namespace std;
////////////////////////////////////////////////////////////////////////////


char* 
lookup32(Elf32_Word stoffset,
       void* start,
       Elf32_Off offset)
{
    char* p = (char*)start + offset;
    char* q = p + stoffset;
    // int i;
    //cout << "p = " << (unsigned int) p <<  endl;
    //cout << "q = " << (unsigned int) q <<  endl;
    //for(i=0;i<20;i++)
    //{
    //    cout << q[i];
    //}
    //cout << endl;
    return q;
}

char* 
lookup64(Elf64_Word stoffset,
	 void* start,
	 Elf64_Off offset)
{
  char* p = (char*)start + offset;
  char* q = p + stoffset;
  //int i;
  //cout << "p = " << (unsigned int) p <<  endl;
  //cout << "q = " << (unsigned int) q <<  endl;
  //for( i=0;i<20;i++)
  //{
  //    cout << q[i];
  //}
  //cout << endl;
  return q;
}

#include <vector>
void make_symbol_vector(map<xed_uint64_t,char*>* sym_map, 
                        vector<xed_uint64_t>* sym_vec) 
{
    map<xed_uint64_t,char*>::iterator i = sym_map->begin();
    for( ; i != sym_map->end() ;i ++) {
        sym_vec->push_back(i->first);
    }
    sort(sym_vec->begin(), sym_vec->end());
}


#include <algorithm>
uint64_t find_symbol_address(vector<xed_uint64_t>* sym_vec, xed_uint64_t tgt) 
{
    vector<xed_uint64_t>::iterator i = lower_bound(sym_vec->begin(), sym_vec->end(), tgt);
    if (i == sym_vec->end())
        return 0;
    if (*i > tgt) {
        // take previous value
        if (i != sym_vec->begin())
            return *(i-1);
    }
    if (*i == tgt) {
        return *i;
    }
    return 0;
    
}

static vector<xed_uint64_t>* global_sym_vec=0;
uint64_t find_symbol_address_global(xed_uint64_t tgt) 
{
    return find_symbol_address(global_sym_vec, tgt);
}

static map<xed_uint64_t,char*>* global_sym_map;
char* get_symbol(xed_uint64_t a) {
    map<xed_uint64_t,char*>::iterator i = global_sym_map->find(a);
    if (i != global_sym_map->end()) {
         return i->second;
    }
    return 0;
}


int xed_disassembly_callback_function(
    uint64_t address,
    char* symbol_buffer,
    uint32_t buffer_length,
    uint64_t* offset,
    void* caller_data) 
{
    (void)caller_data; /* not used */
    uint64_t symbol_address = find_symbol_address_global(address);
    if (symbol_address) {
        char* symbol  = get_symbol(symbol_address);
        if (symbol) {
            if (xed_strlen(symbol) < buffer_length)
                xed_strncpy(symbol_buffer, symbol, buffer_length);
            else {
                xed_strncpy(symbol_buffer, symbol, buffer_length-1);
                symbol_buffer[buffer_length-1]=0;
            }
            *offset = address - symbol_address;
            return 1;
        }
    }
    return 0;
}

void xed_disas_elf_init() {
    xed_register_disassembly_callback(xed_disassembly_callback_function);
}



void
disas_test32(const xed_state_t* dstate,
	     void* start, 
	     Elf32_Off offset,
	     Elf32_Word size, 
	     int ninst,
             Elf32_Addr runtime_vaddr,
             bool decode_only,
             map<xed_uint64_t,char*>& sym_map,
             vector<xed_uint64_t>& sym_vec)
{
  unsigned char* s = (unsigned char*)start;
  unsigned char* a = (unsigned char*)start + offset;
  unsigned char* q = a + size; // end of region
  
  // pass in a function to retreive valid symbol names
  global_sym_map = & sym_map;
  global_sym_vec = & sym_vec;
  xed_disas_test(dstate, s,a,q, ninst, runtime_vaddr, decode_only,get_symbol);
}

void
disas_test64(const xed_state_t* dstate,
	     void* start, 
	     Elf64_Off offset,
	     Elf64_Word size,
	     int ninst,
             Elf64_Addr runtime_vaddr,
             bool decode_only,
             map<xed_uint64_t,char*>& sym_map,
             vector<xed_uint64_t>& sym_vec)
{
  unsigned char* s = (unsigned char*)start;
  unsigned char* a = (unsigned char*)start + offset;
  unsigned char* q = a + size; // end of region

  // pass in a function to retreive valid symbol names
  global_sym_map = & sym_map;
  global_sym_vec = & sym_vec;
  xed_disas_test(dstate, s,a,q, ninst, runtime_vaddr, decode_only,get_symbol);
}


void
process_elf32(void* start,
	      unsigned int length,
	      const char* tgt_section,
	      const xed_state_t* dstate,
	      int ninst,
              xed_bool_t decode_only,
              map<xed_uint64_t,char*>& sym_map,
              vector<xed_uint64_t>& sym_vec)
{
    Elf32_Ehdr* elf_hdr = (Elf32_Ehdr*) start;
    if (elf_hdr->e_machine != EM_386) {
        cerr << "Not an IA32 binary. Consider using the -64 switch" << endl;
        exit(1);
    }

    Elf32_Off shoff = elf_hdr->e_shoff;  // section hdr table offset
    Elf32_Shdr* shp = (Elf32_Shdr*) ((char*)start + shoff);
    int sect_strings  = elf_hdr->e_shstrndx;
    //cout << "String section " << sect_strings << endl;
    int nsect = elf_hdr->e_shnum;
    int i;
    for(i=0;i<nsect;i++) {
        char* name = lookup32(shp[i].sh_name, start, shp[sect_strings].sh_offset);
        xed_bool_t text = false;
        if (shp[i].sh_type == SHT_PROGBITS) {
            if (tgt_section) {
                if (strcmp(tgt_section, name)==0) 
                    text = true;
            }
            else if (shp[i].sh_flags & SHF_EXECINSTR)
                text = true;
        }

        if (text) {
            printf("# SECTION " XED_FMT_D " ", i);
            printf("%25s ", name);
            printf("addr " XED_FMT_LX " ",static_cast<xed_uint64_t>(shp[i].sh_addr)); 
            printf("offset " XED_FMT_LX " ",static_cast<xed_uint64_t>(shp[i].sh_offset));
            printf("size " XED_FMT_LU " ", static_cast<xed_uint64_t>(shp[i].sh_size));
            printf("type " XED_FMT_LU "\n", static_cast<xed_uint64_t>(shp[i].sh_type));

            disas_test32(dstate,
                         start, shp[i].sh_offset, shp[i].sh_size,
                         ninst,
                         shp[i].sh_addr,
                         decode_only,
                         sym_map, 
                         sym_vec);
	}

    }

    (void) length;// pacify compiler
}

void
process_elf64(void* start,
	      unsigned int length,
	      const char* tgt_section,
	      const xed_state_t* dstate,
	      int ninst,
              xed_bool_t decode_only,
              map<xed_uint64_t,char*>& sym_map,
              vector<xed_uint64_t>& sym_vec)
{
    Elf64_Ehdr* elf_hdr = (Elf64_Ehdr*) start;
    if (elf_hdr->e_machine != EM_X86_64) {
        cerr << "Not an x86-64  binary. Consider not using the -64 switch." << endl;
        exit(1);
    }

    Elf64_Off shoff = elf_hdr->e_shoff;  // section hdr table offset
    Elf64_Shdr* shp = (Elf64_Shdr*) ((char*)start + shoff);
    Elf64_Half sect_strings  = elf_hdr->e_shstrndx;
    //cout << "String section " << sect_strings << endl;
    Elf64_Half nsect = elf_hdr->e_shnum;
    if (CLIENT_VERBOSE1) 
        printf("# sections %d\n" , nsect);
    unsigned int i;
    xed_bool_t text = false;
    for( i=0;i<nsect;i++)  {
        char* name = lookup64(shp[i].sh_name, start, shp[sect_strings].sh_offset);
        
        text = false;
        if (shp[i].sh_type == SHT_PROGBITS) {
            if (tgt_section) {
                if (strcmp(tgt_section, name)==0) 
                    text = true;
            }
            else if (shp[i].sh_flags & SHF_EXECINSTR)
                text = true;
        }

        if (text) {
            printf("# SECTION " XED_FMT_U " ", i);
            printf("%25s ", name);
            printf("addr " XED_FMT_LX " ",static_cast<xed_uint64_t>(shp[i].sh_addr)); 
            printf("offset " XED_FMT_LX " ",static_cast<xed_uint64_t>(shp[i].sh_offset));
            printf("size " XED_FMT_LU "\n", static_cast<xed_uint64_t>(shp[i].sh_size));
            disas_test64(dstate,
                         start, shp[i].sh_offset, shp[i].sh_size, 
                         ninst,
                         shp[i].sh_addr, decode_only, sym_map, sym_vec);

        }
    }
    (void) length;// pacify compiler
}


void read_symbols64(void* start, 
                    Elf64_Off offset,
                    Elf64_Word size,
                    Elf64_Off string_table_offset,
                    map<xed_uint64_t,char*>& sym_map) {
    char* a = static_cast<char*>(start);
    Elf64_Sym* p = reinterpret_cast<Elf64_Sym*>(a + offset);
    Elf64_Sym* q = reinterpret_cast<Elf64_Sym*>(a + offset + size);
    int i = 0;
    while(p<q) {
        char* name = lookup64(p->st_name, start, string_table_offset);
/*
        cout << "SYM " << setw(3) << i << " " 
             << hex << setw(16) 
             << p->st_value << dec
             << " " << name << endl;
*/
        if (xed_strlen(name) > 0)
            sym_map[static_cast<xed_uint64_t>(p->st_value)] = name;
        p++; 
        i++;
    }
}

/*-----------------------------------------------------------------*/

int check_binary_32b(void* start) {
    Elf32_Ehdr* elf_hdr = (Elf32_Ehdr*) start;
    if (elf_hdr->e_machine == EM_386) 
        return 1;
    return 0;
}

int check_binary_64b(void* start) {
    Elf64_Ehdr* elf_hdr = (Elf64_Ehdr*) start;
    if (elf_hdr->e_machine == EM_X86_64) 
        return 1;
    return 0;
}
/*-----------------------------------------------------------------*/


void symbols_elf64(void* start, map<xed_uint64_t,char*>& sym_map) {
    Elf64_Ehdr* elf_hdr = (Elf64_Ehdr*) start;
    if (elf_hdr->e_machine != EM_X86_64) {
        cerr << "Not an x86-64  binary. Consider not using the -64 switch." << endl;
        exit(1);
    }

    Elf64_Off shoff = elf_hdr->e_shoff;  // section hdr table offset
    Elf64_Shdr* shp = (Elf64_Shdr*) ((char*)start + shoff);
    Elf64_Half nsect = elf_hdr->e_shnum;
    if (CLIENT_VERBOSE1) 
        printf("# sections %d\n" , nsect);
    unsigned int i;
    Elf64_Half sect_strings  = elf_hdr->e_shstrndx;
    Elf64_Off string_table_offset=0;
    Elf64_Off dynamic_string_table_offset=0;
    for( i=0;i<nsect;i++)  {
        if (shp[i].sh_type == SHT_STRTAB) {
            char* name = lookup32(shp[i].sh_name, start, shp[sect_strings].sh_offset);
            if (strcmp(name,".strtab")==0) {
                cout << "# Found strtab: " << i 
                     << " offset " <<shp[i].sh_offset
                     << " size " << shp[i].sh_size 
                     << endl;
                string_table_offset = shp[i].sh_offset;
            }
            if (strcmp(name,".dynstr")==0) {
                cout << "# Found dynamic strtab: " << i 
                     << " offset " <<shp[i].sh_offset
                     << " size " << shp[i].sh_size 
                     << endl;
                dynamic_string_table_offset = shp[i].sh_offset;
            }
        }
    }

    for( i=0;i<nsect;i++)  {
        if (shp[i].sh_type == SHT_SYMTAB) {
            cout << "# Found symtab: " << i 
                 << " offset " <<shp[i].sh_offset
                 << " size " << shp[i].sh_size 
                 << endl;
            read_symbols64(start,shp[i].sh_offset, shp[i].sh_size, string_table_offset,sym_map);
        }
        else if (shp[i].sh_type == SHT_DYNSYM) {
            cout << "# Found dynamic symtab: " << i 
                 << " offset " <<shp[i].sh_offset
                 << " size " << shp[i].sh_size 
                 << endl;
            read_symbols64(start,shp[i].sh_offset, shp[i].sh_size, dynamic_string_table_offset, sym_map);
        }
    }
}



void read_symbols32(void* start, 
                    Elf32_Off offset,
                    Elf32_Word size,
                    Elf32_Off string_table_offset,
                    map<xed_uint64_t,char*>& sym_map) {
    char* a = static_cast<char*>(start);
    Elf32_Sym* p = reinterpret_cast<Elf32_Sym*>(a + offset);
    Elf32_Sym* q = reinterpret_cast<Elf32_Sym*>(a + offset + size);
    int i = 0;
    while(p<q) {
        char* name = lookup32(p->st_name, start, string_table_offset);
/*
        cout << "SYM " << setw(3) << i << " " 
             << hex << setw(16) 
             << p->st_value << dec
             << " " << name << endl;
*/
        if (xed_strlen(name) > 0)
            sym_map[static_cast<xed_uint64_t>(p->st_value)] = name;
        p++; 
        i++;
    }
}


void symbols_elf32(void* start, map<xed_uint64_t,char*>& sym_map) {
    Elf32_Ehdr* elf_hdr = (Elf32_Ehdr*) start;
    if (elf_hdr->e_machine != EM_386) {
        cerr << "Not an IA32 binary. Consider using the -64 switch" << endl;
        exit(1);
    }

    Elf32_Off shoff = elf_hdr->e_shoff;  // section hdr table offset
    Elf32_Shdr* shp = (Elf32_Shdr*) ((char*)start + shoff);
    Elf32_Half nsect = elf_hdr->e_shnum;
    if (CLIENT_VERBOSE1) 
        printf("# sections %d\n" , nsect);
    unsigned int i;
    Elf32_Off string_table_offset=0;
    Elf32_Off dynamic_string_table_offset=0;
    int sect_strings  = elf_hdr->e_shstrndx;

    for( i=0;i<nsect;i++)  {
        
        if (shp[i].sh_type == SHT_STRTAB) {
            char* name = lookup32(shp[i].sh_name, start, shp[sect_strings].sh_offset);
            if (strcmp(name,".strtab")==0) {
                cout << "# Found strtab: " << i 
                     << " offset " <<shp[i].sh_offset
                     << " size " << shp[i].sh_size 
                     << endl;
                string_table_offset = shp[i].sh_offset;
            }
            if (strcmp(name,".dynstr")==0) {
                cout << "# Found dynamic strtab: " << i 
                     << " offset " <<shp[i].sh_offset
                     << " size " << shp[i].sh_size 
                     << endl;
                dynamic_string_table_offset = shp[i].sh_offset;
            }
        }
    }

    for( i=0;i<nsect;i++)  {
        
        if (shp[i].sh_type == SHT_SYMTAB) {
            cout << "# Found symtab: " << i 
                 << " offset " <<shp[i].sh_offset
                 << " size " << shp[i].sh_size 
                 << endl;
            read_symbols32(start,shp[i].sh_offset, shp[i].sh_size, string_table_offset,sym_map);
        } 
        else if (shp[i].sh_type == SHT_DYNSYM) {
            cout << "# Found dynamic symtab: " << i 
                 << " offset " <<shp[i].sh_offset
                 << " size " << shp[i].sh_size 
                 << endl;
            read_symbols32(start,shp[i].sh_offset, shp[i].sh_size, dynamic_string_table_offset, sym_map);
        }
    }
}




void
xed_disas_elf(const char* input_file_name,
              const xed_state_t* dstate,
              int ninst,
              xed_bool_t sixty_four_bit,
              xed_bool_t decode_only,
              const char* target_section)
{

    void* region = 0;
    unsigned int len = 0;
    xed_disas_elf_init();
    xed_map_region(input_file_name, &region, &len);
    

    map<xed_uint64_t,char*> sym_map;
    vector<xed_uint64_t> sym_vec;

    if (check_binary_64b(region)) {
        xed_state_t local_dstate = *dstate;
        if (sixty_four_bit == 0) {
            /* modify the default dstate values because we were not expecting a
             * 64b binary */
            local_dstate.mmode = XED_MACHINE_MODE_LONG_64;
            local_dstate.addr_width = XED_ADDRESS_WIDTH_64b;
        }

        symbols_elf64(region, sym_map);
        make_symbol_vector(&sym_map, &sym_vec);
        process_elf64(region, len, target_section, &local_dstate, ninst, decode_only, sym_map,sym_vec);
    }
    else if (check_binary_32b(region)) {
        symbols_elf32(region, sym_map);
        make_symbol_vector(&sym_map, &sym_vec);
        process_elf32(region, len, target_section, dstate, ninst, decode_only,sym_map, sym_vec);
    }
    else {
        cerr << "Not a recognized 32b or 64b ELF binary." << endl;
        exit(1);
    }
    xed_print_decode_stats();

}
 


#endif
////////////////////////////////////////////////////////////////////////////
