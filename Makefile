
CXX	    = g++

HEADERS	    = $(wildcard *.h)
CPP	    = $(wildcard *.cpp)
OBJ	    = $(patsubst %.cpp,%.o,$(CPP))
CXXFLAGS = -g -Wno-write-strings -Wno-deprecated
LIB     = -lcurses

ifneq ($(wildcard /opt/local/lib/libusb-legacy/libusb-legacy.dylib),)
  # the legacy USB library is for compiling on OS X Lion
  # (installed with "sudo port install libusb-legacy")
  LIB += -L/opt/local/lib/libusb-legacy -lusb-legacy
else
  LIB += -lusb
endif


all: manip

clean:
	rm *.o

manip: $(OBJ)
	$(CXX) -o manip $(OBJ) $(LIB)

%.o:	%.cpp
	$(CXX) $(OPT) $(CXXFLAGS) -o $@ -c $<

