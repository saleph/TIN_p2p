#include "ProtocolManager.hpp"

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
        std::thread timer;
        Mutex mutexIsStillNewNode;
        bool isStillNewNode = true;
    }
}


const char *p2p::getFormatedIp(in_addr_t addr) {
    if (addr == util::tcpServer->getLocalhostIp()) {
        return ">>THIS HOST<<";
    }
    return inet_ntoa(*(in_addr *) &addr);
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
    for (auto &&nodeLoad : nodesLoad) {
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
        BOOST_LOG_TRIVIAL(debug) << "===> UploadFile: " << newDescriptor.getName() << " saved on >>THIS HOST<<";

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

bool p2p::getFile(const std::string &name) {
    using namespace util;
    FileDescriptor descriptor;
    {
        Guard guard(mutex);

        // find repetitions
        long filesWithSameName = std::count_if(networkDescriptors.begin(), networkDescriptors.end(),
                                               [&name](const FileDescriptor &fd) {
                                                   return fd.getName() == name;
                                               });

        if (filesWithSameName > 1) {
            BOOST_LOG_TRIVIAL(info) << "===> getFile: " << name
                                    << " hashes collision! Use command <filename> <md5>";
            return false;
        }

        auto descriptorPointer = std::find_if(networkDescriptors.begin(), networkDescriptors.end(),
                                              [&name](const FileDescriptor &fd) {
                                                  return fd.getName() == name;
                                              });
        if (descriptorPointer == networkDescriptors.end()) {
            BOOST_LOG_TRIVIAL(info) << "===> getFile: " << name
                                    << " does not exists in the network, try again";
            return false;
        }
        descriptor = *descriptorPointer;
    }

    return getFile(descriptor);
}

bool p2p::getFile(const std::string &name, const std::string &hash) {
    using namespace util;
    FileDescriptor descriptor;
    {
        Guard guard(mutex);
        auto descriptorPointer = std::find_if(networkDescriptors.begin(), networkDescriptors.end(),
                                              [&hash](const FileDescriptor &fd) {
                                                  return fd.getMd5().getHash() == hash;
                                              });
        if (descriptorPointer == networkDescriptors.end() || descriptorPointer->getName() != name) {
            BOOST_LOG_TRIVIAL(info) << "===> getFile: " << name
                                    << " md5: " << hash
                                    << " does not exists in the network, try again";
            return false;
        }
        descriptor = *descriptorPointer;
    }

    return getFile(descriptor);
}

bool p2p::util::getFile(FileDescriptor &descriptor) {
    using namespace util;
    // check if file is stored on our host
    if (descriptor.getHolderIp() == tcpServer->getLocalhostIp()) {
        BOOST_LOG_TRIVIAL(info) << "===> getFile: " << descriptor.getName()
                                << " md5: " << descriptor.getMd5().getHash()
                                << " is present on >>THIS HOST<<; rewrite the file";
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

bool p2p::deleteFile(const std::string &name, const std::string &hash) {
    using namespace util;
    FileDescriptor descriptor;
    {
        Guard guard(mutex);
        auto descriptorPointer = std::find_if(networkDescriptors.begin(), networkDescriptors.end(),
                                              [&hash](const FileDescriptor &fd) {
                                                  return fd.getMd5().getHash() == hash;
                                              });
        if (descriptorPointer == networkDescriptors.end() || descriptorPointer->getName() != name) {
            BOOST_LOG_TRIVIAL(info) << "===> deleteFile: " << name
                                    << " md5: " << hash
                                    << " does not exists in the network, try again";
            return false;
        }
        descriptor = *descriptorPointer;
    }

    return deleteFile(descriptor);
}

bool p2p::deleteFile(const std::string &name) {
    using namespace util;
    FileDescriptor descriptor;
    {
        Guard guard(mutex); // find repetitions
        long filesWithSameName = std::count_if(networkDescriptors.begin(), networkDescriptors.end(),
                                               [&name](const FileDescriptor &fd) {
                                                   return fd.getName() == name;
                                               });

        if (filesWithSameName > 1) {
            BOOST_LOG_TRIVIAL(info) << "===> deleteFile: " << name
                                    << " hashes collision! Use command <filename> <md5>";
            return false;
        }

        auto descriptorPointer = std::find_if(networkDescriptors.begin(), networkDescriptors.end(),
                                              [&name](const FileDescriptor &fd) {
                                                  return fd.getName() == name;
                                              });

        if (descriptorPointer == networkDescriptors.end()) {
            BOOST_LOG_TRIVIAL(info) << "===> deleteFile: " << name
                                    << " does not exists in the network, try again";
            return false;
        }
        descriptor = *descriptorPointer;
    }

    return deleteFile(descriptor);
}


bool p2p::util::deleteFile(FileDescriptor &descriptor) {
    using namespace util;

    // check unauthorized access
    if (descriptor.getOwnerIp() != tcpServer->getLocalhostIp()) {
        BOOST_LOG_TRIVIAL(info) << "===> deleteFile: " << descriptor.getName()
                                << " md5: " << descriptor.getMd5().getHash()
                                << " you are not the owner! Owner ip: "
                                << getFormatedIp(descriptor.getOwnerIp());
        return false;
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

std::vector<FileDescriptor> p2p::getLocalFileDescriptors() {
    using namespace util;
    Guard guard(mutex);
    return util::localDescriptors;
}

std::vector<FileDescriptor> p2p::getNetworkFileDescriptors() {
    using namespace util;
    Guard guard(mutex);
    return util::networkDescriptors;
}