#include <stdio.h> 
#include <stdlib.h> 
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <iostream>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <algorithm>
#include <fcntl.h>
#include "RDTConnection.h"

using namespace std;

void report_error_and_exit(const char* msg)
{
	fprintf(stderr, "%s:%s\n", msg, strerror(errno));
	exit(EXIT_FAILURE);
}

RDTConnection::RDTConnection()
	: next_seq_num(0), next_ack_num(0), cwnd_size(MSS), ssthresh(10*MSS), cwnd_base(0), isFastRecovery(0)
{}

void RDTConnection::update_next_seq_num(int num_bytes)
{
	next_seq_num = (next_seq_num + num_bytes) % MAX_SEQ_NUM;
}

void RDTConnection::update_next_ack_num(int num_bytes)
{
	next_ack_num = (next_ack_num + num_bytes) % MAX_SEQ_NUM;
}

void RDTConnection::log_sent_packet(const Packet* p, int is_dup)
{
	Header h = p -> get_header();
	cout << "SEND" << ' ' << h.seq_num << ' ' << h.ack_num << ' ' << (int)cwnd_size << ' ' << ssthresh << ' ';

	if (h.syn_flag)
		cout << ' ' << "[SYN]";
	if (h.ack_flag)
		cout << ' ' << "[ACK]";
	if (h.fin_flag)
		cout << ' ' << "[FIN]";
	if (is_dup)
		cout << ' ' << "[DUP]";

	cout << endl;

}

void RDTConnection::log_received_packet(const Packet* p, int is_dup)
{
	Header h = p -> get_header();
	cout << "RECV" << ' ' << h.seq_num << ' ' << h.ack_num << ' ' << (int)cwnd_size << ' ' << ssthresh;

	if (h.syn_flag)
		cout << ' ' << "[SYN]";
	if (h.ack_flag)
		cout << ' ' << "[ACK]";
	if (h.fin_flag)
		cout << ' ' << "[FIN]";
	if (is_dup)
		cout << ' ' << "[DUP]";

	cout << endl;

}


void RDTConnection::send_packet(int sockfd, Packet* p, const struct sockaddr *dest_addr, socklen_t addrlen, int isRetransmit)
{
	int num_bytes;
	char* msg = p -> toString();
	num_bytes = p -> get_header().data_size;
	// printf("port number: %d\n", ntohs(((struct sockaddr_in*)dest_addr)->sin_port));
	if (sendto(sockfd, msg, num_bytes + HEADER_SIZE, MSG_CONFIRM, dest_addr, addrlen) == -1)
		report_error_and_exit("Error sending packets");

	if (! isRetransmit)
		update_next_seq_num(num_bytes);
		// update the next seq number if we transmit a new packet
	free(msg);
}

Packet* RDTConnection::recv_packet(int sockfd, struct sockaddr *src_addr, socklen_t* addrlen)
{
	int nread;
	char buf[MAX_BYTES];
	nread = recvfrom(sockfd, buf, MAX_BYTES, MSG_WAITALL, src_addr, addrlen);
	if (nread == -1)
		report_error_and_exit("Error receiving packets");
	else if (nread == 0)
		return NULL;

	Packet* p = new Packet(buf, nread);
	return p;
}

int RDTConnection::client_handshake(int server_fd, struct sockaddr* server_addr, socklen_t addrlen)
{
	cerr << "Start connection to server" << endl;
	srand(time(0));
	next_seq_num = rand() % 1000;
	// initialize seq num randomly
	next_ack_num = 0;
	// initializze ack num to zero
	// cwnd_size = 1 * MAX_DATA_SIZE;

	Packet* syn_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, false, true, false);
	// set syn bit to 1

	struct pollfd fds[1];
  	fds[0].fd = server_fd;
	fds[0].events = POLLIN | POLLHUP | POLLERR;
	fds[0].revents = 0;

  	int poll_ret;
  	// int timeout = 500; 
  	int count = 0;
  	// start connecting to server
  	while (true) {

	    send_packet(server_fd, syn_packet, server_addr, addrlen, false);
	    log_sent_packet(syn_packet);
	    // printf("Start sending syn packet.\n");
	    // send syn packet
	    poll_ret = poll(fds, 1, timeout);
	    // wait for 0.5 seconds
	    if (poll_ret == -1) { 
			report_error_and_exit("Error in poll");
	    }
	    else if (poll_ret == 0)
	    {
	    	count++;
	    	if (count > 20)
	    	{
	    		// we have not received message from server for 10 seconds
	    		fprintf(stderr, "%s\n", "Error: no response from server over 10 seconds");
	    		return false;
	    	}
	    }
	    else if (poll_ret > 0 && fds[0].revents & POLLIN ) {
	    	// receive syn_ack packet from server
	    	// printf("Receiving synack packet\n");
	    	Packet* ack_packet = recv_packet(server_fd, server_addr, &addrlen); 
	    	log_received_packet(ack_packet);
	    	Header h = ack_packet -> get_header();
	    	count = 0;
	    	if (h.syn_flag && h.ack_flag && h.ack_num == next_seq_num + 1)
	    		// check that both syn and ack flags are set
	    	{
	    		next_seq_num = h.ack_num;
	    		next_ack_num = h.seq_num + 1;
	    		break;
	    	}
	    	free(ack_packet);

	    }
	    // if the ack packet is not in correct format
	    // resend the syn message 
  	}

  	Packet* request = new Packet(next_seq_num, next_ack_num, cwnd_size, true, false, false);
  	send_packet(server_fd, request, server_addr, addrlen, false);
  	log_sent_packet(request);
  	// send the file name together with the last packet in handshake
  	free(syn_packet);
  	free(request);
  	return true;
}

// if the handshake is successfull, return true and save the client address
int RDTConnection::server_handshake(int sockfd, struct sockaddr* client_addr, socklen_t* addrlen)
{
	cerr << "Starting listening at server\n";

	cwnd_size = 0;
	ssthresh = 0;

	Packet* syn_packet = recv_packet(sockfd, client_addr, addrlen);
	log_received_packet(syn_packet);
	Header h = syn_packet -> get_header();
	if (! h.syn_flag)
		// do not receive syn packet
		return false;

	// printf("Receiving syn packet from client\n");
	srand(time(0));
	// next_seq_num = rand() % MAX_SEQ_NUM;
	next_seq_num = rand() % 1000;
	// initialize seq number randonly
	next_ack_num = h.seq_num + 1;
	// initialize ack with the seq number of client 
	// cwnd_size = 1 * MAX_DATA_SIZE;

	Packet* ack_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, true, true, false);

	struct pollfd fds[1];
	fds[0].fd = sockfd;
	fds[0].events = POLLIN | POLLHUP | POLLERR;
	fds[0].revents = 0;


	int poll_ret;
	// int timeout = 500; 
	int count = 0;
  	while (true) {
  		// printf("the addrlen of client: %d\n", (int)*addrlen);
    	send_packet(sockfd, ack_packet, client_addr, *addrlen, false);
    	log_sent_packet(ack_packet);
    	// cerr << "Start sending syn ack packet\n";
    	// send the synack packet
    	poll_ret = poll(fds, 1, timeout);
    	// wait for client's final ack packet
	    if (poll_ret == -1) {
	    	report_error_and_exit("Error in poll");
	    }
	    else if (poll_ret == 0)
	    {
	    	count++;
	    	if (count > 20)
	    	{
		    	fprintf(stderr, "%s\n", "ERROR: no response from client for more than 10 seconds");
		    	// to do: record an empty file
		    	return false;
	    	}
	    }
    	else if (poll_ret > 0 && fds[0].revents & POLLIN ) {

    		Packet* request = recv_packet(sockfd, client_addr, addrlen);
    		log_received_packet(request);
    		Header h = request -> get_header();
    		count = 0; // reset the number of timeouts
    		if (h.ack_flag && h.ack_num == next_seq_num + 1)
    			// if the ack packet is in corrent format
    		{
    			// int num_bytes = h.data_size;
    			next_seq_num = h.ack_num;
    			next_ack_num = h.seq_num + h.data_size;
    			/*
    			// save the filename
    			memcpy(filename, request->get_data(), h.data_size);
    			filename[h.data_size] = '\0';
    			*/
    			free(request);
    			break;
    		}
      	
    	}
    
  	}
  	// if the handshake is successful, we are ready to receive file
  	free(syn_packet);
  	free(ack_packet);
  	return true;
}

void RDTConnection::update_cwnd_size()
{
	if (! isFastRecovery)
	{
		if ((int) cwnd_size < ssthresh)
			cwnd_size += 1 * MSS;
		else
			cwnd_size = min(cwnd_size + (MSS * MSS) / cwnd_size, 10240.0);
	}
	else
	{
		//the lost packet is recovered
		cwnd_size = ssthresh;
		isFastRecovery = 0;
	}
}

int comp_seq_num (int first, int second) {

    int diff = second - first;
    if (abs(diff) > (MAX_SEQ_NUM / 2)) {
      	return diff < 0;
    }
    else 
    	return diff > 0;
}

int RDTConnection::send_file(int server_fd, struct sockaddr* server_addr, socklen_t addrlen, const char* filename)
{	
	cerr << "Start sending file " << filename << endl;
	cwnd_base = next_seq_num;

	int my_file_fd;
	my_file_fd = open(filename, O_RDONLY);
	if (my_file_fd == -1)
		report_error_and_exit("ERROR opening file");

	uint16_t nread;
	char buf[MSS];
	nread = read(my_file_fd, buf, MSS);
	if (nread == -1)
		report_error_and_exit("ERROR reading from file");

	Packet* first_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, nread, 1, 0, 0, buf);
	send_packet(server_fd, first_packet, server_addr, addrlen, 0);
	log_sent_packet(first_packet);
	// send the first packet without acknowledgement
	window.push_back(first_packet);
	struct timespec start, now;
	clock_gettime(CLOCK_REALTIME, &start);
	int isTimerOn = 1;
	// start timer on cwnd_base

	struct pollfd fds[1];
	fds[0].fd = server_fd;
	fds[0].events = POLLIN | POLLHUP | POLLERR;
	fds[0].revents = 0;

	int poll_ret, isFastRecovery, num_dup_ack;
	int count = 0;
	int end_of_file = 0;
	while (true)
	{
		poll_ret = poll(fds, 1, unit);
		if (poll_ret == -1)
			report_error_and_exit("ERROR in poll");
		else if (poll_ret == 0)
		{
			count++;
			if (count > 10000 / unit)
			{
	    		// no message from server for 10 seconds
	    		fprintf(stderr, "%s\n", "Error: no response from server over 10 seconds");
	    		return false;
	    	}
		}
		else if (poll_ret > 0 && fds[0].revents & POLLIN ) {

			count = 0;
    		Packet* ack_packet = recv_packet(server_fd, server_addr, &addrlen);
    		Header h = ack_packet -> get_header();
    		if (! h.ack_flag)
    			continue;
			// case 1: ack the oldest outstanding packet
			if (h.ack_num == (window[0]->get_header().seq_num + window[0]->get_header().data_size) % MAX_SEQ_NUM)
			{
				// update cwnd_base
				cwnd_base = h.ack_num;
				// update cwnd_size
				update_cwnd_size();
				// restart counter
				clock_gettime(CLOCK_REALTIME, &start);
				free(window[0]);
				window.pop_front();
				log_received_packet(ack_packet);
			}

			// case 2: duplicated ack
			else if (comp_seq_num(h.ack_num, (window[0]->get_header().seq_num + window[0]->get_header().data_size) % MAX_SEQ_NUM))
			//else if (h.ack_num <= window[0]->get_header().seq_num)
			{
				cerr << "duplicate ark" << endl;
				num_dup_ack++;
				if (isFastRecovery)
					cwnd_size += 1 * MSS;
					// another duplicated ack
				log_received_packet(ack_packet, 1);
			}
			// case 3: ack a newer outstanding packet
			else if (comp_seq_num((window[0]->get_header().seq_num + window[0]->get_header().data_size) % MAX_SEQ_NUM, h.ack_num))
			// else if (h.ack_num > (window[0]->get_header().seq_num + window[0]->get_header().data_size) % MAX_SEQ_NUM)
			{
				cerr << "ack a newer packet" << endl;
				while (!window.empty())
				{
					Packet* temp = window.front();
					if (h.ack_num == (temp->get_header().seq_num + temp->get_header().data_size) % MAX_SEQ_NUM)
					{
						// update cwnd_base
						cwnd_base = h.ack_num;
						// update cwnd_size
						update_cwnd_size();
						// restart counter
						clock_gettime(CLOCK_REALTIME, &start);
						free(temp);
						window.pop_front();
						break;
					}
					free(temp);
					window.pop_front();
				}

				log_received_packet(ack_packet);
			}
			
			if (window.empty())
				isTimerOn = 0;
				// turn off the timer if there is no outstanding packets   	
    	}

    	// check packet timeout
    	clock_gettime(CLOCK_REALTIME, &now);
    	double elapse_time = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000.0;
    	if (isTimerOn && elapse_time > timeout)
    	{
    		cerr << "Timeout! Retransmit the oldest unacked packet and enter slow start." << endl;
    		// reset ssthresh and cwnd_size
    		ssthresh = max((int)cwnd_size/2, 1024);
    		cwnd_size = MSS;
    		send_packet(server_fd, window[0], server_addr, addrlen, true);
    		log_sent_packet(window[0]);
    		// reset the timer
    		clock_gettime(CLOCK_REALTIME, &start);
    		num_dup_ack = 0;
    	}

    	// while there are more packets ready to send
    	int target_seq_num = (cwnd_base + (int)cwnd_size) % MAX_SEQ_NUM;
    	// int absolute_seq_num = next_seq_num;
    	while (comp_seq_num(next_seq_num, target_seq_num))
    	{

    		/* cerr << "next_seq_num: " << next_seq_num << "target_seq_num: " << target_seq_num << " " << "cwnd_base: " << cwnd_base 
    		<< "cwnd_size: " << (int)cwnd_size << endl; 
    		/*
    		if (is_wrap_around)
    		{
    			cerr << "next_seq_num: " << next_seq_num << " " << "target_seq_num: " << target_seq_num << " " << "cwnd_base: " << cwnd_base << endl; 
    		}
    		*/
    		nread = read(my_file_fd, buf, MSS);
    		if (nread == -1)
    			report_error_and_exit("ERROR reading from file");
    		else if (nread == 0)
    		{
    			end_of_file = 1;
    			// cerr << "End of file! No more packets to send.\n";
    			break;
    		}
    		Packet* file_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, nread, 1, 0, 0, buf);
    		window.push_back(file_packet);
    		send_packet(server_fd, file_packet, server_addr, addrlen, 0);
    		// absolute_seq_num += file_packet->get_header().data_size;
    		log_sent_packet(file_packet);
    		isTimerOn = 1; // turn on the timer
    	}

    	// if the whole file is sent and acked
    	if (end_of_file == 1 && window.empty())
    		return true;
	}

	
}


// compare two packets by sequence number
bool my_compare(const Packet* l, const Packet* r)
{
	return l -> get_header().seq_num < r -> get_header().seq_num;
}

int RDTConnection::receive_file(int client_fd, struct sockaddr* client_addr, socklen_t* addrlen, const char* filename)
{
	int my_file_fd;
	my_file_fd = open(filename, O_WRONLY);
	if (my_file_fd == -1)
	{
		cerr << "ERROR opening file " << filename << endl;
		exit(EXIT_FAILURE);
	}

	struct pollfd fds[1];
	fds[0].fd = client_fd;
	fds[0].events = POLLIN | POLLHUP | POLLERR;
	fds[0].revents = 0;

	int poll_ret;
	int count = 0; 
	while (true)
	{
		poll_ret = poll(fds, 1, unit);
		if (poll_ret == -1)
			report_error_and_exit("ERROR in poll");
		else if (poll_ret == 0)
		{
			count++;
			if (count > 10000 / unit)
			{
	    		// no message from client for 10 seconds
	    		fprintf(stderr, "%s\n", "ERROR: no response from client over 10 seconds");
	    		return false;
	    	}
		}
		else if (poll_ret > 0 && fds[0].revents & POLLIN ) 
		{
			count = 0; // receive new message from client
    		Packet* file_packet = recv_packet(client_fd, client_addr, addrlen);
    		log_received_packet(file_packet);
    		Header h = file_packet -> get_header();
    		if (h.fin_flag)
    		{
    			cerr << "Finish receiving file." << endl;
    			next_ack_num = h.seq_num + 1;
    			break;
    		}

    		// case 1: the new packet is the next expected one
    		if (h.seq_num == next_ack_num)
    		{
    			update_next_ack_num(h.data_size);
    			if (write(my_file_fd, file_packet->get_data(), h.data_size) == -1)
    				report_error_and_exit("Error writing packet to file");

    			free(file_packet);
    			// append the new packet to file
    			while (! window.empty() && window[0]->get_header().seq_num == next_ack_num)
    				// if the next packet in buffer is also ready to write
    			{
    				Packet* temp = window[0];
    				window.pop_front();
    				int num_bytes = temp->get_header().data_size;
    				update_next_ack_num(num_bytes);
    				if (write(my_file_fd, temp->get_data(), num_bytes) == -1)
    					report_error_and_exit("Error writing to file");
    				free(temp);
    			}

    			Packet* ack_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, 1, 0, 0);
    			send_packet(client_fd, ack_packet, client_addr, *addrlen, 0); 
    			log_sent_packet(ack_packet);
    			// acknowledge the lastest received packet
    		}

    		// case 2: receive out of order packet. Then buffer the packet
    		else if (h.seq_num > next_ack_num)
    		{
 				int is_buffered = 0;
    			for (int i = 0; i != window.size(); ++i)
    				if (window[i] -> get_header().seq_num == h.seq_num)
    					is_buffered = 1;

    			if (! is_buffered)
    			{
    				window.push_back(file_packet);
					sort(window.begin(), window.end(), my_compare);
				}

    			// sort the buffered packets by their seq number

    			Packet* ack_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, 1, 0, 0);
    			send_packet(client_fd, ack_packet, client_addr, *addrlen, 0);
    			log_sent_packet(ack_packet, 1); 
    			// acknowledge the lastest received packet

    		}

    		else  // receive an old packet received before
    		{
    			Packet* ack_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, 1, 0, 0);
    			send_packet(client_fd, ack_packet, client_addr, *addrlen, 0); 
    			log_sent_packet(ack_packet, 1);
    			free(file_packet);
    		}
		}
	}

	// add all outer of order packets to file
	while (! window.empty())
		// if the next packet in buffer is also ready to write
	{
		Packet* temp = window[0];
		window.pop_front();
		int num_bytes = temp->get_header().data_size;
		update_next_ack_num(num_bytes);
		if (write(my_file_fd, temp->get_data(), num_bytes) == -1)
			report_error_and_exit("Error writing to file");
		free(temp);
	}

	// printf("Finish receiving file");
	return true;
}



int RDTConnection::client_close(int server_fd, struct sockaddr* server_addr, socklen_t addrlen)
{
	cerr << "Start closing connection at client." << endl;
	Packet* fin_packet = new Packet(next_seq_num, 0, cwnd_size, false, false, true);
	next_seq_num += 1;
	// set fin bit to 1

	struct pollfd fds[1];
  	fds[0].fd = server_fd;
	fds[0].events = POLLIN | POLLHUP | POLLERR;
	fds[0].revents = 0;

  	int poll_ret;
  	// int timeout = 500; 
  	int count = 0;
  	// start connecting to server
  	while (true) 
  	{
  		send_packet(server_fd, fin_packet, server_addr, addrlen, false);
	    log_sent_packet(fin_packet);
	    // printf("Start sending syn packet.\n");
	    // send syn packet
	    poll_ret = poll(fds, 1, timeout);
	    // wait for 0.5 seconds
	    if (poll_ret == -1) { 
			report_error_and_exit("Error in poll");
	    }
	    else if (poll_ret == 0)
	    {
	    	count++;
	    	if (count > 10000 / timeout)
	    	{
	    		// we have not received message from server for 10 seconds
	    		fprintf(stderr, "%s\n", "Error: no response from server over 10 seconds");
	    		return false;
	    	}
	    }
	    else if (poll_ret > 0 && fds[0].revents & POLLIN ) {

	    	Packet* ack_packet = recv_packet(server_fd, server_addr, &addrlen); 
	    	log_received_packet(ack_packet);
	    	Header h = ack_packet -> get_header();
	    	count = 0;

	    	if (h.fin_flag && h.ack_flag) // receive fin ack packet from server
	    		break;

	    	if (h.fin_flag)
	    	{
	    		next_ack_num = h.seq_num + 1;
	    		Packet* ack_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, true, false, true);
	    		send_packet(server_fd, ack_packet, server_addr, addrlen, 0);
	    		log_sent_packet(ack_packet);
	    		free(ack_packet);
	    		break;
	    	}
  		}
  	}

  	struct timespec start, now;
	clock_gettime(CLOCK_REALTIME, &start);

	while (true)
	{
		poll_ret = poll(fds, 1, unit);
		if (poll_ret == -1) { 
			report_error_and_exit("Error in poll");
	    }

	    else if (poll_ret > 0 && fds[0].revents & POLLIN ) {

	    	Packet* fin_packet = recv_packet(server_fd, server_addr, &addrlen); 
	    	log_received_packet(fin_packet);
	    	Header h = fin_packet -> get_header();

	    	if (h.fin_flag) 
	    	{	
	    		// receive fin packet from server
	    		next_ack_num = h.seq_num + 1;
	    		Packet* ack_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, true, false, true);
	    		send_packet(server_fd, ack_packet, server_addr, addrlen, 0);
	    		log_sent_packet(ack_packet);
	    		free(ack_packet);
	    	}

	    	free(fin_packet);
  		}

  		// wait for two seconds
  		clock_gettime(CLOCK_REALTIME, &now);
  		int elapse_time = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000.0;
  		if (elapse_time > 2000)
  			break;
	}

	return true;

}

int RDTConnection::server_close(int client_fd, struct sockaddr* client_addr, socklen_t* addrlen)
{
	cerr << "Receive fin packet from client." << endl;
	Packet* ack_packet = new Packet(next_seq_num, next_ack_num, cwnd_size, true, false, true);
	// set fin ack bits to 1
	send_packet(client_fd, ack_packet, client_addr, *addrlen, false);
	log_sent_packet(ack_packet);
	// send fin ack packet
	free(ack_packet);

	Packet* fin_packet = new Packet(next_seq_num, 0, cwnd_size, false, false, true);

	struct pollfd fds[1];
  	fds[0].fd = client_fd;
	fds[0].events = POLLIN | POLLHUP | POLLERR;
	fds[0].revents = 0;

  	int poll_ret;
  	// int timeout = 500; 
  	int count = 0;
  	// start connecting to server
  	while (true) 
  	{
  		send_packet(client_fd, fin_packet, client_addr, *addrlen, false);
	    log_sent_packet(fin_packet);
	    // printf("Start sending syn packet.\n");
	    // send syn packet
	    poll_ret = poll(fds, 1, timeout);
	    // wait for 0.5 seconds
	    if (poll_ret == -1) { 
			report_error_and_exit("Error in poll");
	    }
	    else if (poll_ret == 0)
	    {
	    	count++;
	    	if (count > 10000 / timeout)
	    	{
	    		// we have not received message from server for 10 seconds
	    		fprintf(stderr, "%s\n", "Error: no response from client over 10 seconds");
	    		return false;
	    	}
	    }
	    else if (poll_ret > 0 && fds[0].revents & POLLIN ) {

	    	Packet* fin_ack_packet = recv_packet(client_fd, client_addr, addrlen); 
	    	log_received_packet(fin_ack_packet);
	    	Header h = fin_ack_packet -> get_header();
	    	count = 0;

	    	if (h.fin_flag && h.ack_flag) // receive fin ack packet from client
	    		break;

	    	free(fin_ack_packet);
  		}
  	}
  	
  	free(fin_packet);
	return true;

}


/*
int main()
{
	return -1;
}
*/







