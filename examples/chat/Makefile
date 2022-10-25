CXX = g++
CXXFLAGS = -lpthread -std=c++11 -g 


SRC = $(wildcard *.cc)
OBJ = $(SRC:.cc=.o)
EXEC := $(patsubst %.cc,%,$(SREC))

all:
	$(CXX) $(CXXFLAGS) -o server server.cc -I../../include
	$(CXX) $(CXXFLAGS) -o client client.cc

clean:
	rm -rf $(OBJ) $(EXEC)