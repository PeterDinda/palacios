#ifndef __HVMLOADER_E820_H__
#define __HVMLOADER_E820_H__

/* E820 location in HVM virtual address space. */
#define HVM_E820_PAGE        0x00090000
#define HVM_E820_NR_OFFSET   0x000001E8
#define HVM_E820_OFFSET      0x000002D0

#define HVM_BELOW_4G_RAM_END        0xF0000000
#define HVM_BELOW_4G_MMIO_START     HVM_BELOW_4G_RAM_END
#define HVM_BELOW_4G_MMIO_LENGTH    ((1ULL << 32) - HVM_BELOW_4G_MMIO_START)


/*
 * PC BIOS standard E820 types and structure.
 */
#define E820_RAM          1
#define E820_RESERVED     2
#define E820_ACPI         3
#define E820_NVS          4

struct e820entry {
    uint64_t addr;
    uint64_t size;
    uint32_t type;
} __attribute__((packed));

#define HVM_E820_NR ((unsigned char *)HVM_E820_PAGE + HVM_E820_NR_OFFSET)
#define HVM_E820    ((struct e820entry *)(HVM_E820_PAGE + HVM_E820_OFFSET))

#endif /* __HVMLOADER_E820_H__ */
