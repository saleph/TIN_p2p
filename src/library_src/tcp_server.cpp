#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>
#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <chrono>
#include <ctime>

#include "tcp_server.hpp"
#include "socket_exceptions.hpp"

TcpServer::TcpServer(void (*reactFunc)(uint8_t*, size_t),
		void (*errorCallbackFunc)(SocketOperation) )
{
	react = reactFunc;
	errorCallback = errorCallbackFunc;
}

void TcpServer::startListening()
{
	sockaddr_in recvAddr;
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(LISTEN_PORT);
	recvAddr.sin_addr.s_addr = INADDR_ANY;

	int listenSocket = socket(AF_INET , SOCK_STREAM , 0);
	if (listenSocket == -1)
	{
		throw SocketException("Could not create listening socket. Additional"
				"info: " + strerror(errno));
	}

	if( bind(listenSocket,(struct sockaddr *)&recvAddr , sizeof(recvAddr)) < 0)
	{
		throw SocketException("Could not bind listening socket. Additional"
				"info: " + strerror(errno));
	}

	SocketContext* ctx = new SocketContext();
	*ctx = { this, listenSocket };
	Thread(&TcpServer::actualStartListening, (void*) ctx, NULL);
}

void* TcpServer::actualStartListening(void* args)
{
		TcpServer* serverInstance = (TcpServer*)((SocketContext*)args)->serverInstance;
		int listenSocket = ((SocketContext*)args)->connSocket;
		listen(listenSocket, 5);

		sockaddr_in senderAddr;
		unsigned int senderAddrSize;
		pthread_t lastThread;

		while(!stop)
		{
			int connSock = accept(listenSocket, (sockaddr*)&senderAddr, &senderAddrSize);
			SocketContext* ctx = new SocketContext();
			*ctx = { this, connSock, senderAddr.sin_addr.s_addr };
			threadsVecMutex.lock();
			Thread t = Thread(&TcpServer::handleConnectionHelper,
					(void*) ctx, NULL);
			connectionThreads.push_back(t);
			threadsVecMutex.unlock();
		}

		waitForThreadsToFinish();
		close(connSock);
		delete args;
		finishThread();
}

void* TcpServer::handleConnectionHelper(void* args)
{
	SocketContext* ctx = (SocketContext*) args;
	TcpServer* server = (TcpServer*) ctx->serverInstance;
	server->handleConnection(ctx->connSocket, ctx->connAddr);
	delete ctx;
	server->finishThread();
}

void TcpServer::handleConnection(int sock, in_addr_t senderAddr)
{
	int readLength;
	uint8_t buf[1000];

	do
	{
		readLength = read(sock, (void*)buf, sizeof(buf));
	} while(readLength == sizeof(buf));

	SocketOperation op = { SocketOperation::Type::TcpReceive,
	SocketOperation::Status::Success, senderAddr };

	close(sock);
	if (readLength == -1)
		errorCallback();
	else
		react(buf, readLength, op);
}

void TcpServer::sendData(uint8_t* data, size_t n, in_addr_t toWhom)
{
	SendArgs* args = new SendArgs();
	*args = {data, n, toWhom};
	ActualSendDataContext* ctx = new ActualSendDataContext;
	ctx->serverInstance = this;
	ctx->args = args;
	Guard* g = new Guard(threadsVecMutex);
	connectionThreads.push_back( Thread(actualSendDataHelper, (void*)ctx, NULL) );
	delete g;
}

void* TcpServer::actualSendDataHelper(void* args)
{
	ActualSendDataContext* ctx =(ActualSendDataContext*)args;
	SendArgs* s = ctx->args;
	TcpServer* server = (TcpServer*) ctx->serverInstance;
	server->actualSendData(s->data, s->size, s->toWhom);
	delete ctx->args;
	delete ctx;
	server->finishThread();
}

void TcpServer::actualSendData(uint8_t* data, size_t size, in_addr_t toWhom)
{
	sockaddr_in sendAddr;
	sendAddr.sin_family = AF_INET;
	sendAddr.sin_port = htons(LISTEN_PORT);
	sendAddr.sin_addr.s_addr = toWhom;

	int sendSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (sendSocket == -1)
	{
		std::cout << "Can't open socket\n" << strerror(errno) << std::endl;
		SocketOperation op(SocketOperation::Type::TcpSend,
				SocketOperation::Status::CantOpenSocket, toWhom);
		errorCallback(op);
		return;
	}

	int status = connect(sendSocket, (sockaddr*) &sendAddr, sizeof(sendAddr));

	if (status == -1)
	{
		close(sendSocket);
		std::cout << "Can't connect\n";
		SocketOperation op(SocketOperation::Type::TcpSend,
						SocketOperation::Status::CantConnect, toWhom);
		errorCallback(op);
		return;
	}

	send(sendSocket, data, size, 0);

	close(sendSocket);
	return;
}
