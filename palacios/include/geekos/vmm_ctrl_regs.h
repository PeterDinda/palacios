#ifndef __VMM_CTRL_REGS_H
#define __VMM_CTRL_REGS_H


struct cr0_real {
  uint_t pe    : 1;
  uint_t mp    : 1;
  uint_t em    : 1;
  uint_t ts    : 1;
};


struct cr0_32 {
  uint_t pe    : 1;
  uint_t mp    : 1;
  uint_t em    : 1;
  uint_t ts    : 1;
  uint_t et    : 1;
  uint_t ne    : 1;
  uint_t rsvd1 : 10;
  uint_t wp    : 1;
  uint_t rsvd2 : 1;
  uint_t am    : 1;
  uint_t rsvd3 : 10;
  uint_t nw    : 1;
  uint_t cd    : 1;
  uint_t pg    : 1;
};


struct cr0_64 {
  uint_t pe    : 1;
  uint_t mp    : 1;
  uint_t em    : 1;
  uint_t ts    : 1;
  uint_t et    : 1;
  uint_t ne    : 1;
  uint_t rsvd1 : 10;
  uint_t wp    : 1;
  uint_t rsvd2 : 1;
  uint_t am    : 1;
  uint_t rsvd3 : 10;
  uint_t nw    : 1;
  uint_t cd    : 1;
  uint_t pg    : 1;

  uint_t  rsvd4;  // MBZ
};


struct cr2_32 {
  uint_t pf_vaddr;
};

struct cr2_64 {
  ullong_t pf_vaddr;
};


struct cr3_32 {
  uint_t rsvd1             : 3;
  uint_t pwt               : 1;
  uint_t pcd               : 1;
  uint_t rsvd2             : 7;
  uint_t pdt_base_addr    : 20;
};


struct cr3_32_PAE {
  uint_t rsvd1             : 3;
  uint_t pwt               : 1;
  uint_t pcd               : 1;
  uint_t pdpt_base_addr    : 27;
};


struct cr3_64 {
  uint_t rsvd1             : 3;
  uint_t pwt               : 1;
  uint_t pcd               : 1;
  uint_t rsvd2             : 7;
  ullong_t pml4t_base_addr : 40;
  uint_t rsvd3             : 12; 
};


struct cr4_32 {
  uint_t vme               : 1;
  uint_t pvi               : 1;
  uint_t tsd               : 1;
  uint_t de                : 1;
  uint_t pse               : 1;
  uint_t pae               : 1;
  uint_t mce               : 1;
  uint_t pge               : 1;
  uint_t pce               : 1;
  uint_t osf_xsr           : 1;
  uint_t osx               : 1;
  uint_t rsvd1             : 21;
};

struct cr4_64 {
  uint_t vme               : 1;
  uint_t pvi               : 1;
  uint_t tsd               : 1;
  uint_t de                : 1;
  uint_t pse               : 1;
  uint_t pae               : 1;
  uint_t mce               : 1;
  uint_t pge               : 1;
  uint_t pce               : 1;
  uint_t osf_xsr           : 1;
  uint_t osx               : 1;
  uint_t rsvd1             : 21;
  uint_t rsvd2             : 32;
};

#endif
