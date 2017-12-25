#include <pthread.h>

#ifndef INCLUDE_THREAD_HPP_
#define INCLUDE_THREAD_HPP_

class Thread {
private:
    pthread_t thread_id;
    void ** rv;
public:
    Thread(void * function_pointer(void *), void * arg, void ** retval);
    void * get();
    static void exit();
    bool isCurrentThread();
    void kill(int signal);
};

#endif
