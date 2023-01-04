# This assignment is based on the File Systems project by Prof. Thain
#
GCC=/usr/bin/gcc
HEADER=fs.h disk.h 
SRC=shell.c fs.c disk.c

tinyfs: shell.o fs.o disk.o
	$(GCC) shell.o fs.o disk.o -o tinyfs

shell.o: shell.c
	$(GCC) -Wall shell.c -c -o shell.o -g

fs.o: fs.c fs.h
	$(GCC) -Wall fs.c -c -o fs.o -g

disk.o: disk.c disk.h
	$(GCC) -Wall disk.c -c -o disk.o -g

submit345:
	turnin -c cis345s -p proj3 Makefile p3_report.pdf ${HEADER} ${SRC}

submit545:
	turnin -c cis545s -p proj3 Makefile p3_report.pdf ${HEADER} ${SRC}

clean:
	rm tinyfs disk.o fs.o shell.o
