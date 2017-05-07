# NAME: Bach Hoang
# ID: 104737449
# EMAIL: ko.wing.bird@gmail.com

.SILENT:
files = fuck
ID = 104737449
tar_name = lab2a-$(ID).tar.gz
CC = gcc
CFLAG = -g -Wall -std=gnu99 -lrt -pthread

default: add

add:
	$(CC) $(CFLAG) -o lab2_add lab2_add.c

dist:
	tar -czf $(tar_name) $(files)
clean:
	rm -f lab2a-add *.o
