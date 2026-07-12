CXX = g++

CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

INCLUDES = \
	-IInc \
	-I../Libraries/Logger/Inc \
	-I../Libraries/Config/Inc \
	-I../Libraries/SharedMemory/Inc \
	-I../Libraries/Queue/Inc \
	-I../Libraries/IpcProtocol/Inc \
	-I../Libraries/SQLiteDatabase/Inc

SOURCES = \
	Src/database.cpp \
	Test/main.cpp \
	../Libraries/SharedMemory/Src/shared_memory.cpp \
	../Libraries/Queue/Src/shared_queue.cpp \
	../Libraries/Logger/Src/logger.c \
	../Libraries/Config/Src/config.c \
	../Libraries/SQLiteDatabase/Src/sqlite_database.c

LIBS = \
	-lsqlite3

TARGET = database_test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
