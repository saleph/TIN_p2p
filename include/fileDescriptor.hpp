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
#include <netinet/in.h>

const size_t MAX_FILENAME_LEN = 255;


/// File descriptor class.
/// Manages the main structure which describes a file in the network.
/// Automatically manages the size and MD5 hash of the file during construction.
class FileDescriptor {
public:
	explicit FileDescriptor() = default;
	explicit FileDescriptor(const std::string& filename);

	FileDescriptor(const FileDescriptor &other);

	const Md5Hash& getMd5() const;
	std::string getName() const;
	uint32_t getSize() const;
	void makeValid();
	void makeUnvalid();
	bool isValid() const;
	in_addr_t getHolderIp() const;
	void setHolderIp(in_addr_t holderIp);
	in_addr_t getOwnerIp() const;
	void setOwnerIp(in_addr_t ownerIp);
	time_t getUploadTime() const;
	std::string getFormattedUploadTime() const;
	void setUploadTime(time_t uploadTime);

    FileDescriptor &operator=(const FileDescriptor &other);

private:
	static uint32_t obtainFileSize(const char* fn);
	void setName(std::string filename);

	char name[MAX_FILENAME_LEN+1]{};
	Md5Hash md5;
	uint32_t size{};
	time_t uploadTime{};
	in_addr_t ownerIp{};
	in_addr_t holderIp{};
	bool valid;
};


#endif /* INCLUDE_FILEDESCRIPTOR_HPP_ */
