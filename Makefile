CXX = g++
CXXFLAGS = -O3 -Wall -std=c++17

all: sender receiver

sender: sender.cpp proto.h
	$(CXX) $(CXXFLAGS) -o sender sender.cpp

receiver: receiver.cpp proto.h
	$(CXX) $(CXXFLAGS) -o receiver receiver.cpp

clean:
	rm -f sender receiver
