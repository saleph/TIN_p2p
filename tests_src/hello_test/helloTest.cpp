#define BOOST_TEST_NO_LIB
#include <boost/test/unit_test.hpp>

#include "hello.hpp"

BOOST_AUTO_TEST_CASE(first_test)
{
fun();
  int i = 14;
  BOOST_TEST(i);
  BOOST_TEST(i == 2);
}
