
.PHONY: all
all: bios

.PHONY: bios
bios: biossums BIOS-bochs-latest

.PHONY: clean
clean:
	rm -f  *.o *.a *.s rombios.bin _rombios*_.c
	rm -f  as86-sym.txt ld86-sym.txt 
	rm -f  rombios*.txt rombios*.sym usage biossums
	rm -f  BIOS-bochs-*

BIOS-bochs-latest: rombios.c biossums
	gcc -DBX_SMP_PROCESSORS=1 -E -P $< > _rombios_.c
	bcc -o rombios.s -C-c -D__i86__ -0 -S _rombios_.c
	sed -e 's/^\.text//' -e 's/^\.data//' rombios.s > _rombios_.s
	as86 _rombios_.s -b tmp.bin -u- -w- -g -0 -j -O -l rombios.txt
	-perl makesym.perl < rombios.txt > rombios.sym
	mv tmp.bin BIOS-bochs-latest
	./biossums BIOS-bochs-latest
	rm -f _rombios_.s

BIOS-bochs-latest.elf: rombios.c biossums
	gcc -DBX_SMP_PROCESSORS=1 -E -P $< > _rombios_elf.c
	bcc -o rombios_elf.s -C-c -D__i86__ -0 -S _rombios_elf.c
	sed -e 's/^\.text//' -e 's/^\.data//' rombios_elf.s > _rombios_elf.s
	as86 _rombios_elf.s  -u- -w- -g -0 -j -O -l rombios_elf.txt
	-perl makesym.perl < rombios_elf.txt > rombios_elf.sym
	mv tmp.bin BIOS-bochs-latest.elf
	./biossums BIOS-bochs-latest.elf
#	rm -f _rombios_.s

biossums: biossums.c
	gcc -o biossums biossums.c

