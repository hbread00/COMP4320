//a simple client program, send request to server and get file from it by packet
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace std;

#define SERV_PORT 9877 //default port
#define MAXLINE 4096 
#define BUFFSIZE 8192 
#define PACKET_SIZE 512
#define HEADER_SIZE 6
#define DATA_SIZE 506
#define ACK_SIZE 64

//exit and print error
void error(string errorName) {
    cout << "Error: " << errorName << endl;
    exit(1);
}

//check sum of data in package
int checksum(char pkt[], int len) {
    int sum = 0;
    // except checksum header: pkt[0-5]
    for(int i = 5; i < len; i++) {
        sum += (int)pkt[i];
    }
    return sum;
}

//destroy some packets, replace part of packet with '0'
void gremlin(char pkt[], int len, double prob) {
    if(1.0 * rand() / RAND_MAX > prob) {
        return;
    }
    double n = 1.0 * rand() / RAND_MAX;
    if(n >= 0 && n < 0.5) {
        pkt[(int)(1.0 * rand() / RAND_MAX * len)] = '0';
    }
    else if(n >= 0.5 && n < 0.8) {
        for(int i = 0; i < 2; i++) {
            pkt[(int)(1.0 * rand() / RAND_MAX * len)] = '0';
        }
    }
    else {
        for(int j = 0; j < 3; j++) {
            pkt[(int)(1.0 * rand() / RAND_MAX * len)] = '0';
        }
    }   
}

//determine whether the content matches the header
bool checkPkt(char pkt[], int len) {
    int sum = (pkt[0] - '0') * 10000 
            + (pkt[1] - '0') * 1000 
            + (pkt[2] - '0') * 100 
            + (pkt[3] - '0') * 10 
            + (pkt[4] - '0');
    return sum == checksum(pkt, len);
}

//reassemble packet to file
void reassemble(char pkt[], char *content, int pktlen, int sequence) {
    int seq = sequence;
    for(int i = 0; i < pktlen - HEADER_SIZE; i++) {
        content[seq * DATA_SIZE + i] = pkt[i + HEADER_SIZE];
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int sockfd; //socket
    struct sockaddr_in servaddr; //server address
    struct sockaddr_in cliaddr; //client address
    int servlen; //server length
    int clilen; //clien length
    int recvnum;
    int sendnum;
    char recvbuf[BUFFSIZE]; //receive buff
    char sendbuf[BUFFSIZE]; //send buff
    char *hostnamep; //server host name
    double gremlinprob; //probability of gremlin
    double loseprob; //probability of lose packet
    struct hostent *servnamep;
    int filelen; //file length
    //check input IP
    if (argc != 2) {
        error("no IP");
    }
    hostnamep = argv[1];
    //check get valid IP
    servnamep = gethostbyname(hostnamep);
    if (servnamep == NULL) {
        error("servname");
    }
    //create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == 0) {
        error("sockfd");
    }
    //initialize
    bzero(&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    servaddr.sin_addr.s_addr = inet_addr(hostnamep);
    servlen = sizeof(servaddr);
    //print start info
    cout << "================" << endl;
    cout << "| Client start |" << endl;
    cout << "================" << endl;
    //get gremlin probability
    while(true) {
        cout << "Enter gremlin probability[0, 1): ";
        cin >> gremlinprob;
        if(gremlinprob >= 0 && gremlinprob < 1) {
            break;
        }
        cout << "Wrong input" << endl;
    }
    //get gremlin probability
    while(true) {
        cout << "Enter packet lose probability[0, 1): ";
        cin >> loseprob;
        if(loseprob >= 0 && loseprob < 1) {
            break;
        }
        cout << "Wrong input" << endl;
    }
    //send file name to server
    char filename[128];
    cout << "Enter the requested file name: ";
    cin >> filename;
    //build the sending text
    string reqsend = "GET ";
    reqsend += filename;
    reqsend += " FILE";
    cout << "Client send: " << reqsend << endl;
    bzero(&sendbuf, BUFFSIZE);
    strcpy(sendbuf, reqsend.c_str());
    sendnum = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&servaddr, servlen);
    if(sendnum < 0) {
        error("sento");
    }
    //receive server respond
    recvnum = recvfrom(sockfd, recvbuf, BUFFSIZE, 0, (struct sockaddr *)&servaddr, (socklen_t *)&servlen);
    if(recvnum < 0) {
        error("revfrom");
    }
    cout << "Server respond: " << recvbuf << endl;
    sscanf(recvbuf, "FILE LENGTH %d BYTES", &filelen);
    if(filelen == -1) {
        error("no file");
    }
    //create file stream
    char *filecontp = (char *)malloc(sizeof(char) * filelen);
    //receive packet
    cout << "Receiving..." << endl;
    int sequence = 0;
    int need = 0;
    while(true) {
        // judge odd and even, if odd seq = 1, if even seq = 0
        if(sequence % 2 == 0) {
            need = 0;
        }
        else {
            need = 1;
        }
        //receive
        recvnum = recvfrom(sockfd, recvbuf, BUFFSIZE, 0, (struct sockaddr *)&servaddr, (socklen_t *)&servlen);
        if(recvnum < 0) {
            error("revfrom");
        }
        //receive null, means end
        if(recvnum == 0) {
            cout << "All packet received" << endl;
            break;
        }
        //build a temporary packet
        int pktlen = recvnum;
        char pkt[pktlen];
        memset(&pkt, 0, sizeof(pkt));
        for(int i = 0; i < pktlen; i++) {
            pkt[i] = recvbuf[i];
        }
        int check = pkt[5] - '0';
        int show = sequence;
        if(check != need) {
            show = sequence - 1;
        }
        cout << "Recieve packet [" << show << "] size: " << pktlen << " bytes from server | ";
        //gremlim packet
        gremlin(pkt, sizeof(pkt), gremlinprob);
        //print packet check result
        if(checkPkt(pkt, pktlen)) {
            int seq = pkt[5] - '0';
            if(seq != need) {
                cout << "error" << endl;
                char ackpkt[ACK_SIZE];
                memset(&ackpkt, 0, sizeof(ackpkt));
                int last = 0;
                if(need == 0) {
                    last = 1;
                }
                else {
                    last = 0;
                }
                ackpkt[5] = last + '0';
                for(int j = HEADER_SIZE; j < ACK_SIZE; j++) {
                    ackpkt[j] = '1';
                }
                int sum = checksum(ackpkt, ACK_SIZE);
                ackpkt[0] = sum / 10000 % 10 + '0';
                ackpkt[1] = sum / 1000 % 10 + '0';
                ackpkt[2] = sum / 100 % 10 + '0';
                ackpkt[3] = sum / 10 % 10 + '0';
                ackpkt[4] = sum % 10 + '0';
                continue;
            }
            cout << "pass" << endl;
            //lose packet
            if(1.0 * rand() / RAND_MAX > loseprob) {
                continue;
            }
            char ackpkt[ACK_SIZE];
            memset(&ackpkt, 0, sizeof(ackpkt));
            ackpkt[5] = need + '0';
            for(int j = HEADER_SIZE; j < ACK_SIZE; j++) {
                ackpkt[j] = '1';
            }
            int sum = checksum(ackpkt, ACK_SIZE);
            ackpkt[0] = sum / 10000 % 10 + '0';
            ackpkt[1] = sum / 1000 % 10 + '0';
            ackpkt[2] = sum / 100 % 10 + '0';
            ackpkt[3] = sum / 10 % 10 + '0';
            ackpkt[4] = sum % 10 + '0';
            //put packet in buff
            bzero(&sendbuf, BUFFSIZE);
            for(int k = 0; k < ACK_SIZE; k++) {
                sendbuf[k] = ackpkt[k];
            }
            sendnum = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&servaddr, servlen);
            if(sendnum < 0) {
                error("sento");
            }
        }
        else {
            cout << "error" << endl;
            sendnum = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&servaddr, servlen);
            if(sendnum < 0) {
                error("sento");
            }
            continue;
        }
        //assenble pkt
        reassemble(pkt, filecontp, pktlen, sequence);
        sequence++;
    }
    //write file
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        error("open file");
    }
    fwrite(filecontp, sizeof(char), filelen, fp);
    fclose(fp);
    free(filecontp);
    cout << "File: " << filename << " has been obtained" << endl;
}

