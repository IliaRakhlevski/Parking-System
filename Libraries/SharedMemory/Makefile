CXX = g++

CXXFLAGS = -Wall -Wextra -Werror -g \
           -IInc \
           -I../Logger/Inc

LDFLAGS = -pthread

TARGET = shared_memory_test

SRC = Src/shared_memory.cpp \
      Test/main.cpp \
      ../Logger/Src/logger.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean