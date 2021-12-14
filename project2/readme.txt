Introduction: 
A simple program reliableealize reliable network transmission based on RDT3.0 protocol

How to compile: 
g++ server.cpp -o server
g++ client.cpp -o client

How to run: 
./server
./client <IP address>
The "server.cpp" and the files to be transferred are stored in the directory "server"
The "client.cpp" and the obtained files are stored in the directory "client"
Run the two programs separately and enter according to the program prompts

Message package structure：
|----------- 512 bytes -----------|
| Checksum | Sequence |--- Data --|
|- 5 bytes |- 1 bytes | 506 bytes |

ACK package structure：
|----------- 64 bytes -----------|
| Checksum | Sequence |-- Data --|
|- 5 bytes |- 1 bytes | 58 bytes |

Sequence is only 0 or 1
