#include <boost/log/trivial.hpp>
#include "FileLoader.hpp"

FileLoader::FileLoader(std::string file)
        : filename(std::move(file)) {
    std::ifstream t(filename);
    std::string fileAsString((std::istreambuf_iterator<char>(t)),
                             std::istreambuf_iterator<char>());

    fileContent.resize(fileAsString.length()+1);
    memcpy(fileContent.data(), fileAsString.data(), fileAsString.length()+1);
}

std::vector<uint8_t> &FileLoader::getContent() {
    return fileContent;
}
