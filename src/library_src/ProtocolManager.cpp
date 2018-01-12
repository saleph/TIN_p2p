#include <p2pMessage.hpp>
#include <iostream>
#include "ProtocolManager.hpp"


void p2p::initialize() {
    util::tcpServer = std::make_shared<TcpServer>(&util::processTcpMsg, &util::processTcpError);
    util::udpServer = std::make_shared<UdpServer>(&util::processUdpMsg);
    util::tcpServer->startListening();
    util::udpServer->startListening();
}

void p2p::util::processTcpError(SocketOperation operation) {
    BOOST_LOG_TRIVIAL(debug) << "TCP error";
}

void p2p::util::processTcpMsg(uint8_t *data, uint32_t size, SocketOperation operation) {
    P2PMessage &p2pMessage = *(P2PMessage*)data;
    MessageType messageType = p2pMessage.getMessageType();
    const uint8_t *additionalData = data + sizeof(P2PMessage);
    const uint32_t additionalDataSize = size - sizeof(P2PMessage);
    assert (additionalDataSize == p2pMessage.getAdditionalDataSize());

    messageProcessors.at(messageType)(additionalData, additionalDataSize, operation.connectionAddr);
}

void p2p::util::processUdpMsg(uint8_t *data, uint32_t size, SocketOperation operation) {
    P2PMessage &p2pMessage = *(P2PMessage*)data;
    MessageType messageType = p2pMessage.getMessageType();
    const uint8_t *additionalData = data + sizeof(P2PMessage);
    const uint32_t additionalDataSize = size - sizeof(P2PMessage);
    assert (additionalDataSize == p2pMessage.getAdditionalDataSize());

    messageProcessors.at(messageType)(additionalData, additionalDataSize, operation.connectionAddr);
}

void p2p::util::initProcessingFunctions() {
    messageProcessors[MessageType::HELLO] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::HELLO_REPLY] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::DISCONNECTING] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::CONNECTION_LOST] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::CMD_REFUSED] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::NEW_FILE] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::REVOKE_FILE] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::DISCARD_DESCRIPTOR] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::UPDATE_DESCRIPTOR] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::HOLDER_CHANGE] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::FILE_TRANSFER] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::UPLOAD_FILE] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::GET_FILE] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

    messageProcessors[MessageType::DELETE_FILE] = [] (const uint8_t *data, uint32_t size, in_addr_t sourceAddress) {

    };

}