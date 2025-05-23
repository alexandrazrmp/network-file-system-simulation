#
# Makefile 
#
# Type  make        to compile all the programs
# Type  make clean  to remove the executables

CC = gcc
CFLAGS = -Wall -g
TARGETS = fss_manager fss_console worker

TRG_MANAGER = fss_manager.o List.o Queue.o

all: $(TARGETS)

fss_manager: $(TRG_MANAGER)
	$(CC) $(CFLAGS) -o fss_manager $(TRG_MANAGER)

fss_console: fss_console.c
	$(CC) $(CFLAGS) -o fss_console fss_console.c

worker: worker.c
	$(CC) $(CFLAGS) -o worker worker.c

fss_manager.o: fss_manager.c List.h Queue.h
	$(CC) $(CFLAGS) -c fss_manager.c

List.o: List.c List.h
	$(CC) $(CFLAGS) -c List.c

Queue.o: Queue.c Queue.h
	$(CC) $(CFLAGS) -c Queue.c

clean:
	rm -f fss_manager fss_console worker *.o fss_in fss_out manager_logfile.txt console_logfile.txt
