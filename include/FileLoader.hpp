#ifndef TIN_P2P_FILELOADER_HPP
#define TIN_P2P_FILELOADER_HPP

#include "Md5sum.hpp"
#include "Md5hash.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <streambuf>
#include <utility>

class FileLoader {
    std::string filename;
public:
    explicit FileLoader(std::string file);

    std::vector<uint8_t> getContent();
};


#endif //TIN_P2P_FILELOADER_HPP
