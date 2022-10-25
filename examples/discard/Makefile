CXX = g++
CXXFLAGS = -lpthread -lmymuduo -std=c++11 -g

SRC = discard.cc main.cc
OBJ = $(SRC:.cc=.o)
EXEC = DiscardServer

all: $(EXEC)

$(EXEC): 
	$(CXX) $(CXXFLAGS) -o $(EXEC) $(SRC) 

clean:
	rm -rf $(OBJ) $(EXEC)