
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
	int listenSocket;
	Thread* listenThread;

	struct SocketContext
	{
		Server* serverInstance;
		int connSocket;
		in_addr_t connAddr;

		void setValues(Server* s, int c, in_addr_t a)
		{
			serverInstance = s; connSocket = c; connAddr = a;
		}
	};

	void finishThread();
	void pushConnectionThread(Thread& t);
	void waitForThreadsToFinish();
public:
	Server();
	virtual void startListening() = 0;
	void stopListening();
	virtual ~Server();
};



#endif /* INCLUDE_SERVER_HPP_ */
