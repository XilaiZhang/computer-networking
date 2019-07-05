#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include<signal.h> 
#include "RDTConnection.h"

using namespace std;

int sockfd;
int is_connnected;
char filename[264];

void handle_signal (int sig)
{
    int fd;
    if (sig == SIGQUIT || sig == SIGTERM)
    {
        if (is_connnected)
        {
            char msg[] = "INTERRUPT"; 
            fd = creat(filename, 0777);
            write(fd, msg, strlen(msg));
            close(sockfd);
            exit(0);
        } 

    }
}

int main (int argc, char *argv[]){

    int portno;
    if(argc!=2){
      fprintf(stderr,"Error: wrong number of inputs for server.\n");
      exit(1);
    }

    portno=atoi(argv[1]);
    printf("Use port number: %d\n", portno);
    sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd == -1)
    {
        fprintf(stderr, "Error creating sockets.\n");
        exit(1);
    }

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons((unsigned short)portno);
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) 
    { 
        fprintf(stderr,"Error binding sockets: %s\n", strerror(errno)); 
        exit(1);
    }

    int ret;
    int fd;
    int file_num = 0;
    is_connnected = 0;
    unsigned int addrlen = sizeof(client_addr);
    // RDTConnection* conn = new RDTConnection();

    signal(SIGQUIT, handle_signal);
    signal(SIGTERM, handle_signal);
    while (true){
        RDTConnection* conn = new RDTConnection();
        ret = conn->server_handshake(sockfd, (struct sockaddr*) &client_addr, &addrlen);
        if (ret)
        {
          is_connnected = 1;
          file_num++;
        	cerr << "Handshake success at server." << endl;
        	cerr << "Start to receiving file " << filename << endl;
          sprintf(filename, "%d.file", file_num);
          fd = creat(filename, 0777);
          if (fd == -1)
          {
            cerr << "Error creating file " << filename << endl;
            exit(1);
          }
          if (conn->receive_file(sockfd, (struct sockaddr*) &client_addr, &addrlen, filename))
          {
            cerr << "Successfully receive file: " << filename << endl;
            conn->server_close(sockfd, (struct sockaddr*) &client_addr, &addrlen);
            cerr << "Connection closed" << endl;
          }
          else
          {
            cerr << "ERROR receive file " << filename << endl;
          }
        }
        else
        {
          cerr << "Failed handshake at server" << endl;
        }
        is_connnected = 0;
    }
  
}
