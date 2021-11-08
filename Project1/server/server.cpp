//a simple server program, get request from client and send file to client
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
#include <string>
#include <sstream>

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

//replace to_string
string intToStr(int in) {
    stringstream ss;
    ss << in;
    return ss.str();
}

int main(int argc, char *argv[]) {
    int sockfd; //socket
    struct sockaddr_in servaddr; //server address
    struct sockaddr_in cliaddr; //client address
    int servlen; //server length
    int clilen; //clien length
    int recvnum; 
    int sendnum;
    char recvbuf[BUFFSIZE]; //receive buff
    char sendbuf[BUFFSIZE]; //send buff
    int filelen; //file length
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
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servlen = sizeof(servaddr);
    //bind
    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("error: bind");
        exit(1);
    }
    clilen = sizeof(cliaddr);
    //print start info
    cout << "==============" << endl;
    cout << "|server start|" << endl;
    cout << "==============" << endl;
    //loop
    while(true) {
        cout << "server wait..." << endl;
        //recive
        bzero(&recvbuf, BUFFSIZE);
        recvnum = recvfrom(sockfd, recvbuf, BUFFSIZE, 0, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen);
        if(recvnum < 0) {
            perror("error: recvfrom");
            exit(1);
        }
        //if receive request, print obtained data
        cout << "server receive request from client" << endl;
        cout << "server receive " << recvnum << " bytes: " << recvbuf << endl;
        //get file
        char filename[128];
        char *pfilecont;
        sscanf(recvbuf, "GET %s HTTP/1.0", filename);
        FILE *pf = fopen(filename, "r");
        if (pf == NULL) {
            perror("error: file not found");
            exit(1);
        }
        fseek(pf, 0, SEEK_END);
        filelen = ftell(pf);
        pfilecont = (char *)malloc(sizeof(char) * filelen);
        if (pfilecont == NULL) {
            perror("error: malloc");
            exit(1);
        }
        rewind(pf);
        fread(pfilecont, sizeof(char), filelen, pf);
        fclose(pf);
        //send title
        bzero(&sendbuf, BUFFSIZE);
        string title = "HTTP/1.0 200 Document Follows\r\nContent-Type: text/plain\r\nContent-Length: ";
        title += intToStr(filelen);
        title += "\r\n\r\n";
        strcpy(sendbuf, title.c_str());
        sendnum = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&cliaddr, clilen);
        if(sendnum < 0) {
            perror("error: sendto");
            exit(1);
        }
        //send file length
        string filelength = intToStr(filelen);
        strcpy(sendbuf, filelength.c_str());
        sendnum = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&cliaddr, clilen);
        if(sendnum < 0) {
            perror("error: sendto");
            exit(1);
        }
        //packet[512]
        //packet[0-5]: checksum
        //packet[6-9]: sequence
        //packet[10-511]: data
        //if it is the last packet, the length will < 512
        cout << "sending..." << endl;
        for (int i = 0; i < (filelen + (DATA_SIZE - 1)) / DATA_SIZE; i++) {
            bzero(&sendbuf, BUFFSIZE);
            bool ended = false;
            if (i > 9999) {
                perror("error: file is too large(>5020000 bytes)");
                exit(1);
            }
            //determine whether it is the last package
            int pktlen = PACKET_SIZE;
            if (i == filelen / DATA_SIZE) {
                pktlen = filelen % DATA_SIZE + HEADER_SIZE;
                ended = true;
            }
            char pkt[pktlen];
            memset(&pkt, 0, sizeof(pkt));
            //put sequence header
            pkt[6] = i / 1000 % 10 + '0';
            pkt[7] = i / 100 % 10 + '0';
            pkt[8] = i / 10 % 10 + '0';
            pkt[9] = i % 10 + '0';
            //put data
            for(int j = HEADER_SIZE; j < pktlen; j++) {
                pkt[j] = pfilecont[(i * DATA_SIZE) + (j - HEADER_SIZE)];
            }
            int csum = checksum(pkt, pktlen);
            //put checksum header
            pkt[0] = csum / 100000 % 10 + '0';
            pkt[1] = csum / 10000 % 10 + '0';
            pkt[2] = csum / 1000 % 10 + '0';
            pkt[3] = csum / 100 % 10 + '0';
            pkt[4] = csum / 10 % 10 + '0';
            pkt[5] = csum % 10 + '0';
            //put packet in buff
            for(int j = 0; j < pktlen; j++) {
                sendbuf[j] = pkt[j];
            }
            //send
            cout << "send packet [" << i << "] size: " << sizeof(pkt) << " bytes to client" << endl;
            sendnum = sendto(sockfd, (char *)sendbuf, pktlen, 0, (struct sockaddr *)&cliaddr, clilen);
            if (sendnum < 0) {
                perror("error: sendto");
                exit(1);
            }
            //send NULL after sending all packet
            if(ended) {
                cout << "all packet sent" << endl;
                bzero(&sendbuf, BUFFSIZE);
                sendnum = sendto(sockfd, (char *)sendbuf, 0, 0, (struct sockaddr *)&cliaddr, clilen);
                if (sendnum < 0) {
                    perror("error: sendto");
                    exit(1);
                }
            }
        }
        cout << endl;
        free(pfilecont);
    }
    return 0;
}
