#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <string.h>
#include <iostream>
#include <errno.h>
#include <fcntl.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

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

	int enable = 1;
	setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
		&enable, sizeof enable);
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEPORT,
		&enable, sizeof enable);

	if( bind(listenSocket,(struct sockaddr *)&recvAddr , sizeof(recvAddr)) < 0)
	{
		std::string err = "Could not bind listening socket. Additional"
				"info: ";
		err += strerror(errno);
		throw SocketException(err.c_str());
	}

	SocketContext* ctx = new SocketContext(this, listenSocket, 0);
	listenerThread = new Thread(&TcpServer::actualStartListening, (void*) ctx, NULL);
}

void* TcpServer::actualStartListening(void* args)
{
		TcpServer* serverInstance = (TcpServer*)((SocketContext*)args)->serverInstance;
		serverInstance->listenSocket = ((SocketContext*)args)->connSocket;
		listen(serverInstance->listenSocket, 2500);
		delete (SocketContext*)args;

		sockaddr_in senderAddr;
		unsigned int senderAddrSize = sizeof senderAddr;

		while(1)
		{
			int connSock = accept(serverInstance->listenSocket,
					(sockaddr*)&senderAddr, &senderAddrSize);

            if (connSock == -1)
            {
                if (serverInstance->stop.load())
                {
                    return NULL;
                }
                else continue;
            }

            SocketContext* ctx = new SocketContext(serverInstance, connSock, senderAddr.sin_addr.s_addr);
            if (!serverInstance->addDispatcherThread(&TcpServer::handleConnectionHelper,
					(void*) ctx, NULL))
            {
                delete ctx;
                return NULL;
            }
		}
}

void* TcpServer::handleConnectionHelper(void* args)
{
	SocketContext* ctx = (SocketContext*) args;
	TcpServer* server = (TcpServer*) ctx->serverInstance;
	server->handleConnection(ctx->connSocket, ctx->connAddr);
	delete ctx;
	server->finishThread();
	return NULL;
}

void TcpServer::handleConnection(int sock, in_addr_t senderAddr)
{
    //++debugThrCount;
	int readLength;
	uint8_t* buf;
	P2PMessage msg;

	struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout));

	readLength = recv(sock, (void*)&msg, sizeof(msg), 0);
    if(checkReceiveIssues(readLength, senderAddr, sizeof(msg)))
    {
        return;
    }

	uint32_t bufSize = sizeof(msg) + msg.getAdditionalDataSize();
	buf = new uint8_t[bufSize];
	memcpy((void*)buf, &msg, sizeof(msg));

	uint32_t remainingSize = msg.getAdditionalDataSize();
	uint8_t* streamPointer = (uint8_t*)(buf + sizeof(msg));

	while(remainingSize > 0)
    {
        readLength = recv(sock, streamPointer, remainingSize, 0);
        if(checkReceiveIssues(readLength, senderAddr))
        {
            delete[] buf;
            return;
        }
        remainingSize -= readLength;
        streamPointer += readLength;
    }

	SocketOperation op = { SocketOperation::Type::TcpReceive,
	SocketOperation::Status::Success, senderAddr };

	close(sock);
	if (readLength == -1)
		errorCallback(op);
	else
		react(buf, bufSize, op);

	delete[] buf;
}

bool TcpServer::checkReceiveIssues(int readLength, in_addr_t sender, int expectedSize)
{
    if ( (readLength <= 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        || (expectedSize != -1 && expectedSize != readLength) )
    {
        SocketOperation op(SocketOperation::Type::TcpReceive,
                           SocketOperation::Status::ReceiveFailed,
                           sender);
        errorCallback(op);
        return true;
    }

    return false;
}

void TcpServer::sendData(uint8_t* data, size_t n, in_addr_t toWhom)
{
    uint8_t* copiedData = new uint8_t[n];
    memcpy(copiedData, data, n);
	SendArgs* args = new SendArgs(copiedData, n, toWhom);
	ActualSendDataContext* ctx = new ActualSendDataContext(this, args);
	if(!addDispatcherThread(actualSendDataHelper, (void*)ctx, NULL))
    {
        delete[] copiedData;
        delete args;
        delete ctx;
        return;
    }
}

void* TcpServer::actualSendDataHelper(void* args)
{
	ActualSendDataContext* ctx =(ActualSendDataContext*)args;
	SendArgs* s = ctx->args;
	TcpServer* server = (TcpServer*) ctx->serverInstance;
	server->actualSendData(s->data, s->size, s->toWhom);
	delete[] ctx->args->data;
	delete ctx->args;
	delete ctx;
	server->finishThread();
	return NULL;
}

void TcpServer::actualSendData(uint8_t* data, uint32_t size, in_addr_t toWhom)
{
    Mutex debugMutex;
	sockaddr_in sendAddr;
	sendAddr.sin_family = AF_INET;
	sendAddr.sin_port = htons(LISTEN_PORT);
	sendAddr.sin_addr.s_addr = toWhom;

	int sendSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (sendSocket == -1)
	{
		SocketOperation op(SocketOperation::Type::TcpSend,
				SocketOperation::Status::CantOpenSocket, toWhom);
		errorCallback(op);
		return;
	}

	struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    setsockopt(sendSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                sizeof(timeout));

	int status = connect(sendSocket, (sockaddr*) &sendAddr, sizeof(sendAddr));
	if (status == -1)
	{
		close(sendSocket);
		SocketOperation op(SocketOperation::Type::TcpSend,
						SocketOperation::Status::CantConnect, toWhom);
		errorCallback(op);
		return;
	}

	if(send(sendSocket, data, size, 0) != size)
    {
        SocketOperation op(SocketOperation::Type::TcpSend,
						SocketOperation::Status::SendFailed, toWhom);
        errorCallback(op);
    }
	close(sendSocket);
	return;
}

TcpServer::~TcpServer()
{
}
