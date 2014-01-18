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
	string videoSource = "ximagesrc name=vpsrc";
	string videoConvert = "ffmpegcolorspace";
	string videoSink = "rtpbin.send_rtp_sink_0 rtpbin.send_rtp_src_0 ! udpsink name=vpsink";
	string videoControlSink = "rtpbin.send_rtcp_src_0 ! udpsink name=vcsink";
	string videoControlSource = "udpsrc name=vcsrc ! rtpbin.recv_rtcp_sink_0";

	string audioSource = "pulsesrc name=apsrc";
	string audioConvert = "audioconvert";
	string encodeString;
	string audioSink = "rtpbin.send_rtp_sink_1 rtpbin.send_rtp_src_1 ! udpsink name=apsink";
	string audioControlSink = "rtpbin.send_rtcp_src_1 ! udpsink name=acsink";
	string audioControlSource = "udpsrc name=acsrc ! rtpbin.recv_rtcp_sink_1";
};

struct ServerTemplates {
	string rtpBin = "gstrtpbin name=rtpbin";
	string videoSource = "udpsrc name=vpsrc ! rtpbin.recv_rtp_sink_0 rtpbin.";
	string videoConvert = "videoscale ! ffmpegcolorspace";
	string videoSink = "xvimagesink name=vpsink";
	string videoControlSource = "udpsrc name=vcsrc ! rtpbin.recv_rtcp_sink_0";
	string videoControlSink = "rtpbin.send_rtcp_src_0 ! udpsink name=vcsink";

	string audioSource = "udpsrc name=apsrc ! rtpbin.recv_rtp_sink_1 rtpbin.";
	string decoderString;
	string audioConvert = "audioconvert ! audioresample";
	string audioSink = "autoaudiosink name=apsink";
	string audioControlSource = "udpsrc name=acsrc ! rtpbin.recv_rtcp_sink_1";
	string audioControlSink = "rtpbin.send_rtcp_src_1 ! udpsink name=acsink";
};

class RTPPipelineFactory {
	ClientTemplates ct;
	ServerTemplates st;
public:
	Pipeline* createClientPipeline(string monitorSource, string host, int basePort, Codec vc, Codec ac, bool showPointer) {
		string videoPipeline = ct.videoSource + " ! " + ct.videoConvert + " ! " + vc.encoderString + " ! " + ct.videoSink + " "
				+ ct.videoControlSink + " "
				+ ct.videoControlSource + " ";

		string audioPipeline = ct.audioSource + " ! " + ct.audioConvert + " ! " + ac.encoderString + " ! " + ct.audioSink + " "
				+ ct.audioControlSink + " "
				+ ct.audioControlSource + " ";

		string strPipeline = ct.rtpBin + " ";

		if(vc.name != EMPTY_CAPS.codec.name)
			strPipeline += (" " + videoPipeline);
		if(ac.name != EMPTY_CAPS.codec.name)
			strPipeline += (" " +audioPipeline);

		LOG(DEBUG) << "Pipeline assembled: " << strPipeline;

		Pipeline* pipeline = new Pipeline(strPipeline);

		if (vc.name != EMPTY_CAPS.codec.name) {
			pipeline->set("vpsink", "host", host);
			pipeline->set("vcsink", "host", host);
			pipeline->set("vpsink", "port", basePort + 1);
			pipeline->set("vcsink", "port", basePort + 2);
			pipeline->set("vcsrc", "port", basePort + 1);
			pipeline->set("vpsrc", "show-pointer", showPointer);
		} else {
			LOG(DEBUG)<< "Video disabled";
		}

		if (ac.name != EMPTY_CAPS.codec.name) {
			pipeline->set("apsrc", "device", monitorSource);
			pipeline->set("apsink", "host", host);
			pipeline->set("acsink", "host", host);
			pipeline->set("apsink", "port", basePort + 3);
			pipeline->set("acsink", "port", basePort + 4);
			pipeline->set("acsrc", "port", basePort + 2);
		} else {
			LOG(DEBUG)<< "Audio disabled";
		}

		return pipeline;
	}

	Pipeline* createServerPipeline(int basePort, ClientInfo info) {
		string videoPipeline = st.videoSource + " ! " + info.videoCaps.codec.decoderString + " ! " + st.videoConvert + " ! " + st.videoSink + " "
				+ st.videoControlSource + " "
				+ st.videoControlSink;

		string audioPipeline = st.audioSource + " ! " + info.audioCaps.codec.decoderString + " ! " + st.audioConvert + " ! " + st.audioSink + " "
				+ st.audioControlSink + " "
				+ st.audioControlSource;

		string strPipeline = ct.rtpBin + " ";

		if(info.videoCaps.codec.name != EMPTY_CAPS.codec.name)
			strPipeline += (" " + videoPipeline);
		if(info.audioCaps.codec.name != EMPTY_CAPS.codec.name)
			strPipeline += (" " + audioPipeline);


		LOG(DEBUG)<< "Pipeline assembled: " << strPipeline;
		Pipeline* pipeline = new Pipeline(strPipeline);
		if (info.videoCaps.codec.name != EMPTY_CAPS.codec.name) {
			LOG(DEBUG)<< "Setting video rtpcaps: " << info.videoCaps.rtpCaps;
			pipeline->set("vpsrc", "caps", gst_caps_from_string(info.videoCaps.rtpCaps.c_str()));
			pipeline->set("vpsrc", "port", basePort + 1);
			pipeline->set("vcsrc", "port", basePort + 2);
			pipeline->set("vcsink", "host", info.peerAddress);
			pipeline->set("vcsink", "port", basePort + 1);
			pipeline->set("vpsink", "force-aspect-ratio", true);
		} else {
			LOG(DEBUG)<< "Video disabled";
		}

		if (info.audioCaps.codec.name != EMPTY_CAPS.codec.name) {
			LOG(DEBUG)<< "Setting audio rtpcaps: " << info.audioCaps.rtpCaps;
			pipeline->set("apsrc", "caps", gst_caps_from_string(info.audioCaps.rtpCaps.c_str()));
			pipeline->set("apsrc", "port", basePort + 3);
			pipeline->set("acsrc", "port", basePort + 4);
			pipeline->set("acsink", "host", info.peerAddress);
			pipeline->set("acsink", "port", basePort + 2);
		} else {
			LOG(DEBUG)<< "Audio disabled";
		}

		return pipeline;
	}

	ClientTemplates& getClientTemplates () {
		return ct;
	}

	ServerTemplates& getServerTemplates () {
		return st;
	}
};

} /* namespace cheesy */

#endif /* PIPELINEFACTORY_HPP_ */
