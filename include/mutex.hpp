#include <pthread.h>

#ifndef INCLUDE_MUTEX_HPP_
#define INCLUDE_MUTEX_HPP_


class Mutex {
	pthread_mutex_t mutex;

public:
	Mutex();
	void lock();
	void unlock();
	~Mutex();
};

#endif /* INCLUDE_MUTEX_HPP_ */
