# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -Wall -Werror

# Include directories
INCLUDES = -Iinclude

# Source files
SOURCES = tests/main.cpp src/algorithm.cpp src/io.cpp src/serialization.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Executable name
EXECUTABLE = totalorder

# Default rule to build the executable
all: $(EXECUTABLE)

# Rule to compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Rule to link object files into the executable
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

# Clean rule to remove object files and the executable
clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
