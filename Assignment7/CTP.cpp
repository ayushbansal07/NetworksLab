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
	this->seq_recv = -1;
	sem_init(&(this->sQ),0,1);
	sem_init(&(this->rQ),0,1);
	sem_init(&(this->srsQ),0,1);
	sem_init(&(this->params),0,1);

	pthread_create(&(this->RATE_CONTROLLER), NULL, rate_control_helper,(void *) this);
	pthread_create(&(this->RECEIVER_PROCESS), NULL, receiver_process_helper,(void *) this);
}

void CTP::appSend(char * data,long int size)
{
	sendbuffer_handle(data, size);
}

void CTP::sendbuffer_handle(char * data,long int size)
{
	long int ct = 0;
	while(1)
	{
		sem_wait(&(this->sQ));
		while(this->sending_buffer.size() < SEND_BUFF_CAP && ct < size)
		{
			this->sending_buffer.push(data[ct]);
			ct++;
		}
		//TODO: SIGNAL CONSUMER THAT DATA IS PUT INSIDE SENDING_BUFFER
		sem_post(&(this->sQ));
		pthread_kill(this->RATE_CONTROLLER,SIGUSR1);
		if(ct==size) break;
		usleep(1000);
	}
}

void * CTP::rate_control_helper(void * object)
{
	return ((CTP *) object)->rate_control();
}

void * CTP::rate_control()
{
    while(1)
    {
    	sem_wait(&(this->params));
    	sem_wait(&(this->srsQ));
    	long int curr_wind = max(min(this->cong_wind,this->recv_wind),(long int)MSS);

		cout<<"Rate Control "<<curr_wind<<"Cong_wind = "<<this->cong_wind<<"Recv_wind = "<<this->recv_wind<<"Seding buff sz"<<this->sending_buffer.size()<<"Current window = "<<curr_wind<<endl;
		cout<<"Send Queue = "<<sendQ.size()<<" Resend Q = "<<resendQ.size()<<endl;
    	while(this->baseptr -this->currptr < curr_wind)
    	{
    		while(!this->sendQ.empty())
		    {
		        segment temp_seg;
		        temp_seg = this->sendQ.front();
		        if(temp_seg.seqNo + temp_seg.len - 1 <= this->currptr)
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
		        if(temp_seg.seqNo + temp_seg.len - 1 <= this->currptr)
		        {
		            this->resendQ.pop();
		        }
		        else
		        {
		            break;
		        }
		    }
            if(!this->resendQ.empty())
            {
            	cout<<"INSIDEEEEEEEE RESEND Q PART"<<endl;
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
            	if(this->sending_buffer.empty()) break;
                segment to_send = create_packet(&(this->seq));
                this->sendQ.push(to_send);
                int n = sendto(this->sockfd, &to_send, sizeof(to_send), 0, (struct sockaddr *) &(this->serveraddr), this->serverlen);
                if(n<0) cout<<"Error sending data to server, pkt no = "<<(this->seq - to_send.len)<<endl;
                else {

                    this->baseptr += to_send.len;              
                }
            }
            
    	}
    	sem_post(&(this->params));
		sem_post(&(this->srsQ));
    	pause();
    }
}

segment CTP::create_packet(long int *seqNo)
{
	segment temp;
	temp.ack = false;
	temp.seqNo = *seqNo;
	int ct = 0;
	bzero(temp.data, MSS);
	sem_wait(&(this->sQ));
	while(!this->sending_buffer.empty() && ct < MSS)
	{
		temp.data[ct] = this->sending_buffer.front();
		this->sending_buffer.pop();
		*seqNo = ((*seqNo) + 1)%MAX_SEQ_NO;
		ct++;
	}
	sem_post(&(this->sQ));
	temp.len = ct;
	return temp;
}

void * CTP::receiver_process_helper(void * object)
{
	return ((CTP *) object)->receiver_process();
}

void * CTP::receiver_process()
{
	struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    sockaddr_in clientaddr;
    socklen_t clientlen;
    clientlen = sizeof(clientaddr);
    segment recv_seg;

	while(1)
	{
		if(setsockopt(this->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))< 0) cout<<"Error in sockopt"<<endl;
		int n = recvfrom(this->sockfd, &recv_seg, sizeof(recv_seg), 0, (struct sockaddr *) &(this->serveraddr), &(this->serverlen));
		if(n<0)
		{
			cout<<"HERE DATA PACK NOT RECDVD baseptr = "<<this->baseptr<<"currptr = "<<this->currptr<<endl;
			sem_wait(&(this->params));
			this->ssthresh = max((long int)MSS,this->cong_wind/2);
			this->cong_wind = MSS;
			this->dupAcks = 0;
			this->recv_state = 0;
			resend_data_func();
			sem_post(&(this->params));
		}
		else
		{
			if(recv_seg.ack){
				update_window(recv_seg.ackNo,recv_seg.recv_window);
			}
			else
			{
				//TODO: DATA
				recvbuffer_handle(recv_seg);
			}
		}
	}
}

void CTP::resend_data_func()
{
	sem_wait(&(this->srsQ));
	while(!this->sendQ.empty())
    {
        this->resendQ.push(this->sendQ.front());
        this->sendQ.pop();
    }
    sem_post(&(this->srsQ));
    this->baseptr = this->currptr; 
    /*sem_wait(&(this->srsQ));
	while(!this->sendQ.empty())
    {
        segment temp_seg;
        temp_seg = this->sendQ.front();
        if(temp_seg.seqNo + temp_seg.len - 1 <= this->currptr)
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
        if(temp_seg.seqNo + temp_seg.len - 1 <= this->currptr)
        {
            this->resendQ.pop();
        }
        else
        {
            break;
        }
    }
    sem_post(&(this->srsQ));*/
    pthread_kill(this->RATE_CONTROLLER,SIGUSR1);
}

void CTP::update_window(long int ack_recv, long int recv_window)
{
	cout<<"ACK RECEVIVED = "<<ack_recv<<"Receiver window = "<<recv_window<<endl;
	sem_wait(&(this->params));
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
			//TOSEE??
			//resend_data_func();
		}
	}
	this->last_ack = ack_recv;
	this->currptr = ack_recv;
	sem_post(&(this->params));
	/*sem_wait(&(this->srsQ));
	while(!this->sendQ.empty())
    {
        segment temp_seg;
        temp_seg = this->sendQ.front();
        if(temp_seg.seqNo + temp_seg.len - 1 <= this->currptr)
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
        if(temp_seg.seqNo + temp_seg.len - 1 <= this->currptr)
        {
            this->resendQ.pop();
        }
        else
        {
            break;
        }
    }
    sem_post(&(this->srsQ));*/
	pthread_kill(this->RATE_CONTROLLER,SIGUSR1);

	//TODO: SIGNAL CONSUMER THAT WINDOW IS UPDATED
}

void CTP::recvbuffer_handle(segment recvd_seg)
{
	long int expected_seq = (this->seq_recv + 1)%MAX_SEQ_NO;
	cout<<"Expected = "<<expected_seq<<" recvd = "<<recvd_seg.seqNo<<" len = "<<recvd_seg.len<<endl;
	if(expected_seq >= recvd_seg.seqNo && expected_seq < recvd_seg.seqNo + recvd_seg.len)
	{

		int ct = expected_seq - recvd_seg.seqNo;
		cout<<"Expected = "<<expected_seq<<" recvd = "<<recvd_seg.seqNo<<" ct = "<<ct<<endl;
		int added = 0;
		sem_wait(&(this->rQ));
		while(this->recv_buffer.size() < RECV_BUFFER_CAP && ct < recvd_seg.len && ct>=0)
		{
			this->recv_buffer.push(recvd_seg.data[ct]);
			ct++;
			added++;
		}
		sem_post(&(this->rQ));
		this->seq_recv = (this->seq_recv + added)%MAX_SEQ_NO;
	}
	cout<<"Recv Buff handle sz = "<<this->recv_buffer.size()<<endl;
	send_ack(this->seq_recv);
}

void CTP::send_ack(long int ackNo)
{

	segment temp;
	temp.ack = true;
	temp.ackNo = ackNo;
	temp.recv_window = (RECV_BUFFER_CAP - this->recv_buffer.size());
	cout<<"SENDING ACKKKKKKKKKKKKKKKKKKKKKK recv wind = "<<temp.recv_window<<" ack no = "<<ackNo<<endl;
	int n = sendto(this->sockfd, &temp,sizeof(temp),0,(struct sockaddr *)&(this->serveraddr),this->serverlen);
	if(n<0) cout<<"Error sending ack"<<endl;
}

int CTP::appRecieve(char * data,long int size)
{
	int ct = 0;
	while(1)
	{
		/*int sz = this->recv_buffer.size();
		if(sz > 0)
			cout<<"Recv Buff size = "<<sz<<endl;*/
		sem_wait(&(this->rQ));
		while(!this->recv_buffer.empty() && ct < size)
		{
			data[ct] = this->recv_buffer.front();
			this->recv_buffer.pop();
			ct++;
		}
		sem_post(&(this->rQ));
		if(ct==size) break;
		usleep(1000);
	}
	return ct;
}

/*int main()
{
	struct sockaddr_in serveraddr;
	CTP *my = new CTP(0,serveraddr);
	char *data = "lion";
	my->appSend(data,2);
}*/