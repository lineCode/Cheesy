#include "PulseMonitorSource.hpp"
#include "RTPPipelineFactory.hpp"
#include "CapsClient.hpp"
#include "CapsServer.hpp"
#include "Caps.hpp"
#include <iostream>
#include <getopt.h>
#include <gst/gst.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "easylogging++.h"

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
}

void printUsage() {
	std::cerr << "Usage: cheesy [-d][-v][-p <daemonPort>][-m <index>] <target>..." << std::endl;
	std::cerr << "Options:" << std::endl;
	std::cerr << "\t-d\t\tstart the cheesy daemon" << std::endl;
	std::cerr << "\t-v\t\tgenerate verbose output" << std::endl;
	std::cerr << "\t-p <port>\tport to listen/connect to" << std::endl;
	std::cerr << "\t-m <index>\tindex of the monitor source" << std::endl;
	std::cerr << "\t-y \tshow mouse pointer" << std::endl;
	std::cerr << "\t-w \tenable fullscreen mode" << std::endl;
	std::cerr << "\t-f <codecsFile>\tuse given codecs file" << std::endl;
	std::cerr << "\t-c <codecsName>\tuse given codec profile" << std::endl;
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

static void make_window_black(GtkWidget *window)
{
    GdkColor color;
    gdk_color_parse("black", &color);
    gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &color);
}

int main(int argc, char *argv[]) {
	gst_init(0, NULL);
	gtk_init(0, NULL);

	_START_EASYLOGGINGPP(argc, argv);
	configureLogger();
	int c;
	int daemonPort = 1111;
	int monitorSourceIndex = 0;
	bool verbose = false;
	bool daemon = false;
	std::string codecsFile = "/etc/cheesy/codecs";
	std::string codecName = "MPEG4_HIGH";
	bool showPointer = false;
	bool fullscreen = false;

	while ((c = getopt(argc, argv, "wydvh?p:m:f:c:")) != -1) {
		switch (c) {
		case 'w':
			fullscreen = true;
			break;
		case 'd':
			daemon = true;
			break;
		case 'y':
			showPointer = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'p':
			daemonPort = atoi(optarg);
			break;
		case 'f':
			codecsFile = optarg;
			break;
		case 'c':
			codecName = optarg;
			break;
		case 'm':
			monitorSourceIndex = atoi(optarg);
			break;
		case 'h':
			printUsage();
			return 1;
			break;
		case ':':
			printUsage();
			return 1;
			break;
		case '?':
			printUsage();
			return 1;
			break;
		}
	}
	using namespace cheesy;

	loadCodecs(codecsFile);

	if (verbose) {
		gst_debug_set_default_threshold(GST_LEVEL_TRACE);
	}
	else {
		gst_debug_set_default_threshold(GST_LEVEL_NONE);
	}

	Pipeline* pipeline = NULL;
	RTPPipelineFactory factory;

	if (daemon) {
		CapsServer server(daemonPort);
		GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		if(fullscreen)
			gtk_window_fullscreen(GTK_WINDOW(window));
		make_window_black(window);
		gtk_widget_show_all(window);

		while (true) {
			LOG(INFO) << "Waiting for incoming connection";
			ConnectionInfo ci = server.accept();
			LOG(INFO) << "Accepted connection: " << ci.peerAddress;

			if (pipeline != NULL && pipeline->isRunning()) {
				pipeline->stop();
				delete pipeline;
			}

			pipeline = factory.createServerPipeline(daemonPort, ci);
			pipeline->setXVtarget(window);
			pipeline->play();
		}
	} else if ((argc - optind) == 1) {
		auto monitors = getPulseMonitorSource();

		if (monitors.size() > monitorSourceIndex) {
			string host =  argv[optind];
			LOG(INFO) << "Selected monitor source: " << monitors[monitorSourceIndex];

			cheesy::RTPPipelineFactory f;
			Pipeline* pipeline = f.createClientPipeline(monitors[monitorSourceIndex], host, daemonPort, Codec::getCodec(codecName),showPointer);
			pipeline->play();

			std::string rtpCaps = pipeline->getPadCaps("vpsink","sink");
			CapsClient client;
			try {
				client.connect(host, daemonPort);
				LOG(DEBUG) << "Announcing caps. Codec: " << codecName << " rtp-caps: " << rtpCaps;
				client.announce({ rtpCaps, Codec::getCodec(codecName)});
			} catch(std::exception& ex) {
				LOG(ERROR) << "Can't connect to host: " << ex.what();
				return 1;
			}

			//pipeline->join();
			try {
				client.join();
			} catch(std::exception& ex) {
				LOG(INFO) << "You were kicked.";
			}

			pipeline->stop();
			return 0;
		} else {
			LOG(FATAL) << "Monitor source not found";
		}
	}
}
