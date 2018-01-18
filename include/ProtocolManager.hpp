#ifndef TIN_P2P_PROTOCOLMANAGER_HPP
#define TIN_P2P_PROTOCOLMANAGER_HPP


#include <memory>
#include "udp_server.hpp"
#include "tcp_server.hpp"
#include "p2pMessage.hpp"
#include "messageType.hpp"
#include "fileDescriptor.hpp"
#include <boost/log/trivial.hpp>
#include <unordered_map>
#include <vector>

namespace p2p {
    const char *getFormatedIp(in_addr_t addr);
    void startSession();
    void endSession();
    std::vector<FileDescriptor> getLocalFileDescriptors();
    std::vector<FileDescriptor> getNetworkFileDescriptors();
    bool uploadFile(const std::string &name);
    bool getFile(const std::string &name);
    bool getFile(const std::string &name, const std::string &hash);
    bool deleteFile(const std::string &name);
    bool deleteFile(const std::string &name, const std::string &hash);

};


#endif //TIN_P2P_PROTOCOLMANAGER_HPP
