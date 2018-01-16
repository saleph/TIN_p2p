#include <FileLoader.hpp>
#include "ProtocolManager.hpp"


int main() {
    p2p::startSession();
    int i;
    std::cin >> i;
    p2p::closeSession();
}