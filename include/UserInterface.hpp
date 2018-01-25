#ifndef INCLUDE_USERINTERFACE_HPP_
#define INCLUDE_USERINTERFACE_HPP_

#include <iostream>
#include <vector>
#include <FileDescriptor.hpp>


class UserInterface {
public:
    UserInterface();
    void start();

private:
    int checkInput(const std::string &inputString);
    std::vector<std::string> split(const std::string &inputStream, char delim);
    void showFileDescriptors(std::vector<FileDescriptor> fileDescriptors);
    void help();
    bool isConnected = false;
};



#endif
