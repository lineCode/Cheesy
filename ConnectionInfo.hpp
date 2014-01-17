/*
 * ConnectionInfo.hpp
 *
 *  Created on: Jan 8, 2014
 *      Author: elchaschab
 */

#ifndef CONNECTIONINFO_HPP_
#define CONNECTIONINFO_HPP_

#include "Caps.hpp"
#include <string>

namespace cheesy {
	struct ClientInfo {
		std::string peerAddress;
		Caps videoCaps;
		Caps audioCaps;
	};
}

#endif /* CONNECTIONINFO_HPP_ */
