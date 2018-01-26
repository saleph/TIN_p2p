
#ifndef INCLUDE_SERVER_HPP_
#define INCLUDE_SERVER_HPP_

#include <vector>
#include <atomic>

#include "SocketOperation.hpp"
#include "Thread.hpp"
#include "Guard.hpp"

class Server
{
protected:
	std::vector <Thread> connectionThreads;
	Thread* listenerThread;
	Mutex threadsVecMutex;
	Mutex stopMutex;
	int listenSocket;
	std::atomic<bool> stop;

	struct SocketContext
	{
		Server* serverInstance;
		int connSocket;
		in_addr_t connAddr;

		SocketContext(Server* s, int c, in_addr_t a) : serverInstance(s), connSocket(c), connAddr(a)
		{
		}
	};

	void finishThread();
	void pushConnectionThread(Thread& t);
	void waitForThreadsToFinish();
	bool addDispatcherThread(void * function_pointer(void *), void * arg, void ** retval);
public:
	Server();
	virtual void startListening() = 0;
	void stopListening();
	virtual ~Server();
	static in_addr_t getLocalhostIp();
};



#endif /* INCLUDE_SERVER_HPP_ */
