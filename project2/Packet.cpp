#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Packet.h"

/*
const uint16_t HEADER_SIZE = 12;
const uint16_t MAX_DATA_SIZE = 512;
const uint16_t MAX_BYTES = 1024;

struct Header
{
	uint16_t seq_num; // max seq num 25600
	uint16_t ack_num; 
	uint16_t cwnd; // 512 - 10240
	uint16_t data_size; // max data size 512
	bool ack_flag;
	bool syn_flag;
	bool fin_flag; 
};

class Packet
{
public:
	Packet(uint16_t seq_num, uint16_t ack_num, uint16_t cwnd, bool is_ack, bool is_syn, bool is_fin);
	Packet(uint16_t seq_num, uint16_t ack_num, uint16_t cwnd, uint16_t length, bool is_ack, bool is_syn, bool is_fin, const char* data);
	Packet(const char* arr, int length);
	Header get_header();
	char* toString();


private:
	Header m_header;
	char m_data[MAX_DATA_SIZE];
};
*/

// create an empty packet
Packet::Packet(uint16_t seq_num, uint16_t ack_num, uint16_t cwnd, bool is_ack, bool is_syn, bool is_fin)
	: m_header{seq_num, ack_num, cwnd, 0, is_ack, is_syn, is_fin}
{
	memset(m_data, 0, MAX_DATA_SIZE);
}

// create a packet with data
Packet::Packet(uint16_t seq_num, uint16_t ack_num, uint16_t cwnd, uint16_t length, bool is_ack, bool is_syn, bool is_fin, const char* data)
	: m_header{seq_num, ack_num, cwnd, length, is_ack, is_syn, is_fin}
{
	if (length > MAX_DATA_SIZE)
	{		
		fprintf(stderr, "Packet too large!\n");
		exit(EXIT_FAILURE);
	}

	memcpy(m_data, data, length);
	// copy the actual data into packet

}

// read a packet from char array
Packet::Packet(const char* arr, int length)
{
	int num_bytes;
	memcpy(&m_header, arr, sizeof(struct Header));
	num_bytes = m_header.data_size;
	if (HEADER_SIZE + num_bytes > length)
	{
		fprintf(stderr, "Error creating packet from string!\n");
		exit(EXIT_FAILURE);
	}
	memcpy(m_data, arr + HEADER_SIZE, num_bytes);
}

Header Packet::get_header() const
{
	return m_header;
}

char* Packet::get_data()
{
	return m_data;
} 

char* Packet::toString() const
{
	// convert packet to char array representation
	char* arr = (char*) calloc(MAX_BYTES, sizeof(char));
	memcpy(arr, (const void*) &m_header, sizeof(struct Header));
	// fill in the first 8 bytes + 3 bits of arr with the bit battern of header
	memcpy(arr + HEADER_SIZE, m_data, m_header.data_size);
	// fill in the rest of the arr with actual message
	return arr;
}




