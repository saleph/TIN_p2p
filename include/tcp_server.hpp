
#ifndef INCLUDE_TCP_SERVER_HPP_
#define INCLUDE_TCP_SERVER_HPP_

#include <cstdint>
#include <cstddef>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "server.hpp"

class TcpServer : public Server
{
private:
	// arguments struct for actualSendData
	struct SendArgs
	{
		uint8_t* data;
		size_t size;
		in_addr_t toWhom;
	};

	// pthreads can't handle instance methods therefore context struct
	// with pointer to instance is needed
	struct ActualSendDataContext
	{
		TcpServer* serverInstance;
		SendArgs* args;
	};

	const int LISTEN_PORT = 3333;

	void (*react)(uint8_t*, uint32_t, SocketOperation);
	void (*errorCallback)(SocketOperation op);


	static void* actualStartListening(void* context);
	static void* handleConnectionHelper(void* context);
	static void* actualSendDataHelper(void* context);
	void handleConnection(int connSocket, in_addr_t senderAddr);
	void actualSendData(uint8_t* data, uint32_t size, in_addr_t toWhom);
public:
	TcpServer(void (*react)(uint8_t*, uint32_t, SocketOperation),
			void (*errorCallbackFunc)(SocketOperation));
	void startListening();
	void sendData(uint8_t* data, size_t n, in_addr_t toWhom);
};



#endif /* INCLUDE_TCP_SERVER_HPP_ */
