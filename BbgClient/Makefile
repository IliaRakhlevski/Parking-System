TARGET = bbg_client

CC = gcc

INCLUDES = \
	-IInclude \


CFLAGS = \
	-Wall \
	-Wextra \
	-Wpedantic \
	-std=c11 \
	$(INCLUDES)

LDFLAGS =

SRC = \
	Test/main.c \
	Src/bbg_client.c

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)
	rm -f $(TARGET)

.PHONY: all clean
