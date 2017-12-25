#ifndef INCLUDE_MD5HASH_HPP_
#define INCLUDE_MD5HASH_HPP_

#include <string>
#include <cassert>
#include <cstring>
#include <cstdio>


static const int MD5_HASH_LENGTH = 32;

class Md5Hash {
	char hash[MD5_HASH_LENGTH + 1];

public:
	Md5Hash() { }
	Md5Hash(const std::string &h) {
		assert(h.size() == MD5_HASH_LENGTH);
		strcpy(hash, h.c_str());
	}

	Md5Hash(const char *h) {
		assert(strlen(h) == MD5_HASH_LENGTH);
		strcpy(hash, h);
	}

	std::string getHash() const {
		return std::string(hash);
	}
};

#endif /* INCLUDE_MD5HASH_HPP_ */
