# NAME:  BACH HOANG
# ID:    104737449
# EMAIL: ko.wing.bird@gmail.com

.SILENT:
files = Makefile lab1a.c README
ID = 104737449
tar_name = lab1a-$(ID).tar.gz
CC = gcc
CFLAG = -g -Wall
SRC = lab1a.c
OBJ = lab1a

all:
	$(CC) $(FLAG) -std=c99 -o $(OBJ) $(SRC)
dist:
	tar -czf $(tar_name) $(files)
clean:
	rm -f lab1a *.o
