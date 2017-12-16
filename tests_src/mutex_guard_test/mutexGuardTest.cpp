#define BOOST_TEST_NO_LIB
#include <pthread.h>
#include <boost/test/unit_test.hpp>
#include "guard.hpp"
#include "mutex.hpp"

BOOST_AUTO_TEST_SUITE(MutexTest);

struct HelperArgStruct {
	Mutex* m;
	int* num;
	bool* running;

	HelperArgStruct(Mutex* m, int* num, bool* run) {
		this->m = m;
		this->num = num;
		this->running = run;
	}
};

void* incrementFunc(void* arguments)
{
	HelperArgStruct* args = (HelperArgStruct*)arguments;
	args->m->lock();
	while(*(args->running))
	{
		++(*(args->num));
	}
	args->m->unlock();
}

BOOST_AUTO_TEST_CASE(checkMutexLocksThread)
{
	pthread_t thread1, thread2, thread3;
	Mutex m;
	bool run1 = true, run2 = true, run3 = true;
	int n1 = 0, n2 = 0, n3 = 0;

	HelperArgStruct args1(&m, &n1, &run1), args2(&m, &n2, &run2),
			args3(&m, &n3, &run3);

	pthread_create( &thread1, NULL, incrementFunc, &args1);
	usleep(10000);
	pthread_create( &thread2, NULL, incrementFunc, &args2);
	pthread_create( &thread3, NULL, incrementFunc, &args3);

	BOOST_TEST(n1 != 0);
	BOOST_TEST(n2 == 0);
	BOOST_TEST(n3 == 0);

	run1 = false;
	usleep(10000);

	BOOST_TEST( ((n2 != 0 && n3 == 0) || (n2 == 0 && n3 != 0)) );
	run2 = false;
	run3 = false;

	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);
}

void* incrementFuncGuard(void* arguments)
{
	HelperArgStruct* args = (HelperArgStruct*)arguments;
	Guard g(*(args->m));
	while(*(args->running))
	{
		++(*(args->num));
	}
}

BOOST_AUTO_TEST_CASE(checkGuardLocksThread)
{
	pthread_t thread1, thread2, thread3;
	Mutex m;
	bool run1 = true, run2 = true, run3 = true;
	int n1 = 0, n2 = 0, n3 = 0;

	HelperArgStruct args1(&m, &n1, &run1), args2(&m, &n2, &run2),
			args3(&m, &n3, &run3);

	pthread_create( &thread1, NULL, incrementFuncGuard, &args1);
	usleep(10000);
	pthread_create( &thread2, NULL, incrementFuncGuard, &args2);
	pthread_create( &thread3, NULL, incrementFuncGuard, &args3);

	BOOST_TEST(n1 != 0);
	BOOST_TEST(n2 == 0);
	BOOST_TEST(n3 == 0);

	run1 = false;
	usleep(10000);

	BOOST_TEST( ((n2 != 0 && n3 == 0) || (n2 == 0 && n3 != 0)) );
	run2 = false;
	run3 = false;

	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);
}

BOOST_AUTO_TEST_SUITE_END();
