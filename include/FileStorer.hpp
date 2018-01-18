#ifndef TIN_P2P_FILESTORER_HPP
#define TIN_P2P_FILESTORER_HPP


#include <utility>
#include <string>
#include <vector>
#include <fstream>
#include "md5hash.hpp"
#include "md5sum.hpp"

class FileStorer {
public:
    explicit FileStorer(std::string name)
            : filename(std::move(name))
    { }

    void storeFile(const std::vector<uint8_t> &content) {
        std::ofstream file(filename, std::ios_base::out);
        file << (char*)content.data();
        file.close();
    }

    Md5Hash getHash() const {
        return Md5sum(filename).getMd5Hash();
    }

private:
    std::string filename;
};


#endif //TIN_P2P_FILESTORER_HPP
