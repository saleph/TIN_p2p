#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>
#include <iostream>
#include <errno.h>
#include <fcntl.h>

#include "tcp_server.hpp"
#include "socket_exceptions.hpp"
#include "p2pMessage.hpp"

TcpServer::TcpServer(void (*reactFunc)(uint8_t* data, uint32_t size, SocketOperation),
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
		std::string err = "Could not create listening socket. Additional"
				"info: ";
		err += strerror(errno);
		throw SocketException(err.c_str());
	}

	if( bind(listenSocket,(struct sockaddr *)&recvAddr , sizeof(recvAddr)) < 0)
	{
		std::string err = "Could not bind listening socket. Additional"
				"info: ";
		err += strerror(errno);
		throw SocketException(err.c_str());
	}

	SocketContext* ctx = new SocketContext();
	*ctx = { this, listenSocket };
	Thread t(&TcpServer::actualStartListening, (void*) ctx, NULL);
	listenThread = &t;
}

void* TcpServer::actualStartListening(void* args)
{
		TcpServer* serverInstance = (TcpServer*)((SocketContext*)args)->serverInstance;
		serverInstance->listenSocket = ((SocketContext*)args)->connSocket;
		delete (SocketContext*)args;
		listen(serverInstance->listenSocket, 5);

		sockaddr_in senderAddr;
		unsigned int senderAddrSize;
		pthread_t lastThread;

		while(1)
		{
			int connSock = accept(serverInstance->listenSocket,
					(sockaddr*)&senderAddr, &senderAddrSize);
			SocketContext* ctx = new SocketContext();
			*ctx = { serverInstance, connSock, senderAddr.sin_addr.s_addr };
			serverInstance->threadsVecMutex.lock();
			Thread t = Thread(&TcpServer::handleConnectionHelper,
					(void*) ctx, NULL);
			serverInstance->connectionThreads.push_back(t);
			serverInstance->threadsVecMutex.unlock();
		}

		/*std::cout << "CLOSING FFS\n";
		close(listenSocket);
		delete (SocketContext*)args;
		serverInstance->finishThread();*/
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
	uint8_t* buf;
	P2PMessage msg;

	readLength = read(sock, (void*)&msg, sizeof(msg));
	buf = new uint8_t[sizeof(msg) + msg.getAdditionalDataSize()];
	memcpy((void*)buf, &msg, sizeof(msg));
	readLength = read(sock, (void*)(buf + sizeof(msg)), msg.getAdditionalDataSize());

	SocketOperation op = { SocketOperation::Type::TcpReceive,
	SocketOperation::Status::Success, senderAddr };

	close(sock);
	if (readLength == -1)
		errorCallback(op);
	else
		react(buf, readLength, op);

	delete buf;
}

void TcpServer::sendData(uint8_t* data, size_t n, in_addr_t toWhom)
{
	SendArgs* args = new SendArgs();
	*args = {data, n, toWhom};
	ActualSendDataContext* ctx = new ActualSendDataContext;
	ctx->serverInstance = this;
	ctx->args = args;
	Thread t = Thread(actualSendDataHelper, (void*)ctx, NULL);
	pushConnectionThread(t);
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

void TcpServer::actualSendData(uint8_t* data, uint32_t size, in_addr_t toWhom)
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
