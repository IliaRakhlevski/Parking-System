CXX = g++

CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

INCLUDES = \
	-IInc \
	-I../Libraries/Logger/Inc \
	-I../Libraries/Config/Inc \
	-I../Libraries/SharedMemory/Inc \
	-I../Libraries/Queue/Inc \
	-I../Libraries/IpcProtocol/Inc

SOURCES = \
	Src/database.cpp \
	Test/main.cpp \
	../Libraries/SharedMemory/Src/shared_memory.cpp \
	../Libraries/Queue/Src/shared_queue.cpp \
	../Libraries/Logger/Src/logger.c \
	../Libraries/Config/Src/config.c

TARGET = database_test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
