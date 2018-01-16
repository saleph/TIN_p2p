#include <p2pMessage.hpp>
#include <iostream>
#include "FileLoader.hpp"
#include "mutex.hpp"
#include "guard.hpp"
#include "ProtocolManager.hpp"


// definitions HAVE TO BE THERE
// otherwise - multiple definitions error
namespace p2p {
    namespace util {
        std::unordered_map<MessageType, std::function<void(const uint8_t *, uint32_t, in_addr_t)>> messageProcessors;
        std::shared_ptr<TcpServer> tcpServer;
        std::shared_ptr<UdpServer> udpServer;

        std::vector<FileDescriptor> localDescriptors;
        std::vector<FileDescriptor> networkDescriptors;
        Mutex mutex;

        void processTcpMsg(uint8_t *data, uint32_t size, SocketOperation operation);

        void processTcpError(SocketOperation operation);

        void processUdpMsg(uint8_t *data, uint32_t size, SocketOperation operation);

        void initProcessingFunctions();

        void joinToNetwork();

        void quitFromNetwork();

        void moveLocalDescriptorsIntoOtherNodes();

        in_addr_t findLeastLoadedNode();

        void discardDescriptor(FileDescriptor &descriptor);

        void changeHolderNode(FileDescriptor &descriptor, in_addr_t send);

        std::vector<uint8_t> getFileContent(FileDescriptor &descriptor);


    }
}

void p2p::closeSession() {
    util::tcpServer->stopListening();
    util::udpServer->stopListening();
    util::tcpServer.reset();
    util::tcpServer.reset();
    util::quitFromNetwork();
}

void p2p::startSession() {
    util::initProcessingFunctions();
    util::tcpServer = std::make_shared<TcpServer>(&util::processTcpMsg, &util::processTcpError);
    util::udpServer = std::make_shared<UdpServer>(&util::processUdpMsg);
    util::tcpServer->startListening();
    util::udpServer->startListening();
    util::joinToNetwork();
}

void p2p::util::processTcpError(SocketOperation operation) {
    BOOST_LOG_TRIVIAL(debug) << "TCP error";
}

void p2p::util::processTcpMsg(uint8_t *data, uint32_t size, SocketOperation operation) {
    P2PMessage &p2pMessage = *(P2PMessage *) data;
    MessageType messageType = p2pMessage.getMessageType();
    const uint8_t *additionalData = data + sizeof(P2PMessage);
    const uint32_t additionalDataSize = size - sizeof(P2PMessage);
    assert (additionalDataSize == p2pMessage.getAdditionalDataSize());

    messageProcessors.at(messageType)(additionalData, additionalDataSize, operation.connectionAddr);
}

void p2p::util::processUdpMsg(uint8_t *data, uint32_t size, SocketOperation operation) {
    P2PMessage &p2pMessage = *(P2PMessage *) data;
    MessageType messageType = p2pMessage.getMessageType();
    const uint8_t *additionalData = data + sizeof(P2PMessage);
    const uint32_t additionalDataSize = size - sizeof(P2PMessage);
    assert (additionalDataSize == p2pMessage.getAdditionalDataSize());

    messageProcessors.at(messageType)(additionalData, additionalDataSize, operation.connectionAddr);
}

void p2p::util::joinToNetwork() {
    P2PMessage message{};
    message.setMessageType(MessageType::HELLO);
    message.setAdditionalDataSize(0u);
    uint32_t messageSize = sizeof(P2PMessage);

    udpServer->broadcast((uint8_t *) &message, messageSize);
    BOOST_LOG_TRIVIAL(debug) << ">>> HELLO: joining to network";
}

void p2p::util::quitFromNetwork() {
    P2PMessage message{};
    message.setMessageType(MessageType::DISCONNECTING);
    message.setAdditionalDataSize(0u);
    uint32_t messageSize = sizeof(P2PMessage);

    udpServer->broadcast((uint8_t *) &message, messageSize);
    // wait 10ms
    usleep(10000);
    BOOST_LOG_TRIVIAL(debug) << ">>> DISCONNECTIONG: quiting from network";
    moveLocalDescriptorsIntoOtherNodes();
}

void p2p::util::moveLocalDescriptorsIntoOtherNodes() {
    Guard guard(mutex);

    for (auto &&localDescriptor : localDescriptors) {
        discardDescriptor(localDescriptor);
    }
    BOOST_LOG_TRIVIAL(debug) << "### Local "<< localDescriptors.size() << " descriptors discarded";

    for (auto &&localDescriptor : localDescriptors) {
        in_addr_t nodeToSend = findLeastLoadedNode();
        changeHolderNode(localDescriptor, nodeToSend);
    }
    localDescriptors.clear();
}

void p2p::util::discardDescriptor(FileDescriptor &descriptor) {
    P2PMessage message{};
    message.setMessageType(MessageType::DISCARD_DESCRIPTOR);
    message.setAdditionalDataSize(sizeof(FileDescriptor));

    // build a message
    std::vector<uint8_t> buffer(sizeof(P2PMessage) + sizeof(FileDescriptor));
    memcpy(buffer.data(), (uint8_t *) &message, sizeof(P2PMessage));
    memcpy(buffer.data() + sizeof(P2PMessage), (uint8_t *) &descriptor, sizeof(FileDescriptor));
    udpServer->broadcast(buffer.data(), buffer.size());
    BOOST_LOG_TRIVIAL(debug) << ">>> DISCARD_DESCRIPTOR: " << descriptor.getName();
}

in_addr_t p2p::util::findLeastLoadedNode() {
    std::unordered_map<in_addr_t, int> nodesLoad;

    // count uses
    for (auto &&descriptor : networkDescriptors) {
        nodesLoad[descriptor.getHolderIp()]++;
    }

    // find max element
    auto mapElement = std::min_element(nodesLoad.begin(), nodesLoad.end(),
                              [](const std::pair<in_addr_t, int> &p1, const std::pair<in_addr_t, int> &p2) {
                                  return p1.second < p2.second;
                              });

    return mapElement->first;
}

void p2p::util::changeHolderNode(FileDescriptor &descriptor, in_addr_t newNodeAddress) {
    P2PMessage message{};
    message.setMessageType(MessageType::HOLDER_CHANGE);

    // preset new holder IP
    descriptor.setHolderIp(newNodeAddress);

    // get file as array
    auto fileContent = getFileContent(descriptor);

    message.setAdditionalDataSize(sizeof(FileDescriptor) + fileContent.size());

    std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());

    // prepare message
    memcpy(buffer.data(), (uint8_t*)&message, sizeof(P2PMessage));
    // put descriptor
    memcpy(buffer.data() + sizeof(P2PMessage), (uint8_t*)&descriptor, sizeof(FileDescriptor));
    // put file content
    memcpy(buffer.data() + sizeof(P2PMessage) + sizeof(FileDescriptor), fileContent.data(), fileContent.size());

    tcpServer->sendData(buffer.data(), buffer.size(), newNodeAddress);
    BOOST_LOG_TRIVIAL(debug) << ">>> HOLDER_CHANGE: " << descriptor.getName() << " to " << newNodeAddress;
}

std::vector<uint8_t> p2p::util::getFileContent(FileDescriptor &descriptor) {
    FileLoader loader(descriptor.getName());
    return loader.getContent();
}

void p2p::util::initProcessingFunctions() {
    messageProcessors[MessageType::HELLO] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        BOOST_LOG_TRIVIAL(debug) << "<<< HELLO from: " << ntohl(sourceAddress);
        Guard guard(mutex);
        P2PMessage message = {};
        message.setMessageType(MessageType::HELLO_REPLY);

        unsigned long additionalDataSize = localDescriptors.size() * sizeof(FileDescriptor);
        message.setAdditionalDataSize(additionalDataSize);

        // create a vector with size of the whole message (empty)
        std::vector<uint8_t> buffer(additionalDataSize + sizeof(P2PMessage));

        // write into underlying array P2Pmessage
        memcpy(buffer.data(), &message, sizeof(P2PMessage));

        // write descriptors
        memcpy(buffer.data() + sizeof(P2PMessage),
               (uint8_t *) localDescriptors.data(),
               localDescriptors.size() * sizeof(FileDescriptor));

        // send message
        util::tcpServer->sendData(buffer.data(), buffer.size(), sourceAddress);
    };

    messageProcessors[MessageType::HELLO_REPLY] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        BOOST_LOG_TRIVIAL(debug) << "<<< HELLO_REPLY from: " << ntohl(sourceAddress) << " "
                                 << size / sizeof(FileDescriptor) << " descriptors received";
        std::vector<FileDescriptor> buffer(size / sizeof(FileDescriptor));

        memcpy(buffer.data(), data, size);

        Guard guard(mutex);
        networkDescriptors.insert(networkDescriptors.end(), buffer.begin(), buffer.end());
    };

    messageProcessors[MessageType::DISCONNECTING] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::CONNECTION_LOST] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::CMD_REFUSED] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::NEW_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor newFileDescriptor(nullptr);
        memcpy(&newFileDescriptor, data, sizeof(FileDescriptor));

        networkDescriptors.push_back(newFileDescriptor);
    };

    messageProcessors[MessageType::REVOKE_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor revokedFileDescriptor(nullptr);
        memcpy(&revokedFileDescriptor, data, sizeof(FileDescriptor));

        for (std::vector<FileDescriptor>::const_iterator i = networkDescriptors.begin();
             i != networkDescriptors.end(); ++i) {
            if (i->getName() == revokedFileDescriptor.getName()) {
                networkDescriptors.erase(i);
                break;
            }
        }
    };

    messageProcessors[MessageType::DISCARD_DESCRIPTOR] = [](const uint8_t *data, uint32_t size,
                                                            in_addr_t sourceAddress) {
    };

    messageProcessors[MessageType::UPDATE_DESCRIPTOR] = [](const uint8_t *data, uint32_t size,
                                                           in_addr_t sourceAddress) {
        FileDescriptor updatedFileDescriptor(nullptr);
        memcpy(&updatedFileDescriptor, data, sizeof(FileDescriptor));

        for (int i = 0; i < networkDescriptors.size(); i++) {
            if (networkDescriptors[i].getName() == updatedFileDescriptor.getName()) {
                networkDescriptors[i] = updatedFileDescriptor;
                break;
            }
        }
    };

    messageProcessors[MessageType::HOLDER_CHANGE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::FILE_TRANSFER] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor fileDescriptor(nullptr);
        memcpy(&fileDescriptor, data, sizeof(FileDescriptor));


    };

    messageProcessors[MessageType::UPLOAD_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor newFileDescriptor(nullptr);
        memcpy(&newFileDescriptor, data, sizeof(FileDescriptor));

        localDescriptors.push_back(newFileDescriptor);

        //TODO - obsluga pliku

        P2PMessage message = {};
        message.setMessageType(MessageType::NEW_FILE);
        message.setAdditionalDataSize(sizeof(FileDescriptor));

        std::vector<uint8_t> buffer(sizeof(FileDescriptor) + sizeof(P2PMessage));

        memcpy(buffer.data(), &message, sizeof(P2PMessage));
        memcpy(buffer.data() + sizeof(P2PMessage), &newFileDescriptor, sizeof(FileDescriptor));

        udpServer->broadcast(buffer.data(), buffer.size());
    };

    messageProcessors[MessageType::GET_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor fileDescriptor(nullptr);
        memcpy(&fileDescriptor, data, sizeof(FileDescriptor));

        P2PMessage message = {};
        message.setMessageType(MessageType::FILE_TRANSFER);
        message.setAdditionalDataSize(sizeof(FileDescriptor) + fileDescriptor.getSize());

        std::vector<uint8_t> buffer(sizeof(FileDescriptor) + sizeof(P2PMessage) + fileDescriptor.getSize());
        memcpy(buffer.data(), &message, sizeof(P2PMessage));
        memcpy(buffer.data() + sizeof(P2PMessage), &fileDescriptor, sizeof(FileDescriptor));
        /*
        FILE *stream = fopen(fileDescriptor.getName(), "rb");
        fread(void *ptr, size_t size, size_t nitems, FILE *stream);
        */

        tcpServer->sendData(buffer.data(), buffer.size(), sourceAddress);
    };

    messageProcessors[MessageType::DELETE_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor deletedFileDescriptor(nullptr);
        memcpy(&deletedFileDescriptor, data, sizeof(FileDescriptor));

        for (std::vector<FileDescriptor>::const_iterator i = localDescriptors.begin();
             i != localDescriptors.end(); ++i) {
            if (i->getName() == deletedFileDescriptor.getName()) {
                localDescriptors.erase(i);
                break;
            }
        }

        //TODO - obsluga pliku


        P2PMessage message = {};
        message.setMessageType(MessageType::REVOKE_FILE);
        message.setAdditionalDataSize(sizeof(FileDescriptor));

        std::vector<uint8_t> buffer(sizeof(FileDescriptor) + sizeof(P2PMessage));

        memcpy(buffer.data(), &message, sizeof(P2PMessage));
        memcpy(buffer.data() + sizeof(P2PMessage), &deletedFileDescriptor, sizeof(FileDescriptor));

        udpServer->broadcast(buffer.data(), buffer.size());
    };

}