#include "Guard.hpp"

Guard::Guard(Mutex& m) {
	mutex = &m;
	mutex->lock();
}

Guard::~Guard() {
	mutex->unlock();
}
