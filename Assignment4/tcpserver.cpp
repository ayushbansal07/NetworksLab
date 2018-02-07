/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string>

#define BUFSIZE 1024
#define MAX_FILE_NAME 30

using namespace std;

#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

/*
 * error - wrapper for perror
 */
struct file_details{
    char filename[MAX_FILE_NAME];
    int filesize;
};

void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int parentfd; /* parent socket */
  int childfd; /* child socket */
  int portno; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buffer */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  parentfd = socket(AF_INET, SOCK_STREAM, 0);
  if (parentfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));

  /* this is an Internet address */
  serveraddr.sin_family = AF_INET;

  /* let the system figure out our IP address */
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* this is the port we will listen on */
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(parentfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * listen: make this socket ready to accept connection requests 
   */
  if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen\n");
  printf("Server Running ....\n");
  /* 
   * main loop: wait for a connection request, echo input line, 
   * then close connection.
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /* 
     * accept: wait for a connection request 
     */
    childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
    if (childfd < 0) 
      error("ERROR on accept\n");
    
    /* 
     * gethostbyaddr: determine who sent the message 
     */
    /*hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server established connection with %s (%s)\n", 
	   hostp->h_name, hostaddrp);*/
    
    /* 
     * read: read input string from the client
     */

    int x = fork();
    if(x<0) cout<<"Error creating new thread"<<endl;
    else if(x==0)
    {

        close(parentfd);
        bzero(buf, BUFSIZE);

        file_details fd;
        n = read(childfd, &fd, sizeof(fd));

        if (n < 0) 
          error("ERROR reading from socket\n");
        printf("server received %d bytes\n", n);

        string filename = string(fd.filename);
        //cout<<"GERE#########"<<endl;
        int filesize = fd.filesize;

        cout<<"File Name: "<<filename<<" File Size: "<<filesize<<" bytes"<<endl;
        
        /* 
         * write: echo the input string back to the client 
         */
        n = write(childfd, &fd, sizeof(fd));
        if (n < 0) 
          error("ERROR writing to socket\n");

        /*char * buf_tokens = strtok(buf," ");
        string filename;
        filename = string(buf_tokens);
        buf_tokens = strtok(NULL, " ");
        int filesize = atoi(buf_tokens);
        cout<<"File Name "<<filename<<" File Size "<<filesize<<endl;*/
        /*while(buf_tokens!= NULL){
          filename
          buf_tokens = strtok(NULL, " ");
        }*/

        int buffersize = 0, recv_size = 0,size = 0, read_size, write_size;
        FILE *image = fopen(("rec_" + filename).c_str(), "w");
        char verify = '1';
        bzero(buf,BUFSIZE);

        while(recv_size < filesize) {
            ioctl(childfd, FIONREAD, &buffersize); 

            //We check to see if there is data to be read from the socket    
            if(buffersize > 0 ) {
              //cout<<"INSIDE "<<buffersize<<endl;

                if((read_size = read(childfd,buf, buffersize)) < 0){
                    printf("%s", strerror(errno));
                }
                //cout<<"HERE########### "<<read_size<<" "<<buf<<endl;

                write_size = fwrite(buf,1,(buffersize), image);
                //cout<<filename<<" "<<recv_size<<endl;
                /*if(write_size != buffersize) {
                  printf("write and buffersizes wrong\n");
                }

                if(read_size !=write_size) {
                    printf("error in read write\n");
                }*/

                //Increment the total number of bytes read
                recv_size += read_size;

                  //Send our handshake verification info
                write(childfd, &verify, sizeof(char));
                bzero(buf,BUFSIZE);

            }
        }
        cout<<"Received FILE successfully"<<endl;
        fclose(image);

        unsigned char result[MD5_DIGEST_LENGTH];
        char * file_buffer_md5;
        int file_descript = open(("rec_" + filename).c_str(), O_RDONLY);

        file_buffer_md5 = (char *) mmap(0, filesize, PROT_READ, MAP_SHARED, file_descript, 0);
        MD5((unsigned char*) file_buffer_md5, filesize, result);
        munmap(file_buffer_md5, filesize); 
        n = write(childfd, result, MD5_DIGEST_LENGTH);
        if (n < 0) 
          error("ERROR writing MD5 to socket");
        
        close(file_descript);

        close(childfd);

        exit(0);
    }


    
  }
}
