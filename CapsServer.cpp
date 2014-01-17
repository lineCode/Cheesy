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
#include <assert.h>

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

ClientInfo CapsServer::accept(bool disableVideo, bool disableSound) {
	assert(!(disableVideo && disableSound));
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
	std::string videoCodecName;
	std::string rtpVideoCaps;
	std::string audioCodecName;
	std::string rtpAudioCaps;
	Caps videoCaps;
	Caps audioCaps;

	std::getline(response_stream, videoCodecName);
	std::getline(response_stream, rtpVideoCaps);
	std::getline(response_stream, audioCodecName);
	std::getline(response_stream, rtpAudioCaps);

	//FIXME make sending rtp audio caps work
	rtpAudioCaps = OPUS.rtpCaps;
	if(disableVideo || videoCodecName == EMPTY_CAPS.codec.name)
		videoCaps = EMPTY_CAPS;
	else
		videoCaps = {rtpVideoCaps, Codec::getCodec(videoCodecName)};

	if(disableSound || audioCodecName == EMPTY_CAPS.codec.name)
		audioCaps = EMPTY_CAPS;
	else
		audioCaps = {rtpAudioCaps, Codec::getCodec(audioCodecName)};

	boost::asio::streambuf request;
    std::ostream request_stream(&request);

	if(disableVideo) {
		request_stream << "disable-video" << '\n';
	} else if(disableSound) {
		request_stream << "disable-sound" << '\n';
	} else {
		request_stream << "ok" << '\n';
	}
    boost::asio::write(*socket, request);

	return {peerAddr, videoCaps, audioCaps};
}
} /* namespace cheesy */
