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
#include <openssl/md5.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define BUFSIZE 1024
#define MAX_FILENAME_SIZE 30

/* 
 * error - wrapper for perror
 */

using namespace std;

struct file_details{
    char filename[MAX_FILENAME_SIZE];
    int filesize;
};

void print_md5_sum(unsigned char* md) {
    int i;
    for(i=0; i <MD5_DIGEST_LENGTH; i++) {
            printf("%02x",md[i]);
    }
}

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
    /*std::stringstream ss;
    ss << filesize;
    string msg = filename + " " + ss.str();
    strcpy(buf,msg.c_str()); */

    file_details filedetails;
    filedetails.filesize = filesize;
    strcpy(filedetails.filename,filename.c_str());
    

    /* send the message line to the server */
    n = write(sockfd, &filedetails, sizeof(filedetails));
    if (n < 0) 
      error("ERROR writing FIle Details to socket");

    /* print the server's reply */
    bzero(buf, BUFSIZE);

    file_details fd_rec;
    n = read(sockfd, &fd_rec, BUFSIZE);
    if (n < 0) 
      error("ERROR reading File Details acknowledgement from socket");

    if(strcmp(filedetails.filename,fd_rec.filename)==0 && filedetails.filesize == fd_rec.filesize){
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

    fclose(file);

    unsigned char result[MD5_DIGEST_LENGTH];
    char * file_buffer_md5;
    int file_descript = open(filename.c_str(), O_RDONLY);

    file_buffer_md5 = (char *) mmap(0, filesize, PROT_READ, MAP_SHARED, file_descript, 0);
    MD5((unsigned char*) file_buffer_md5, filesize, result);
    munmap(file_buffer_md5, filesize); 

    unsigned char result_rec[MD5_DIGEST_LENGTH];
    n = read(sockfd, result_rec, MD5_DIGEST_LENGTH);
    if (n < 0) 
      error("ERROR reading MD5 from socket");
    if(memcmp(result,result_rec,MD5_DIGEST_LENGTH)==0)
    {
        cout<<"MD5 Matched"<<endl;
    }
    else
    {
        cout<<"MD5 NOT Matched"<<endl;
    }

    close(file_descript);


    /*file_buffer = mmap(0, file_size, PROT_READ, MAP_SHARED, file_descript, 0);
    MD5((unsigned char*) file_buffer, file_size, result);
    munmap(file_buffer, file_size); 
    print_md5_sum(result);
*/

    close(sockfd);
    return 0;
}
