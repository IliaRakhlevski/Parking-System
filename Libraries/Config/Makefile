CC = gcc

CFLAGS = -Wall -Wextra -Werror -g \
         -IInc \
         -I../Logger/Inc

LDFLAGS = -pthread

TARGET = config_test

SRC = Src/config.c \
      Test/main.c \
      ../Logger/Src/logger.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
