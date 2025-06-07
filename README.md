Compilation Instructions:

compile all using make

run manager using 
./nfs_manager -l <manager_logfile> -c <config_file> -n <worker_limit> -p <port_number> -b <bufferSize>
(worker limit is optional since there is a default)

run console using
./nfs_console -l <console-logfile> -h <host_IP> -p <host_port>

run client using
./nfs_client -p <port_number>

Technical Report:

Implemented a Network File Synchronization System using 3 components(executables): nfs_manager, nfs_console and nfs_client.

nfs_manager: 
Takes input given by user as arguements in main function and initializes worker limit, its logfile and the configuration file which has pairs of 
directories (source and target) that are meant to get synchronized. 
It then calls function get_list for all entries which connects to the source directory's port and gets all file names in the directory and stores
them in a queue. This queue is used for hadling a worker limit amount of worker processes that sync a single source file to a single target file
and maintaining order in the process...
Then a single worker handler thread starts that will handle such processes.
Using mutexes for handling when worker limit is not reached and only busy-waiting when queue is empty (could fix that, no time) it starts
new worker threads with FIFO order.
The manager then tries to connect to the console and enters a while(1) loop that will get instructions from the console.
Instructions can be addition of many source and target directory processes in queue, cancellation of many source->target directory processes in 
queue, or shutdown, where the loop breaks.


nfs_console:
The console executable has a quite simple implementation. 
It first connects to the manager on the port that is given to it as an arguement.
It takes a logfile as std input through main function arguements where it stores all instructions it gets.
Instructions are given to the console in the form :
add <source> <target>, cancel <source>, shutdown
inside a while(1) loop that only breaks when shutdown instruction is given or some unexpected error occures
(invalid input is simply ignored)
It parses the instruction, making sure it is in valid form and writes to its logfile accordingly, before sending it to the manager through the
socket where they are connected.


nfs_client:
An executrable that runs in the background, always. Many clients can run at once.
Clients are listening for connections so they are behaving more like servers.
They can accept 3 instructions:
List (sent by manager):
where they simply send the manager the file names of a flat directory that is local to them (the specific dir they were asked to)
Pull (sent by a worker thread of nfs_manager) : 
where they are sending through the socket 


Other points:
push is half-implemented (it only opens target file but fails to get data from worker), however pull works correctly!! (and the worker gets the data correctly!)