//a simple server program, get request from client and send file to client
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>

using namespace std;

#define SERV_PORT 9877 //default port
#define MAXLINE 4096 
#define BUFFSIZE 8192 
#define PACKET_SIZE 512
#define HEADER_SIZE 6
#define DATA_SIZE 506

//switch int to string, replace to_string
string intToStr(int in) {
    stringstream ss;
    ss << in;
    return ss.str();
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

//exit and print error
void error(string errorName) {
    cout << "Error: " << errorName << endl;
    exit(1);
}

int main(int argc, char const *argv[]) {
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
    int filelen; //file length
    double gremlinProb;

    //print start info
    cout << "================" << endl;
    cout << "| Server start |" << endl;
    cout << "================" << endl;

    while(true) {
        cout << "Enter gremlin probability[0, 1): ";
        cin >> gremlinProb;
        if(gremlinProb >= 0 && gremlinProb < 1) {
            break;
        }
        cout << "Wrong input" << endl;
    }

    //create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == 0) {
        error("socket");
    }

    //initialize
    bzero(&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servlen = sizeof(servaddr);

    //bind
    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        error("bind");
    }
    clilen = sizeof(cliaddr);

    //loop
    while(true) {
        cout << endl;
        cout << "Server wait..." << endl;
        cout << endl;

        //recive
        bzero(&recvbuf, BUFFSIZE);
        recvnum = recvfrom(sockfd, recvbuf, BUFFSIZE, 0, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen);
        if(recvnum < 0) {
            error("recvfrom");
        }

        //if receive request, print obtained data
        cout << "Server receive request from client" << endl;
        cout << "server receive " << recvnum << " bytes: " << recvbuf << endl;

        //get file
        char filename[64];
        char *filecontp;
        sscanf(recvbuf, "GET %s FILE", filename);
        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
            error("file not found");
        }
        fseek(fp, 0, SEEK_END);
        filelen = ftell(fp);
        filecontp = (char *)malloc(sizeof(char) * filelen);
        if (filecontp == NULL) {
            error("malloc");
        }
        rewind(fp);
        fread(filecontp, sizeof(char), filelen, fp);
        fclose(fp);

        //send title
        bzero(&sendbuf, BUFFSIZE);
        string title = "FILE LENGTH ";
        title += intToStr(filelen);
        title += " BYTES";
        strcpy(sendbuf, title.c_str());
        sendnum = sendto(sockfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr *)&cliaddr, clilen);
        if(sendnum < 0) {
            error("sendto");
        }
        cout << "Sending..." << endl;
        int sequence = 0;
        int need = 0;
        bool ended = false;
        bool willend = false;
        while(true) {
            // judge odd and even, if odd seq = 1, if even seq = 0
            if(sequence % 2 == 0) {
                need = 0;
            }
            else {
                need = 1;
            }
            //send NULL after sending all packet
            if(ended) {
                cout << "All packet sent" << endl;
                bzero(&sendbuf, BUFFSIZE);
                sendnum = sendto(sockfd, (char *)sendbuf, 0, 0, (struct sockaddr *)&cliaddr, clilen);
                if (sendnum < 0) {
                    error("sendto");
                }
                break;
            }
            ended = false;
            willend = false;
            if (sequence > 9999) {
                error("file is too large(>5020000 bytes)");
            }
            //determine whether it is the last package
            int slen = PACKET_SIZE;
            if (sequence == filelen / DATA_SIZE) {
                slen = filelen % DATA_SIZE + HEADER_SIZE;
                willend = true;
            }
            char spkt[slen];
            memset(&spkt, 0, sizeof(spkt));
            //put sequence header
            spkt[5] = need + '0';
            //put data
            for(int i = HEADER_SIZE; i < slen; i++) {
                spkt[i] = filecontp[(sequence * DATA_SIZE) + (i - HEADER_SIZE)];
            }
            int csum = checksum(spkt, slen);
            //put checksum header
            spkt[0] = csum / 10000 % 10 + '0';
            spkt[1] = csum / 1000 % 10 + '0';
            spkt[2] = csum / 100 % 10 + '0';
            spkt[3] = csum / 10 % 10 + '0';
            spkt[4] = csum % 10 + '0';
            //put packet in buff
            bzero(&sendbuf, BUFFSIZE);
            for(int j = 0; j < slen; j++) {
                sendbuf[j] = spkt[j];
            }
            //send
            cout << "Send packet [" << sequence << "] size: " << slen << " bytes to client" << endl;
            sendnum = sendto(sockfd, (char *)sendbuf, slen, 0, (struct sockaddr *)&cliaddr, clilen);
            if (sendnum < 0) {
                error("sendto");
            }
            //recive ack
            int timer = 50;
            while(timer > 0) {
                timer--;
                bzero(&recvbuf, BUFFSIZE);
                recvnum = recvfrom(sockfd, recvbuf, BUFFSIZE, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, (socklen_t *)&clilen);
                if(recvnum < 0) {
                    usleep(1000);
                    continue;
                }
                break;
            }
            //build a temporary packet
            int rlen = 1;
            if(recvnum >= 0) {
                rlen = recvnum;
            }
            char rpkt[rlen];
            memset(&rpkt, 0, sizeof(rpkt));
            if(recvnum < 0) {
                continue;
            }
            for(int k = 0; k < rlen; k++) {
                rpkt[k] = recvbuf[k];
            }
            //gremlim packet
            gremlin(rpkt, sizeof(rpkt), gremlinProb);
            //if pkt broke, end this round and resent
            if(!checkPkt(rpkt, rlen)) {
                continue;
            }
            int ack = rpkt[5] - '0';
            if(need != ack) {
                continue;
            }
            if (willend) {
                ended = true;
            }
            sequence++;
        }
        free(filecontp);
    }
    return 0;
}
