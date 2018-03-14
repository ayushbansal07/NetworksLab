#include "CTP.h"
#include <iostream>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
using namespace std;

void sig_wake(int sigid){}

CTP::CTP(int sockfd, struct sockaddr_in serveraddr)
{
	signal(SIGUSR1,sig_wake);
	this->sockfd = sockfd;
	this->serveraddr = serveraddr;
	this->serverlen = sizeof(serveraddr);
	this->cong_wind = MSS;
	this->ssthresh = SEND_BUFF_CAP;
	this->dupAcks = 0;
	this->currptr = -1;
	this->baseptr = -1;
	this->seq = 0;
	this->last_ack = -1;
	this->recv_state = 0;
	this->recv_wind = SEND_BUFF_CAP;
}

int CTP::appSend(char * data,long int size)
{
	sendbuffer_handle(data, size);
}

void CTP::sendbuffer_handle(char * data,long int size)
{
	long int ct = 0;
	while(1)
	{
		while(this->sending_buffer.size() < SEND_BUFF_CAP && ct < size)
		{
			this->sending_buffer.push(data[ct]);
			ct++;
		}
		//TODO: SIGNAL CONSUMER THAT DATA IS PUT INSIDE SENDING_BUFFER
		if(ct==size) break;
		usleep(1000);
	}
}

void * CTP::rate_control(void * threadid)
{
    while(1)
    {
    	long int curr_wind = min(this->cong_wind,this->recv_wind);
    	while(baseptr - currptr < curr_wind && !this->sending_buffer.empty())
    	{
            if(!this->resendQ.empty())
            {
                segment temp_seg = this->resendQ.front();
                this->resendQ.pop();
                this->sendQ.push(temp_seg);
                cout<<"Resending pkt = "<<temp_seg.seqNo<<endl;
                int n = sendto(this->sockfd, &temp_seg, sizeof(temp_seg), 0, (struct sockaddr *) &(this->serveraddr), this->serverlen);
                if(n<0) cout<<"Error sending data to server, pkt no = "<<seq<<endl;
                else {
                    this->baseptr += temp_seg.len;
                }
            }
            else
            {
                segment to_send = create_packet(&(this->seq));
                this->sendQ.push(to_send);
                int n = sendto(this->sockfd, &to_send, sizeof(to_send), 0, (struct sockaddr *) &(this->serveraddr), this->serverlen);
                if(n<0) cout<<"Error sending data to server, pkt no = "<<(this->seq - to_send.len)<<endl;
                else {
                    this->baseptr += to_send.len;              
                }
            }
            
    	}
    	sleep(0);
    }
}

void * CTP::receiver_process(void * threadid)
{
	struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    sockaddr_in clientaddr;
    socklen_t clientlen;
    segment recv_seg;

	while(1)
	{
		if(setsockopt(this->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) cout<<"Error in sockopt"<<endl;

		int n = recvfrom(this->sockfd, &recv_seg, sizeof(recv_seg), 0, (struct sockaddr *) &clientaddr, &clientlen);
		if(n<0)
		{
			this->ssthresh = max((long int)MSS,this->cong_wind/2);
			this->cong_wind = MSS;
			this->dupAcks = 0;
			this->recv_state = 0;
			resend_data_func();
		}
		else
		{
			if(recv_seg.ack){
				update_window(recv_seg.ackNo,recv_seg.recv_window);
			}
			else
			{
				//TODO: DATA
			}
		}
	}
}

void CTP::resend_data_func()
{
	while(!this->sendQ.empty())
    {
        this->resendQ.push(this->sendQ.front());
        this->sendQ.pop();
    }
    this->baseptr = this->currptr; 
}

void CTP::update_window(long int ack_recv, long int recv_window)
{
	this->recv_wind = recv_window;
	if(this->recv_state == 0)
	{
		//SLow Start
		if(ack_recv == this->last_ack)
		{
			this->dupAcks += 1;
		}
		else
		{
			this->cong_wind += MSS;
			this->dupAcks = 0;
		}
		if(this->dupAcks == 3)
		{
			this->ssthresh = max((long int)MSS, this->cong_wind/2);
			this->cong_wind = this->ssthresh + 3*MSS;
			resend_data_func();
			this->recv_state = 2;
		}
		if(this->cong_wind >= this->ssthresh)
		{
			this->recv_state = 1;
		}
	}
	else if(this->recv_state == 1)
	{
		//Congestion Avaoidance
		if(ack_recv == this->last_ack)
		{
			this->dupAcks += 1;
		}
		else
		{
			this->cong_wind += max((long int)MSS,MSS*MSS/this->cong_wind);
			this->dupAcks = 0;
		}
		if(this->dupAcks == 3)
		{
			this->ssthresh = max((long int)MSS, this->cong_wind/2);
			this->cong_wind = this->ssthresh + 3*MSS;
			resend_data_func();
			this->recv_state = 2;
		}
	}
	else
	{
		//Fast Recovery
		if(ack_recv == this->last_ack)
		{
			this->cong_wind += MSS;
		}
		else
		{
			this->cong_wind = this->ssthresh;
			this->dupAcks = 0;
			this->recv_state = 1;
		}
	}
	this->last_ack = ack_recv;
	this->currptr = ack_recv;
	while(!this->sendQ.empty())
    {
        segment temp_seg;
        temp_seg = this->sendQ.front();
        if(temp_seg.seqNo <= this->currptr)
        {
            this->sendQ.pop();
        }
        else
        {
            break;
        }
    }
    while(!this->resendQ.empty())
    {
        segment temp_seg;
        temp_seg = this->resendQ.front();
        if(temp_seg.seqNo <= this->currptr)
        {
            this->resendQ.pop();
        }
        else
        {
            break;
        }
    }
	//TODO: SIGNAL CONSUMER THAT WINDOW IS UPDATED
}

segment CTP::create_packet(long int *seqNo)
{
	segment temp;
	temp.ack = false;
	temp.seqNo = *seqNo;
	int ct = 0;
	bzero(temp.data, MSS);
	while(!this->sending_buffer.empty() && ct < MSS)
	{
		temp.data[ct] = this->sending_buffer.front();
		this->sending_buffer.pop();
		*seqNo = ((*seqNo) + 1)%MAX_SEQ_NO;
	}
	temp.len = ct;
	return temp;
}

int main()
{
	cout<<MAX_SEQ_NO<<endl;
	struct sockaddr_in serveraddr;
	CTP *my = new CTP(0,serveraddr);
	char *data = "lion";
	my->appSend(data,2);
}