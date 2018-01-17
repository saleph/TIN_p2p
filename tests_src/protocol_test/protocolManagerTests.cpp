#include <boost/test/unit_test.hpp>
#include <random>
#include <thread>

#include "p2pMessage.hpp"
#include "ProtocolManager.hpp"
//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE(ProtocolTests);

struct P2PFixture
{
    P2PFixture()
    {
    }

    ~P2PFixture()
    {
    }
};

BOOST_GLOBAL_FIXTURE(P2PFixture);

BOOST_AUTO_TEST_CASE(hello_and_helloreply)
{
}

BOOST_AUTO_TEST_SUITE_END()