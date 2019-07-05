#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <iostream>
#include "RDTConnection.h"

using namespace std;

int main (int argc, char *argv[]){

    char* hostname;
    char* portno;
    char* filename;

    if(argc!=4){
    fprintf(stderr,"ERROR: invalid command line arguments of client.\n");
    exit(1);
    }

    hostname=argv[1];
    portno=argv[2];
    filename=argv[3];

    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd == -1){
        fprintf(stderr, "Error creating socket.\n");
        exit(1);
    }

    struct hostent* server_name;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port= htons(atoi(portno));
    server_name = gethostbyname(hostname);
    if (server_name == NULL)
    {
        fprintf(stderr, "Error searching hostname.\n");
        exit(1);
    }
    memcpy((char*)&server_addr.sin_addr.s_addr, (char*)server_name->h_addr, server_name->h_length);
    /*
    if(inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)  
    { 
      fprintf(stderr, "Invalid address/ Address not supported\n");
      exit(1);  
    } 
    */
    int ret;
    RDTConnection* conn = new RDTConnection();
    ret = conn->client_handshake(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if (ret)
    {
      cerr << "Handshake success at client." << endl;
      if (conn->send_file(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr), filename))
      {
        cerr << "Finish sending file " << filename << endl;
        conn->client_close(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
        cerr << "Connection closed" << endl;
      }
      else
        cerr << "Failed sending file " << filename << endl;
    }
    close(sockfd);
    return 0;
}
