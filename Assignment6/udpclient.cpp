/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>
#include <sys/time.h>
#include "utils.h"
#include <vector>

#include <openssl/md5.h>
#include <sys/mman.h>
#include <fcntl.h>

using namespace std;


/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFF_SIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    /* get a message from the user */

    string filename;
    cout<<"Enter File name"<<endl;
    cin>>filename;
    FILE * file;
    file = fopen(filename.c_str(),"r");
    fseek(file,0,SEEK_END);
    int filesize = ftell(file);
    fseek(file,0,SEEK_SET);
	int no_of_chunks = filesize/BUFF_SIZE + 1;
	
    file_details filedetails;
    filedetails.filesize = filesize;
    strcpy(filedetails.filename,filename.c_str());
    filedetails.no_of_chunks = no_of_chunks;


	serverlen = sizeof(serveraddr);
    n = sendto(sockfd, &filedetails, sizeof(filedetails), 0, (struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR writing FIle Details to socket");

  	file_details fd_rec;
    n = recvfrom(sockfd, &fd_rec, sizeof(fd_rec), 0, (struct sockaddr *) &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR reading File Details acknowledgement from socket");

    if(strcmp(filedetails.filename,fd_rec.filename)==0 && filedetails.filesize == fd_rec.filesize && fd_rec.no_of_chunks == filedetails.no_of_chunks) {
        cout<<"Ackowledgement from server succesfully received"<<endl;
    }
    else    {
        cout<<"UNSUCCESSFULL Acknowledgement from server"<<endl;
    }

    /*****************/

    cout<<"Sending File as Byte Stream"<<endl;

    int file_readSize;
    vector<segment> data_segs(WINDOW_SIZE);	//Stores all the datagrams currently in the window.
    int seq = 0;
    int baseptr = -1;
    int currptr = -1;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    int seg_ct = 0;
    int sent_size = 0;
    
    while(!feof(file))
    {
    	while(baseptr - currptr < WINDOW_SIZE && sent_size < filesize)
    	{
    		bzero(data_segs[seq%WINDOW_SIZE].data, BUFF_SIZE);
			data_segs[seq%WINDOW_SIZE].seqNo = seq;
		    data_segs[seq%WINDOW_SIZE].len = fread(data_segs[seq%WINDOW_SIZE].data, 1, sizeof(data_segs[seq%WINDOW_SIZE].data), file);	//Read BUFF_SIZE (which is equal to size of data_seg.data char array) data from file.
		    n = sendto(sockfd, &data_segs[seq%WINDOW_SIZE], sizeof(data_segs[seq%WINDOW_SIZE]), 0, (struct sockaddr *) &serveraddr, serverlen);
		    if(n<0) cout<<"Error sending data to server, pkt no = "<<seq<<endl;
		    else {
		    	sent_size += data_segs[seq%WINDOW_SIZE].len;
		    	baseptr ++;
		    	seq  = (seq +1)%MAX_SEQ_NO;
		    }
    	}
	    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) error("Error in sockopt"); //Timer of 1 sec.
    	

        int ack_no = -1;
        while(1)
        {
        	n = recvfrom(sockfd, &ack_no, sizeof(ack_no), 0, (struct sockaddr *) &serveraddr, &serverlen);
        	if(n<0) {
        		//Timeout happened while receiving ack
        		for(int i=0;i<baseptr - currptr && sent_size < filesize;i++)
        		{
					cout<<"Resending, pkt = "<<(seq - WINDOW_SIZE + i)<<endl;
		    		n = sendto(sockfd, &data_segs[(seq - WINDOW_SIZE + i)%WINDOW_SIZE], sizeof(data_segs[(seq - WINDOW_SIZE + i)%WINDOW_SIZE]), 0, (struct sockaddr *) &serveraddr, serverlen);
		    		if(n<0){ 
		    			cout<<"Error sending(r) data to server, pkt no = "<<(seq - WINDOW_SIZE + i)<<endl;
		    			i--;
		    		}
				}
        		
        		if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) error("Error in sockopt");
        		ack_no = -1;        		
        	}
        	else {
        		if(ack_no == (currptr + 1)%MAX_SEQ_NO) {
        			currptr ++;
        			break;
        		}
        	}
        }
        
    }
    
    //Receive pending acks
    while(currptr != baseptr)
    {
    	int ack_no = -1;
        while(1)
        {
        	n = recvfrom(sockfd, &ack_no, sizeof(ack_no), 0, (struct sockaddr *) &serveraddr, &serverlen);
        	if(n<0) {
        	
        		for(int i=0;i<baseptr - currptr && sent_size < filesize;i++)
        		{
					cout<<"Resending, pkt = "<<(seq - WINDOW_SIZE + i)<<endl;
		    		n = sendto(sockfd, &data_segs[(seq - WINDOW_SIZE + i)%WINDOW_SIZE], sizeof(data_segs[(seq - WINDOW_SIZE + i)%WINDOW_SIZE]), 0, (struct sockaddr *) &serveraddr, serverlen);
		    		if(n<0){ 
		    			cout<<"Error sending(r) data to server, pkt no = "<<(seq - WINDOW_SIZE + i)<<endl;
		    			i--;
		    		}
				}
        		
        		if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) error("Error in sockopt");
        		ack_no = -1;        		
        	}
        	else {
        		if(ack_no == (currptr + 1)%MAX_SEQ_NO) {
        			currptr ++;
        			break;
        		}
        	}
        }
    }

    fclose(file);

    cout<<"File Sent successfully"<<endl;
    //getchar();

    /*unsigned char result[MD5_DIGEST_LENGTH];
    char * file_buffer_md5;
    int file_descript = open(filename.c_str(), O_RDONLY);

    file_buffer_md5 = (char *) mmap(0, filedetails.filesize, PROT_READ, MAP_SHARED, file_descript, 0);
    MD5((unsigned char*) file_buffer_md5, filedetails.filesize, result);
    munmap(file_buffer_md5, filesize); 

    unsigned char result_rec[MD5_DIGEST_LENGTH];
    // n = read(sockfd, result_rec, MD5_DIGEST_LENGTH);
    n = recvfrom(sockfd, &result_rec, sizeof(result_rec), 0, (struct sockaddr *) &serveraddr, &serverlen);
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

    close(file_descript);*/
    close(sockfd);
    return 0;
}
