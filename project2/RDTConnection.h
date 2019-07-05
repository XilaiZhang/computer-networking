#include <deque>
#include "Packet.h"

using namespace std;

// parameters for reliable data transfer
const uint16_t MSS = 512;
const uint16_t MAX_SEQ_NUM = 25600;
const uint16_t MAX_CWIND_SIZE = 10240;
const int timeout = 500; // retransmission rto = 500ms
const int unit = 5; // a unit of time

class RDTConnection
{
public:
	RDTConnection();
	void update_next_seq_num(int num_bytes);
	void send_packet(int sockfd, Packet* p, const struct sockaddr *dest_addr, socklen_t addrlen, int isRetransmit);
	Packet* recv_packet(int sockfd, struct sockaddr *src_addr, socklen_t* addrlen);
	int client_handshake(int server_fd, struct sockaddr* server_addr, socklen_t addrlen);
	int server_handshake(int sockfd, struct sockaddr* client_addr, socklen_t* addrlen);
	void update_cwnd_size();
	void update_next_ack_num(int num_bytes);
	int send_file(int server_fd, struct sockaddr* server_addr, socklen_t addrlen, const char* filename);
	int receive_file(int client_fd, struct sockaddr* server_addr, socklen_t* addrlen, const char* filename);
	void log_sent_packet(const Packet* p, int is_dup = 0);
	void log_received_packet(const Packet* p, int is_dup = 0);
	int server_close(int client_fd, struct sockaddr* client_addr, socklen_t* addrlen);
	int client_close(int server_fd, struct sockaddr* server_addr, socklen_t addrlen);

private:
	int next_seq_num;
	int next_ack_num;
	double cwnd_size;
	int ssthresh;
	int cwnd_base;
	int isFastRecovery;
	deque<Packet*> window;
};
