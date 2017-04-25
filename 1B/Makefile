# NAME:  Bach Hoang
# ID:    104737449
# EMAIL: ko.wing.bird@gmail.com

.SILENT:
files = lab1b-client.c lab1b-server.c Makefile my.key README
ID = 104737449
tar_name = lab1b-$(ID).tar.gz
CC = gcc
CFLAG = -g -Wall -D_GNU_SOURCE

default: server client

server:
	$(CC) $(CFLAG) -lmcrypt -std=c99 -o lab1b-server lab1b-server.c
client:
	$(CC) $(CFLAG) -lmcrypt -std=c99 -o lab1b-client lab1b-client.c	
dist:
	tar -czf $(tar_name) $(files)
clean:
	rm -f lab1b-server lab1b-client *.o