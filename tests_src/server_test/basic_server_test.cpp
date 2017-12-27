#include <boost/test/unit_test.hpp>
#include <random>

#include "p2pMessage.hpp"
#include "tcp_server.hpp"
#include "udp_server.hpp"
//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE(ServerTests)

struct MyConfig
{
    MyConfig()   {  }
    ~MyConfig()  {  }

    static uint8_t* receivedData;
    static uint32_t receivedDataSize;
    static int errorCallbackCount;
    static int tcpResolveCallbackCount;
    static int tcpLongCallbackCount;
    static int udpLongCallbackCount;
    static int udpResolveCallbackCount;

    static Mutex mTcp;
    static Mutex mUdp;

    static void tcpResolveCallback(uint8_t* x, uint32_t s, SocketOperation op)
    {
        MyConfig::receivedData = new uint8_t[s];
        memcpy(MyConfig::receivedData, x, s);
        MyConfig::receivedDataSize = s;
        ++MyConfig::tcpResolveCallbackCount;
        return;
    }

    static void tcpLongCallback(uint8_t* x, uint32_t s, SocketOperation op)
    {
        usleep(500000);
        mTcp.lock();
        ++MyConfig::tcpLongCallbackCount;
        mTcp.unlock();
        return;
    }

    static void udpLongCallback(uint8_t* x, uint32_t s, SocketOperation op)
    {
        usleep(500000);
        mUdp.lock();
        ++MyConfig::udpLongCallbackCount;
        mUdp.unlock();
        return;
    }

    static void udpResolveCallback(uint8_t* x, uint32_t s, SocketOperation op)
    {
        MyConfig::receivedData = new uint8_t[s];
        memcpy(MyConfig::receivedData, x, s);
        MyConfig::receivedDataSize = s;
        ++MyConfig::udpResolveCallbackCount;
        return;
    }

    static void errorCallback(SocketOperation s)
    {
        ++MyConfig::errorCallbackCount;
        return;
    }
};

Mutex MyConfig::mTcp;
Mutex MyConfig::mUdp;
int MyConfig::errorCallbackCount = 0;
int MyConfig::tcpResolveCallbackCount = 0;
int MyConfig::udpResolveCallbackCount = 0;
int MyConfig::tcpLongCallbackCount = 0;
int MyConfig::udpLongCallbackCount = 0;
uint32_t MyConfig::receivedDataSize = 0;
uint8_t* MyConfig::receivedData = NULL;

BOOST_TEST_GLOBAL_FIXTURE( MyConfig );

BOOST_AUTO_TEST_CASE(checkTcpServerWorksAtAll)
{
    TcpServer* server = new TcpServer(&MyConfig::tcpResolveCallback, &MyConfig::errorCallback);
    server->startListening();
    usleep(100000);

    P2PMessage msg;
    msg.setMessageType(MessageType::UPLOAD_FILE);
    msg.setAdditionalDataSize(10000);
    int sentDataSize = sizeof(msg) + 10000;
    uint8_t* data = new uint8_t[sentDataSize];
    memcpy(data, &msg, sizeof(msg));
    for (int i = sizeof(msg); i < sentDataSize; ++i)
    {
        data[i] = rand() % 256;
    }
    server->sendData(data, sentDataSize, inet_addr("127.0.0.1"));
    usleep(100000);

    BOOST_TEST(MyConfig::tcpResolveCallbackCount == 1);

    for (int i = 0; i < sentDataSize; ++i)
    {
        BOOST_TEST(MyConfig::receivedData[i] == data[i]);
    }

    server->stopListening();

    delete[] MyConfig::receivedData;

    delete server;
}

BOOST_AUTO_TEST_CASE(checkUdpServerWorksAtAll)
{
    UdpServer* server = new UdpServer(&MyConfig::udpResolveCallback);
    server->startListening();
    usleep(100000);

    uint32_t sentDataSize = 400;
    uint8_t* data = new uint8_t[sentDataSize];
    for (int i = 0; i < sentDataSize; ++i)
    {
        data[i] = rand() % 256;
    }
    server->broadcast(data, sentDataSize);
    usleep(100000);

    BOOST_TEST(MyConfig::udpResolveCallbackCount == 1);

    for (int i = 0; i < sentDataSize; ++i)
    {
        BOOST_TEST(MyConfig::receivedData[i] == data[i]);
    }

    server->stopListening();

    delete[] MyConfig::receivedData;

    delete server;
}

BOOST_AUTO_TEST_CASE(checkServersWaitForDispatchersToFinish)
{
    TcpServer tcp(&MyConfig::tcpLongCallback, &MyConfig::errorCallback);
    UdpServer udp(&MyConfig::udpLongCallback);
    tcp.startListening();
    udp.startListening();

    P2PMessage msg;
    msg.setMessageType(MessageType::UPLOAD_FILE);
    msg.setAdditionalDataSize(10);
    int sentDataSize = sizeof(msg) + 10;
    uint8_t* data = new uint8_t[sentDataSize];
    memcpy(data, &msg, sizeof(msg));

    for (int i = sizeof(msg); i < sentDataSize; ++i)
    {
        data[i] = rand() % 256;
    }
    for(int i = 0; i < 24; ++i)
    {
        udp.broadcast(data, sentDataSize);
        tcp.sendData(data, sentDataSize, inet_addr("127.0.0.1"));
    }

    usleep(100000);

    BOOST_TEST(MyConfig::tcpLongCallbackCount < 24);
    BOOST_TEST(MyConfig::udpLongCallbackCount < 24);

    tcp.stopListening();
    udp.stopListening();

    BOOST_TEST(MyConfig::tcpLongCallbackCount == 24);
    BOOST_TEST(MyConfig::udpLongCallbackCount == 24);

    delete[] data;
    //while(1);
}

BOOST_AUTO_TEST_SUITE_END();
