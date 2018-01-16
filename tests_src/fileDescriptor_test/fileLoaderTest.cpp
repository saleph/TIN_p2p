#include <boost/test/unit_test.hpp>
#include <FileLoader.hpp>


BOOST_AUTO_TEST_SUITE(fileLoader);

BOOST_AUTO_TEST_CASE(simpleLoad)
{
    const std::string name = "../tests_src/protocol_test/example.txt";
    const std::string content = "md5 test\n";
    FileLoader loader(name);
    BOOST_TEST(content == std::string((char*)loader.getContent().data()));
}

BOOST_AUTO_TEST_SUITE_END();