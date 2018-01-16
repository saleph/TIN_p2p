#include <FileLoader.hpp>
#include "ProtocolManager.hpp"


int main() {
    int i;
    p2p::startSession();
    std::cin >> i;
    p2p::uploadFile("example.txt");
    std::cin >> i;
    p2p::endSession();
}