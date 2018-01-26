#include "Server.hpp"

#include <stdio.h> //printf
#include <string.h>    //memset
#include <errno.h> //errno
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

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
    usleep(50000);
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

in_addr_t Server::getLocalhostIp()
{
    FILE *f;
    char line[100] , *p , *c;

    f = fopen("/proc/net/route" , "r");

    while(fgets(line , 100 , f))
    {
        p = strtok(line , " \t");
        c = strtok(NULL , " \t");

        if(p!=NULL && c!=NULL)
        {
            if(strcmp(c , "00000000") == 0)
            {
                break;
            }
        }
    }

    //which family do we require , AF_INET or AF_INET6
    int fm = AF_INET;
    struct ifaddrs *ifaddr, *ifa;
    int family , s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1)
    {
        throw std::runtime_error("Could not get local ip address");
    }

    //Walk through linked list, maintaining head pointer so we can free list later
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }

        family = ifa->ifa_addr->sa_family;

        if(strcmp( ifa->ifa_name , p) == 0)
        {
            if (family == fm)
            {
                s = getnameinfo( ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6) , host , NI_MAXHOST , NULL , 0 , NI_NUMERICHOST);

                if (s != 0)
                {
                    throw std::runtime_error("Could not get local ip address");
                }
            }
        }
    }

    fclose(f);
    freeifaddrs(ifaddr);

    return inet_addr(host);
}

Server::Server() : stop(false)
{
    listenerThread = nullptr;
}

Server::~Server()
{
}
