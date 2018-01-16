#include <boost/log/trivial.hpp>
#include "FileLoader.hpp"

FileLoader::FileLoader(std::string file)
        : filename(std::move(file)) {
}

std::vector<uint8_t> FileLoader::getContent() {
    std::ifstream t("p2pfiles/" + filename);
    std::string fileAsString((std::istreambuf_iterator<char>(t)),
                             std::istreambuf_iterator<char>());

    std::vector<uint8_t> fileContent(fileAsString.length()+1);
    memcpy(fileContent.data(), fileAsString.data(), fileAsString.length()+1);
    return fileContent;
}
