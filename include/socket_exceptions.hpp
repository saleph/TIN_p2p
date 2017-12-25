/*
 * socket_exceptions.hpp
 *
 *  Created on: Dec 25, 2017
 *      Author: kolo
 */

#ifndef INCLUDE_SOCKET_EXCEPTIONS_HPP_
#define INCLUDE_SOCKET_EXCEPTIONS_HPP_

class SocketException : public std::runtime_error
{
public:
	SocketException(const char* s) : std::runtime_error(s) { };
};

#endif /* INCLUDE_SOCKET_EXCEPTIONS_HPP_ */
