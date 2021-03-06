#include "FileDescriptor.hpp"

FileDescriptor::FileDescriptor(const std::string& filename) {
	setName(filename);
	this->size = obtainFileSize(name);
	this->md5 = Md5sum(filename).getMd5Hash();
}

FileDescriptor::FileDescriptor(const FileDescriptor &other) {
    *this = other;
}

const Md5Hash& FileDescriptor::getMd5() const {
	return md5;
}

std::string FileDescriptor::getName() const {
	return std::string(name);
}

uint32_t FileDescriptor::getSize() const {
	return size;
}

uint32_t FileDescriptor::getHolderIp() const {
	return holderIp;
}

void FileDescriptor::setHolderIp(uint32_t holderIp) {
	this->holderIp = holderIp;
}

uint32_t FileDescriptor::getOwnerIp() const {
	return ownerIp;
}

void FileDescriptor::setOwnerIp(uint32_t ownerIp) {
	this->ownerIp = ownerIp;
}

time_t FileDescriptor::getUploadTime() const {
	return uploadTime;
}

std::string FileDescriptor::getFormattedUploadTime() const {
	char *formattedTime = ctime(&uploadTime);
	return std::string(formattedTime);
}

void FileDescriptor::setUploadTime(time_t uploadTime) {
	this->uploadTime = uploadTime;
}

uint32_t FileDescriptor::obtainFileSize(const char* fn) {
	struct stat st;
	if (stat(fn, &st) == -1) {
		throw std::invalid_argument(std::string(fn) + " doesn't exist");
	}
	return st.st_size;
}

void FileDescriptor::setName(std::string filename) {
	if (filename.size() > MAX_FILENAME_LEN) {
		throw std::invalid_argument("filename can't be longer than "
				+ std::to_string(MAX_FILENAME_LEN));
	}
    std::copy(filename.begin(), filename.end(), name);
    name[filename.size()] = 0;
}

void FileDescriptor::makeValid() {
	valid = true;
}

void FileDescriptor::makeUnvalid() {
	valid = false;
}

bool FileDescriptor::isValid() const {
	return valid;
}

FileDescriptor &FileDescriptor::operator=(const FileDescriptor &other) {
	strcpy(name, other.name);
    md5 = other.md5;
    size = other.size;
    uploadTime = other.uploadTime;
    ownerIp = other.ownerIp;
    holderIp = other.holderIp;
    valid = other.valid;

    return *this;
}
