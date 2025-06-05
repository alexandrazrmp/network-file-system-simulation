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
Then a worker handler thread starts that will handle such processes.
The manager then tries to connect to the console and enters a non ending loop that will get instructions from the console.
Instructions can be addition of a source and target directory, 

//////////////////////////////////////////////////////////////////////////////

nfs_console:
The console executable has a quite simple implementation. It takes a logfile as std input through main function arguements where it stores all
instructions it gets.
Instructions are given to the console in the form :
add <source> <target>, status <directory>, cancel <source>, sync <directory>, shutdown
inside a while(1) loop that only breaks when shutdown instruction is given or some unexpected error occures
(invalid input is simply ignored)
It parses the instruction, making sure it is in valid form and writes to its logfile accordingly, before sending it to the manager through the
nfs_in named pipe.


nfs_client:
An executrable that is being executed through fork() in nfs_manager as its child process.
Its arguements deter the sync operation it must do:
if there is a specific filename where the operation must be done then there is two options:
    (1)delete the file through delete_file()
    (2)write or overwrite the file if it is new or if it is just modified (same operation) 
or else if there is no specific filename then that arguement should be "ALL" and the two operaions above (1) and (2) are done to all files
from the source directory




Other points:
