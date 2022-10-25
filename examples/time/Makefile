CXX = g++
CXXFLAGS = -lpthread -lmymuduo -std=c++11 -g 


SRC = $(wildcard *.cc)
OBJ = $(SRC:.cc=.o)
EXEC := $(patsubst %.cc,%,$(SREC))

all:
	$(CXX) $(CXXFLAGS) -o timeserver timeserver.cc 
	$(CXX) $(CXXFLAGS) -o timeclient timeclient.cc

clean:
	rm -rf $(OBJ) $(EXEC)