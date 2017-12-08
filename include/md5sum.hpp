#ifndef INCLUDE_MD5SUM_HPP_
#define INCLUDE_MD5SUM_HPP_

#include <string>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <ctype.h>
#include <iostream>
#include <exception>
#include <unistd.h>
#include "md5hash.hpp"


class Md5sum {
	const std::string filename;

public:
	Md5sum(const std::string &filename);

	Md5Hash getMd5Hash() const;
};





#endif /* INCLUDE_MD5SUM_HPP_ */
