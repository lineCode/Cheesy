CXX     := g++
TARGET  := cheesy
SRCS    := cheesy.cpp PulseMonitorSource.cpp CapsServer.cpp CapsClient.cpp Caps.cpp
OBJS    := ${SRCS:.cpp=.o} 
DEPS    := ${SRCS:.cpp=.dep} 
PREFIX  := /usr/bin/
CXXFLAGS = -pedantic -std=c++11 `pkg-config --cflags libpulse gtk+-2.0 gstreamer-interfaces-0.10 gstreamer-0.10` -D_ELPP_THREAD_SAFE
LDFLAGS = 
LIBS    =  -lboost_system `pkg-config --libs libpulse gtk+-2.0 gstreamer-interfaces-0.10 gstreamer-0.10`

.PHONY: all clean

ifeq ($(UNAME), Darwin)
 CXXFLAGS +=  -stdlib=libc++
endif

all: release

debug: CXXFLAGS+=-g -O0 -D_ELPP_STACKTRACE_ON_CRASH
debug: ${TARGET}

build: release

release: CXXFLAGS+=-D_ELPP_DISABLE_DEBUG_LOGS -D_ELPP_DISABLE_LOGGING_FLAGS_FROM_ARG -D_ELPP_DISABLE_DEFAULT_CRASH_HANDLING
release: ${TARGET}

${TARGET}: ${OBJS} 
	${CXX} ${LDFLAGS} -o $@ $^ ${LIBS} 

${OBJS}: %.o: %.cpp %.dep 
	${CXX} ${CXXFLAGS} -o $@ -c $< 

${DEPS}: %.dep: %.cpp Makefile 
	${CXX} ${CXXFLAGS} -MM $< > $@ 

install: ${TARGET}
install:
	mkdir -p ${DESTDIR}/${PREFIX}
	cp ${TARGET} ${DESTDIR}/${PREFIX}
	mkdir -p  ${DESTDIR}/etc/cheesy/
	cp codecs ${DESTDIR}/etc/cheesy/

clean:
	rm -f *~ *.dep *.o ${TARGET} 

distclean: clean

