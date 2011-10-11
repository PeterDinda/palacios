all: v3_ctrl v3_stop v3_cons v3_mem v3_monitor v3_stream v3_user_host_dev_example v3_os_debug v3_user_keyed_stream_example v3_user_keyed_stream_file v3_core_move v3_save v3_load



v3_ctrl : v3_ctrl.c v3_pause.c v3_continue.c v3_launch.c v3_stop.c v3_create.c v3_free.c
	gcc -static v3_launch.c -o v3_launch
	gcc -static v3_stop.c -o v3_stop 
	gcc -static v3_pause.c -o v3_pause
	gcc -static v3_continue.c -o v3_continue
	gcc -static v3_create.c -o v3_create
	gcc -static v3_free.c -o v3_free


v3_save : v3_save.c v3_ctrl.h
	gcc -static v3_save.c -o v3_save 
v3_load : v3_load.c v3_ctrl.h
	gcc -static v3_load.c -o v3_load 

v3_mem : v3_mem.c v3_ctrl.h
	gcc -static v3_mem.c -o v3_mem 

v3_cons : v3_cons.c v3_cons_sc.c v3_ctrl.h
	gcc  -static v3_cons.c -o v3_cons -lcurses
	gcc  -static v3_cons_sc.c -o v3_cons_sc -lcurses

v3_stream : v3_stream.c v3_ctrl.h
	gcc -static v3_stream.c -o v3_stream 

v3_monitor : v3_cons.c v3_ctrl.h
	gcc -static v3_monitor.c -o v3_monitor

v3_user_host_dev_example: v3_user_host_dev_example.c v3_user_host_dev.h v3_user_host_dev.c
	gcc -static -I../linux_module v3_user_host_dev_example.c v3_user_host_dev.c -o v3_user_host_dev_example

v3_os_debug: v3_os_debug.c v3_user_host_dev.h v3_user_host_dev.c
	gcc -static -I../linux_module v3_os_debug.c v3_user_host_dev.c -o v3_os_debug

v3_user_keyed_stream_example: v3_user_keyed_stream_example.c v3_user_keyed_stream.h v3_user_keyed_stream.c
	gcc -static -I../linux_module v3_user_keyed_stream_example.c v3_user_keyed_stream.c -o v3_user_keyed_stream_example


v3_user_keyed_stream_file: v3_user_keyed_stream_file.c v3_user_keyed_stream.h v3_user_keyed_stream.c
	gcc -static -I../linux_module v3_user_keyed_stream_file.c v3_user_keyed_stream.c -o v3_user_keyed_stream_file

v3_core_move : v3_core_move.c v3_ctrl.h
	gcc -static v3_core_move.c -o v3_core_move

v3_inject_ecc_scrubber_mce: v3_inject_ecc_scrubber_mce.c
	gcc -static -I../linux_module v3_inject_ecc_scrubber_mce.c -o v3_inject_ecc_scrubber_mce

clean:
	rm -f v3_ctrl v3_cons v3_mem v3_monitor v3_stream v3_user_host_dev_example v3_os_debug v3_user_keyed_stream_example v3_user_keyed_stream_file v3_core_migrate
