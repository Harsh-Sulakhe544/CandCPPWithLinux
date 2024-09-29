# Makefile for compiling the server application

# Compiler and flags
CXX = g++
CXXFLAGS = -w -Wall -Wextra -O2 -g

# Source files  
SRCS = server.cpp avl.cpp hashtable.cpp heap.cpp thread_pool.cpp zset.cpp 

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = server

# Default target
all: $(TARGET)

# Rule to link object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread  # , link the pthread-library

# Rule to compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean

