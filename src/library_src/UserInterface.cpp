#include "UserInterface.hpp"
#include "ProtocolManager.hpp"
#include <FileLoader.hpp>


UserInterface::UserInterface() {
    std::string inputString;

    do {
        getline(std::cin, inputString);
    } while (checkInput(inputString)!= 0);
}

int UserInterface::checkInput(const std::string &inputString) {
    std::vector<std::string> tokens = split(inputString, ' ');

    if (tokens[0] == "connect") {
        if (isConnected) {
            std::cout << "Node is already connected" << std::endl;
            return 2;
        }

        p2p::startSession();
        isConnected = true;
        return 1;
    }

    if (tokens[0] == "disconnect") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 3;
        }

        p2p::endSession();
        return 0;
    }

    if (tokens[0] == "upload") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 4;
        }

        for (int i = 1; i < tokens.size(); i++) {
            p2p::uploadFile(tokens[i]);
        }
        return 5;
    }

    if (tokens[0] == "delete") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 6;
        }
        for (int i = 1; i < tokens.size(); i++) {
            p2p::deleteFile(tokens[i], Md5sum(tokens[i]).getMd5Hash());
        }
        return 7;
    }

    if (tokens[0] == "get") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 8;
        }

        for (int i = 1; i < tokens.size(); i++) {
            p2p::getFile(tokens[i], Md5sum(tokens[i]).getMd5Hash());
        }
        return 9;
    }

    if (tokens[0] == "slf") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 8;
        }

        std::vector<FileDescriptor> fileDecriptors = p2p::getLocalFileDescriptors();
        if (fileDecriptors.empty()) {
            std::cout << "There are no local files" << std::endl;
            return 9;
        }

        showFileDescriptors(fileDecriptors);
        return 10;
    }

    if (tokens[0] == "saf") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 11;
        }

        std::vector<FileDescriptor> fileDecriptors = p2p::getNetworkFileDescriptors();
        if (fileDecriptors.empty()) {
            std::cout << "There are no files in network" << std::endl;
            return 12;
        }

        showFileDescriptors(fileDecriptors);
        return 13;
    }
}

std::vector<std::string> UserInterface::split(const std::string &inputStream, char delim) {
    std::vector<std::string> tokens;

    std::stringstream ss(inputStream);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *((std::back_inserter(tokens))++) = item;
    }

    return tokens;
}

void UserInterface::showFileDescriptors(std::vector<FileDescriptor> fileDescriptors) {
    for (int i = 0; i < fileDescriptors.size(); i++) {
        std::cout << "name: " << fileDescriptors[i].getName();
        std::cout << " md5: " << fileDescriptors[i].getMd5().getHash();
        std::cout << " owner :" << fileDescriptors[i].getOwnerIp();
        std::cout << " holder: " << fileDescriptors[i].getHolderIp();
        std::cout << std::endl;
    }
}

