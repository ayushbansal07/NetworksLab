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
#include <queue>

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
    queue<segment> data_seg;	//Stores all the datagrams currently in the window.
    int seq = 0;
    int baseptr = -1;
    int currptr = -1;

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    int seg_ct = 0;
    int sent_size = 0;
    int window_sz = INIT_WINDOW_SIZE;
    int already_read = -1;
    queue<segment> resending_q;
    int ct_same_ack = 0;
    int ack_no = -1;

    while(!feof(file))
    {
        //cout<<"Window Size "<<window_sz<<endl;
    	while(baseptr - currptr < window_sz && sent_size < filesize)
    	{
            
            if(baseptr < already_read)
            {
                //TODO
                segment temp_seg = resending_q.front();
                resending_q.pop();
                data_seg.push(temp_seg);
                cout<<"Resending pkt = "<<temp_seg.seqNo<<endl;
                n = sendto(sockfd, &temp_seg, sizeof(temp_seg), 0, (struct sockaddr *) &serveraddr, serverlen);
                if(n<0) cout<<"Error sending data to server, pkt no = "<<seq<<endl;
                else {
                    baseptr ++;
                }
            }
            else
            {
                segment temp_seg;
                bzero(temp_seg.data, BUFF_SIZE);
                temp_seg.seqNo = seq;
                temp_seg.len = fread(temp_seg.data, 1, sizeof(temp_seg.data), file);    //Read BUFF_SIZE (which is equal to size of data_seg.data char array) data from file.
                data_seg.push(temp_seg);
                n = sendto(sockfd, &temp_seg, sizeof(temp_seg), 0, (struct sockaddr *) &serveraddr, serverlen);
                if(n<0) cout<<"Error sending data to server, pkt no = "<<seq<<endl;
                else {
                    sent_size += temp_seg.len;
                    baseptr ++;
                    seq  = (seq +1)%MAX_SEQ_NO;
                    already_read++;
                }
            }
            
    	}
	    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) error("Error in sockopt"); //Timer of 1 sec.

    	n = recvfrom(sockfd, &ack_no, sizeof(ack_no), 0, (struct sockaddr *) &serveraddr, &serverlen);
    	if(n<0) {
            window_sz = max(INIT_WINDOW_SIZE,window_sz/2);
            while(!data_seg.empty())
            {
                resending_q.push(data_seg.front());
                data_seg.pop();
            }
            baseptr = currptr;        		
    		if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) error("Error in sockopt");
    		//ack_no = -1;  		
    	}
    	else {
            int diff_ack_curr = ack_no - currptr;

            currptr = ack_no;
            window_sz += diff_ack_curr;
            //cout<<"Differece  = "<<diff_ack_curr<<" ack_no = "<<ack_no<<" currptr = "<<currptr<<"Window = "<<window_sz<<endl;
            while(!data_seg.empty())
            {
                segment temp_seg;
                temp_seg = data_seg.front();
                if(temp_seg.seqNo <= currptr)
                {
                    data_seg.pop();
                }
                else
                {
                    break;
                }
            }
            while(!resending_q.empty())
            {
                segment temp_seg;
                temp_seg = resending_q.front();
                if(temp_seg.seqNo <= currptr)
                {
                    resending_q.pop();
                }
                else
                {
                    break;
                }
            }
    	}
    
        
    }
    
    //Receive pending acks
    cout<<"HERE#############################################################"<<endl;
    while(1)
    {
        //int ack_no = -1;
        if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) error("Error in sockopt");
        n = recvfrom(sockfd, &ack_no, sizeof(ack_no), 0, (struct sockaddr *) &serveraddr, &serverlen);
        if(n<0) break;
        currptr = ack_no;
    }

    while(!data_seg.empty())
    {
        resending_q.push(data_seg.front());
        data_seg.pop();
    }

    while(!resending_q.empty())
    {
        segment temp_seg;
        temp_seg = resending_q.front();
        if(temp_seg.seqNo <= currptr)
        {
            resending_q.pop();
        }
        else
        {
            break;
        }
    }

    while(!resending_q.empty())
    {
        n = recvfrom(sockfd, &ack_no, sizeof(ack_no), 0, (struct sockaddr *) &serveraddr, &serverlen);
        if(n<0)
        {
            queue<segment> temp_q;
            while(!resending_q.empty())
            {
                segment this_seg = resending_q.front();
                cout<<"Resedning pkt(r) = "<<this_seg.seqNo<<endl;
                resending_q.pop();
                temp_q.push(this_seg);
                n = sendto(sockfd, &this_seg,sizeof(this_seg),0,(struct sockaddr *)&serveraddr,serverlen);
                if(n<0)
                {
                    cout<<"Error sending(r) data to server, pkt no = "<<this_seg.seqNo<<endl;
                }
            }
            resending_q = temp_q;
        }
        else
        {
            currptr = ack_no;
            while(!resending_q.empty())
            {
                segment temp_seg;
                temp_seg = resending_q.front();
                if(temp_seg.seqNo <= currptr)
                {
                    resending_q.pop();
                }
                else
                {
                    break;
                }
            }

        }
    }

    /*while(currptr != baseptr)
    {
    	//int ack_no = -1;
        while(1)
        {
            //cout<<"WAITING FOR ACK ###########"<<"currptr = "<<currptr<<" baseptr = "<<baseptr<<endl;
        	n = recvfrom(sockfd, &ack_no, sizeof(ack_no), 0, (struct sockaddr *) &serveraddr, &serverlen);
            if(ack_no == currptr) ct_same_ack++;
            else ct_same_ack = 0;
        	if(n<0 || ct_same_ack >=3) {

                for(int i=0;i<baseptr -currptr;i++)
                {
                    segment this_seg = data_seg.front();
                    cout<<"Resedning pkt(r) = "<<this_seg.seqNo<<endl;
                    n = sendto(sockfd, &this_seg,sizeof(this_seg),0,(struct sockaddr *)&serveraddr,serverlen);
                    if(n<0)
                    {
                        cout<<"Error sending(r) data to server, pkt no = "<<this_seg.seqNo<<endl;
                        i--;
                    }
                    else
                    {
                        data_seg.pop();
                        data_seg.push(this_seg);
                    }
                }
        		
        		if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) error("Error in sockopt");
        		//ack_no = -1;   
                break;     		
        	}
        	else {
                currptr = ack_no;
                while(!data_seg.empty())
                {
                    segment temp_seg;
                    temp_seg = data_seg.front();
                    if(temp_seg.seqNo <= currptr)
                    {
                        data_seg.pop();
                    }
                    else
                    {
                        break;
                    }
                }
                break;
        	}
        }
        cout<<"Base ptr = "<<baseptr<<" currptr = "<<currptr<<" ack = "<<ack_no<<endl;
    }*/

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
