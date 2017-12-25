#include "server.hpp"
#include <iostream>

void Server::finishThread()
{
	Guard* g = new Guard(threadsVecMutex);
	for (auto i = connectionThreads.begin(); i != connectionThreads.end(); ++i)
	{
		if ( i->isCurrentThread() )
		{
			connectionThreads.erase(i);
			delete g;
			pthread_detach(pthread_self());
			Thread::exit();
		}
	}
	// listener thread
	delete g;
	pthread_detach(pthread_self());
	Thread::exit();
}

void Server::pushConnectionThread(Thread& t)
{
	threadsVecMutex.lock();
	connectionThreads.push_back(t);
	threadsVecMutex.unlock();
}

void Server::stopListening()
{
	stop = true;
	waitForThreadsToFinish();
}

void Server::waitForThreadsToFinish()
{
	for (Thread t : connectionThreads)
	{
		t.get();
	}
}

Server::~Server()
{

}
