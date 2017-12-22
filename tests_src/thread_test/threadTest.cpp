#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE ThreadsTest
#include <boost/test/unit_test.hpp>
#include "thread.hpp"
#include <signal.h>

void *fun(void * x)
{
   int y=(int)x;
   for (int i=1;i<=1000;i++) {
       y++;
   }
   return (void *) (y);
}

Thread * t1;

void * exit_fun() {
    t1->exit();
}

void *fun2(void *)
{
    int i;
    for (i=1;i<100;i++) {
        if (i == 35) {
            exit_fun();
        }
    }
    return (void *)i;
}

bool signalConfirmed = false;
void sig_fun(int sig) {
    signalConfirmed = true;
}

void *fun3(void *) {
    signal(10,sig_fun);
    sleep(4);
}

BOOST_AUTO_TEST_CASE(createGetThreadsTest)
{
    void * retval1;
    void * retval2;
    void * arg1 = (void *)4;
    void * arg2 = (void *)5;

    Thread * thread1 = new Thread(fun,arg1,&retval1);
    Thread * thread2 = new Thread(fun,arg2,&retval2);

    BOOST_CHECK((int)thread1->get() == 1004);
    BOOST_CHECK((int)retval1 == 1004);
    BOOST_CHECK((int)thread2->get() == 1005);
    BOOST_CHECK((int)retval2 == 1005);
}

BOOST_AUTO_TEST_CASE(exitThreadTest)
{
    void * retval;
    t1 = new Thread(fun2,NULL,&retval);
    BOOST_CHECK((int)t1->get() != 100);
}

BOOST_AUTO_TEST_CASE(killThreadTest)
{
    void * retval;
    Thread * thread = new Thread(fun3,NULL,&retval);
    sleep(1);
    thread->kill(10);
    thread->get();
    BOOST_CHECK(signalConfirmed == true);
}


