#include <FileLoader.hpp>
#include "ProtocolManager.hpp"


int main() {
    int i;
    p2p::startSession();
    int counter1 = 0;
    int counter2 = 0;
    int counter3 = 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    bool spin = true;
    std::string name;
    while (spin) {
        std::cin >> i;
        switch (i) {
            case 0:
                spin = false;
            case 1:
                p2p::uploadFile("example" + std::to_string(counter1++) + ".txt");
                break;
            case 2:
                name = "example" + std::to_string(counter2++) + ".txt";
                p2p::getFile(name, Md5sum(name).getMd5Hash());
                break;
            case 3:
                name = "example" + std::to_string(counter3++) + ".txt";
                p2p::deleteFile(name, Md5sum(name).getMd5Hash());
                break;
            default:
                std::cerr << "wron opt\n";
                break;
        }
    }
#pragma clang diagnostic pop
    p2p::endSession();
}