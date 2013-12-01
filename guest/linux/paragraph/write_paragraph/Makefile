all: write_paragraph_devmem write_paragraph_sysfs

write_paragraph_devmem: write_paragraph_devmem.c
	gcc -static write_paragraph_devmem.c -o write_paragraph_devmem

write_paragraph_sysfs: write_paragraph_sysfs.c
	gcc -static write_paragraph_sysfs.c -o write_paragraph_sysfs

clean:
	rm -f write_paragraph_devmem write_paragraph_sysfs

