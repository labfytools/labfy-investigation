CC = gcc

PKG_CONFIG = pkg-config

.DEFAULT_GOAL := all

CFLAGS = -std=c17 \
          -Wall \
          -Wextra \
          -Werror \
          -g \
          -Iinclude \
          $(shell $(PKG_CONFIG) --cflags gtk4 sqlite3)

LDFLAGS = $(shell $(PKG_CONFIG) --libs gtk4 sqlite3)

TEST_CFLAGS = -std=c17 \
              -Wall \
              -Wextra \
              -Werror \
              -Iinclude \
              $(shell $(PKG_CONFIG) --cflags glib-2.0 gio-2.0)

TEST_LDFLAGS = $(shell $(PKG_CONFIG) --libs glib-2.0 gio-2.0)

SRC := $(shell find src -name "*.c")

OBJ := $(SRC:.c=.o)

TARGET = labfy-investigation

TEST_NODE = tests/test_investigation_node
TEST_TREE_MODEL = tests/test_investigation_tree_model
TEST_TREE_BUILDER = tests/test_investigation_tree_builder
TEST_PROJECT = tests/test_investigation_project
TEST_DATABASE = tests/test_database
TEST_STATEMENT = tests/test_statement
TEST_TRANSACTION = tests/test_transaction
TEST_ERROR = tests/test_error

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(TEST_NODE): \
	tests/test_investigation_node.c \
	src/core/investigation_node.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_TREE_MODEL): \
	tests/test_investigation_tree_model.c \
	src/core/investigation_node.c \
	src/core/investigation_tree_model.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_TREE_BUILDER): \
	tests/test_investigation_tree_builder.c \
	src/core/investigation_node.c \
	src/core/investigation_tree_model.c \
	src/core/investigation_tree_builder.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) \
		$(shell $(PKG_CONFIG) --libs gio-2.0)

$(TEST_PROJECT): \
	tests/test_investigation_project.c \
	src/core/investigation_project.c \
	src/database/database.c \
	src/database/schema.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_DATABASE): \
	tests/test_database.c \
	src/database/database.c \
	src/database/schema.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_STATEMENT): \
	tests/test_statement.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_TRANSACTION): \
	tests/test_transaction.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_ERROR): \
	tests/test_error.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

test: \
	$(TEST_NODE) \
	$(TEST_TREE_MODEL) \
	$(TEST_TREE_BUILDER) \
	$(TEST_PROJECT) \
	$(TEST_DATABASE) \
	$(TEST_STATEMENT) \
	$(TEST_TRANSACTION) \
	$(TEST_ERROR)
	@echo "Exécution des tests..."
	@./$(TEST_NODE)
	@./$(TEST_TREE_MODEL)
	@./$(TEST_TREE_BUILDER)
	@./$(TEST_PROJECT)
	@./$(TEST_DATABASE)
	@$(TEST_STATEMENT)
	@$(TEST_TRANSACTION)
	@$(TEST_ERROR)
	@echo "Tous les tests sont valides."

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET) \
		$(TEST_NODE) \
		$(TEST_TREE_MODEL) \
		$(TEST_TREE_BUILDER) \
		$(TEST_PROJECT) \
		$(TEST_DATABASE) \
		$(TEST_STATEMENT) \
		$(TEST_TRANSACTION) \
		$(TEST_ERROR)

.PHONY: clean run test
