#ifndef TIN_P2P_FILELOADER_HPP
#define TIN_P2P_FILELOADER_HPP

#include "md5sum.hpp"
#include "md5hash.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <streambuf>
#include <utility>

class FileLoader {
    std::string filename;
    std::vector<uint8_t> fileContent;
public:
    explicit FileLoader(std::string file);

    std::vector<uint8_t> &getContent();
};


#endif //TIN_P2P_FILELOADER_HPP
