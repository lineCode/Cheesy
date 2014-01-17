/*
 * CheesyServer.h
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#ifndef CHEESYSERVER_H_
#define CHEESYSERVER_H_

#include <string>
#include <boost/asio.hpp>
#include "Caps.hpp"
#include "ConnectionInfo.hpp"

namespace cheesy {

class CapsServer {
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::socket* socket;
    boost::asio::ip::tcp::acceptor acceptor;
public:
	CapsServer(int port);
	virtual ~CapsServer();

	void close();
	ClientInfo accept(bool disableVideo, bool disableSound);
};

} /* namespace cheesy */

#endif /* CHEESYSERVER_H_ */
