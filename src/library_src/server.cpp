#include "server.hpp"

#include <unistd.h>

void Server::finishThread()
{
    if (stop) return;

	threadsVecMutex.lock();
	for (auto i = connectionThreads.begin(); i != connectionThreads.end(); ++i)
	{
		if ( i->isCurrentThread() )
		{
			connectionThreads.erase(i);
            threadsVecMutex.unlock();
			pthread_detach(pthread_self());
			Thread::exit();
		}
	}
	// listener thread
	threadsVecMutex.unlock();
	pthread_detach(pthread_self());
	Thread::exit();
}

void Server::stopListening()
{
    threadsVecMutex.lock();
    stop = true;
    shutdown(listenSocket, SHUT_RDWR);
	waitForThreadsToFinish();
	close(listenSocket);
	threadsVecMutex.unlock();
}

bool Server::addDispatcherThread(void * function_pointer(void *), void * arg, void ** retval)
{
    threadsVecMutex.lock();
    if (stop)
    {
        threadsVecMutex.unlock();
        return false;
    }
    Thread t(function_pointer, arg , retval);
    connectionThreads.push_back(t);
    threadsVecMutex.unlock();
    return true;
}

void Server::waitForThreadsToFinish()
{
	for (Thread t : connectionThreads)
	{
		t.get();
	}
}

Server::Server()
{

}

Server::~Server()
{

}
