CC = gcc

PKG_CONFIG = pkg-config

CFLAGS = -Wall -Wextra -Wpedantic -Werror -std=c17 -g \
	-Iinclude \
	$(shell $(PKG_CONFIG) --cflags gtk4 sqlite3)

LDFLAGS = $(shell $(PKG_CONFIG) --libs gtk4 sqlite3)

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

TARGET = enquete-gui

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
