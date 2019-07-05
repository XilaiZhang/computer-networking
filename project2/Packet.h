// declaration of class Packet
#include <stdint.h> 


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
	Header get_header() const;
	char* toString() const;
	char* get_data();
	
private:
	Header m_header;
	char m_data[MAX_DATA_SIZE];
};