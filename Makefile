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
TEST_INVESTIGATION_RECORD = tests/test_investigation_record
TEST_INVESTIGATION_DAO := tests/test_investigation_dao
TEST_INVESTIGATION_SESSION := tests/test_investigation_session
TEST_BACKGROUND_TASK := tests/test_background_task
TEST_TASK_MANAGER := tests/test_task_manager
TEST_TOOL_REGISTRY := tests/test_tool_registry
TEST_TOOL_PROCESS := tests/test_tool_process
TEST_TOOL_TASK := tests/test_tool_task

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
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_DATABASE): \
	tests/test_database.c \
	src/database/database.c \
	src/database/transaction.c \
	src/database/statement.c \
	src/database/schema.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_STATEMENT): \
	tests/test_statement.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/transaction.c \
	src/database/statement.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_TRANSACTION): \
	tests/test_transaction.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_ERROR): \
	tests/test_error.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_INVESTIGATION_RECORD): \
	tests/test_investigation_record.c \
	src/models/investigation_record.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_INVESTIGATION_DAO): \
	tests/test_investigation_dao.c \
	src/dao/investigation_dao.c \
	src/models/investigation_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_INVESTIGATION_SESSION): \
	tests/test_investigation_session.c \
	src/core/investigation_session.c \
	src/core/investigation_project.c \
	src/dao/investigation_dao.c \
	src/models/investigation_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_BACKGROUND_TASK): \
	tests/test_background_task.c \
	src/core/background_task.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_TASK_MANAGER): \
	tests/test_task_manager.c \
	src/core/task_manager.c \
	src/core/background_task.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_TOOL_REGISTRY): \
	tests/test_tool_registry.c \
	src/core/tool_registry.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_TOOL_PROCESS): \
	tests/test_tool_process.c \
	src/core/tool_process.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_TOOL_TASK): \
	tests/test_tool_task.c \
	src/core/tool_task.c \
	src/core/tool_registry.c \
	src/core/tool_process.c \
	src/core/background_task.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

test: \
	$(TEST_NODE) \
	$(TEST_TREE_MODEL) \
	$(TEST_TREE_BUILDER) \
	$(TEST_PROJECT) \
	$(TEST_DATABASE) \
	$(TEST_STATEMENT) \
	$(TEST_TRANSACTION) \
	$(TEST_ERROR) \
	$(TEST_INVESTIGATION_RECORD) \
	$(TEST_INVESTIGATION_DAO) \
	$(TEST_INVESTIGATION_SESSION) \
	$(TEST_BACKGROUND_TASK) \
	$(TEST_TASK_MANAGER) \
	$(TEST_TOOL_REGISTRY) \
	$(TEST_TOOL_PROCESS) \
	$(TEST_TOOL_TASK)
	@echo "Exécution des tests..."
	@./$(TEST_NODE)
	@./$(TEST_TREE_MODEL)
	@./$(TEST_TREE_BUILDER)
	@./$(TEST_PROJECT)
	@./$(TEST_DATABASE)
	@$(TEST_STATEMENT)
	@$(TEST_TRANSACTION)
	@$(TEST_ERROR)
	@$(TEST_INVESTIGATION_RECORD)
	@$(TEST_INVESTIGATION_DAO)
	@$(TEST_INVESTIGATION_SESSION)
	@$(TEST_BACKGROUND_TASK)
	@$(TEST_TASK_MANAGER)
	@$(TEST_TOOL_REGISTRY)
	@$(TEST_TOOL_PROCESS)
	@$(TEST_TOOL_TASK)
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
		$(TEST_ERROR) \
		$(TEST_INVESTIGATION_RECORD) \
		$(TEST_INVESTIGATION_DAO) \
		$(TEST_INVESTIGATION_SESSION) \
		$(TEST_BACKGROUND_TASK) \
		$(TEST_TASK_MANAGER) \
		$(TEST_TOOL_REGISTRY) \
		$(TEST_TOOL_PROCESS) \
		$(TEST_TOOL_TASK)

.PHONY: clean run test
