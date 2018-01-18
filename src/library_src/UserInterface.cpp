#include "UserInterface.hpp"
#include "ProtocolManager.hpp"
#include <FileLoader.hpp>
#include <signal.h>

UserInterface::UserInterface() {}

void UserInterface::start() {
    std::cout << "TIN p2p" << std::endl;
    std::cout << "START" << std::endl;
    help();
    std::string inputString;
    int canFinish = 1;
    do {
        try {
            getline(std::cin, inputString);
            canFinish = checkInput(inputString);
        }
        catch (std::invalid_argument &) {
            std::cout << "This file does not exist" << std::endl;
        }
    } while (canFinish != 0);
}

int UserInterface::checkInput(const std::string &inputString) {
    std::vector<std::string> tokens = split(inputString, ' ');

    if (tokens[0] == "connect" || tokens[0] == "c") {
        if (isConnected) {
            std::cout << "Node is already connected" << std::endl;
            return 2;
        }

        p2p::startSession();
        isConnected = true;
        return 1;
    }

    if (tokens[0] == "disconnect" || tokens[0] == "diss") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 3;
        }

        p2p::endSession();
        return 0;
    }

    if (tokens[0] == "upload" || tokens[0] == "u") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 4;
        }

        for (int i = 1; i < tokens.size(); i++) {
            p2p::uploadFile(tokens[i]);
        }
        return 5;
    }

    if (tokens[0] == "delete" || tokens[0] == "d") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 6;
        }
        for (int i = 1; i < tokens.size(); i++) {
            p2p::deleteFile(tokens[i]);
        }
        return 7;
    }

    if (tokens[0] == "deletemd5" || tokens[0] == "dmd5") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 6;
        }
        for (int i = 1; i < tokens.size(); i += 2) {
            p2p::deleteFile(tokens[i], tokens[i + 1]);
        }
        return 7;
    }

    if (tokens[0] == "get" || tokens[0] == "g") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 8;
        }

        for (int i = 1; i < tokens.size(); i++) {
            p2p::getFile(tokens[i]);
        }
        return 9;
    }


    if (tokens[0] == "getmd5" || tokens[0] == "gmd5") {
        if (!isConnected) {
            std::cout << "Node is disconnected" << std::endl;
            return 8;
        }

        for (int i = 1; i < tokens.size(); i += 2) {
            p2p::getFile(tokens[i], tokens[i + 1]);
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

    if (tokens[0] == "help") {
        help();
        return 14;
    }

    std::cout << "Incorrect command" << std::endl;
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
    std::cout << std::endl;
    for (int i = 0; i < fileDescriptors.size(); i++) {
        std::cout << "name: " << fileDescriptors[i].getName();
        std::cout << " md5: " << fileDescriptors[i].getMd5().getHash();
        std::cout << " owner: " << p2p::getFormatedIp(fileDescriptors[i].getOwnerIp());
        std::cout << " holder: " << p2p::getFormatedIp(fileDescriptors[i].getHolderIp());
        std::cout << std::endl;
    }
    std::cout << std::endl;
}


void UserInterface::help() {
    std::cout << std::endl;
    std::cout << "Available commands:" << std::endl;
    std::cout << "connect" << std::endl;
    std::cout << "disconnect" << std::endl;
    std::cout << "upload <filenames>" << std::endl;
    std::cout << "delete <filenames>" << std::endl;
    std::cout << "deletemd5 <filenames> <md5>" << std::endl;
    std::cout << "get <filenames>" << std::endl;
    std::cout << "getmd5 <filenames> <md5>" << std::endl;
    std::cout << "saf (show all files in network)" << std::endl;
    std::cout << "slf (show local files)" << std::endl;
    std::cout << "help" << std::endl;
    std::cout << std::endl;
};

