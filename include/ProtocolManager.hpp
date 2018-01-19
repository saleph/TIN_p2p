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
#include <p2pMessage.hpp>
#include <iostream>
#include <FileStorer.hpp>
#include <thread>
#include "FileLoader.hpp"
#include "mutex.hpp"
#include "guard.hpp"
#include "FileDeleter.hpp"
#include <memory>
#include "udp_server.hpp"
#include "tcp_server.hpp"
#include "p2pMessage.hpp"
#include "messageType.hpp"
#include "fileDescriptor.hpp"
#include <boost/log/trivial.hpp>
#include <unordered_map>
#include <vector>
#include <thread>

namespace p2p {
    namespace util {
        extern std::unordered_map<MessageType, std::function<void(const uint8_t *, uint32_t, in_addr_t)>> msgProcessors;

        void initProcessingFunctions();
    }

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



// private members
namespace p2p {
    namespace util {
        extern std::shared_ptr<TcpServer> tcpServer;
        extern std::shared_ptr<UdpServer> udpServer;

        extern std::vector<FileDescriptor> localDescriptors;
        extern std::vector<FileDescriptor> networkDescriptors;
        extern std::vector<in_addr_t> nodesAddresses;
        extern Mutex mutex;

        // starting state estimators
        const unsigned NEW_NODE_STATE_DURATION_MS = 5000u;
        extern bool isStillNewNode;
        extern std::thread timer;
        extern Mutex mutexIsStillNewNode;

        void processTcpMsg(uint8_t *data, uint32_t size, SocketOperation operation);
        void processTcpError(SocketOperation operation);
        void processUdpMsg(uint8_t *data, uint32_t size, SocketOperation operation);
        void joinToNetwork();
        void quitFromNetwork();
        void moveLocalDescriptorsIntoOtherNodes();
        in_addr_t findLeastLoadedNode();
        void discardDescriptor(FileDescriptor &descriptor);
        void changeHolderNode(FileDescriptor &descriptor, in_addr_t newNodeAddress);
        std::vector<uint8_t> getFileContent(const std::string &name);
        void storeFileContent(std::vector<uint8_t> &content, const std::string &name);
        void publishDescriptor(FileDescriptor &descriptor);
        void uploadFile(FileDescriptor &descriptor);
        in_addr_t findOtherLeastLoadedNode();
        void removeDuplicatesFromLists();
        void sendCommandRefused(MessageType messageType, const char *msg, in_addr_t sourceAddress);
        void sendShutdown();
        void publishLostNode(in_addr_t nodeAddress);
        void requestGetFile(FileDescriptor &descriptor);
        void requestDeleteFile(FileDescriptor &descriptor);
        bool isDescriptorUnique(FileDescriptor &descriptor);
        bool getFile(FileDescriptor &descriptor);
        bool deleteFile(FileDescriptor &descriptor);
        FileDescriptor &getRepetedDescriptor(FileDescriptor &descriptor);
    }
}

#endif //TIN_P2P_PROTOCOLMANAGER_HPP
