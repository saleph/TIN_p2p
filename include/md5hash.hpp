#ifndef INCLUDE_MD5HASH_HPP_
#define INCLUDE_MD5HASH_HPP_

#include <string>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <array>


static const int MD5_HASH_LENGTH = 32;

class Md5Hash {
	char hash[MD5_HASH_LENGTH + 1] = {0};

public:
	Md5Hash() = default;
	Md5Hash(const Md5Hash &other) {
		strcpy(hash, other.hash);
	}

	explicit Md5Hash(const std::string &h) {
		assert(h.size() == MD5_HASH_LENGTH);
		strcpy(hash, h.c_str());
	}

	template <unsigned long size>
	explicit Md5Hash(const std::array<char, size> &array) {
		assert(strlen(array.data()) == MD5_HASH_LENGTH);
		strcpy(hash, array.data());
	}

	std::string getHash() const {
		return std::string(hash);
	}

	Md5Hash &operator=(const Md5Hash &other) {
		strcpy(hash, other.hash);
		return *this;
	}

	bool operator==(const Md5Hash &other) const {
		return getHash() == other.getHash();
	}

    bool operator!=(const Md5Hash &other) const {
        return !operator==(other);
    }
};

#endif /* INCLUDE_MD5HASH_HPP_ */
