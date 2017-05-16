# NAME: Bach Hoang
# ID: 104737449
# EMAIL: ko.wing.bird@gmail.com

.SILENT:
files = lab2_list.c SortedList.h SortedList.c Makefile README lab2b_list.csv lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png lab2_list.gp test.sh profile.out
ID = 104737449
tar_name = lab2b-$(ID).tar.gz
CC = gcc
CFLAG = -g -Wall -std=gnu99 -lrt -pthread

default: list

list:
	$(CC) $(CFLAG) -o lab2_list SortedList.c lab2_list.c
tests:
	chmod +x test.sh
	./test.sh
profile: profile.gperf

profile.gperf: lab2_list Makefile
	rm -f ./raw.gperf
	LD_PRELOAD=$(HOME)/lib/libprofiler.so.0 CPUPROFILE=./raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	pprof --text ./lab2_list ./raw.gperf > profile.out
	pprof --list=thread_func ./lab2_list ./raw.gperf >> profile.out
	rm -f ./raw.gperf
graphs:
	gnuplot ./lab2_list.gp
dist:
	tar -czf $(tar_name) $(files)
clean:
	rm -f lab2_list *.h *.c *.o *.png *.csv *.gp test.sh profile.out
