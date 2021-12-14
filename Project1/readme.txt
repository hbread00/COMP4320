Introduction: 
A simple program reliableealize unreliable network transmission based on UDP protocol

How to compile: 
g++ server.cpp -o server
g++ client.cpp -o client

How to run: 
./server
./client <IP address>

How to use: 
The "server.cpp" and the files to be transferred are stored in the directory "server"
The "client.cpp" and the obtained files are stored in the directory "client"
After compiling, run the two programs separately and enter according to the program prompts

Message package structure：
|----------- 512 bytes -----------|
| Checksum | Sequence |--- Data --|
|- 6 bytes |- 4 bytes | 502 bytes |
