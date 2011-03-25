all: v3_ctrl v3_stop v3_cons v3_mem v3_monitor v3_serial v3_net


v3_ctrl : v3_ctrl.c v3_ctrl.h
	gcc -static v3_ctrl.c -o v3_ctrl 

v3_stop : v3_stop.c v3_ctrl.h
	gcc -static v3_stop.c -o v3_stop 

v3_mem : v3_mem.c v3_ctrl.h
	gcc -static v3_mem.c -o v3_mem 


v3_cons : v3_cons.c v3_ctrl.h
	gcc -static v3_cons.c -o v3_cons -lcurses

v3_serial : v3_serial.c v3_ctrl.h
	gcc -static v3_serial.c -pthread -o v3_serial 

v3_monitor : v3_cons.c v3_ctrl.h
	gcc -static v3_monitor.c -o v3_monitor

v3_net : v3_net.c v3_ctrl.h
	gcc -static v3_net.c -o v3_net

clean:
	rm -f v3_ctrl v3_cons v3_mem v3_monitor v3_serial v3_net
