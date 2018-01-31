/* 
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#define BUFSIZE 1024
#define MAX_FILENAME_SIZE 30

/* 
 * error - wrapper for perror
 */

using namespace std;

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd,(struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");

    //Read filename from user
    string filename;
    cout<<"Enter File name"<<endl;
    cin>>filename;
    FILE * file;
    file = fopen(filename.c_str(),"r");
    fseek(file,0,SEEK_END);
    int filesize = ftell(file);
    fseek(file,0,SEEK_SET);

    bzero(buf, BUFSIZE);
    //char * intsz = itoa(filesize);
    std::stringstream ss;
    ss << filesize;
    string msg = filename + " " + ss.str();
    strcpy(buf,msg.c_str()); 

    /* send the message line to the server */
    n = write(sockfd, buf, strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");

    /* print the server's reply */
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, BUFSIZE);
    if (n < 0) 
      error("ERROR reading from socket");
    if(strcmp(buf,msg.c_str())==0){
        cout<<"Ackowledgement from server succesfully received"<<endl;
    }
    else    {
        cout<<"UNSUCCESSFULL Acknowledgement from server"<<endl;
    }

    cout<<"Sending File as Byte Stream"<<endl;

    int file_readSize;
    char verify = '0';
    while(!feof(file))
    {
        file_readSize = fread(buf, 1, sizeof(buf)-1, file);
        write(sockfd, buf, file_readSize);

        while(read(sockfd, &verify, sizeof(char))<0);

        if(verify!= '1'){
          cout<<"Error Receiving acknowledgement from server"<<endl;
        }
        verify = '0';
        bzero(buf, BUFSIZE);
    }


    close(sockfd);
    return 0;
}
