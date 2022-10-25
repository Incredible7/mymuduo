CXX = g++
CXXFLAGS = -lpthread -lmymuduo -std=c++11 -g 


SRC = $(wildcard *.cc)
OBJ = $(SRC:.cc=.o)
EXEC := $(patsubst %.cc,%,$(SREC))

all:
	$(CXX) $(CXXFLAGS) -o chargenserver main.cc chargen.cc -I./
	$(CXX) $(CXXFLAGS) -o chargenclient chargenclient.cc

clean:
	rm -rf $(OBJ) $(EXEC)