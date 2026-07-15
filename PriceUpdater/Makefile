CC := gcc

TARGET := price_updater

CFLAGS := \
	-std=c11 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-g

CPPFLAGS := \
	-IInc \
	-I../Libraries/Config/Inc \
	-I../Libraries/Logger/Inc \
	-I../Libraries/SQLiteDatabase/Inc

LDLIBS := \
	-lsqlite3 \
	-pthread

SOURCE_FILES := \
	Test/main.c \
	../Libraries/Config/Src/config.c \
	../Libraries/Logger/Src/logger.c \
	../Libraries/SQLiteDatabase/Src/sqlite_database.c

OBJECT_DIR := Build

OBJECT_FILES := \
	$(OBJECT_DIR)/main.o \
	$(OBJECT_DIR)/config.o \
	$(OBJECT_DIR)/logger.o \
	$(OBJECT_DIR)/sqlite_database.o

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECT_FILES)
	$(CC) $(OBJECT_FILES) -o $@ $(LDLIBS)

$(OBJECT_DIR)/main.o: Test/main.c
	@mkdir -p $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJECT_DIR)/config.o: ../Libraries/Config/Src/config.c
	@mkdir -p $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJECT_DIR)/logger.o: ../Libraries/Logger/Src/logger.c
	@mkdir -p $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJECT_DIR)/sqlite_database.o: ../Libraries/SQLiteDatabase/Src/sqlite_database.c
	@mkdir -p $(OBJECT_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJECT_DIR) $(TARGET)
