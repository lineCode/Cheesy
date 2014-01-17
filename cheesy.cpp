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
#include "easylogging++.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>
#include <boost/program_options.hpp>

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

		void startDaemon(int daemonPort, bool fullscreen, bool drawIntoRoot, bool disableVideo, bool disableSound) {
			gulong windowID;

			if(!disableVideo) {
				if(!checkXvExtension())
					factory.getServerTemplates().videoSink="ximagesink name=vpsink";

				if(!drawIntoRoot) {
					GtkWidget* gtkWin = makeGtkWindow(fullscreen);
					windowID = GDK_WINDOW_XWINDOW(gtkWin->window);
				} else {
					Display* dis = XOpenDisplay(NULL);
					windowID = RootWindow(dis,0);
				}
			}
			CapsServer server(daemonPort);

			while (true) {
				LOG(INFO) << "Waiting for incoming connection";
				ClientInfo ci = server.accept(disableVideo, disableSound);
				LOG(INFO) << "Accepted connection: " << ci.peerAddress;

				if (pipeline != NULL && pipeline->isRunning()) {
					pipeline->stop();
					delete pipeline;
				}

				pipeline = factory.createServerPipeline(daemonPort, ci);

				if(!disableVideo)
					pipeline->setXwindowID(windowID);
				pipeline->play(false);
			}
		}

		void startClient(string host, int daemonPort, string videoCodecName, string soundCodecName, string monitorSource, bool showPointer, signed long xid) {
			//setting the xid after the pipeline ist created doesn't work so just inject it into the client templates
			if(xid != -1)
				factory.getClientTemplates().videoSource = "ximagesrc name=vpsrc xid=" + std::to_string(xid);

			Pipeline* pipeline = factory.createClientPipeline(monitorSource, host, daemonPort, Codec::getCodec(videoCodecName), Codec::getCodec(soundCodecName) ,showPointer);

			pipeline->play();
			std::string videoRtpCaps = EMPTY_CAPS.rtpCaps;
			std::string soundRtpCaps = EMPTY_CAPS.rtpCaps;
			bool disableVideo = videoCodecName == EMPTY_CAPS.codec.name;
			bool disableSound = soundCodecName == EMPTY_CAPS.codec.name;
			assert(!(disableVideo && disableSound));

			if(!disableVideo){
				videoRtpCaps = pipeline->getPadCaps("vpsink","sink");
			}

			if(!disableSound){
				soundRtpCaps = pipeline->getPadCaps("apsink","sink");
			}

			CapsClient client;
			try {
				client.connect(host, daemonPort);
				LOG(DEBUG) << "video codec: " << videoCodecName;
				LOG(DEBUG) << "video rtp-caps: " << videoRtpCaps;
				LOG(DEBUG) << "sound codec: " << soundCodecName;
				LOG(DEBUG) << "sound rtp-caps: " << soundRtpCaps;

				ServerInfo sopt = client.announce({ videoRtpCaps, Codec::getCodec(videoCodecName)}, { soundRtpCaps, Codec::getCodec(soundCodecName)});

				bool reconnect = false;
				if(!disableVideo && !sopt.hasVideo) {
					LOG(INFO) << "Server doesn't support video";
					videoCodecName = EMPTY_CAPS.codec.name;
					reconnect = true;
				}

				if(!disableSound && !sopt.hasSound) {
					LOG(INFO) << "Server doesn't support sound";
					soundCodecName = EMPTY_CAPS.codec.name;
					reconnect = true;
				}

				if(reconnect) {
					LOG(INFO) << "Reconnecting";
					client.close();
					pipeline->stop();
					startClient(host, daemonPort, videoCodecName, soundCodecName, monitorSource, showPointer, xid);
				}
			} catch(std::exception& ex) {
				LOG(ERROR) << "Can't connect to host: " << ex.what();
			}

			try {
				client.join();
			} catch(std::exception& ex) {
				LOG(INFO) << "You were kicked.";
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

	po::options_description genericDesc("Generic options");
	genericDesc.add_options()
		("help,h", "produce help message")
		("verbose,v", "enable verbose output");

    po::options_description bothDesc("Options for both daemon and client");
    bothDesc.add_options()
        ("port", po::value<int>(&port)->default_value(11111), "port number to use")
        ("disable-sound,s", "disable sound")
		("disable-video,p", "disable video")
    	("codecs-file,f", po::value<string>(&codecsFile)->default_value("/etc/cheesy/codecs"), "use given codecs file");

    po::options_description clientDesc("Client options");
    clientDesc.add_options()
		("show-pointer,y", "show the mouse pointer")
		("xid,x", po::value<signed long>(&xid), "capture from this window")
		("video-codec,n", po::value<string>(&videoCodecName)->default_value("MPEG4_HIGH"), "use given video codec profile")
		//("audio-codec,a", po::value<string>(&audioCodecName)->default_value("OPUS"), "use given audio codec profile")
		("monitor,m", po::value<int>(&monitorSourceIndex)->default_value(0), "use given pulse monitor source index");

    po::options_description daemonDesc("Daemon options");
    daemonDesc.add_options()
		("daemon,d", "run as a daemon")
		("fullscreen,w", "run in fullscreen mode")
		("root,r", "draw into the root window");

    po::options_description hidden("Hidden options");
    hidden.add_options()
    	("ip-address", po::value< string >(), "The target ip-address");

    po::positional_options_description p;
    p.add("ip-address", -1);


    po::options_description cmdline_options;
    cmdline_options.add(genericDesc).add(bothDesc).add(clientDesc).add(daemonDesc).add(hidden);

    po::options_description visible("Allowed options");
    visible.add(genericDesc).add(bothDesc).add(clientDesc).add(daemonDesc);

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

	if(vm.count("disable-video") && vm.count("disable-sound")) {
		std::cerr << "disabling video and audio at the same time doesn't do anything" << std::endl;
	}

	if(vm.count("disable-video"))
		videoCodecName = EMPTY_CAPS.codec.name;
	else
		gtk_init(0, NULL);

	if(vm.count("disable-sound"))
		audioCodecName = EMPTY_CAPS.codec.name;

	if (vm.count("daemon")) {
		cheesy.startDaemon(port, vm.count("fullscreen"), vm.count("root"), vm.count("disable-video"), vm.count("disable-sound"));
	} else if (vm.count("ip-address")) {
		std::vector<string> monitors;

		if(vm.count("disable-sound")) {
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
