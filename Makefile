# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11
# Add any other flags you need, such as optimization flags (-O2), warning flags (-Wall), etc.

# Source files
SRCS = io.cpp main.cpp serialization.cpp algorithm.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable name
EXEC = totalorder

# Build rule
$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

# Rule to compile .cpp files into .o object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean rule
clean:
	rm -f $(EXEC) $(OBJS)

# Phony target to avoid conflicts with filenames
.PHONY: clean
