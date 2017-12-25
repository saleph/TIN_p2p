#include "thread.hpp"
#include <signal.h>

Thread::Thread(void * function_pointer(void *), void * arg, void ** retval) {
    pthread_create(&thread_id,NULL,function_pointer,arg);
    if (retval != NULL)
        rv = retval;
}

void * Thread::get() {
    pthread_join(thread_id,rv);
    return *rv;
}

void Thread::exit() {
    pthread_exit(NULL);
}

bool Thread::isCurrentThread() {
	return pthread_equal(this->thread_id, pthread_self());
}

void Thread::kill(int signal) {
    pthread_kill(thread_id,signal);
}
