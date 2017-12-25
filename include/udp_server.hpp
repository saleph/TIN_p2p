#ifndef INCLUDE_UDP_SERVER_HPP_
#define INCLUDE_UDP_SERVER_HPP_

#include <cstdint>
#include <cstddef>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "server.hpp"

class UdpServer : public Server
{
	const int PORT = 2000;
	int broadcastEnable = 1;
	sockaddr_in broadcastAddr;

	struct HandleBroadcastArgs
	{
		UdpServer* serverInstance;
		uint8_t* data;
		uint32_t dataSize;
		in_addr_t senderIp;
	};

	void (*react)(uint8_t* data, uint32_t size, SocketOperation op);
	static void* actualStartListening(void* ctx);
	static void* handleBroadcastReceive(void* ctx);
public:
	UdpServer(void (*receiveBroadcastCallback)(uint8_t* data, uint32_t size,
			SocketOperation op));
	void startListening();
	void broadcast(uint8_t* bytes, uint32_t size);
};



#endif /* INCLUDE_UDP_SERVER_HPP_ */
