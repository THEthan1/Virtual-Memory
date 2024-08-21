CC := g++
CFLAGS := -Wall -std=c++11 -pthread -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -pthread -g $(INCS)
SRC := VirtualMemory.cpp PhisicalMemory.cpp
OBJ := $(SRC:.cpp=.o)
LIB := libVirtualMemory.a

PHONY: all clean

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ): VirtualMemory.h PhisicalMemory.h MemoryConstants.h

clean:
	rm -f $(OBJ) $(LIB)
