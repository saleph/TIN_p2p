cmake_minimum_required(VERSION 3.5)

project(tin_p2p)
include(CTest)
enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -DBOOST_LOG_DYN_LINK")
set(LIB_NAME p2pLib)
set(APP_NAME p2p)
set(TESTS_NAME p2pTests)

SET(BOOST_LIBRARYDIR "~/boost_1_65_1")

find_package(Boost COMPONENTS unit_test_framework log system filesystem REQUIRED)

file(GLOB APP_SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE LIB_SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/library_src/*.cpp)
file(GLOB_RECURSE TESTS_SOURCE_FILES ${PROJECT_SOURCE_DIR}/tests_src/*.cpp)
file(GLOB_RECURSE APP_INCLUDE_FILES ${PROJECT_SOURCE_DIR}/include/*.hpp)

message(STATUS "APP_FILES " ${APP_SOURCE_FILES})
message(STATUS "LIB_FILES " ${LIB_SOURCE_FILES})
message(STATUS "TESTS_FILES " ${TESTS_SOURCE_FILES})

include_directories(${PROJECT_SOURCE_DIR}/include)
set(LIBS ${Boost_LIBRARIES} pthread)

add_library(${LIB_NAME} STATIC ${LIB_SOURCE_FILES} ${APP_INCLUDE_FILES})
target_link_libraries(${LIB_NAME} ${LIBS})

add_executable(${APP_NAME} ${APP_SOURCE_FILES})
target_link_libraries(${APP_NAME} ${LIB_NAME} ${LIBS})

add_executable(${TESTS_NAME} ${TESTS_SOURCE_FILES})
target_link_libraries(${TESTS_NAME} ${LIB_NAME} ${LIBS})
add_test(tests ${TESTS_NAME})

