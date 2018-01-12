#include "udp_server.hpp"

#include <string.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>

#include "socket_exceptions.hpp"

#define BUF_SIZE 1024

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
	listenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	setsockopt(listenSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof broadcastEnable);

	sockaddr_in recvAddr;
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(PORT);
	recvAddr.sin_addr.s_addr = INADDR_ANY;

	if(bind(listenSocket, (struct sockaddr *) &recvAddr, sizeof recvAddr) == -1)
	{
		std::string err = "Could not create broadcast listening socket"
				" Additional info: ";
		err += strerror(errno);
		throw SocketException(err.c_str());
	}

	SocketContext* ctx = new SocketContext((Server*)this, listenSocket, 0);

	Thread t(&UdpServer::actualStartListening, (void*)ctx, NULL);
}

void* UdpServer::actualStartListening(void* ctx)
{
	UdpServer* server = (UdpServer*)(((SocketContext*)ctx)->serverInstance);
	int mySocket = ((SocketContext*)ctx)->connSocket;
	delete (SocketContext*)ctx;
	sockaddr_in sender;
	unsigned int slen = sizeof(sockaddr);
	uint8_t* buf;
    uint32_t rcvlen;

	while(1)
	{
		buf = new uint8_t[BUF_SIZE];
		sockaddr_in sender;
        rcvlen = recvfrom(mySocket, buf, BUF_SIZE, 0, (sockaddr*)&sender, &slen);
        if (rcvlen == 0)
        {
            if (server->stop) return NULL;
            else continue;
        }

        HandleBroadcastArgs* args = new HandleBroadcastArgs(server, buf, rcvlen, sender.sin_addr.s_addr);
		if(!server->addDispatcherThread(&UdpServer::handleBroadcastReceive, (void*)args , NULL))
        {
            delete args;
            return NULL;
        }
	}
}

void* UdpServer::handleBroadcastReceive(void* handleArgs)
{
	HandleBroadcastArgs* args = (HandleBroadcastArgs*) handleArgs;
	UdpServer* server = args->serverInstance;
	SocketOperation op = { SocketOperation::Type::UdpReceive,
			SocketOperation::Status::Success, args->senderIp
	};
	server->react(args->data, args->dataSize, op);
	delete[] args->data;
	delete args;
	server->finishThread();
}

UdpServer::~UdpServer()
{

}
