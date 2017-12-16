#include "mutex.hpp"

#ifndef INCLUDE_GUARD_HPP_
#define INCLUDE_GUARD_HPP_

class Guard {
	Mutex* mutex;
public:
	Guard(Mutex&);
	~Guard();
};



#endif /* INCLUDE_GUARD_HPP_ */
