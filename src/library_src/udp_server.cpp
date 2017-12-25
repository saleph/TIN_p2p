#include "udp_server.hpp"

#include <string.h>
#include <iostream>
#include <errno.h>

UdpServer::UdpServer(void (*receiveBroadcastCallback)(uint8_t*, uint32_t, SocketOperation op))
{
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_port = htons(PORT);
	broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
	react = receiveBroadcastCallback;
}

void UdpServer::broadcast(uint8_t* bytes, uint32_t size)
{
	int mySocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (mySocket == -1)
	{
		throw SocketException("Could not create broadcast socket. Additional"
						"info: " + strerror(errno));
	}
	else if ((setsockopt(mySocket, SOL_SOCKET, SO_BROADCAST,
		&broadcastEnable, sizeof broadcastEnable)) == -1)
	{
		close(mySocket);
		throw SocketException("Could not set socket broadcast option. Additional"
						"info: " + strerror(errno));
	}
	else
	{
		sendto(mySocket, bytes, size, 0,
							(struct sockaddr *)&broadcastAddr, sizeof broadcastAddr);
		close(mySocket);
	}
}

void UdpServer::startListening()
{
	int* mySocket = new int;
	mySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	setsockopt(*mySocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof broadcastEnable);

	sockaddr_in recvAddr;
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(PORT);
	recvAddr.sin_addr.s_addr = INADDR_ANY;

	if(bind(*mySocket, (struct sockaddr *) &recvAddr, sizeof recvAddr) == -1)
	{
		throw SocketException("Could not create broadcast listening socket"
				" Additional info: " + strerror(errno));
	}

	Thread t(&UdpServer::actualStartListening, (void*)mySocket, NULL);
	pushConnectionThread(t);
}

void* UdpServer::actualStartListening(void* socket)
{
	int mySocket = *((int*)socket);
	delete socket;
	sockaddr_in sender;
	unsigned int slen = sizeof(sockaddr);
	uint8_t* buf;

	while(!stop)
	{
		buf = new uint8_t[1024];
		sockaddr_in sender;
		uint32_t len = recvfrom(mySocket, buf, sizeof(buf), 0, (sockaddr*)&sender, &slen);
		HandleBroadcastArgs* args = new HandleBroadcastArgs;
		*args = { this, buf, len, sender.sin_addr.s_addr };
		Thread t(&UdpServer::handleBroadcastReceive, (void*)args , NULL);
	}


	close(mySocket);
	finishThread();
}

void* UdpServer::handleBroadcastReceive(void* handleArgs)
{
	HandleBroadcastArgs* args = (HandleBroadcastArgs*) handleArgs;
	SocketOperation op = { SocketOperation::Type::UdpReceive,
			SocketOperation::Status::Success, args->senderIp
	};
	react(args->data, args->dataSize, op);
	delete args->data;
	delete args;
	finishThread();
}
