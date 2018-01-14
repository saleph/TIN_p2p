#ifndef TIN_P2P_PROTOCOLMANAGER_HPP
#define TIN_P2P_PROTOCOLMANAGER_HPP


#include <memory>
#include "udp_server.hpp"
#include "tcp_server.hpp"
#include "p2pMessage.hpp"
#include "messageType.hpp"
#include <boost/log/trivial.hpp>
#include <unordered_map>
#include <vector>

namespace p2p {
    namespace util {
        std::unordered_map<MessageType, std::function<void(const uint8_t*, uint32_t, in_addr_t)>> messageProcessors;
        std::shared_ptr<TcpServer> tcpServer;
        std::shared_ptr<UdpServer> udpServer;
		
		std::vector<FileDescriptor> localDescriptors;
		std::vector<FileDescriptor> networkDescriptors;

        void processTcpMsg(uint8_t *data, uint32_t size, SocketOperation operation);
        void processTcpError(SocketOperation operation);
        void processUdpMsg(uint8_t *data, uint32_t size, SocketOperation operation);
        void initProcessingFunctions();
    }

    void initialize();
};


#endif //TIN_P2P_PROTOCOLMANAGER_HPP
