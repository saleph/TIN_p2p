#include "server.hpp"

#include <unistd.h>
#include <iostream>

void Server::finishThread()
{
    stopMutex.lock();
    if (stop.load())
    {
        stopMutex.unlock();
        return;
    }
	threadsVecMutex.lock();
	stopMutex.unlock();

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
}

void Server::stopListening()
{
    stopMutex.lock();
    stop = true;
    shutdown(listenSocket, SHUT_RDWR);
    threadsVecMutex.lock();
    stopMutex.unlock();

	waitForThreadsToFinish();
	close(listenSocket);
	threadsVecMutex.unlock();

	if (listenerThread != nullptr)
    {
        listenerThread->get();
        delete listenerThread;
    }
}

bool Server::addDispatcherThread(void * function_pointer(void *), void * arg, void ** retval)
{
    stopMutex.lock();
    if (stop)
    {
        stopMutex.unlock();
        return false;
    }
    threadsVecMutex.lock();
    stopMutex.unlock();
    connectionThreads.emplace_back(function_pointer, arg , retval);
    threadsVecMutex.unlock();
    return true;
}

void Server::waitForThreadsToFinish()
{
	for (auto i = connectionThreads.begin(); i != connectionThreads.end(); ++i)
	{
		i->get();
	}
}

Server::Server() : stop(false)
{
    listenerThread = nullptr;
}

Server::~Server()
{
}
