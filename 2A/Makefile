# NAME: Bach Hoang
# ID: 104737449
# EMAIL: ko.wing.bird@gmail.com

.SILENT:
files = lab2_add.c lab2_list.c SortedList.h SortedList.c Makefile README lab2_add.csv lab2_list.csv lab2_add-1.png lab2_add-2.png lab2_add-3.png lab2_add-4.png lab2_add-5.png lab2_list-1.png lab2_list-2.png lab2_list-3.png lab2_list-4.png lab2_add.gp lab2_list.gp sample.sh
ID = 104737449
tar_name = lab2a-$(ID).tar.gz
CC = gcc
CFLAG = -g -Wall -std=gnu99 -lrt -pthread

default: build

build: add list

add:
	$(CC) $(CFLAG) -o lab2_add lab2_add.c
list:
	$(CC) $(CFLAG) -o lab2_list SortedList.c lab2_list.c
tests:
	./sample.sh
graphs:
	gnuplot ./lab2_add.gp
	gnuplot ./lab2_list.gp
dist:
	tar -czf $(tar_name) $(files)
clean:
	rm -f lab2-add lab2_list SortedList.h *.c *.o *.png *.csv *.gp sample.sh
