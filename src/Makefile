CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall
TARGET   = ctx

SRCS = main.cpp \
       core/ignore.cpp core/revision.cpp core/scanner.cpp core/delta.cpp core/blob_store.cpp \
       lang/plugin_registry.cpp lang/cpp_plugin.cpp lang/python_plugin.cpp lang/js_plugin.cpp \
       serialize/xml_serializer.cpp serialize/json_serializer.cpp \
       third_party/tinyxml2/tinyxml2.cpp

OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
