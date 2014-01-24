/*
 * CheesyClient.h
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#ifndef CHEESYCLIENT_H_
#define CHEESYCLIENT_H_

#include <string>
#include <boost/asio.hpp>
#include "Caps.hpp"

namespace cheesy {

struct ServerInfo {
	bool hasVideo = true;
	bool hasAudio = true;
};

class CapsClient {
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::socket socket;
public:
	CapsClient();
	virtual ~CapsClient();
	void connect(std::string host, int port);
	ServerInfo announce(Caps videoCaps, Caps audioCaps);
	std::string join();
	void close();
};

} /* namespace cheesy */

#endif /* CHEESYCLIENT_H_ */
