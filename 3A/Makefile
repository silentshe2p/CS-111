# NAME: Bach Hoang
# ID: 104737449
# EMAIL: ko.wing.bird@gmail.com

.SILENT:
files = lab3a.c ext2_fs.h Makefile README
ID = 104737449
tar_name = lab3a-$(ID).tar.gz
CC = gcc
CFLAG = -g -Wall -std=gnu99

default: lab3a

lab3a:
	$(CC) $(CFLAG) -o lab3a lab3a.c
dist:
	tar -czf $(tar_name) $(files)
clean:
	rm -f lab3a *.o *.h *.c *.csv Makefile README
re:
	rm -f lab3a summary.csv
