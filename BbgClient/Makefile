TARGET = bbg_client

CC = gcc

INCLUDES = \
	-IInclude \
	-I../Libraries/Config/Inc \
	-I../Libraries/Logger/Inc \
	-I../Libraries/IpcProtocol/Inc

CFLAGS = \
	-Wall \
	-Wextra \
	-Wpedantic \
	-std=c11 \
	$(INCLUDES)

LDFLAGS = \
	-pthread

SRC = \
	Test/main.c \
	Src/bbg_client.c \
	../Libraries/Config/Src/config.c \
	../Libraries/Logger/Src/logger.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)
	rm -f $(TARGET)
	find . -type f -name "*.o" -delete

.PHONY: all clean
