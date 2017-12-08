#define BOOST_TEST_NO_LIB
#include <boost/test/unit_test.hpp>

#include "md5sum.hpp"

BOOST_AUTO_TEST_SUITE(Md5Test);

BOOST_AUTO_TEST_CASE(checkExampleFileMD5HashValidity)
{
	const std::string EXAMPLE_FILE_MD5 = "90ebef7754cd9e4441622f39f10c63d3";
	Md5sum md5("tests_src/md5_test/example.txt");
	BOOST_TEST(md5.getMd5Hash().getHash() == EXAMPLE_FILE_MD5);
}

BOOST_AUTO_TEST_CASE(checkNotExistingFile)
{
	BOOST_CHECK_THROW(Md5sum("/x"), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END();
