#include <memory>
#include "md5sum.hpp"

Md5sum::Md5sum(const std::string &fn)
	: filename(fn)
{
	const int FILE_EXISTS_FLAG = F_OK | R_OK;
	if( access( filename.c_str(), FILE_EXISTS_FLAG ) == -1 ) {
	    throw std::invalid_argument(filename + " doesn't exist");
	}

	std::array<char, 500> buffer {0};
	std::string cmd = "md5sum -b " + filename;

	// excecute linux md5sum command
	FILE *fp = popen(cmd.c_str(), "r");
	fscanf(fp, "%s", buffer.data());
	pclose(fp);

    hash = Md5Hash(buffer);
}

Md5Hash Md5sum::getMd5Hash() const {
	return hash;
}

Md5sum::Md5sum(std::istream &stream) {
    std::string fileContent{std::istreambuf_iterator<char>(stream), {}};

    std::array<char, 500> buffer {0};
    std::string cmd = "echo \"" + fileContent + "\" | md5sum";

    // excecute linux md5sum command
    FILE *fp = popen(cmd.c_str(), "r");
    fscanf(fp, "%s", buffer.data());
    pclose(fp);

    hash = Md5Hash(buffer);
}
