
#ifndef INCLUDE_SERVER_HPP_
#define INCLUDE_SERVER_HPP_

#include <vector>

#include "socket_operation.hpp"
#include "thread.hpp"
#include "guard.hpp"

class Server
{
protected:
	std::vector <Thread> connectionThreads;
	Mutex threadsVecMutex;
	bool stop;

	struct SocketContext
	{
		Server* serverInstance;
		int connSocket;
		in_addr_t connAddr;
	};

	void finishThread();
	void pushConnectionThread(Thread& t);
	void Server::waitForThreadsToFinish();
public:
	virtual void startListening() = 0;
	void stopListening();
	virtual ~Server();
};



#endif /* INCLUDE_SERVER_HPP_ */
