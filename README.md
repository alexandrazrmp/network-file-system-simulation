# Network File System Simulation

## Overview
This project implements a Network File Synchronization System composed of three main components:
- nfs_manager
- nfs_console
- nfs_client

The system allows synchronization of files between directories across a network using a manager-worker architecture.

---

## Compilation

Compile all components using:
make

---

## Execution

### Run manager
./nfs_manager -l <manager_logfile> -c <config_file> -n <worker_limit> -p <port_number> -b <bufferSize>

Note: worker limit is optional (default is used if omitted)

### Run console
./nfs_console -l <console_logfile> -h <host_IP> -p <host_port>

### Run client
./nfs_client -p <port_number>

---

## System Architecture

### nfs_manager
- Initializes worker limit, logfile, and configuration file
- Reads source-target directory pairs from config
- Connects to clients and retrieves file lists
- Stores tasks in a queue (FIFO)
- Uses worker threads to synchronize files
- Uses mutexes for worker control
- Handles instructions from console:
  - add <source> <target>
  - cancel <source>
  - shutdown

---

### nfs_console
- Connects to manager
- Accepts user commands:
  - add <source> <target>
  - cancel <source>
  - shutdown
- Logs all commands
- Sends valid instructions to manager

---

### nfs_client
- Runs continuously (multiple instances allowed)
- Listens for incoming connections
- Supports:
  - LIST → returns file names from directory
  - PULL → sends file data to manager worker

---

## Notes / Limitations

- Worker threads use busy-waiting when queue is empty
- PUSH operation is partially implemented (file opens but data transfer incomplete)
- PULL works correctly
- File transfer is not implemented in streaming loops (single-pass)

---

## Technologies Used
- C
- POSIX Threads
- Sockets (TCP/IP)
- Linux environment
