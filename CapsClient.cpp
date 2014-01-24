/*
 * CheesyClient.cpp
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#include <iostream>

#include "CapsClient.hpp"
#include "easylogging++.h"
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

ServerInfo CapsClient::announce(Caps videoCaps, Caps audioCaps) {
	ServerInfo soptions;
	boost::asio::streambuf request;
    std::ostream request_stream(&request);

    request_stream << videoCaps.codec.name << '\n';
    request_stream << videoCaps.rtpCaps << '\n';
    request_stream << audioCaps.codec.name << '\n';
    request_stream << audioCaps.rtpCaps << '\n';

    boost::asio::write(socket, request);

	boost::asio::streambuf response;
	boost::asio::read_until(socket, response, "\n");

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string strResponse;

	std::getline(response_stream, strResponse);
	soptions.hasVideo = true;
	soptions.hasAudio = true;
	if(strResponse == "disable-video")
		soptions.hasVideo = false;
	else if(strResponse == "disable-audio")
		soptions.hasAudio = false;

	LOG(DEBUG) << "Response: " + strResponse;
	return soptions;
}

void CapsClient::close() {
	socket.close();
}

std::string CapsClient::join() {
	boost::asio::streambuf response;
	boost::asio::read_until(socket, response, "\n");

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string s;

	std::getline(response_stream, s);
	return s;
}

} /* namespace cheesy */
