default: server client
bunch= Packet.cpp RDTConnection.cpp 
server:
	g++ -std=c++11 $(bunch) server.cpp -o server
client:
	g++ -std=c++11 $(bunch) client.cpp -o client
clean:
	rm -rf *.tar.gz server client 
