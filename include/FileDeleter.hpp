#ifndef TIN_P2P_FILEDELETER_HPP
#define TIN_P2P_FILEDELETER_HPP

#include <cstdio>
#include <string>

class FileDeleter {
    std::string filename;
public:
    FileDeleter(const std::string &name)
            : filename(name)
    { }

    bool deleteFile() {
        return unlink(filename.c_str()) == 0;
    }
};


#endif //TIN_P2P_FILEDELETER_HPP
