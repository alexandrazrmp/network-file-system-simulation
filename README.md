Compilation Instructions:

compile all using make

run manager using 
./fss_manager -l <manager_logfile> -c <config_file> -n <worker_limit>
(worker limit is optional)

run console using
./fss_console -l <console-logfile>

if you wish to run worker use 
./worker src_dir tgt_dir ALL FULL
(./worker /home/users/sdi2200048/ergasies/SysPro/hw1-alexandrazrmp/source_dir /home/users/sdi2200048/ergasies/SysPro/hw1-alexandrazrmp/target_dir ALL FULL)
(used for testing)

run bash script using (no need for compialation)
./fss_script.sh -p <path> -c <command>
(not fully implemented)

Technical Report:

Implemented a File Synchronization System using 3 components(executables): fss_manager, fss_console and worker.

fss_manager: 
Takes input given by user as arguements in main function and initializes worker limit, its logfile and the configuration file which has pairs of 
directories (source and target) that are meant to be synchronized at all times. 
It then creates two named pipes (fss_in and fss_out) for communtication with the console executable and parses the configuration file to create 
initial logs to sync_info_mem_store list which is used to keep sync-pair information.
Inotify is initialized and add_directory_watch() is used to associate a watch descriptor to sync_info_mem_store entries (directory pairs,
but actually just the source dir of the pair) so that fss_manager is notified for any changes: deletion, modification or addition.
List sync_info_mem_store and Queue WorkerQueue are initialized globally inside fss_manager and are implemented in List.c and Queue.c.
WorkerQueue is used to store pending worker processes that cannot start because worker limit is reached.
Select is used inside an endless loop to determine whether a signal has occured or input is given through fss_in by the console
Missing requested corrrect logfile format (exec report from worker is being printed to show that it is implemented but not written to the logfile
due to wrong handling of where (in which function) I print to the logfile)
(could change it, but I must start another project, thank you for your understanding)

fss_console:
The console executable has a quite simple implementation. It takes a logfile as std input through main function arguements where it stores all
instructions it gets.
Instructions are given to the console in the form :
add <source> <target>, status <directory>, cancel <source>, sync <directory>, shutdown
inside a while(1) loop that only breaks when shutdown instruction is given or some unexpected error occures
(invalid input is simply ignored)
It parses the instruction, making sure it is in valid form and writes to its logfile accordingly, before sending it to the manager through the
fss_in named pipe.


worker:
An executrable that is being executed through fork() in fss_manager as its child process.
Its arguements deter the sync operation it must do:
if there is a specific filename where the operation must be done then there is two options:
    (1)delete the file through delete_file()
    (2)write or overwrite the file if it is new or if it is just modified (same operation) 
or else if there is no specific filename then that arguement should be "ALL" and the two operaions above (1) and (2) are done to all files
from the source directory

fss_script:
Not fully implemented due to lack of information due to my manager logfile being incomplete.
Purge and list all are implemented with comments explaining each instruction.

Other points:
I have noticed that fss_manager must start with enough workers to parse the config file plus one. From then on it has no problem with the limit.
The queue, i believe, is correct so I have not been able to find what is wrong.