CXX = g++
CXXFLAGS = -lpthread -lmymuduo -std=c++11 -g

SRC = daytime.cc main.cc
OBJ = $(SRC:.cc=.o)
EXEC = DaytimeServer

all: $(EXEC)

$(EXEC): 
	$(CXX) $(CXXFLAGS) -o $(EXEC) $(SRC) 

clean:
	rm -rf $(OBJ) $(EXEC)