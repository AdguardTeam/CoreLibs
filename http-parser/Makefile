CFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
CFLAGS += $(LOCAL_CFLAGS)

CXXFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
CXXFLAGS += $(LOCAL_CXXFLAGS)

CXXFLAGS+=-fPIC -Wall -g3 -ggdb
CFLAGS+=-fPIC -Wall -g3 -ggdb

OBJS = src/parser.o src/nodejs_http_parser/http_parser.o

all: libhttpparser-c.a

libhttpparser-c.a: $(OBJS)
	$(AR) rcs $@ $^

clean:
	$(RM) libhttpparser-c.a $(OBJS)
