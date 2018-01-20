#include "ProtocolManager.hpp"

void p2p::util::initProcessingFunctions() {
    // =================================================================================================================
    // first message sent by new node; in reply we pass our local descriptors
    msgProcessors[MessageType::HELLO] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        if (sourceAddress == udpServer->getLocalhostIp()) {
            // its our hello
            return;
        }
        BOOST_LOG_TRIVIAL(debug) << "<<< HELLO from: " << getFormatedIp(sourceAddress);
        {
            Guard guard(mutex);
            // save node address for later
            nodesAddresses.push_back(sourceAddress);
        }

        // construct p2pmessage
        P2PMessage message = {};
        message.setMessageType(MessageType::HELLO_REPLY);
        unsigned long additionalDataSize = localDescriptors.size() * sizeof(FileDescriptor);
        message.setAdditionalDataSize
                (additionalDataSize);

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

        // NETWORK BALANCING
        // estimate new average node load
        uint32_t averageNodesLoad = getAverageNodesLoad();
        uint32_t thisNodeLoad = getThisNodeLoad();
        // if this node is storing more than average - move excess number of bytes (rounding to file sizes)
        int64_t sizeToMoveFromThisNode = thisNodeLoad - averageNodesLoad;

        BOOST_LOG_TRIVIAL(debug) << "size to move from this node: " << sizeToMoveFromThisNode;

        if (sizeToMoveFromThisNode < 0) {
            // nothing to send from this node
            return;
        }

        moveFilesWithSumaricSizeToNode(sizeToMoveFromThisNode, sourceAddress);
    };

    // =================================================================================================================
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

        // gather descriptors
        {
            Guard guard(mutex);
            // preserve source address
            nodesAddresses.push_back(sourceAddress);
            networkDescriptors.insert(networkDescriptors.end(), buffer.begin(), buffer.end());
        }
    };

    // =================================================================================================================
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

    // =================================================================================================================
    // if someone signals that some node quit "definitely not gently"
    msgProcessors[MessageType::CONNECTION_LOST] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
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

    // =================================================================================================================
    // node refused to perform operation, which we requested for
    msgProcessors[MessageType::CMD_REFUSED] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        MessageType messageType = *(MessageType *) data;

        // get description of the problem
        const char *errorDescription = (const char *) (data + sizeof(MessageType));

        BOOST_LOG_TRIVIAL(info) << "<<< CMD_REFUSED: node " << getFormatedIp(sourceAddress)
                                << " refused command, message: " << errorDescription;
    };

    // =================================================================================================================
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

    // =================================================================================================================
    // new file descriptor received - put it into network descriptors list
    msgProcessors[MessageType::NEW_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor newFileDescriptor = *(FileDescriptor *) data;

        // check collisions
        Guard guard(mutex);
        if (!isDescriptorUnique(newFileDescriptor)) {
            // get descriptor with the same md5
            FileDescriptor repetedDescriptor = getRepetedDescriptor(newFileDescriptor);

            BOOST_LOG_TRIVIAL(debug) << "<<< NEW_FILE: hashes collision! "
                                     << repetedDescriptor.getName() << " and " << newFileDescriptor.getName()
                                     << " md5: " << repetedDescriptor.getMd5().getHash()
                                     << " upload times (old, new): "
                                     << repetedDescriptor.getUploadTime() << " vs "
                                     << newFileDescriptor.getUploadTime()
                                     << "; earlier file choosen (or with < filename)";

            // if system_clock can't distinguish version between collisions based on time
            if (repetedDescriptor.getUploadTime() == newFileDescriptor.getUploadTime()) {
                // if new file has "lower" name
                if (newFileDescriptor.getName() < repetedDescriptor.getName()) {
                    // remove old descriptor
                    networkDescriptors.erase(std::remove_if(networkDescriptors.begin(), networkDescriptors.end(),
                                                            [&repetedDescriptor](const FileDescriptor &fd) {
                                                                return fd.getMd5() == repetedDescriptor.getMd5();
                                                            }));
                    networkDescriptors.push_back(newFileDescriptor);
                }
                // if already present file has lower name - do nothing
                return;
            }

            // if new desriptor is earlier version - choose it
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

    // =================================================================================================================
    msgProcessors[MessageType::REVOKE_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor revokedFileDescriptor = *(FileDescriptor *) data;

        BOOST_LOG_TRIVIAL(debug) << "<<< REVOKE_FILE: " << revokedFileDescriptor.getName() << " "
                                 << " md5: " << revokedFileDescriptor.getMd5().getHash();

        Md5Hash revokedFileHash = revokedFileDescriptor.getMd5();

        Guard guard(mutex);
        networkDescriptors.erase(std::remove_if(networkDescriptors.begin(), networkDescriptors.end(),
                                                [&revokedFileHash](const FileDescriptor &fileDescriptor) {
                                                    return fileDescriptor.getMd5() == revokedFileHash;
                                                }));
        localDescriptors.erase(std::remove_if(localDescriptors.begin(), localDescriptors.end(),
                                                [&revokedFileHash](const FileDescriptor &fileDescriptor) {
                                                    return fileDescriptor.getMd5() == revokedFileHash;
                                                }));
    };

    // =================================================================================================================
    // discard descriptor request - file is present in network, but cannot be accessed nor deleted
    msgProcessors[MessageType::DISCARD_DESCRIPTOR] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor descriptor = *(FileDescriptor *) data;
        BOOST_LOG_TRIVIAL(debug) << "<<< DISCARD_DESCRIPTOR: " << descriptor.getName()
                                 << " md5: " << descriptor.getMd5().getHash()
                                 << " from " << getFormatedIp(sourceAddress);
        descriptor.makeUnvalid();

        Guard guard(mutex);
        bool descriptorPresence = false;
        // make this descriptor no longer valid
        for (auto &&networkDescriptor : networkDescriptors) {
            if (networkDescriptor.getMd5() == descriptor.getMd5()) {
                networkDescriptor.makeUnvalid();
                descriptorPresence = true;
            }
        }
        // check if this descriptor has been lost in some broadcast
        if (!descriptorPresence) {
            // that's mean this descriptor has not been received
            networkDescriptors.push_back(descriptor);
        }
        for (auto &&localDescriptor : localDescriptors) {
            if (localDescriptor.getMd5() == descriptor.getMd5()) {
                localDescriptor.makeUnvalid();
            }
        }
    };

    // =================================================================================================================
    // update descriptor request - something changed (holderNode)
    msgProcessors[MessageType::UPDATE_DESCRIPTOR] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor updatedDescriptor = *(FileDescriptor *) data;

        BOOST_LOG_TRIVIAL(debug) << "<<< UPDATE_DESCRIPTOR: " << updatedDescriptor.getName()
                                 << " md5: " << updatedDescriptor.getMd5().getHash()
                                 << " from " << getFormatedIp(sourceAddress);

        Guard guard(mutex);
        bool descriptorPresence = false;
        // update particular descriptor
        for (auto &&networkDescriptor : networkDescriptors) {
            if (networkDescriptor.getMd5() == updatedDescriptor.getMd5()) {
                networkDescriptor = updatedDescriptor;
                descriptorPresence = true;
            }
        }
        // check if this descriptor has been lost in some broadcast
        if (!descriptorPresence) {
            // that's mean this descriptor has not been received
            networkDescriptors.push_back(updatedDescriptor);
        }
        // update particular descriptor
        for (auto &&localDescriptor : localDescriptors) {
            if (localDescriptor.getMd5() == updatedDescriptor.getMd5()) {
                localDescriptor = updatedDescriptor;
            }
        }
    };

    // =================================================================================================================
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

    // =================================================================================================================
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

    // =================================================================================================================
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

    // =================================================================================================================
    // other node want to access a file stored in our node
    msgProcessors[MessageType::GET_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        FileDescriptor descriptor = *(FileDescriptor *) data;
        BOOST_LOG_TRIVIAL(debug) << "<<< GET_FILE: request for " << descriptor.getName()
                                 << " md5: " << descriptor.getMd5().getHash()
                                 << " from " << getFormatedIp(sourceAddress);
        {
            Guard guard(mutex);
            // check validity of requested file
            for (auto &&localDescriptor : localDescriptors) {
                if (localDescriptor.getMd5() == descriptor.getMd5() && !localDescriptor.isValid()) {
                    // request for discarded file
                    sendCommandRefused(MessageType::GET_FILE,
                                       "request for discarded file! try again for a while",
                                       sourceAddress);
                    // do not send the file
                    return;
                }
            }
        }

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

    // =================================================================================================================
    // someone requested to delete file sored in our machine
    msgProcessors[MessageType::DELETE_FILE] = [](const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {
        // file was discarded already by request node - we have to delete it and publish REVOKE
        FileDescriptor descriptor = *(FileDescriptor *) data;

        {
            Guard guard(mutex);
            // check validity of file requested to delete
            for (auto &&localDescriptor : localDescriptors) {
                if (localDescriptor.getMd5() == descriptor.getMd5() && !localDescriptor.isValid()) {
                    // request for discarded file
                    sendCommandRefused(MessageType::GET_FILE,
                                       "delete request for discarded file! try again for a while",
                                       sourceAddress);
                    // do not delete the file
                    return;
                }
            }
        }

        FileDeleter deleter(descriptor.getMd5().getHash());
        if (!deleter.deleteFile()) {
            // error - file should exsist
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