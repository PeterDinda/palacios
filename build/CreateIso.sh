#!/bin/sh

#make DEBUG_ALL=1 world
cp vmm.img iso
mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o test.iso iso

