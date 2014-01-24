#include "PulseMonitorSource.hpp"
#include "RTPPipelineFactory.hpp"
#include "CapsClient.hpp"
#include "CapsServer.hpp"
#include "Caps.hpp"
#include <iostream>
#include <assert.h>
#include <gst/gst.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>
#include <boost/program_options.hpp>
#include "easylogging++.h"

namespace po = boost::program_options;

_INITIALIZE_EASYLOGGINGPP

class codecloader_failed :
  public std::exception
{
public:
	codecloader_failed(std::string message): what_(message)
  {
  }

  virtual const char *what() const throw()
  {
    return what_.c_str();
  }

private:
  std::string what_;
};

void loadCodecs(std::string filename) {
	std::ifstream is(filename);

	if(!is)
		throw codecloader_failed("Unable to open codecs file: " + filename);

	std::vector<std::string> result;
	std::string line;

	while (std::getline(is, line)) {
		std::stringstream lineStream(line);
		std::string cell;

		while (std::getline(lineStream, cell, ':')) {
			result.push_back(cell);
		}

		if(!result.size() == 3)
			throw codecloader_failed("Invalid codec file entry: " + line);
		cheesy::Codec c = {result[0], result[1], result[2]};
		cheesy::Codec::setCodec(c.name, c);

		LOG(DEBUG) << "Loaded codec: " << result[0] << ":" << result[1] << ":" << result[2];

		result.clear();
	}

	cheesy::Codec::setCodec(cheesy::EMPTY_CAPS.codec.name, cheesy::EMPTY_CAPS.codec);
}

void configureLogger() {
	el::Configurations defaultConf;
	defaultConf.setToDefault();

	defaultConf.set(el::Level::Info, el::ConfigurationType::Format,
			"%datetime %msg");
	defaultConf.set(el::Level::Fatal, el::ConfigurationType::Format,
			"%datetime %level %loc %msg");
	defaultConf.set(el::Level::Debug, el::ConfigurationType::Format,
			"%datetime %level %loc %msg");

	// default logger uses default configurations
//	el::Loggers::reconfigureLogger("default", defaultConf);

	// To set GLOBAL configurations you may use
	defaultConf.setGlobally(el::ConfigurationType::Format, "%datetime %level %msg");
	el::Loggers::reconfigureLogger("default", defaultConf);
}

static gboolean delete_event( GtkWidget *widget, GdkEvent  *event, gpointer   data ) {
    LOG(INFO) << "Window closed by user: " << (char *)data;
    exit(0);
    return FALSE;
}

bool checkXvExtension() {
	Display *dpy;
	unsigned int ver, rev, eventB, reqB, errorB;

	if (!(dpy = XOpenDisplay(NULL))) {
		LOG(ERROR)<< "Can't open display";
		return false;
	}

	if ((Success != XvQueryExtension(dpy, &ver, &rev, &reqB, &eventB, &errorB))) {
		LOG(INFO)<< "You don't have XVideo extension";
		return false;
	}

    unsigned int nadaptors;
    XvAdaptorInfo *ainfo;
	XvQueryAdaptors(dpy, RootWindow(dpy, 0), &nadaptors, &ainfo);

    if(nadaptors == 0) {
    	LOG(INFO)<< "XVideo disabled";
        return false;

    }

	LOG(INFO)<< "XVideo extension found";

	return true;
}

GtkWidget* makeGtkWindow(bool fullscreen) {
	if(fullscreen)
		LOG(DEBUG) << "Creating fullscreen GTK window";
	else
		LOG(DEBUG) << "Creating GTK window";

	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//explicit cast to suppress warning
	char* cbname = (char*)"delete_event";
	g_signal_connect (G_OBJECT (window), cbname, G_CALLBACK (delete_event), cbname);
	if(fullscreen)
		gtk_window_fullscreen(GTK_WINDOW(window));
	GdkColor color;
	gdk_color_parse("black", &color);
	gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &color);
	gtk_widget_show_all(window);
	return window;
}

namespace cheesy {
	class Cheesy {
		Pipeline* pipeline = NULL;
		RTPPipelineFactory factory;

	public:
		Cheesy() {
		}

		void startDaemon(int daemonPort, bool fullscreen, bool drawIntoRoot, bool disableVideo, bool disableAudio, signed long xid) {
			if(xid == -1) {
				if(!disableVideo) {
					if(!checkXvExtension())
						factory.getServerTemplates().videoSink="ximagesink name=vpsink";

					if(!drawIntoRoot) {
						GtkWidget* gtkWin = makeGtkWindow(fullscreen);
						xid = GDK_WINDOW_XWINDOW(gtkWin->window);
					} else {
						Display* dis = XOpenDisplay(NULL);
						xid = RootWindow(dis,0);
					}
				}
			}

			CapsServer server(daemonPort);
			while (true) {
				LOG(INFO) << "Waiting for incoming connection";
				ClientInfo ci = server.accept(disableVideo, disableAudio);
				LOG(INFO) << "Accepted connection: " << ci.peerAddress;

				if (pipeline != NULL && pipeline->isRunning()) {
					pipeline->stop();
					delete pipeline;
				}

				pipeline = factory.createServerPipeline(daemonPort, ci);

				if(!disableVideo)
					pipeline->setXwindowID(xid);
				pipeline->play(false);
			}
		}

		void startClient(string host, int daemonPort, string videoCodecName, string audioCodecName, string monitorSource, bool showPointer, signed long xid) {
			//setting the xid after the pipeline ist created doesn't work so just inject it into the client templates
			if(xid != -1)
				factory.getClientTemplates().videoSource = "ximagesrc name=vpsrc xid=" + std::to_string(xid);

			Pipeline* pipeline = factory.createClientPipeline(monitorSource, host, daemonPort, Codec::getCodec(videoCodecName), Codec::getCodec(audioCodecName) ,showPointer);

			pipeline->play();
			std::string videoRtpCaps = EMPTY_CAPS.rtpCaps;
			std::string audioRtpCaps = EMPTY_CAPS.rtpCaps;
			bool disableVideo = videoCodecName == EMPTY_CAPS.codec.name;
			bool disableAudio = audioCodecName == EMPTY_CAPS.codec.name;
			assert(!(disableVideo && disableAudio));

			if(!disableVideo){
				videoRtpCaps = pipeline->getPadCaps("vpsink","sink");
			}

			if(!disableAudio){
				audioRtpCaps = pipeline->getPadCaps("apsink","sink");
			}

			CapsClient client;
			try {
				client.connect(host, daemonPort);
			} catch(std::exception& ex) {
				LOG(ERROR) << "Can't connect to host: " << ex.what();
				return;
			}

			LOG(DEBUG) << "video codec: " << videoCodecName;
			LOG(DEBUG) << "video rtp-caps: " << videoRtpCaps;
			LOG(DEBUG) << "audio codec: " << audioCodecName;
			LOG(DEBUG) << "audio rtp-caps: " << audioRtpCaps;

			ServerInfo sinfo = client.announce({ videoRtpCaps, Codec::getCodec(videoCodecName)}, { audioRtpCaps, Codec::getCodec(audioCodecName)});

			bool reconnect = false;
			if(!disableVideo && !sinfo.hasVideo) {
				LOG(INFO) << "Server doesn't support video";
				videoCodecName = EMPTY_CAPS.codec.name;
				reconnect = true;
			}

			if(!disableAudio && !sinfo.hasAudio) {
				LOG(INFO) << "Server doesn't support audio";
				audioCodecName = EMPTY_CAPS.codec.name;
				reconnect = true;
			}

			if(reconnect) {
				LOG(INFO) << "Reconnecting";
				client.close();
				pipeline->stop();
				startClient(host, daemonPort, videoCodecName, audioCodecName, monitorSource, showPointer, xid);
				return;
			}

			try {
				string response = client.join();
				if(response == "kicked") {
					LOG(INFO) << "You were kicked.";
				} else
					LOG(INFO) << "Server: " + response;

			} catch(std::exception& ex) {
				LOG(ERROR) << "Connection error: " << ex.what();
			}

			pipeline->stop();
		}
	};
}

int main(int argc, char *argv[]) {
	using std::string;
	gst_init(0, NULL);

	_START_EASYLOGGINGPP(argc, argv);
	configureLogger();
	int port;
	int monitorSourceIndex;
	std::string codecsFile;
	std::string videoCodecName;
	std::string audioCodecName = "OPUS";
	signed long xid = -1;

	po::options_description genericDesc("Options for both daemon and client");
	genericDesc.add_options()
		("help,h", "Produce help message")
		("verbose,v", "Enable verbose output")
		("port,p", po::value<int>(&port)->default_value(11111), "Port number to use")
		("disable-audio,a", "Disable audio")
		("disable-video,t", "Disable video")
    	("codecs-file,f", po::value<string>(&codecsFile)->default_value("/etc/cheesy/codecs"), "Use given codecs file")
		("xid,x", po::value<signed long>(&xid), "Capture from this window in client mode. If -d is supplied draw into this window.");

    po::options_description clientDesc("Client options");
    clientDesc.add_options()
		("show-pointer,y", "Show the mouse pointer")
		("video-codec,u", po::value<string>(&videoCodecName)->default_value("MPEG4_HIGH"), "Use given video codec profile")
		//("audio-codec,s", po::value<string>(&audioCodecName)->default_value("OPUS"), "use given audio codec profile")
		("monitor,m", po::value<int>(&monitorSourceIndex)->default_value(0), "Use given pulse monitor source index");

    po::options_description daemonDesc("Daemon options");
    daemonDesc.add_options()
		("daemon,d", "Run as a daemon")
		("fullscreen,w", "Run in fullscreen mode")
		("root,r", "Draw into the root window");

    po::options_description hidden("Hidden options");
    hidden.add_options()
    	("ip-address", po::value< string >(), "The target ip-address");

    po::positional_options_description p;
    p.add("ip-address", -1);

    po::options_description cmdline_options;
    cmdline_options.add(genericDesc).add(clientDesc).add(daemonDesc).add(hidden);

    po::options_description visible;
    visible.add(genericDesc).add(clientDesc).add(daemonDesc);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cerr << "Usage: cheesy [options] [ip-address]\n";
        std::cerr << visible;
        return 0;
    }

    using namespace cheesy;

	loadCodecs(codecsFile);

	if (vm.count("verbose")) {
		gst_debug_set_default_threshold(GST_LEVEL_TRACE);
	}
	else {
		gst_debug_set_default_threshold(GST_LEVEL_NONE);
	}

	Cheesy cheesy;

	if(vm.count("disable-video") && vm.count("disable-audio")) {
		std::cerr << "disabling video and audio at the same time doesn't do anything" << std::endl;
	}

	if(vm.count("disable-video"))
		videoCodecName = EMPTY_CAPS.codec.name;
	else
		gtk_init(0, NULL);

	if(vm.count("disable-audio"))
		audioCodecName = EMPTY_CAPS.codec.name;

	if (vm.count("daemon")) {
		cheesy.startDaemon(port, vm.count("fullscreen"), vm.count("root"), vm.count("disable-video"), vm.count("disable-audio"), xid);
	} else if (vm.count("ip-address")) {
		std::vector<string> monitors;

		if(vm.count("disable-audio")) {
			monitors.push_back("none");
			monitorSourceIndex = 0;
		}
		else
			monitors = getPulseMonitorSource();

		if (monitors.size() > monitorSourceIndex) {
			string host =  vm["ip-address"].as< string >();
			string monitorSource =  monitors[monitorSourceIndex];
			LOG(INFO) << "Selected monitor source: " << monitorSource;

			cheesy.startClient(host, port, videoCodecName, audioCodecName, monitorSource, vm.count("show-pointer"), xid);
		} else {
			LOG(FATAL) << "Monitor source not found";
			return 1;
		}
		return 0;
	} else {
        std::cerr << "Usage: cheesy [options] [ip-address]\n";
        std::cerr << visible;
        return 0;
	}
}
