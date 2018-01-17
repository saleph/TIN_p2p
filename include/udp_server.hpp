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
	bool isSelfBroadcastDisable = true;
	sockaddr_in broadcastAddr;

	struct HandleBroadcastArgs
	{
		UdpServer* serverInstance;
		uint8_t* data;
		uint32_t dataSize;
		in_addr_t senderIp;

		HandleBroadcastArgs(UdpServer* s, uint8_t* d, uint32_t si, in_addr_t ip)
		: serverInstance(s), data(d), dataSize(si), senderIp(ip) {}
	};

	// callback if broadcast is received
	void (*react)(uint8_t* data, uint32_t size, SocketOperation op);

	static void* actualStartListening(void* ctx);
	static void* handleBroadcastReceive(void* ctx);
public:
	UdpServer(void (*receiveBroadcastCallback)(uint8_t* data, uint32_t size,
			SocketOperation op));
    // starts thread that will run callbacks in new threads if broadcast is received
	void startListening();
	void enableSelfBroadcasts();
	void disableSelfBroadcasts();
	// sends broadcast from current thread
	void broadcast(uint8_t* bytes, uint32_t size);
	~UdpServer();
};



#endif /* INCLUDE_UDP_SERVER_HPP_ */
