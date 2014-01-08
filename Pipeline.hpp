/*
 * Pipeline.hpp
 *
 *  Created on: Jan 4, 2014
 *      Author: elchaschab
 */

#ifndef PIPELINE_HPP_
#define PIPELINE_HPP_

#include <gst/gst.h>
#include <string>
#include <exception>
#include <boost/thread.hpp>
#include <boost/exception/all.hpp>
#include "easylogging++.h"

namespace cheesy {

using std::string;

class pipeline_failed :
  public boost::exception,
  public std::exception
{
public:
	pipeline_failed(string message): what_(message)
  {
  }

  virtual const char *what() const throw()
  {
    return what_.c_str();
  }

private:
  std::string what_;
};

class Pipeline {
	GstElement* pipeline;
	boost::thread msgThread;
	GstBus* bus;
	string strPipeline;
public:
	Pipeline(string strPipeline) : pipeline(NULL), bus(NULL), strPipeline(strPipeline){
		GError *error = NULL;
		this->pipeline = gst_parse_launch(strPipeline.c_str(), &error);
		if(pipeline == NULL) {
			throw pipeline_failed(error->message);
		}
	}

	string getPadCaps(string elementName, string padName) {
		GstElement *e = gst_bin_get_by_name(GST_BIN(pipeline), elementName.c_str());
		GstPad * pad = gst_element_get_static_pad (e, padName.c_str());
		gst_pad_use_fixed_caps (pad);
		GstCaps* caps = gst_pad_get_negotiated_caps (pad);
		return gst_caps_to_string (caps);
	}

	void setPadCaps(string elementName, string padName, string capsString) {
		GstElement *e = gst_bin_get_by_name(GST_BIN(pipeline), elementName.c_str());
		GstPad * pad = gst_element_get_static_pad (e, padName.c_str());
		GstCaps* caps = gst_caps_from_string(capsString.c_str());
		gst_pad_set_caps(pad, caps);
	}

	template<typename T> void get(string elementName, string propertyName, T* value) {
		GstElement *e = gst_bin_get_by_name(GST_BIN(pipeline), elementName.c_str());
		g_object_get(e, "caps", &value, NULL);
	}

	template<typename T> void set(string elementName, string propertyName, T value) {
		GstElement *e = gst_bin_get_by_name(GST_BIN(pipeline), elementName.c_str());
		g_object_set(e, propertyName.c_str(), value, NULL);
	}

	void set(string elementName, string propertyName, string value) {
		GstElement *e = gst_bin_get_by_name(GST_BIN(pipeline), elementName.c_str());
		g_object_set(e, propertyName.c_str(), value.c_str(), NULL);
	}

	bool join() {
		if(msgThread.joinable()) {
			msgThread.join();
			return true;
		} else {
			return false;
		}
	}

	bool isRunning() {
		return msgThread.joinable();
	}

	void stop() {
		LOG(INFO) << "Stopping pipeline";
		gst_bus_post (bus, gst_message_new_eos(NULL));
		this->join();
	}

	void play() {
		GstMessage *msg;
		this->bus = gst_element_get_bus(pipeline);
		gst_element_set_state(pipeline, GST_STATE_PLAYING);

		// wait for the pipeline to preroll
		msg = gst_bus_poll(bus, GST_MESSAGE_ASYNC_DONE, -1);

		LOG(INFO) << "Pipeline playing";

		msgThread = boost::thread([=](){
			GstMessage* m = gst_bus_poll(this->bus, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS), -1);

			GError *err = NULL;
			gchar *dbg = NULL;


			switch (GST_MESSAGE_TYPE(m)) {
			case GST_MESSAGE_EOS:
				break;

			case GST_MESSAGE_ERROR:
				gst_message_parse_error(m, &err, &dbg);
				if (err) {
					LOG(ERROR) << "Pipeline error: " << err->message;
					g_error_free(err);
				}
				if (dbg) {
					LOG(DEBUG) << "Pipeline debug details: " << dbg;
					g_free(dbg);
				}

				break;
			}

			gst_message_unref(m);
			gst_element_set_state(pipeline, GST_STATE_NULL);
			gst_object_unref(pipeline);
			gst_object_unref(bus);
			LOG(INFO) << "Pipeline stopped";
		});

	}
};


} /* namespace cheesy */

#endif /* PIPELINE_HPP_ */
