//a simple client program, send request to server and get file from it by packet
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

#define SERV_PORT 9877 //default port
#define MAXLINE 4096 
#define BUFFSIZE 8192
#define PACKET_SIZE 512
#define DATA_SIZE 502
#define HEADER_SIZE 10

//check sum of data in package, except checksum header: pkt[0-5]
int checksum(char c[], int len) {
    int sum = 0;
    for(int i = 6; i < len; i++) {
        sum += (int)c[i];
    }
    return sum;
}

//destroy some packets, replace part of packet with 'x'
void gremlin(char c[], int len, double p) {
    if(1.0 * rand() / RAND_MAX > p) {
        return;
    }
    double n = 1.0 * rand() / RAND_MAX;
    if(n >= 0 && n < 0.5) {
        c[(int)(1.0 * rand() / RAND_MAX * len)] = '0';
    }
    else if(n >= 0.5 && n < 0.8) {
        for(int i = 0; i < 2; i++) {
            c[(int)(1.0 * rand() / RAND_MAX * len)] = '0';
        }
    }
    else {
        for(int j = 0; j < 3; j++) {
            c[(int)(1.0 * rand() / RAND_MAX * len)] = '0';
        }
    }   
}

//determine whether the content matches the header
bool checkPkt(char pkt[], int len) {
    int sum = (pkt[0] - '0') * 100000 + (pkt[1] - '0') * 10000 + (pkt[2] - '0') * 1000 
            + (pkt[3] - '0') * 100 + (pkt[4] - '0') * 10 + (pkt[5] - '0');
    return sum == checksum(pkt, len);
}

//reassemble packet to file
void reassemble(char pkt[], char *content, int pktlen) {
    int seq = (pkt[6] - '0') * 1000 + (pkt[7] - '0') * 100 + (pkt[8] - '0') * 10 + (pkt[9] - '0');
    for(int i = 0; i < pktlen - HEADER_SIZE; i++) {
        content[seq * DATA_SIZE + i] = pkt[i + HEADER_SIZE];
    }
}

int main(int argc, char *argv[]) {
    while (true) {
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
        char *phostname; //server host name
        double gremlinp; //probability of gremlin
        struct hostent *pservname;
        int filelen; //file length
        //check input IP
        if (argc != 2) {
            perror("error: no IP");
            exit(1);
        }
        phostname = argv[1];
        //check get valid IP
        pservname = gethostbyname(phostname);
        if (pservname == NULL) {
            perror("error: servname");
            exit(1);
        }
        //create socket
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == 0) {
            perror("error: sockfd");
            exit(1);
        }
        //initialize
        bzero(&servaddr, sizeof(struct sockaddr_in));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(SERV_PORT);
        servaddr.sin_addr.s_addr = inet_addr(phostname);
        servlen = sizeof(servaddr);
        //print start info
        cout << "==============" << endl;
        cout << "|client start|" << endl;
        cout << "==============" << endl;
        //get gremlin probability
        while(true) {
            cout << "enter gremlin probability[0, 1]: ";
            cin >> gremlinp;
            if(gremlinp >= 0 && gremlinp <= 1) {
                break;
            }
            cout << "wrong input" << endl;
        }
        //send file name to server
        bzero(&sendbuf, BUFFSIZE);
        char filename[128];
        cout << "enter the requested file name: ";
        cin >> filename;
        //build the sending text
        string reqsend = "GET ";
        reqsend += filename;
        reqsend += " HTTP/1.0";
        cout << "client send: " << reqsend << endl;
        strcpy(sendbuf, reqsend.c_str());
        sendnum = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&servaddr, servlen);
        if(sendnum < 0) {
            perror("error: sento");
            exit(1);
        }
        //receive server respond
        recvnum = recvfrom(sockfd, recvbuf, BUFFSIZE, 0, (struct sockaddr *)&servaddr, (socklen_t *)&servlen);
        if(recvnum < 0) {
            perror("error: revfrom");
            exit(1);
        }
        cout << "server respond: \n" << recvbuf;
        //get file length
        recvnum = recvfrom(sockfd, recvbuf, BUFFSIZE, 0, (struct sockaddr *)&servaddr, (socklen_t *)&servlen);
        if(recvnum < 0) {
            perror("error: recvfrom");
            exit(1);
        }
        sscanf(recvbuf, "%d", &filelen);
        if(filelen == -1) {
            cout << "client end" << endl;
            break;
        }
        //create file stream
        char *pfilecont = (char *)malloc(sizeof(char) * filelen);
        FILE *pf = fopen(filename, "w");
        if (pf == NULL) {
            perror("error: open file");
            exit(1);
        }
        //receive packet
        cout << "receiving..." << endl;
        while(true) {
            //receive
            recvnum = recvfrom(sockfd, recvbuf, BUFFSIZE, 0, (struct sockaddr *)&servaddr, (socklen_t *)&servlen);
            if(recvnum < 0) {
                perror("error: revfrom");
                exit(1);
            }
            //receive null, means end
            if(recvnum == 0) {
                cout << "all packet received" << endl;
                break;
            }
            //build a temporary packet
            int pktlen = recvnum;
            char pkt[pktlen];
            memset(&pkt, 0, sizeof(pkt));
            for(int i = 0; i < pktlen; i++) {
                pkt[i] = recvbuf[i];
            }
            //gremlim packet
            gremlin(pkt, sizeof(pkt), gremlinp);
            int seq = ((pkt[6] - '0') * 1000 + (pkt[7] - '0') * 100 + (pkt[8] - '0') * 10 + (pkt[9] - '0'));
            cout << "recieve packet [" << seq << "] size: " << pktlen << " bytes from server | ";
            //print packet check result
            if(checkPkt(pkt, pktlen)) {
                cout << "pass" << endl;
            }
            else {
                cout << "error" << endl;
            }
            //assenble pkt
            reassemble(pkt, pfilecont, pktlen);
        }
        //write file
        fwrite(pfilecont, sizeof(char), filelen, pf);
        fclose(pf);
        free(pfilecont);
        cout << "file: " << filename << " has been obtained" << endl;
        char con;
        while(true) {
            cout << "continue(y/n): ";
            cin >> con;
            if(con == 'y' || con == 'n') {
                break;
            }
            cout << "wrong input" << endl;
        }
        if(con == 'n') {
            cout << "client end" << endl;
            break;
        }
    }
    return 0;
}
