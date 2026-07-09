CXX = g++

CXXFLAGS = -Wall -Wextra -Werror -g \
           -IInc \
           -I../Logger/Inc \
           -I../SharedMemory/Inc

LDFLAGS = -pthread

TARGET = shared_queue_test

SRC = Src/shared_queue.cpp \
      Test/main.cpp \
      ../SharedMemory/Src/shared_memory.cpp \
      ../Logger/Src/logger.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
