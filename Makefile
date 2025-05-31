#
# Makefile 
#
# Type  make        to compile all the programs
# Type  make clean  to remove the executables


#gcc <filename>.c -lpthread

CC = gcc
CFLAGS = -Wall -g
TARGETS = nfs_manager nfs_console worker nfs_client

TRG_MANAGER = nfs_manager.o List.o Queue.o 

all: $(TARGETS)

nfs_manager: $(TRG_MANAGER)
	$(CC) $(CFLAGS) -o nfs_manager $(TRG_MANAGER)

nfs_console: nfs_console.c
	$(CC) $(CFLAGS) -o nfs_console nfs_console.c

nfs_client: nfs_client.c
	$(CC) $(CFLAGS) -o nfs_client nfs_client.c

worker: worker.c
	$(CC) $(CFLAGS) -o worker worker.c

nfs_manager.o: nfs_manager.c List.h Queue.h
	$(CC) $(CFLAGS) -c nfs_manager.c

List.o: List.c List.h
	$(CC) $(CFLAGS) -c List.c

Queue.o: Queue.c Queue.h
	$(CC) $(CFLAGS) -c Queue.c

clean:
	rm -f nfs_manager nfs_console worker *.o nfs_in nfs_out manager_logfile.txt console_logfile.txt
