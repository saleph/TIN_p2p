
#ifndef INCLUDE_SOCKET_OPERATION_HPP_
#define INCLUDE_SOCKET_OPERATION_HPP_

#include <chrono>
#include <arpa/inet.h>

struct SocketOperation
{
	enum Type
	{
		TcpSend,
		TcpReceive,
		UdpReceive
	};

	enum Status
	{
		Success,
		CantOpenSocket,
		CantConnect,
		ReceiveFailed,
		SendFailed
	};

	Type type;
	Status status;
	in_addr_t connectionAddr;

	SocketOperation(Type t, Status s, in_addr_t addr) : type(t), status(s),
			connectionAddr(addr) {}

    SocketOperation() {}

};



#endif /* INCLUDE_SOCKET_OPERATION_HPP_ */
