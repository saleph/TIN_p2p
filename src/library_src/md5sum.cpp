#include "md5sum.hpp"

Md5sum::Md5sum(const std::string &fn)
	: filename(fn)
{
	const int FILE_EXISTS_FLAG = F_OK | R_OK;
	if( access( filename.c_str(), FILE_EXISTS_FLAG ) == -1 ) {
	    throw std::invalid_argument(filename + " doesn't exist");
	}
}

Md5Hash Md5sum::getMd5Hash() const {
	char buffer[500];
	std::string cmd = "md5sum -b " + filename;

	// excecute linux md5sum command
	FILE *fp = popen(cmd.c_str(), "r");
	fscanf(fp, "%s", buffer);
	pclose(fp);

	return Md5Hash(buffer);
}
