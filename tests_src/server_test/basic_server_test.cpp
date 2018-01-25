#include <boost/test/unit_test.hpp>
#include <random>
#include <iostream>

#include "P2PMessage.hpp"
#include "TcpServer.hpp"
#include "UdpServer.hpp"
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
    static std::map <int, std::tuple<bool, uint8_t*, uint32_t>> idToResponseMap;

    static Mutex mTcp;
    static Mutex mUdp;
    static Mutex mapMutex;

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
        //std::cout << "CALLBACK BY THREAD " << pthread_self() << "BEFORE " <<
         //MyConfig::tcpLongCallbackCount << std::endl;
        ++MyConfig::tcpLongCallbackCount;
        //std::cout << "AFTER "<<
         //MyConfig::tcpLongCallbackCount << std::endl;
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

    static void udpMapResponse(uint8_t* x, uint32_t s, SocketOperation op)
    {
        uint32_t id = *( (uint32_t*)(x +sizeof(P2PMessage)) );
        uint8_t* copied = new uint8_t[s];
        memcpy(copied, x, s);

        mapMutex.lock();
        idToResponseMap[id] = std::make_tuple(true, copied, s);
        mapMutex.unlock();

        return;
    }

    static void tcpMapResponse(uint8_t* x, uint32_t s, SocketOperation op)
    {
        uint32_t id = *( (uint32_t*)(x +sizeof(P2PMessage)) );
        uint8_t* copied = new uint8_t[s];
        memcpy(copied, x, s);

        mapMutex.lock();
        idToResponseMap[id] = std::make_tuple(false, copied, s);
        mapMutex.unlock();

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
Mutex MyConfig::mapMutex;
int MyConfig::errorCallbackCount = 0;
int MyConfig::tcpResolveCallbackCount = 0;
int MyConfig::udpResolveCallbackCount = 0;
int MyConfig::tcpLongCallbackCount = 0;
int MyConfig::udpLongCallbackCount = 0;
uint32_t MyConfig::receivedDataSize = 0;
uint8_t* MyConfig::receivedData = NULL;
std::map <int, std::tuple<bool, uint8_t*, uint32_t> >MyConfig::idToResponseMap;

BOOST_TEST_GLOBAL_FIXTURE( MyConfig );

BOOST_AUTO_TEST_CASE(checkTcpServerWorksAtAll)
{
    TcpServer* server = new TcpServer(&MyConfig::tcpResolveCallback, &MyConfig::errorCallback);
    server->startListening();

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

    delete[] data;
    delete[] MyConfig::receivedData;

    delete server;
}

BOOST_AUTO_TEST_CASE(checkUdpDoesntReceiveSelfBroadcasts)
{
    UdpServer* server = new UdpServer(&MyConfig::udpResolveCallback);
    server->startListening();
    MyConfig::udpResolveCallbackCount = 0;

    uint32_t sentDataSize = 400;
    uint8_t* data = new uint8_t[sentDataSize];
    for (int i = 0; i < sentDataSize; ++i)
    {
        data[i] = rand() % 256;
    }
    server->broadcast(data, sentDataSize);

    usleep(50000);

    server->stopListening();
    BOOST_TEST(MyConfig::udpResolveCallbackCount == 0);
}

BOOST_AUTO_TEST_CASE(checkUdpServerWorksAtAll)
{
    MyConfig::udpResolveCallbackCount = 0;
    Server::getLocalhostIp();
    UdpServer* server = new UdpServer(&MyConfig::udpResolveCallback);
    server->enableSelfBroadcasts();
    server->startListening();

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

    delete[] data;
    delete[] MyConfig::receivedData;

    delete server;
}

BOOST_AUTO_TEST_CASE(checkServersWaitForDispatchersToFinish)
{
    MyConfig::tcpResolveCallbackCount = 0;
    TcpServer tcp(&MyConfig::tcpLongCallback, &MyConfig::errorCallback);
    UdpServer udp(&MyConfig::udpLongCallback);
    udp.enableSelfBroadcasts();
    tcp.startListening();
    udp.startListening();

    int num_sends = 100;

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
    for(int i = 0; i < num_sends; ++i)
    {
        udp.broadcast(data, sentDataSize);
        tcp.sendData(data, sentDataSize, inet_addr("127.0.0.1"));
    }

    BOOST_TEST(MyConfig::tcpLongCallbackCount < num_sends);
    BOOST_TEST(MyConfig::udpLongCallbackCount < num_sends);

    tcp.stopListening();
    udp.stopListening();

    BOOST_TEST(MyConfig::tcpLongCallbackCount == num_sends);
    BOOST_TEST(MyConfig::udpLongCallbackCount == num_sends);

    delete[] data;
    //while(1);
}

BOOST_AUTO_TEST_CASE(checkMultipleRecipientsGetExpectedResponses)
{
    srand(time(NULL));
    TcpServer tcp(&MyConfig::tcpMapResponse, &MyConfig::errorCallback);
    UdpServer udp(&MyConfig::udpMapResponse);
    udp.enableSelfBroadcasts();
    tcp.startListening();
    udp.startListening();

    std::map<int, std::tuple<bool, uint8_t*, uint32_t> > expectedIdToResponseMap;

    uint32_t num_sends = 100;
    uint8_t* buf;
    uint32_t dataSize;
    bool isUdp;
    P2PMessage msg;

    usleep(50000);

    for (uint32_t id = 1; id <= num_sends; ++id)
    {
        isUdp = rand() % 2;
        dataSize = 50 + rand() % 900;

        msg.setMessageType(MessageType::UPLOAD_FILE);
        msg.setAdditionalDataSize(dataSize);

        buf = new uint8_t[sizeof(P2PMessage) + dataSize];

        memcpy(buf, &msg, sizeof(P2PMessage));
        memcpy(buf + sizeof(P2PMessage), &id, sizeof(uint32_t));

        for (uint8_t* bufpointer = buf + sizeof(P2PMessage) + sizeof(uint32_t);
             bufpointer != buf + sizeof(P2PMessage) + dataSize;
             ++bufpointer)
        {
            *bufpointer = rand() % 256;
        }

        expectedIdToResponseMap[id] = std::make_tuple(isUdp, buf, dataSize + sizeof(P2PMessage));

        if (isUdp)
        {
            udp.broadcast(buf, dataSize + sizeof(P2PMessage));
        }
        else
        {
            tcp.sendData(buf, dataSize + sizeof(P2PMessage), inet_addr("127.0.0.1"));
        }
    }

    usleep(50000);
    tcp.stopListening();
    udp.stopListening();

    BOOST_TEST(MyConfig::idToResponseMap.size() == expectedIdToResponseMap.size());

    for (int id = 1; id <= num_sends; ++id )
    {
        std::tuple <bool, uint8_t*, uint32_t> receivedTuple, expectedTuple;
        receivedTuple = MyConfig::idToResponseMap[id];
        expectedTuple = expectedIdToResponseMap[id];


        bool receivedUdp = std::get<0>(receivedTuple);
        uint8_t* receivedData = std::get<1>(receivedTuple);
        uint32_t receivedSize = std::get<2>(receivedTuple);

        bool expectedUdp = std::get<0>(expectedTuple);
        uint8_t* expectedData = std::get<1>(expectedTuple);
        uint32_t expectedSize = std::get<2>(expectedTuple);

        BOOST_TEST(receivedUdp == expectedUdp);
        BOOST_TEST(receivedSize == expectedSize);

        for (int i = 0; i < receivedSize; ++i)
        {
            BOOST_TEST(expectedData[i] == receivedData[i]);
        }
        delete[] receivedData;
        delete[] expectedData;
    }
}

BOOST_AUTO_TEST_SUITE_END();
