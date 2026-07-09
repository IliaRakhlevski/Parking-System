CC = gcc

CFLAGS = -Wall -Wextra -Werror -g -IInc
LDFLAGS = -pthread

TARGET = logger_test

SRC = Src/logger.c Test/main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
