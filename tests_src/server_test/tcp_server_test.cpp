#include <boost/test/unit_test.hpp>

#include "tcp_server.hpp"

BOOST_AUTO_TEST_SUITE(TcpServerTest);


BOOST_AUTO_TEST_SUITE_END();

int errorCallbackCount = 0;
int resolveCallbackCount = 0;

uint8_t* receivedData;
size_t receivedDataSize;

void resolveCallback(uint8_t* x, uint32_t s, SocketOperation op)
{
	receivedData = x;
	receivedDataSize = s;
	++resolveCallbackCount;
	return;
}

void errorCallback(SocketOperation s)
{
	++errorCallbackCount;
	return;
}

BOOST_AUTO_TEST_CASE(checkTcpServerWorksAtAll)
{
	TcpServer server(&resolveCallback, &errorCallback);
	uint8_t data[10000];
	server.startListening();
	server.sendData(data, sizeof(data), inet_addr("127.0.0.1"));
	usleep(500000);
	BOOST_CHECK(resolveCallbackCount == 1);
}
