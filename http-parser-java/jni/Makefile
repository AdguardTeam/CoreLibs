JAVA_HOME = $(shell sh -c 'echo $${JAVA_HOME:-$$(dirname $$(dirname $$(readlink -f $$(which javac))))}')

CFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
CFLAGS += $(LOCAL_CFLAGS)

CXXFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
CXXFLAGS += $(LOCAL_CXXFLAGS)

CXXFLAGS+=-fPIC -Wall -g3 -ggdb
CFLAGS+=-fPIC -Wall -g3 -ggdb

OBJS = parser.o ../../http-parser/src/parser.o ../../http-parser/src/nodejs_http_parser/http_parser.o

all: libhttpparser.so

libhttpparser.so: $(OBJS)
	$(CXX) $(LDFLAGS) -shared -o libhttpparser.so $(OBJS)

clean:
	$(RM) libhttpparser.so $(OBJS)