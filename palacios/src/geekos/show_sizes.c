#include <stdio.h>
#include "vmm_sizes.h"

int main()
{
  printf("KERNEL_LOAD_ADDRESS      =   0x%x\n", KERNEL_LOAD_ADDRESS);
  printf("KERNEL_SETUP_LENGTH      =   0x%x\n", KERNEL_SETUP_LENGTH);
  printf("KERNEL_CORE_LENGTH       =   0x%x\n", KERNEL_CORE_LENGTH);
  printf("BIOS_START               =   0x%x\n", BIOS_START);
  printf("BIOS_LENGTH              =   0x%x\n", BIOS_LENGTH);
  printf("BIOS2_START              =   0x%x\n", BIOS2_START);
  printf("VGA_BIOS_START           =   0x%x\n", VGA_BIOS_START);
  printf("VGA_BIOS_LENGTH          =   0x%x\n", VGA_BIOS_LENGTH);
  printf("VMXASSIST_START          =   0x%x\n", VMXASSIST_START);
  printf("VMXASSIST_LENGTH         =   0x%x\n", VMXASSIST_LENGTH);
  printf("\n");
  printf("KERNEL_START             =   0x%x\n", KERNEL_START);
  printf("KERNEL_END               =   0x%x\n", KERNEL_END);
  printf("VM_BOOT_PACKAGE_START    =   0x%x\n", VM_BOOT_PACKAGE_START);
  printf("VM_BOOT_PACKAGE_END      =   0x%x\n", VM_BOOT_PACKAGE_END);

  
  return 0;

}
