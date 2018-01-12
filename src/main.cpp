#define BOOST_TEST_MODULE My Test
#include <boost/test/included/unit_test.hpp>
#include "p2pMessage.hpp"
#include "fileDescriptor.hpp"
#include "messageType.hpp"

BOOST_AUTO_TEST_CASE(first_test)
{

  int i = 1;
  BOOST_TEST(i);
}
