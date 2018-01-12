#include <boost/test/unit_test.hpp>

#include "p2pMessage.hpp"
#include "tcp_server.hpp"
#include "udp_server.hpp"
//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE(ServerTests)

struct Config
{
    Config()   {  }
    ~Config()  {  }

    static SocketOperation operation;

    static void tcpResolveCallback(uint8_t* x, uint32_t s, SocketOperation op)
    {
        return;
    }

    static void errorCallback(SocketOperation s)
    {
        operation = s;
        return;
    }
};

SocketOperation Config::operation;

BOOST_TEST_GLOBAL_FIXTURE( Config );

BOOST_AUTO_TEST_CASE(checkCantConnectCallback)
{
    TcpServer server(&Config::tcpResolveCallback, &Config::errorCallback);
    uint8_t* data = new uint8_t[50];
    server.sendData(data, 50, inet_addr("127.0.0.1"));
    usleep(10000);

    BOOST_TEST(Config::operation.status == SocketOperation::Status::CantConnect);

}

BOOST_AUTO_TEST_CASE(checkReceiveFailCallback)
{
    TcpServer server(&Config::tcpResolveCallback, &Config::errorCallback);

    usleep(10000);

    server.startListening();

    uint8_t* data = new uint8_t[50];
    server.sendData(data, 1, inet_addr("127.0.0.1"));

    usleep(200000);
    BOOST_TEST(Config::operation.status == SocketOperation::Status::ReceiveFailed);

    Config::operation.status = SocketOperation::Status::Success;

    P2PMessage msg;
    msg.setMessageType(MessageType::UPLOAD_FILE);
    msg.setAdditionalDataSize(50000);
    memcpy(data, &msg, sizeof(msg));
    server.sendData(data, 50, inet_addr("127.0.0.1"));
    usleep(200000);
    server.stopListening();
    BOOST_TEST(Config::operation.status == SocketOperation::Status::ReceiveFailed);
}

BOOST_AUTO_TEST_SUITE_END();
