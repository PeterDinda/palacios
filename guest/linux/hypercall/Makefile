all: hypercall

hypercall: hypercall.c hcall.h
	gcc -Wall -static hypercall.c -o hypercall

clean:
	rm -f hypercall
