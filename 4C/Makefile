# NAME: Bach Hoang
# ID: 104737449
# EMAIL: ko.wing.bird@gmail.com

.SILENT:
files = lab4c_tcp.c lab4c_tls.c Makefile README
ID = 104737449
tar_name = lab4c-$(ID).tar.gz
CC = gcc
CFLAG = -g -Wall -std=gnu99

default: tcp

tcp:
	$(CC) $(CFLAG) -o lab4c_tcp lab4c_tcp.c
tls:
	$(CC) $(CFLAG) -o lab4c_tls lab4c_tls.c
dist:
	tar -czf $(tar_name) $(files)
clean:
	rm -f lab4c_tcp lab4c_tls *.o
