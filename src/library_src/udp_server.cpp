#include "udp_server.hpp"

#include <string.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>

#include "socket_exceptions.hpp"

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
		std::string err = "Could not create broadcast socket. Additional"
				"info: ";
		err += strerror(errno);
		throw SocketException(err.c_str());
	}
	else if ((setsockopt(mySocket, SOL_SOCKET, SO_BROADCAST,
		&broadcastEnable, sizeof broadcastEnable)) == -1)
	{
		close(mySocket);
		std::string err = "Could not set socket broadcast option. Additional"
				"info: ";
		err += strerror(errno);
		throw SocketException(err.c_str());
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
	int mySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	setsockopt(mySocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof broadcastEnable);

	sockaddr_in recvAddr;
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(PORT);
	recvAddr.sin_addr.s_addr = INADDR_ANY;

	if(bind(mySocket, (struct sockaddr *) &recvAddr, sizeof recvAddr) == -1)
	{
		std::string err = "Could not create broadcast listening socket"
				" Additional info: ";
		err += strerror(errno);
		throw SocketException(err.c_str());
	}

	SocketContext* ctx = new SocketContext;
	ctx->setValues((Server*)this, mySocket, 0);

	Thread t(&UdpServer::actualStartListening, (void*)ctx, NULL);
	pushConnectionThread(t);
}

void* UdpServer::actualStartListening(void* ctx)
{
	UdpServer* server = (UdpServer*)(((SocketContext*)ctx)->serverInstance);
	int mySocket = ((SocketContext*)ctx)->connSocket;
	delete (SocketContext*)ctx;
	sockaddr_in sender;
	unsigned int slen = sizeof(sockaddr);
	uint8_t* buf;

	while(1)
	{
		buf = new uint8_t[1024];
		sockaddr_in sender;
		uint32_t len = recvfrom(mySocket, buf, sizeof(buf), 0, (sockaddr*)&sender, &slen);
		HandleBroadcastArgs* args = new HandleBroadcastArgs;
		*args = { server, buf, len, sender.sin_addr.s_addr };
		Thread t(&UdpServer::handleBroadcastReceive, (void*)args , NULL);
	}


	close(mySocket);
	server->finishThread();
}

void* UdpServer::handleBroadcastReceive(void* handleArgs)
{
	HandleBroadcastArgs* args = (HandleBroadcastArgs*) handleArgs;
	SocketOperation op = { SocketOperation::Type::UdpReceive,
			SocketOperation::Status::Success, args->senderIp
	};
	args->serverInstance->react(args->data, args->dataSize, op);
	delete args->data;
	delete args;
	args->serverInstance->finishThread();
}
