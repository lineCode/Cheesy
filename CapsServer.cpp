/*
 * CheesyServer.cpp
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#include <iostream>
#include "CapsServer.hpp"
#include "Caps.hpp"
#include "ConnectionInfo.hpp"

namespace cheesy {

using boost::asio::ip::tcp;

CapsServer::CapsServer(int port) : io_service(), socket(NULL), acceptor(io_service){
    tcp::resolver resolver(io_service);
    tcp::resolver::query query("0.0.0.0", std::to_string(port));
    boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

    acceptor.open(endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
}

CapsServer::~CapsServer() {
	this->close();
}

void CapsServer::close() {
	if(socket != NULL)
		socket->close();
}

ConnectionInfo CapsServer::accept() {
	acceptor.listen();
	boost::asio::ip::tcp::socket* s = new boost::asio::ip::tcp::socket(io_service);
	acceptor.accept(*s);

	if(socket != NULL && socket->is_open()) {
		socket->close();
		delete socket;
	}

	socket = s;

	std::string peerAddr = socket->remote_endpoint().address().to_string();

	boost::asio::streambuf response;
	boost::asio::read_until(*socket, response,"\n");

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string codecName;
	std::string rtpCaps;

	std::getline(response_stream, codecName);
	std::getline(response_stream, rtpCaps);

	return {peerAddr,{rtpCaps, Codec::getCodec(codecName)}};
}
} /* namespace cheesy */
