#define BOOST_TEST_NO_LIB
#include <boost/test/unit_test.hpp>

#include "fileDescriptor.hpp"
#include "md5sum.hpp"
#include "md5hash.hpp"
#include <sys/stat.h>
#include <stdexcept>

const std::string EXAMPLE_FILE = "tests_src/md5_test/example.txt";

BOOST_AUTO_TEST_SUITE(FileDescriptorTests);

BOOST_AUTO_TEST_CASE(exampleFileMd5HashValidity)
{
	Md5sum md5(EXAMPLE_FILE);
	FileDescriptor descriptor(EXAMPLE_FILE);
	BOOST_TEST(md5.getMd5Hash().getHash() == descriptor.getMd5().getHash());
}

BOOST_AUTO_TEST_CASE(exampleFileSizeValidity)
{
	struct stat st;
	stat(EXAMPLE_FILE.c_str(), &st);
	size_t fileSize = st.st_size;

	FileDescriptor descriptor(EXAMPLE_FILE);
	BOOST_TEST(descriptor.getSize() == fileSize);
}

BOOST_AUTO_TEST_CASE(fileNotExistThrowsInvalidArgument)
{
	BOOST_CHECK_THROW(FileDescriptor(""), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END();
