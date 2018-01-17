#include <p2pMessage.hpp>
#include <iostream>
#include <FileStorer.hpp>
#include <thread>
#include "FileLoader.hpp"
#include "mutex.hpp"
#include "guard.hpp"
#include "ProtocolManager.hpp"
#include "FileDeleter.hpp"

// definitions HAVE TO BE THERE
// otherwise - multiple definitions error
namespace p2p {
    namespace util {
        std::unordered_map<MessageType, std::function<void(const uint8_t *, uint32_t, in_addr_t)>> msgProcessors;
        std::shared_ptr<TcpServer> tcpServer;
        std::shared_ptr<UdpServer> udpServer;

        std::vector<FileDescriptor> localDescriptors;
        std::vector<FileDescriptor> networkDescriptors;
        std::vector<in_addr_t> nodesAddresses;
        Mutex mutex;

        // starting state estimators
        const unsigned NEW_NODE_STATE_DURATION_MS = 5000u;
        std::thread timer;
        Mutex mutexIsStillNewNode;
        bool isStillNewNode = true;

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

        FileDescriptor &getRepetedDescriptor(FileDescriptor &descriptor);

    }

    const char *getFormatedIp(in_addr_t addr) {
        if (addr == util::tcpServer->getLocalhostIp()) {
            return ">>THIS HOST<<";
        }
        return inet_ntoa(*(in_addr *) &addr);
    }
}

void p2p::endSession() {
    util::quitFromNetwork();
    util::udpServer->stopListening();
    util::tcpServer->stopListening();

    // wait for performed actions
    usleep(10000);
    Guard guard(util::mutex);
    util::tcpServer.reset();
    util::tcpServer.reset();
    // prevent corruption
    util::timer.join();
}

void p2p::startSession() {
    // fire "new node state" timer
    // as long as the node is marked as "new" it collects all the HELLO_REPLY,
    // what is not what we want for older nodes
    using namespace util;
    timer = std::thread([&isStillNewNode, &mutexIsStillNewNode, NEW_NODE_STATE_DURATION_MS]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(NEW_NODE_STATE_DURATION_MS));
        Guard guard(util::mutexIsStillNewNode);
        isStillNewNode = false;
    });
    initProcessingFunctions();
    tcpServer = std::make_shared<TcpServer>(&processTcpMsg, &processTcpError);
    udpServer = std::make_shared<UdpServer>(&processUdpMsg);
    udpServer->enableSelfBroadcasts();
    tcpServer->startListening();
    udpServer->startListening();
    joinToNetwork();
}

void p2p::util::processTcpError(SocketOperation operation) {
    // we lost the node
    in_addr_t lostNode = operation.connectionAddr;
    BOOST_LOG_TRIVIAL(info) << ">>> CONNECTION_LOST: detected connection lost with " << getFormatedIp(lostNode);
    // publish this information
    publishLostNode(lostNode);
}


void p2p::util::publishLostNode(in_addr_t nodeAddress) {
    P2PMessage message{};
    message.setMessageType(MessageType::CONNECTION_LOST);
    message.setAdditionalDataSize(sizeof(in_addr_t));

    // prepare buffer
    std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());
    memcpy(buffer.data(), (uint8_t *) &message, sizeof(P2PMessage));
    memcpy(buffer.data() + sizeof(P2PMessage), (uint8_t *) &nodeAddress, sizeof(in_addr_t));
    // publish this information
    udpServer->broadcast(buffer.data(), buffer.size());
}

void p2p::util::processTcpMsg(uint8_t *data, uint32_t size, SocketOperation operation) {
    P2PMessage &p2pMessage = *(P2PMessage *) data;
    MessageType messageType = p2pMessage.getMessageType();
    const uint8_t *additionalData = data + sizeof(P2PMessage);
    const uint32_t additionalDataSize = size - sizeof(P2PMessage);
    assert (additionalDataSize == p2pMessage.getAdditionalDataSize());

    msgProcessors.at(messageType)(additionalData, additionalDataSize, operation.connectionAddr);
}

void p2p::util::processUdpMsg(uint8_t *data, uint32_t size, SocketOperation operation) {
    P2PMessage &p2pMessage = *(P2PMessage *) data;
    MessageType messageType = p2pMessage.getMessageType();
    const uint8_t *additionalData = data + sizeof(P2PMessage);
    const uint32_t additionalDataSize = size - sizeof(P2PMessage);
    assert (additionalDataSize == p2pMessage.getAdditionalDataSize());

    msgProcessors.at(messageType)(additionalData, additionalDataSize, operation.connectionAddr);
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
    BOOST_LOG_TRIVIAL(debug) << ">>> DISCONNECTING: start node closing procedure";
    moveLocalDescriptorsIntoOtherNodes();
    sendShutdown();
}


void p2p::util::sendShutdown() {
    P2PMessage message{};
    message.setMessageType(MessageType::SHUTDOWN);
    message.setAdditionalDataSize(0);

    udpServer->broadcast((uint8_t *) &message, sizeof(P2PMessage));
}

void p2p::util::moveLocalDescriptorsIntoOtherNodes() {
    // we already have mutex taken in p2p::util::endSession()

    for (auto &&localDescriptor : localDescriptors) {
        discardDescriptor(localDescriptor);
    }

    // wait for our discards
    usleep(100000);

    for (auto &&localDescriptor : localDescriptors) {
        in_addr_t nodeToSend;
        try {
            nodeToSend = findOtherLeastLoadedNode();
        } catch (std::logic_error &e) {
            BOOST_LOG_TRIVIAL(debug) << "===> endSession: no other node exists, current files will be lost";
            // no need to revoke file: noone is listening
            localDescriptors.clear();
            return;
        }
        changeHolderNode(localDescriptor, nodeToSend);
    }
    localDescriptors.clear();
}

void p2p::util::discardDescriptor(FileDescriptor &descriptor) {
    descriptor.makeUnvalid();

    P2PMessage message{};
    message.setMessageType(MessageType::DISCARD_DESCRIPTOR);
    message.setAdditionalDataSize(sizeof(FileDescriptor));

    // build a message
    std::vector<uint8_t> buffer(sizeof(P2PMessage) + sizeof(FileDescriptor));
    memcpy(buffer.data(), (uint8_t *) &message, sizeof(P2PMessage));
    memcpy(buffer.data() + sizeof(P2PMessage), (uint8_t *) &descriptor, sizeof(FileDescriptor));
    udpServer->broadcast(buffer.data(), buffer.size());
    BOOST_LOG_TRIVIAL(debug) << ">>> DISCARD_DESCRIPTOR: " << descriptor.getName()
                             << " md5: " << descriptor.getMd5().getHash();
}

in_addr_t p2p::util::findLeastLoadedNode() {
    Guard guard(mutex);
    if (networkDescriptors.empty() || nodesAddresses.empty()) {
        return tcpServer->getLocalhostIp();
    }

    std::unordered_map<in_addr_t, int> nodesLoad;

    // initialize loads
    for (auto &&address : nodesAddresses) {
        nodesLoad[address] = 0;
    }

    // count uses
    for (auto &&descriptor : networkDescriptors) {
        nodesLoad[descriptor.getHolderIp()] += descriptor.getSize();
    }

    // find min element
    auto mapElement = std::min_element(nodesLoad.begin(), nodesLoad.end(),
                                       [](const std::pair<in_addr_t, int> &p1,
                                          const std::pair<in_addr_t, int> &p2) {
                                           return p1.second < p2.second;
                                       });
    return mapElement->first;
}

in_addr_t p2p::util::findOtherLeastLoadedNode() {
    Guard guard(mutex);
    if (networkDescriptors.empty() || nodesAddresses.empty()) {
        throw std::logic_error("p2p::util::findOtherLeasLoadedNode(): other node not exist");
    }

    in_addr_t thisNodeAddress = tcpServer->getLocalhostIp();

    // map for easier collection of data
    std::unordered_map<in_addr_t, unsigned long> nodesLoad;

    // initialize loads
    for (auto &&address : nodesAddresses) {
        nodesLoad[address] = 0;
    }

    // count uses
    for (auto &&descriptor : networkDescriptors) {
        nodesLoad[descriptor.getHolderIp()] += descriptor.getSize();
    }

    in_addr_t leastLoadNode{};
    unsigned long min = ULONG_MAX;
    // find min element
    for (auto && nodeLoad : nodesLoad) {
        if (nodeLoad.second < min && nodeLoad.first != thisNodeAddress) {
            min = nodeLoad.second;
            leastLoadNode = nodeLoad.first;
        }
    }

    // if any other node is not found
    if (min == ULONG_MAX) {
        throw std::logic_error("p2p::util::findOtherLeasLoadedNode(): other node not exist");
    }
    return leastLoadNode;
}

void p2p::util::changeHolderNode(FileDescriptor &descriptor, in_addr_t newNodeAddress) {
    P2PMessage message{};
    message.setMessageType(MessageType::HOLDER_CHANGE);

    // preset new holder IP
    descriptor.setHolderIp(newNodeAddress);

    // get file as array
    auto fileContent = getFileContent(descriptor.getMd5().getHash());

    message.setAdditionalDataSize(sizeof(FileDescriptor) + fileContent.size());

    std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());

    // prepare message
    memcpy(buffer.data(), (uint8_t *) &message, sizeof(P2PMessage));
    // put descriptor
    memcpy(buffer.data() + sizeof(P2PMessage), (uint8_t *) &descriptor, sizeof(FileDescriptor));
    // put file content
    memcpy(buffer.data() + sizeof(P2PMessage) + sizeof(FileDescriptor), fileContent.data(), fileContent.size());

    tcpServer->sendData(buffer.data(), buffer.size(), newNodeAddress);
    BOOST_LOG_TRIVIAL(debug) << ">>> HOLDER_CHANGE: " << descriptor.getName() << " to "
                             << getFormatedIp(newNodeAddress);
}

std::vector<uint8_t> p2p::util::getFileContent(const std::string &name) {
    FileLoader loader(name);
    return loader.getContent();
}

void p2p::util::storeFileContent(std::vector<uint8_t> &content, const std::string &name) {
    FileStorer storer(name);
    storer.storeFile(content);
}

bool p2p::uploadFile(const std::string &name) {
    using namespace util;
    // create new descriptor (autofill MD5 and its size)
    FileDescriptor newDescriptor(name);

    // set upload time
    newDescriptor.setUploadTime(std::time(nullptr));

    in_addr_t thisHostAddress = util::tcpServer->getLocalhostIp();
    // set owner id as this host
    newDescriptor.setOwnerIp(thisHostAddress);

    // find least loaded node
    in_addr_t leastLoadNodeAddress = util::findLeastLoadedNode();

    // set holder IP
    newDescriptor.setHolderIp(leastLoadNodeAddress);

    // make descriptor valid
    newDescriptor.makeValid();

    // check if descriptor is unique
    {
        Guard guard(mutex);
        if (!isDescriptorUnique(newDescriptor)) {
            BOOST_LOG_TRIVIAL(debug) << "===> UploadFile: hashes collision! " << newDescriptor.getName()
                                     << " md5: " << newDescriptor.getMd5().getHash()
                                     << "; choose another file!";
            return false;
        }
    }

    if (leastLoadNodeAddress == thisHostAddress) {
        // store file with name as its md5
        auto fileContent = util::getFileContent(newDescriptor.getName());
        util::storeFileContent(fileContent, newDescriptor.getMd5().getHash());

        // we are the least load node - only publish the descriptor
        util::publishDescriptor(newDescriptor);
        BOOST_LOG_TRIVIAL(debug) << "===> UploadFile: " << newDescriptor.getName() << " saved on this host";

        Guard guard(util::mutex);
        util::localDescriptors.push_back(newDescriptor);
        return true;
    }

    util::uploadFile(newDescriptor);
    BOOST_LOG_TRIVIAL(debug) << "===> UploadFile: " << newDescriptor.getName()
                             << " saved in node " << getFormatedIp(leastLoadNodeAddress);
    return true;
}

void p2p::util::publishDescriptor(FileDescriptor &descriptor) {
    P2PMessage message{};
    message.setMessageType(MessageType::NEW_FILE);
    message.setAdditionalDataSize(sizeof(FileDescriptor));

    // prepare buffer
    std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());

    // put into buffer P2Pmessage and new's file's descriptor
    memcpy(buffer.data(), &message, sizeof(P2PMessage));
    memcpy(buffer.data() + sizeof(P2PMessage), &descriptor, sizeof(FileDescriptor));

    udpServer->broadcast(buffer.data(), buffer.size());
}

void p2p::util::uploadFile(FileDescriptor &descriptor) {
    P2PMessage message{};
    message.setMessageType(MessageType::UPLOAD_FILE);
    // load file specified by user
    auto fileContent = getFileContent(descriptor.getName());

    // set additional data size to size of descritor + content
    message.setAdditionalDataSize(sizeof(FileDescriptor) + fileContent.size());

    // prepare buffer
    std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());

    memcpy(buffer.data(), &message, sizeof(P2PMessage));
    memcpy(buffer.data() + sizeof(P2PMessage), &descriptor, sizeof(FileDescriptor));
    memcpy(buffer.data() + sizeof(P2PMessage) + sizeof(FileDescriptor), fileContent.data(), fileContent.size());

    in_addr_t holderNode = descriptor.getHolderIp();
    tcpServer->sendData(buffer.data(), buffer.size(), holderNode);
}

bool p2p::getFile(const std::string &name, const Md5Hash &hash) {
    using namespace util;
    FileDescriptor descriptor;
    {
        Guard guard(mutex);
        auto descriptorPointer = std::find_if(networkDescriptors.begin(), networkDescriptors.end(),
                                              [&hash](const FileDescriptor &fd) {
                                                  return fd.getMd5() == hash;
                                              });
        if (descriptorPointer == networkDescriptors.end() || descriptorPointer->getName() != name) {
            BOOST_LOG_TRIVIAL(info) << "===> getFile: " << name
                                    << " md5: " << hash.getHash()
                                    << " does not exists in the network, try again";
            return false;
        }
        descriptor = *descriptorPointer;
    }
    // check if file is stored on our host
    if (descriptor.getHolderIp() == tcpServer->getLocalhostIp()) {
        BOOST_LOG_TRIVIAL(info) << "===> getFile: " << name
                                << " md5: " << hash.getHash()
                                << " is present on this host; rewrite the file";
        // we already have the file - just rewrite the file
        FileLoader loader(descriptor.getMd5().getHash());
        auto content = loader.getContent();
        FileStorer storer(descriptor.getName());
        storer.storeFile(content);

        return true;
    }
    util::requestGetFile(descriptor);
    return true;
}


void p2p::util::requestGetFile(FileDescriptor &descriptor) {
    P2PMessage message{};
    message.setMessageType(MessageType::GET_FILE);
    message.setAdditionalDataSize(sizeof(FileDescriptor));

    // prepare buffer
    std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());
    memcpy(buffer.data(), &message, sizeof(P2PMessage));
    memcpy(buffer.data() + sizeof(P2PMessage), &descriptor, sizeof(FileDescriptor));

    // send request
    tcpServer->sendData(buffer.data(), buffer.size(), descriptor.getHolderIp());
    BOOST_LOG_TRIVIAL(debug) << ">>> GET_FILE: " << descriptor.getName()
                             << " md5: " << descriptor.getMd5().getHash();
}

bool p2p::deleteFile(const std::string &name, const Md5Hash &hash) {
    using namespace util;
    FileDescriptor descriptor;
    {
        Guard guard(mutex);
        auto descriptorPointer = std::find_if(networkDescriptors.begin(), networkDescriptors.end(),
                                              [&hash](const FileDescriptor &fd) {
                                                  return fd.getMd5() == hash;
                                              });
        if (descriptorPointer == networkDescriptors.end() || descriptorPointer->getName() != name) {
            BOOST_LOG_TRIVIAL(info) << "===> deleteFile: " << name
                                    << " md5: " << hash.getHash()
                                    << " does not exists in the network, try again";
            return false;
        }
        descriptor = *descriptorPointer;
    }

    // discard descriptor
    util::discardDescriptor(descriptor);
    usleep(10000);
    util::requestDeleteFile(descriptor);
    return true;
}


void p2p::util::requestDeleteFile(FileDescriptor &descriptor) {
    P2PMessage message{};
    message.setMessageType(MessageType::DELETE_FILE);
    message.setAdditionalDataSize(sizeof(FileDescriptor));

    // prepare buffer
    std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());
    memcpy(buffer.data(), &message, sizeof(P2PMessage));
    memcpy(buffer.data() + sizeof(P2PMessage), &descriptor, sizeof(FileDescriptor));

    // send request
    tcpServer->sendData(buffer.data(), buffer.size(), descriptor.getHolderIp());
    BOOST_LOG_TRIVIAL(debug) << ">>> DELETE_FILE: " << descriptor.getName()
                             << " md5: " << descriptor.getMd5().getHash();
}

void p2p::util::initProcessingFunctions() {
    // first message sent by new node; in reply we pass our local descriptors
    msgProcessors[MessageType::HELLO] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        if (sourceAddress == udpServer->getLocalhostIp()) {
            // its our hello
            return;
        }
        BOOST_LOG_TRIVIAL(debug) << "<<< HELLO from: " << getFormatedIp(sourceAddress);
        Guard guard(mutex);
        // save node address for later
        nodesAddresses.push_back(sourceAddress);

        // construct p2pmessage
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

    // replay for other nodes
    msgProcessors[MessageType::HELLO_REPLY] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        { // check if this node is already older node
            Guard guard(mutexIsStillNewNode);
            if (!isStillNewNode) {
                // we are adult node and we have all descriptors
                BOOST_LOG_TRIVIAL(debug) << "<<< HELLO_REPLY: hello reply received as adult node - do nothing";
                return;
            } else {
                // remove duplicates if present
                removeDuplicatesFromLists();
            }
        }

        if (sourceAddress == udpServer->getLocalhostIp()) {
            // our broadcast, skip
            return;
        }
        BOOST_LOG_TRIVIAL(debug) << "<<< HELLO_REPLY from: " << getFormatedIp(sourceAddress) << " "
                                 << size / sizeof(FileDescriptor) << " descriptors received";

        std::vector<FileDescriptor> buffer(size / sizeof(FileDescriptor));

        // put descriptors
        memcpy((uint8_t *) buffer.data(), data, size);

        Guard guard(mutex);
        // preserve source address
        nodesAddresses.push_back(sourceAddress);
        networkDescriptors.insert(networkDescriptors.end(), buffer.begin(), buffer.end());
    };

    // message sent by node, which starts shutdown; discards every his descriptor
    // discarding is not neccessary (quiting node should do it even before this message)
    // but it ensures, that no one will interrupt the collapsing node
    msgProcessors[MessageType::DISCONNECTING] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        if (sourceAddress == udpServer->getLocalhostIp()) {
            // our broadcast, skip
            return;
        }

        Guard guard(mutex);
        // mark descriptors of disconnecting node as discarded
        for (auto &&descriptor : networkDescriptors) {
            if (descriptor.getHolderIp() == sourceAddress) {
                descriptor.makeUnvalid();
            }
        }
        BOOST_LOG_TRIVIAL(debug) << "<<< DISCONNECTING: node " << sourceAddress << " start disconnecting";
    };

    // if someone signals that some node quit "definitely not gently"
    msgProcessors[MessageType::CONNECTION_LOST] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        if (size != sizeof(in_addr_t)) {
            throw std::runtime_error("CONNECTION_LOST: additional data does not contain IP address of the lost node");
        }
        // only additional information is lostNode IP
        in_addr_t lostNodeAddress = *(in_addr_t *) data;

        int lostDescriptorsNumber = 0;
        Guard guard(mutex);
        // revoke descriptors from lost node
        networkDescriptors.erase(std::remove_if(networkDescriptors.begin(), networkDescriptors.end(),
                                                [lostNodeAddress, &lostDescriptorsNumber](
                                                        const FileDescriptor &fileDescriptor) {
                                                    ++lostDescriptorsNumber;
                                                    return fileDescriptor.getHolderIp() == lostNodeAddress;
                                                }));
        // remove node address from space
        nodesAddresses.erase(std::remove_if(nodesAddresses.begin(), nodesAddresses.end(),
                                            [sourceAddress](const in_addr_t &addr) {
                                                return sourceAddress == addr;
                                            }), nodesAddresses.end());
        BOOST_LOG_TRIVIAL(debug) << "<<< CONNECTION_LOST: with node " << lostNodeAddress
                                 << "; lost " << lostDescriptorsNumber << " descriptors";
    };

    // node refused to perform operation, which we requested for
    msgProcessors[MessageType::CMD_REFUSED] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        MessageType messageType = *(MessageType *) data;

        // get description of the problem
        const char *errorDescription = (const char *) (data + sizeof(MessageType));

        BOOST_LOG_TRIVIAL(info) << "<<< CMD_REFUSED: node " << getFormatedIp(sourceAddress)
                                << " refused command, message: " << errorDescription;
    };

    // last message sent by collapsing node - nothing will be valid, so ensure that everything is deleted
    msgProcessors[MessageType::SHUTDOWN] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) -> void {
        if (sourceAddress == udpServer->getLocalhostIp()) {
            // its our broadcast, skip
            return;
        }

        Guard guard(mutex);
        // remove all associated data
        nodesAddresses.erase(std::remove_if(nodesAddresses.begin(), nodesAddresses.end(),
                                            [sourceAddress](const in_addr_t &addr) {
                                                return sourceAddress == addr;
                                            }), nodesAddresses.end());
        networkDescriptors.erase(std::remove_if(networkDescriptors.begin(), networkDescriptors.end(),
                                                [sourceAddress](const FileDescriptor &fileDescriptor) {
                                                    return fileDescriptor.getHolderIp() == sourceAddress;
                                                }));
        BOOST_LOG_TRIVIAL(debug) << "<<< SHUTDOWN: node " << getFormatedIp(sourceAddress) << " have been closed";
    };

    // new file descriptor received - put it into network descriptors list
    msgProcessors[MessageType::NEW_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor newFileDescriptor = *(FileDescriptor *) data;

        // check collisions
        Guard guard(mutex);
        if (!isDescriptorUnique(newFileDescriptor)) {
            FileDescriptor repetedDescriptor = getRepetedDescriptor(newFileDescriptor);

            BOOST_LOG_TRIVIAL(debug) << "<<< NEW_FILE: hashes collision! md5: "
                                     << repetedDescriptor.getMd5().getHash()
                                     << " upload times (old, new): "
                                     << repetedDescriptor.getUploadTime() << " vs "
                                     << newFileDescriptor.getUploadTime()
                                     << "; earlier file choosen";

            if (repetedDescriptor.getUploadTime() > newFileDescriptor.getUploadTime()) {
                networkDescriptors.erase(std::remove_if(networkDescriptors.begin(), networkDescriptors.end(),
                [&repetedDescriptor](const FileDescriptor &fd) {
                    return fd.getMd5() == repetedDescriptor.getMd5();
                }));
                networkDescriptors.push_back(newFileDescriptor);
            }
            return;
        }

        // normal insert
        networkDescriptors.push_back(newFileDescriptor);

        BOOST_LOG_TRIVIAL(debug) << "<<< NEW_FILE: " << newFileDescriptor.getName()
                                 << " md5: " << newFileDescriptor.getMd5().getHash()
                                 << " in node: " << getFormatedIp(sourceAddress);

    };

    msgProcessors[MessageType::REVOKE_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        if (size != sizeof(FileDescriptor)) {
            throw std::runtime_error("REVOKE_FILE: received data is not a descriptor");
        }
        FileDescriptor revokedFileDescriptor = *(FileDescriptor *) data;

        BOOST_LOG_TRIVIAL(debug) << "<<< REVOKE_FILE: " << revokedFileDescriptor.getName() << " "
                                 << " md5: " << revokedFileDescriptor.getMd5().getHash();

        Md5Hash revokedFileHash = revokedFileDescriptor.getMd5();

        Guard guard(mutex);
        networkDescriptors.erase(std::remove_if(networkDescriptors.begin(), networkDescriptors.end(),
                                                [&revokedFileHash](const FileDescriptor &fileDescriptor) {
                                                    return fileDescriptor.getMd5() == revokedFileHash;
                                                }));
    };

    // discard descriptor request - file is present in network, but cannot be accessed nor deleted
    msgProcessors[MessageType::DISCARD_DESCRIPTOR] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor descriptor = *(FileDescriptor *) data;
        BOOST_LOG_TRIVIAL(debug) << "<<< DISCARD_DESCRIPTOR: " << descriptor.getName()
                                 << " md5: " << descriptor.getMd5().getHash()
                                 << " from " << getFormatedIp(sourceAddress);

        Guard guard(mutex);
        // make this descriptor no longer valid
        for (auto &&networkDescriptor : networkDescriptors) {
            if (networkDescriptor.getMd5() == descriptor.getMd5()) {
                networkDescriptor.makeUnvalid();
            }
        }
    };

    // update descriptor request - something changed (holderNode)
    msgProcessors[MessageType::UPDATE_DESCRIPTOR] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor updatedDescriptor = *(FileDescriptor *) data;

        BOOST_LOG_TRIVIAL(debug) << "<<< UPDATE_DESCRIPTOR: " << updatedDescriptor.getName()
                                 << " md5: " << updatedDescriptor.getMd5().getHash()
                                 << " from " << getFormatedIp(sourceAddress);

        Guard guard(mutex);
        // update particular descriptor
        for (auto &&networkDescriptor : networkDescriptors) {
            if (networkDescriptor.getMd5() == updatedDescriptor.getMd5()) {
                networkDescriptor = updatedDescriptor;
            }
        }
    };

    // received file to store locally. Store it and publish updated descriptor
    msgProcessors[MessageType::HOLDER_CHANGE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor updatedDescriptor = *(FileDescriptor *) data;
        BOOST_LOG_TRIVIAL(debug) << "<<< HOLDER_CHANGE: store here " << updatedDescriptor.getName()
                                 << " md5: " << updatedDescriptor.getMd5().getHash()
                                 << " from " << getFormatedIp(sourceAddress);
        // this descriptor will be valid now
        updatedDescriptor.makeValid();

        // get file content
        std::vector<uint8_t> fileContent(size - sizeof(FileDescriptor));
        memcpy(fileContent.data(), data + sizeof(FileDescriptor), size - sizeof(FileDescriptor));

        // store this file by its md5 hash
        storeFileContent(fileContent, updatedDescriptor.getMd5().getHash());

        // update local descriptors table
        {
            Guard guard(mutex);
            localDescriptors.push_back(updatedDescriptor);
        }

        // publish new descriptor
        P2PMessage message{};
        message.setMessageType(MessageType::UPDATE_DESCRIPTOR);
        message.setAdditionalDataSize(sizeof(FileDescriptor));

        // prepare message
        std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());
        memcpy(buffer.data(), (uint8_t *) &message, sizeof(P2PMessage));
        memcpy(buffer.data() + sizeof(P2PMessage), (uint8_t *) &updatedDescriptor, sizeof(FileDescriptor));

        udpServer->broadcast(buffer.data(), buffer.size());
        BOOST_LOG_TRIVIAL(debug) << ">>> UPDATE_DESCRIPTOR: " << updatedDescriptor.getName()
                                 << " md5: " << updatedDescriptor.getMd5().getHash();
    };

    // reply for our request for file
    msgProcessors[MessageType::FILE_TRANSFER] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor descriptor = *(FileDescriptor *) data;

        // at the beginning was the descriptor present
        std::vector<uint8_t> buffer(size - sizeof(FileDescriptor));

        // copy the file content to our buffer
        memcpy(buffer.data(), data + sizeof(FileDescriptor), size - sizeof(FileDescriptor));

        // store received file into FS
        storeFileContent(buffer, descriptor.getName());
        auto storedFileHash = Md5sum(descriptor.getName()).getMd5Hash();

        // check hash
        if (storedFileHash != descriptor.getMd5()) {
            BOOST_LOG_TRIVIAL(debug) << "<<< FILE_TRANSFER: md5 differs!!! is: "
                                     << storedFileHash.getHash()
                                     << " should be: " << descriptor.getMd5().getHash();
            return;
        }

        BOOST_LOG_TRIVIAL(debug) << "<<< FILE_TRANSFER: received " << descriptor.getName()
                                 << " md5: " << descriptor.getMd5().getHash()
                                 << " from " << getFormatedIp(sourceAddress);
    };

    // request for upload a file: other node send us a file via TCP and we have to publish it in the network
    msgProcessors[MessageType::UPLOAD_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor descriptor = *(FileDescriptor *) data;

        // prepare buffer
        std::vector<uint8_t> buffer(size - sizeof(FileDescriptor));
        // copy file content
        memcpy(buffer.data(), data + sizeof(FileDescriptor), size - sizeof(FileDescriptor));
        // store file as its hash
        auto newFileName = descriptor.getMd5().getHash();
        storeFileContent(buffer, newFileName);
        // check hash
        auto newFileHash = Md5sum(newFileName).getMd5Hash();

        if (newFileHash != descriptor.getMd5()) {
            BOOST_LOG_TRIVIAL(debug) << "<<< UPLOAD_FILE: hashes differ!!! is: " << newFileHash.getHash()
                                     << " should be: " << descriptor.getMd5().getHash()
                                     << "; file not published into networ";
            sendCommandRefused(MessageType::UPLOAD_FILE, "file's hash differ! Try again.", sourceAddress);
            // does nothing more, network does not now about the file
            return;
        }

        // we have valid file here
        BOOST_LOG_TRIVIAL(debug) << ">>> NEW_FILE: publishing descriptor into the network of the "
                                 << "properly received file " << descriptor.getName()
                                 << " md5: " << descriptor.getMd5().getHash();
        {
            // append to our local descriptors
            Guard guard(mutex);
            localDescriptors.push_back(descriptor);
        }

        // notify the network about new file
        P2PMessage message{};
        message.setMessageType(MessageType::NEW_FILE);
        message.setAdditionalDataSize(sizeof(FileDescriptor));

        buffer.resize(sizeof(P2PMessage) + message.getAdditionalDataSize());

        memcpy(buffer.data(), &message, sizeof(P2PMessage));
        memcpy(buffer.data() + sizeof(P2PMessage), (uint8_t *) &descriptor, sizeof(FileDescriptor));

        udpServer->broadcast(buffer.data(), buffer.size());
    };

    // other node want to access a file stored in our node
    msgProcessors[MessageType::GET_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor descriptor = *(FileDescriptor *) data;
        BOOST_LOG_TRIVIAL(debug) << "<<< GET_FILE: request for " << descriptor.getName()
                                 << " md5: " << descriptor.getMd5().getHash()
                                 << " from " << getFormatedIp(sourceAddress);

        P2PMessage message{};
        // set message type as file transfer
        message.setMessageType(MessageType::FILE_TRANSFER);
        // get file content
        auto fileContent = getFileContent(descriptor.getMd5().getHash());
        // set msg additional size to size of the descriptor and file content length
        message.setAdditionalDataSize(sizeof(FileDescriptor) + fileContent.size());
        // prepare buffer
        std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());

        // prepare message
        memcpy(buffer.data(), &message, sizeof(P2PMessage));
        memcpy(buffer.data() + sizeof(P2PMessage), &descriptor, sizeof(FileDescriptor));
        memcpy(buffer.data() + sizeof(P2PMessage) + sizeof(FileDescriptor), fileContent.data(), fileContent.size());

        tcpServer->sendData(buffer.data(), buffer.size(), sourceAddress);
    };

    // someone requested to delete file sored in our machine
    msgProcessors[MessageType::DELETE_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        // file was discarded already by request node - we have to delete it and publish REVOKE
        FileDescriptor descriptor = *(FileDescriptor *) data;

        Guard guard(mutex);

        FileDeleter deleter(descriptor.getMd5().getHash());
        if (!deleter.deleteFile()) {
            BOOST_LOG_TRIVIAL(debug) << "<<< DELETE_FILE: " << descriptor.getName()
                                     << " md5: " << descriptor.getMd5().getHash()
                                     << " does not exist!; send cmd_refused";
            sendCommandRefused(MessageType::DELETE_FILE, "file does not exist", sourceAddress);
            return;
        }

        auto removedFileHash = descriptor.getMd5();
        // file successfully deleted!
        // remove descriptor from localDescriptors
        localDescriptors.erase(std::remove_if(localDescriptors.begin(), localDescriptors.end(),
                                              [&removedFileHash](const FileDescriptor &fd) {
                                                  return fd.getMd5() == removedFileHash;
                                              }));

        // publish revoke
        P2PMessage message{};
        message.setMessageType(MessageType::REVOKE_FILE);
        message.setAdditionalDataSize(sizeof(FileDescriptor));

        std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());
        memcpy(buffer.data(), &message, sizeof(P2PMessage));
        memcpy(buffer.data() + sizeof(P2PMessage), &descriptor, sizeof(FileDescriptor));

        udpServer->broadcast(buffer.data(), buffer.size());
    };

}

void p2p::util::removeDuplicatesFromLists() {
    Guard guard(mutex);
    auto descriptorSorter = [](const FileDescriptor &fd1, const FileDescriptor &fd2) {
        return fd1.getMd5().getHash() > fd2.getMd5().getHash();
    };
    auto descriptorComparator = [](const FileDescriptor &fd1, const FileDescriptor &fd2) {
        return fd1.getMd5() == fd2.getMd5();
    };
    // remove duplicates from descriptors
    std::sort(networkDescriptors.begin(), networkDescriptors.end(), descriptorSorter);
    networkDescriptors.erase(std::unique(networkDescriptors.begin(), networkDescriptors.end(), descriptorComparator),
                             networkDescriptors.end());

    // remove duplicates from adresses
    std::sort(nodesAddresses.begin(), nodesAddresses.end());
    nodesAddresses.erase(std::unique(nodesAddresses.begin(), nodesAddresses.end()), nodesAddresses.end());
}

void p2p::util::sendCommandRefused(MessageType messageType, const char *msg, in_addr_t sourceAddress) {
    P2PMessage message{};
    uint32_t stringSize = strlen(msg) + 1;
    // prepare cmd_refused
    message.setMessageType(MessageType::CMD_REFUSED);
    message.setAdditionalDataSize(sizeof(MessageType) + stringSize);

    // prepareBuffer
    std::vector<uint8_t> buffer(sizeof(P2PMessage) + message.getAdditionalDataSize());
    memcpy(buffer.data(), &message, sizeof(P2PMessage));
    memcpy(buffer.data() + sizeof(P2PMessage), &messageType, sizeof(MessageType));
    memcpy(buffer.data() + sizeof(P2PMessage) + sizeof(MessageType), msg, stringSize);

    tcpServer->sendData(buffer.data(), buffer.size(), sourceAddress);
}

bool p2p::util::isDescriptorUnique(FileDescriptor &descriptor) {
    for (auto &&networkDescriptor : networkDescriptors) {
        if (networkDescriptor.getMd5() == descriptor.getMd5()) {
            return false;
        }
    }
    return true;
}

FileDescriptor &p2p::util::getRepetedDescriptor(FileDescriptor &descriptor) {
    for (auto &&networkDescriptor : networkDescriptors) {
        if (networkDescriptor.getMd5() == descriptor.getMd5()) {
            return networkDescriptor;
        }
    }
}