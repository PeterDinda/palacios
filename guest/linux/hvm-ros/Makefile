all: libv3_hvm_ros_user.a test

libv3_hvm_ros_user.a: v3_hvm_ros_user.o v3_hvm_ros_user_low_level.o
	ar ruv libv3_hvm_ros_user.a v3_hvm_ros_user.o v3_hvm_ros_user_low_level.o

v3_hvm_ros_user.o: v3_hvm_ros_user.c v3_hvm_ros_user.h 
	gcc -Wall -c v3_hvm_ros_user.c

v3_hvm_ros_user_low_level.o: v3_hvm_ros_user_low_level.S
	gcc -Wall -c v3_hvm_ros_user_low_level.S


test: test.c libv3_hvm_ros_user.a v3_hvm_ros_user.h
	gcc -Wall -static test.c -L. -lv3_hvm_ros_user -o test

clean:
	rm -f libv3_hvm_ros_user.a v3_hvm_ros_user.o v3_hvm_ros_user_low_level.o test
