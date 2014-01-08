/*
 * Caps.hpp
 *
 *  Created on: Jan 4, 2014
 *      Author: elchaschab
 */

#ifndef CAPS_HPP_
#define CAPS_HPP_

#include <gst/gst.h>
#include <string>
#include <map>

namespace cheesy {

using std::string;

class Codec {
	static std::map<string, Codec> codecMap;
public:
	string name;
	string encoderString;
	string decoderString;

	static Codec getCodec(string name) {
		return codecMap[name];
	}

	static void setCodec(string name, Codec c) {
		codecMap[name] = c;
	}
};

struct Caps {

public:
	string rtpCaps;
	Codec codec;
};

static Caps OPUS = {
		"application/x-rtp, media=(string)audio, payload=(int)96, clock-rate=(int)48000, encoding-name=(string)X-GST-OPUS-DRAFT-SPITTKA-00, encoding-params=(string)1",
		{"OPUS", "opusenc ! rtpopuspay", "rtoopusdepay ! opusdec"}
};

} /* namespace cheesy */

#endif /* CAPS_HPP_ */
