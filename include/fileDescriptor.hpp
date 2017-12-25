#ifndef INCLUDE_FILEDESCRIPTOR_HPP_
#define INCLUDE_FILEDESCRIPTOR_HPP_


#include "md5hash.hpp"
#include "md5sum.hpp"
#include "messageType.hpp"
#include <stdint.h>
#include <stdexcept>
#include <cstring>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

const size_t MAX_FILENAME_LEN = 255;


/// File descriptor class.
/// Manages the main structure which describes a file in the network.
/// Automatically manages the size and MD5 hash of the file during construction.
class FileDescriptor {
public:
	FileDescriptor(const std::string& filename);

	const Md5Hash& getMd5() const;
	std::string getName() const;
	uint32_t getSize() const;
	uint32_t getHolderIp() const;
	void setHolderIp(uint32_t holderIp);
	uint32_t getOwnerIp() const;
	void setOwnerIp(uint32_t ownerIp);
	time_t getUploadTime() const;
	std::string getFormattedUploadTime() const;
	void setUploadTime(time_t uploadTime);

private:
	static uint32_t obtainFileSize(const char* fn);
	void setName(const std::string& name);

	char name[MAX_FILENAME_LEN+1];
	Md5Hash md5;
	uint32_t size;
	time_t uploadTime;
	uint32_t ownerIp;
	uint32_t holderIp;
};


#endif /* INCLUDE_FILEDESCRIPTOR_HPP_ */
