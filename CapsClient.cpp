/*
 * CheesyClient.cpp
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#include <iostream>

#include "CapsClient.hpp"

namespace cheesy {

using boost::asio::ip::tcp;

CapsClient::CapsClient() : socket(io_service) {
}

CapsClient::~CapsClient() {
}


void CapsClient::connect(string host, int port) {
	    tcp::resolver resolver(io_service);
	    tcp::resolver::query query(host, std::to_string(port));
	    tcp::resolver::iterator iterator = resolver.resolve(query);
	    boost::asio::connect(socket, iterator);
}
void CapsClient::announce(Caps caps) {
    boost::asio::streambuf request;
    std::ostream request_stream(&request);
    request_stream << caps.codec.name << '\n';
    request_stream << caps.rtpCaps << '\n';
    boost::asio::write(socket, request);
}

void CapsClient::join() {
	boost::asio::streambuf response;
	boost::asio::read_until(socket, response,"\n");

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string s;

	std::getline(response_stream, s);

}

} /* namespace cheesy */
