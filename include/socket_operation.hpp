
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
		CantConnect
	};

	Type type;
	Status status;
	in_addr_t connectionAddr;

	SocketOperation(Type t, Status s, in_addr_t addr) : type(t), status(s),
			connectionAddr(addr) {}

	/*std::string toString()
	{
		std::string operationTypeString = (type == Send ? "Send to" : "Receive from");
		std::string operationStatusString;
		char buf[INET_ADDRSTRLEN];

		switch(status)
		{
			case Success:
				operationStatusString = "Success";
				break;

		}

		return "Operation " + operationTypeString
				+ inet_ntop(AF_INET, (void*)connectionAddr, buf, sizeof(buf) )
				+ start_time + " ended at " + end_time + " with status "
				+ operationStatusString;
	}*/

};



#endif /* INCLUDE_SOCKET_OPERATION_HPP_ */
