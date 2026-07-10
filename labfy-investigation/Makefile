CC = gcc

PKG_CONFIG = pkg-config

CFLAGS = -std=c17 \
          -Wall \
          -Wextra \
          -Werror \
          -g \
          -Iinclude \
          $(shell $(PKG_CONFIG) --cflags gtk4 sqlite3)

LDFLAGS = $(shell $(PKG_CONFIG) --libs gtk4 sqlite3)

SRC := $(shell find src -name "*.c")

OBJ := $(SRC:.c=.o)

TARGET = labfy-investigation

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
