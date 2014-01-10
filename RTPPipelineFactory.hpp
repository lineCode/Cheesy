/*
 * PipelineFactory.hpp
 *
 *  Created on: Jan 7, 2014
 *      Author: elchaschab
 */

#ifndef PIPELINEFACTORY_HPP_
#define PIPELINEFACTORY_HPP_

#include <string>
#include "Pipeline.hpp"
#include "Caps.hpp"
#include "ConnectionInfo.hpp"
#include "easylogging++.h"

namespace cheesy {

using std::string;

struct ClientTemplates {
	string rtpBin = "gstrtpbin name=rtpbin";
	string videoSource = "ximagesrc";
	string colospaceConvert = "ffmpegcolorspace";
	string encoderString;
	string videoSink =
			"rtpbin.send_rtp_sink_0 rtpbin.send_rtp_src_0 ! udpsink name=vpsink";
	string videoControlSink = "rtpbin.send_rtcp_src_0 ! udpsink name=vcsink";
	string videoControlSource = "udpsrc name=vcsrc ! rtpbin.recv_rtcp_sink_0";
	string audioPipelineString =
			"pulsesrc name=psrc ! audioconvert ! opusenc ! rtpopuspay ! capsfilter name=acfilter ! rtpbin.send_rtp_sink_1 \
		rtpbin.send_rtp_src_1 ! udpsink name=apsink \
		rtpbin.send_rtcp_src_1 ! udpsink name=acsink \
		udpsrc name=acsrc ! rtpbin.recv_rtcp_sink_1";

};

struct ServerTemplates {
	string rtpBin = "gstrtpbin name=rtpbin";
	string videoSource = "udpsrc name=vpsrc ! rtpbin.recv_rtp_sink_0 rtpbin.";
	string decoderString;
	string colospaceConvert = "ffmpegcolorspace";
	string videoSink = "xvimagesink force-aspect-ratio=TRUE";
	string videoControlSource = "udpsrc name=vcsrc ! rtpbin.recv_rtcp_sink_0";
	string videoControlSink = "rtpbin.send_rtcp_src_0 ! udpsink name=vcsink";
	string audioPipelineString =
			"udpsrc name=apsrc ! rtpbin.recv_rtp_sink_1 \
rtpbin. ! rtpopusdepay ! opusdec ! audioconvert ! audioresample ! autoaudiosink \
udpsrc name=acsrc ! rtpbin.recv_rtcp_sink_1 \
rtpbin.send_rtcp_src_1 ! udpsink name=acsink";
};

class RTPPipelineFactory {
	ClientTemplates ct;
	ServerTemplates st;
public:
	Pipeline* createClientPipeline(string monitorSource, string host, int basePort, Codec c) {
		string strPipeline = ct.rtpBin + " "
				+ ct.videoSource + " ! " + ct.colospaceConvert + " ! " + c.encoderString + " ! " + ct.videoSink + " "
				+ ct.videoControlSink + " "
				+ ct.videoControlSource + " "
				+ ct.audioPipelineString;

		LOG(DEBUG) << "Pipeline assembled: " << strPipeline;

		Pipeline* pipeline = new Pipeline(strPipeline);
		pipeline->set("psrc", "device", monitorSource);

		pipeline->set("vpsink", "host", host);
		pipeline->set("vcsink", "host", host);
		pipeline->set("apsink", "host", host);
		pipeline->set("acsink", "host", host);

		pipeline->set("vpsink", "port", basePort + 1);
		pipeline->set("vcsink", "port", basePort + 2);
		pipeline->set("apsink", "port", basePort + 3);
		pipeline->set("acsink", "port", basePort + 4);

		pipeline->set("vcsrc", "port", basePort + 1);
		pipeline->set("acsrc", "port", basePort + 2);

		pipeline->set("acfilter", "caps", gst_caps_from_string(OPUS.rtpCaps.c_str()));
		return pipeline;
	}

	Pipeline* createServerPipeline(int basePort, ConnectionInfo info) {
		string strPipeline = st.rtpBin + " "
				+ st.videoSource + " ! " + info.caps.codec.decoderString + " ! " + st.colospaceConvert + " ! " + st.videoSink + " "
				+ st.videoControlSource + " "
				+ st.videoControlSink + " "
				+ st.audioPipelineString;

		LOG(DEBUG) << "Pipeline assembled: " << strPipeline;
		Pipeline* pipeline = new Pipeline(strPipeline);
		pipeline->set("vpsrc", "caps", gst_caps_from_string(info.caps.rtpCaps.c_str()));
		pipeline->set("apsrc", "caps", gst_caps_from_string(OPUS.rtpCaps.c_str()));

		pipeline->set("vpsrc", "port", basePort + 1);
		pipeline->set("vcsrc", "port", basePort + 2);
		pipeline->set("apsrc", "port", basePort + 3);
		pipeline->set("acsrc", "port", basePort + 4);

		pipeline->set("vcsink", "host", info.peerAddress);
		pipeline->set("acsink", "host", info.peerAddress);
		pipeline->set("vcsink", "port", basePort + 1);
		pipeline->set("acsink", "port", basePort + 2);
		return pipeline;
	}

	ClientTemplates getClientTemplates () {
		return ct;
	}

	ServerTemplates getServerTemplates () {
		return st;
	}
};

} /* namespace cheesy */

#endif /* PIPELINEFACTORY_HPP_ */
