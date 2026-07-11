CC = gcc

CFLAGS = -Wall -Wextra -std=c17 -pthread

INCLUDES = \
	-IInc \
	-I../Logger/Inc

SOURCES = \
	Src/sqlite_database.c \
	Test/main.c \
	../Logger/Src/logger.c

LIBS = \
	-lsqlite3

TARGET = sqlite_database_test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean

