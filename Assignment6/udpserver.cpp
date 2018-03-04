/* 
 * udpserver.c - A UDP echo server 
 * usage: udpserver <port_for_server>
 */

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/md5.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <errno.h>
#include "utils.h"

using namespace std;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  int portno; /* port to listen on */
  socklen_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFF_SIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port_for_server>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */

    file_details fd;
    //recieve file details.
    n = recvfrom(sockfd, &fd, sizeof(fd), 0, (struct sockaddr *) &clientaddr, &clientlen);
    if(n<0) cout<<"ERROR reading File Details from socket"<<endl;

    string filename = fd.filename;
    int filesize = fd.filesize;
    int no_of_chunks = fd.no_of_chunks;
    cout<<"File Name: "<<filename<<" File Size: "<<filesize<<" bytes "<<no_of_chunks<<" chunks to be recieved"<<endl;

    n = sendto(sockfd, &fd, sizeof(fd), 0, (struct sockaddr *) &clientaddr, clientlen);
    if(n<0) cout<<"Error acknowledging file details"<<endl;

    /*****************/

    FILE *recv_file = fopen(("rec_" + filename).c_str(), "w");
    segment data_seg;
    bzero(data_seg.data, BUFF_SIZE);
    int recv_size = 0;
    int write_size = 0;
    int buffersize = 0;
    int seq_rec = -1;
    
    //int debug_loops = 0;

    while(recv_size < filesize){
    
    //if(debug_loops%8 ==0)
   // 	sleep(3);

      bzero(data_seg.data, BUFF_SIZE);
      //ioctl(sockfd, FIONREAD, &buffersize); 
      //if(buffersize > 0)
      //{
        n = recvfrom(sockfd, &data_seg, sizeof(data_seg), 0, (struct sockaddr *) &clientaddr, &clientlen);
        if(n<0) cout<<"Error receving data"<<endl;

        // sleep(3);
        int seqNo = data_seg.seqNo;
        if(seqNo == (seq_rec + 1)%MAX_SEQ_NO)
        {
          write_size = fwrite(data_seg.data,1,data_seg.len, recv_file);
          recv_size += data_seg.len;
          seq_rec = seqNo;
          //cout<<"Recevive size "<<recv_size<<" Seq No "<<seqNo<<endl;
          //debug_loops++;
        }
        n = sendto(sockfd, &seq_rec, sizeof(seqNo), 0, (struct sockaddr *) &clientaddr,clientlen);
        if(n<0) cout<<"Error sending ACK, pkt = "<<seqNo<<endl;

        
      //}

    }

    cout<<"Received FILE successfully"<<endl;
    fclose(recv_file);

    /*unsigned char result[MD5_DIGEST_LENGTH];
    char * file_buffer_md5;
    int file_descript = open(("rec_" + filename).c_str(), O_RDONLY);

    file_buffer_md5 = (char *) mmap(0, filesize, PROT_READ, MAP_SHARED, file_descript, 0);
    MD5((unsigned char*) file_buffer_md5, filesize, result);
    munmap(file_buffer_md5, filesize); 
    // n = write(childfd, result, MD5_DIGEST_LENGTH);
    n = sendto(sockfd, &result, sizeof(result), 0, (struct sockaddr *) &clientaddr,clientlen);
    if (n < 0) 
      error("ERROR writing MD5 to socket");
    
    close(file_descript);*/
  }
}

//3396326 - 472250
