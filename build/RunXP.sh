#!/bin/sh

/usr/local/qemu/bin/qemu-system-x86_64   -serial file:serial.out -m 1024 -fda vmm.img -cdrom /opt/vmm-tools/isos/winxp.iso
