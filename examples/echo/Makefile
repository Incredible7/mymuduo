CXX = g++
CXXFLAGS = -lpthread -lmymuduo -std=c++11 -g

SRC = echo.cc main.cc
OBJ = $(SRC:.cc=.o)
EXEC = EchoServer

all: $(EXEC)

$(EXEC): 
	$(CXX) $(CXXFLAGS) -o $(EXEC) $(SRC) 

clean:
	rm -rf $(OBJ) $(EXEC)