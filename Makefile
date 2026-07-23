CC = gcc

PKG_CONFIG = pkg-config

.DEFAULT_GOAL := all

CFLAGS = -std=c17 \
          -Wall \
          -Wextra \
          -Werror \
          -g \
          -Iinclude \
          -MMD \
          -MP \
          $(shell $(PKG_CONFIG) --cflags gtk4 sqlite3)

LDFLAGS = $(shell $(PKG_CONFIG) --libs gtk4 sqlite3)

TEST_CFLAGS = -std=c17 \
              -Wall \
              -Wextra \
              -Werror \
              -Iinclude \
              $(shell $(PKG_CONFIG) --cflags glib-2.0 gio-2.0)

TEST_LDFLAGS = $(shell $(PKG_CONFIG) --libs glib-2.0 gio-2.0)

EVIDENCE_RECORD_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
EVIDENCE_TYPE_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
ENTITY_TYPE_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
EVIDENCE_TYPE_DAO_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
GRAPH_NODE_POSITION_DAO_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
EVIDENCE_IMPORT_DIALOG_TEST_CFLAGS := \
	-std=c17 \
	-Wall \
	-Wextra \
	-Werror \
	-Iinclude \
	$(shell $(PKG_CONFIG) --cflags gtk4)

EVIDENCE_IMPORT_DIALOG_TEST_LDFLAGS := $(shell $(PKG_CONFIG) --libs gtk4)

EVIDENCE_INTEGRITY_VERIFIER_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
ENTITY_RECORD_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
ENTITY_TYPE_DAO_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
ENTITY_DAO_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
EVIDENCE_ENTITY_DAO_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
RELATION_RECORD_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
RELATION_DAO_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
RELATION_EVIDENCE_DAO_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
RELATION_SERVICE_TEST_CFLAGS := \
	$(TEST_CFLAGS) \
	-Wpedantic \
	-DRELATION_SERVICE_ENABLE_TEST_HOOKS
INVESTIGATION_GRAPH_MODEL_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
INVESTIGATION_GRAPH_LOADER_TEST_CFLAGS := $(TEST_CFLAGS) -Wpedantic
INVESTIGATION_GRAPH_LOAD_TASK_TEST_CFLAGS := \
	$(TEST_CFLAGS) \
	-Wpedantic \
	-DINVESTIGATION_GRAPH_LOAD_TASK_ENABLE_TEST_HOOKS

SRC := $(shell find src -name "*.c")

OBJ := $(SRC:.c=.o)

DEP := $(OBJ:.o=.d)

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
TEST_EVIDENCE_RECORD := tests/test_evidence_record
TEST_INVESTIGATION_DAO := tests/test_investigation_dao
TEST_EVIDENCE_DAO := tests/test_evidence_dao
TEST_INVESTIGATION_SESSION := tests/test_investigation_session
TEST_BACKGROUND_TASK := tests/test_background_task
TEST_TASK_MANAGER := tests/test_task_manager
TEST_TOOL_REGISTRY := tests/test_tool_registry
TEST_TOOL_PROCESS := tests/test_tool_process
TEST_TOOL_TASK := tests/test_tool_task
TEST_TOOL_CATALOG := tests/test_tool_catalog
TEST_TOOL_INITIALIZER := tests/test_tool_initializer
TEST_FILE_HASH := tests/test_file_hash
TEST_EVIDENCE_INTEGRITY_VERIFIER := tests/test_evidence_integrity_verifier
TEST_EVIDENCE_COPY := tests/test_evidence_copy
TEST_EVIDENCE_IMPORTER := tests/test_evidence_importer
TEST_EVIDENCE_IMPORT_TASK := tests/test_evidence_import_task
TEST_EVIDENCE_TYPE := tests/test_evidence_type
TEST_ENTITY_TYPE := tests/test_entity_type
TEST_EVIDENCE_TYPE_DAO := tests/test_evidence_type_dao
TEST_EVIDENCE_IMPORT_DIALOG := tests/test_evidence_import_dialog
TEST_EVIDENCE_LIST_ITEM := tests/test_evidence_list_item
TEST_EVIDENCE_LIST_MODEL := tests/test_evidence_list_model
TEST_EVIDENCE_CATEGORY_ITEM := tests/test_evidence_category_item
TEST_EVIDENCE_CATEGORY_MODEL := tests/test_evidence_category_model
TEST_EVIDENCE_INTEGRITY_TASK := tests/test_evidence_integrity_task
TEST_ENTITY_RECORD := tests/test_entity_record
TEST_ENTITY_TYPE_DAO := tests/test_entity_type_dao
TEST_ENTITY_DAO := tests/test_entity_dao
TEST_EVIDENCE_ENTITY_DAO := tests/test_evidence_entity_dao
TEST_RELATION_RECORD := tests/test_relation_record
TEST_RELATION_DAO := tests/test_relation_dao
TEST_RELATION_EVIDENCE_DAO := tests/test_relation_evidence_dao
TEST_RELATION_SERVICE := tests/test_relation_service
TEST_INVESTIGATION_GRAPH_MODEL := tests/test_investigation_graph_model
TEST_INVESTIGATION_GRAPH_LOADER := tests/test_investigation_graph_loader
TEST_INVESTIGATION_GRAPH_LOAD_TASK := tests/test_investigation_graph_load_task
TEST_GRAPH_NODE_POSITION_DAO := tests/test_graph_node_position_dao
TEST_OSINT_SELECTION_CONTEXT := tests/test_osint_selection_context
TEST_OSINT_ACTION_CATALOG := tests/test_osint_action_catalog
TEST_OSINT_DNS_QUERY := tests/test_osint_dns_query
TEST_OSINT_DNS_PROPOSAL := tests/test_osint_dns_proposal
TEST_OSINT_DNS_INTEGRATION := tests/test_osint_dns_integration
TEST_OSINT_EXECUTION_DAO := tests/test_osint_execution_dao
TEST_OSINT_EXECUTION_INTEGRITY := tests/test_osint_execution_integrity
TEST_EVIDENCE_RECLASSIFICATION := tests/test_evidence_reclassification
TEST_SOCIAL_ACCOUNT_SERVICE := tests/test_social_account_service
TEST_SOCIAL_PLATFORM := tests/test_social_platform
TEST_PERSON_ENTITY_SERVICE := tests/test_person_entity_service
TEST_EML_ANALYZER := tests/test_eml_analyzer
TEST_IBAN_ANALYZER := tests/test_iban_analyzer
TEST_EXIFTOOL_METADATA := tests/test_exiftool_metadata
TEST_PDF_PASSWORD_RECOVERY := tests/test_pdf_password_recovery

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
	src/database/schema.c \
	src/database/error.c
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

$(TEST_EVIDENCE_RECORD): \
	tests/test_evidence_record.c \
	src/models/evidence_record.c \
	src/models/entity_record.c
	$(CC) $(EVIDENCE_RECORD_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_LIST_ITEM): \
	tests/test_evidence_list_item.c \
	src/widgets/evidence_list_item.c \
	src/models/evidence_record.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_LIST_MODEL): \
	tests/test_evidence_list_model.c \
	src/widgets/evidence_list_model.c \
	src/widgets/evidence_list_item.c \
	src/models/evidence_record.c
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

$(TEST_EVIDENCE_DAO): \
	tests/test_evidence_dao.c \
	src/dao/evidence_dao.c \
	src/models/evidence_record.c \
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

$(TEST_TOOL_CATALOG): \
	tests/test_tool_catalog.c \
	src/core/tool_catalog.c \
	src/core/tool_registry.c \
	src/core/tool_process.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_TOOL_INITIALIZER): \
	tests/test_tool_initializer.c \
	src/core/tool_initializer.c \
	src/core/task_manager.c \
	src/core/background_task.c \
	src/core/tool_registry.c \
	src/core/tool_catalog.c \
	src/core/tool_process.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_FILE_HASH): \
	tests/test_file_hash.c \
	src/core/file_hash.c
	$(CC) $(TEST_CFLAGS) \
		-DFILE_HASH_ENABLE_TEST_HOOKS \
		$^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_INTEGRITY_VERIFIER): \
	tests/test_evidence_integrity_verifier.c \
	src/core/evidence_integrity_verifier.c \
	src/core/file_hash.c
	$(CC) $(EVIDENCE_INTEGRITY_VERIFIER_TEST_CFLAGS) \
		$^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_COPY): \
	tests/test_evidence_copy.c \
	src/core/evidence_copy.c \
	src/core/file_hash.c
	$(CC) $(TEST_CFLAGS) \
		-DEVIDENCE_COPY_ENABLE_TEST_HOOKS \
		$^ -o $@ $(TEST_LDFLAGS)
	
$(TEST_EVIDENCE_IMPORTER): \
	tests/test_evidence_importer.c \
	src/core/evidence_importer.c \
	src/core/evidence_copy.c \
	src/core/file_hash.c \
	src/dao/evidence_dao.c \
	src/models/evidence_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) \
		-DEVIDENCE_IMPORTER_ENABLE_TEST_HOOKS \
		$^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_EVIDENCE_IMPORT_TASK): \
	tests/test_evidence_import_task.c \
	src/core/evidence_import_task.c \
	src/core/evidence_importer.c \
	src/core/evidence_copy.c \
	src/core/file_hash.c \
	src/core/background_task.c \
	src/core/task_manager.c \
	src/dao/evidence_dao.c \
	src/models/evidence_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) \
		-DEVIDENCE_IMPORTER_ENABLE_TEST_HOOKS \
		$^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_EVIDENCE_TYPE): \
	tests/test_evidence_type.c \
	src/models/evidence_type.c
	$(CC) $(EVIDENCE_TYPE_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_ENTITY_TYPE): \
	tests/test_entity_type.c \
	src/models/entity_type.c
	$(CC) $(ENTITY_TYPE_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_TYPE_DAO): \
	tests/test_evidence_type_dao.c \
	src/dao/evidence_type_dao.c \
	src/models/evidence_type.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(EVIDENCE_TYPE_DAO_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_EVIDENCE_IMPORT_DIALOG): \
	tests/test_evidence_import_dialog.c \
	src/views/evidence_import_dialog.c \
	src/models/evidence_type.c
	$(CC) $(EVIDENCE_IMPORT_DIALOG_TEST_CFLAGS) $^ \
		-o $@ \
		$(EVIDENCE_IMPORT_DIALOG_TEST_LDFLAGS)

$(TEST_EVIDENCE_CATEGORY_ITEM): \
	tests/test_evidence_category_item.c \
	src/widgets/evidence_category_item.c \
	src/widgets/evidence_list_item.c \
	src/models/evidence_record.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_CATEGORY_MODEL): \
	tests/test_evidence_category_model.c \
	src/widgets/evidence_category_model.c \
	src/widgets/evidence_category_item.c \
	src/widgets/evidence_list_item.c \
	src/models/evidence_record.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_INTEGRITY_TASK): \
	tests/test_evidence_integrity_task.c \
	src/core/evidence_integrity_task.c \
	src/core/evidence_integrity_verifier.c \
	src/core/file_hash.c \
	src/core/background_task.c \
	src/core/task_manager.c
	$(CC) $(TEST_CFLAGS) \
		-DFILE_HASH_ENABLE_TEST_HOOKS \
		$^ -o $@ $(TEST_LDFLAGS)

$(TEST_ENTITY_RECORD): \
	tests/test_entity_record.c \
	src/models/entity_record.c
	$(CC) $(ENTITY_RECORD_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_ENTITY_TYPE_DAO): \
	tests/test_entity_type_dao.c \
	src/dao/entity_type_dao.c \
	src/models/entity_type.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(ENTITY_TYPE_DAO_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_ENTITY_DAO): \
	tests/test_entity_dao.c \
	src/dao/entity_dao.c \
	src/models/entity_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(ENTITY_DAO_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_EVIDENCE_ENTITY_DAO): \
	tests/test_evidence_entity_dao.c \
	src/dao/evidence_entity_dao.c \
	src/dao/evidence_dao.c \
	src/dao/entity_dao.c \
	src/models/evidence_record.c \
	src/models/entity_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(EVIDENCE_ENTITY_DAO_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_RELATION_RECORD): \
	tests/test_relation_record.c \
	src/models/relation_record.c
	$(CC) $(RELATION_RECORD_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_RELATION_DAO): \
	tests/test_relation_dao.c \
	src/dao/relation_dao.c \
	src/dao/entity_dao.c \
	src/models/relation_record.c \
	src/models/entity_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(RELATION_DAO_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_RELATION_EVIDENCE_DAO): \
	tests/test_relation_evidence_dao.c \
	src/dao/relation_evidence_dao.c \
	src/dao/relation_dao.c \
	src/dao/evidence_dao.c \
	src/dao/entity_dao.c \
	src/models/relation_record.c \
	src/models/evidence_record.c \
	src/models/entity_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(RELATION_EVIDENCE_DAO_TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_RELATION_SERVICE): \
	tests/test_relation_service.c \
	src/core/relation_service.c \
	src/dao/relation_dao.c \
	src/dao/relation_evidence_dao.c \
	src/dao/evidence_dao.c \
	src/dao/entity_dao.c \
	src/models/relation_record.c \
	src/models/evidence_record.c \
	src/models/entity_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(RELATION_SERVICE_TEST_CFLAGS) $^ -o $@ \
		$(TEST_LDFLAGS) -lsqlite3

$(TEST_INVESTIGATION_GRAPH_MODEL): \
	tests/test_investigation_graph_model.c \
	src/models/investigation_graph_model.c \
	src/models/entity_record.c \
	src/models/relation_record.c
	$(CC) $(INVESTIGATION_GRAPH_MODEL_TEST_CFLAGS) $^ -o $@ \
		$(TEST_LDFLAGS)

$(TEST_INVESTIGATION_GRAPH_LOADER): \
	tests/test_investigation_graph_loader.c \
	src/core/investigation_graph_loader.c \
	src/dao/entity_dao.c \
	src/dao/relation_dao.c \
	src/models/investigation_graph_model.c \
	src/models/entity_record.c \
	src/models/relation_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(INVESTIGATION_GRAPH_LOADER_TEST_CFLAGS) $^ -o $@ \
		$(TEST_LDFLAGS) -lsqlite3

$(TEST_GRAPH_NODE_POSITION_DAO): \
	tests/test_graph_node_position_dao.c \
	src/dao/graph_node_position_dao.c \
	src/models/graph_node_position.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(GRAPH_NODE_POSITION_DAO_TEST_CFLAGS) $^ -o $@ \
		$(TEST_LDFLAGS) -lsqlite3

$(TEST_OSINT_SELECTION_CONTEXT): \
	tests/test_osint_selection_context.c \
	src/models/osint_selection_context.c \
	src/models/entity_record.c \
	src/models/relation_record.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_OSINT_ACTION_CATALOG): \
	tests/test_osint_action_catalog.c \
	src/models/osint_action_catalog.c \
	src/models/osint_selection_context.c \
	src/models/entity_record.c \
	src/models/relation_record.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_OSINT_DNS_QUERY): \
	tests/test_osint_dns_query.c \
	src/models/osint_dns_query.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_OSINT_DNS_PROPOSAL): \
	tests/test_osint_dns_proposal.c \
	src/models/osint_dns_proposal.c \
	src/models/osint_dns_query.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_OSINT_DNS_INTEGRATION): \
	tests/test_osint_dns_integration.c \
	src/core/osint_dns_integration.c \
	src/models/osint_dns_proposal.c \
	src/models/osint_dns_query.c \
	src/models/entity_record.c \
	src/models/relation_record.c \
	src/models/osint_execution_record.c \
	src/dao/entity_dao.c \
	src/dao/relation_dao.c \
	src/dao/osint_execution_dao.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_OSINT_EXECUTION_DAO): \
	tests/test_osint_execution_dao.c \
	src/dao/osint_execution_dao.c \
	src/models/osint_execution_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_OSINT_EXECUTION_INTEGRITY): \
	tests/test_osint_execution_integrity.c \
	src/core/osint_execution_integrity.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_RECLASSIFICATION): \
	tests/test_evidence_reclassification.c \
	src/core/evidence_reclassification.c \
	src/core/file_hash.c \
	src/dao/evidence_dao.c \
	src/models/evidence_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_SOCIAL_ACCOUNT_SERVICE): \
	tests/test_social_account_service.c \
	src/core/social_account_service.c \
	src/dao/entity_dao.c \
	src/dao/evidence_entity_dao.c \
	src/models/entity_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_SOCIAL_PLATFORM): \
	tests/test_social_platform.c \
	src/models/social_platform.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_PERSON_ENTITY_SERVICE): \
	tests/test_person_entity_service.c \
	src/core/person_entity_service.c \
	src/dao/entity_dao.c \
	src/dao/evidence_entity_dao.c \
	src/models/entity_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_EML_ANALYZER): tests/test_eml_analyzer.c src/core/eml_analyzer.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_IBAN_ANALYZER): tests/test_iban_analyzer.c src/core/iban_analyzer.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EXIFTOOL_METADATA): tests/test_exiftool_metadata.c \
	src/core/exiftool_metadata.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_PDF_PASSWORD_RECOVERY): tests/test_pdf_password_recovery.c \
	src/core/pdf_password_recovery.c \
	src/core/tool_process.c \
	src/core/background_task.c \
	src/core/task_manager.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_INVESTIGATION_GRAPH_LOAD_TASK): \
	tests/test_investigation_graph_load_task.c \
	src/core/investigation_graph_load_task.c \
	src/core/investigation_graph_loader.c \
	src/core/background_task.c \
	src/dao/entity_dao.c \
	src/models/investigation_graph_layout.c \
	src/models/graph_node_position.c \
	src/dao/graph_node_position_dao.c \
	src/dao/relation_dao.c \
	src/models/investigation_graph_model.c \
	src/models/entity_record.c \
	src/models/relation_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(INVESTIGATION_GRAPH_LOAD_TASK_TEST_CFLAGS) $^ -o $@ \
		$(TEST_LDFLAGS) -lsqlite3

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
	$(TEST_EVIDENCE_RECORD) \
	$(TEST_EVIDENCE_LIST_ITEM) \
	$(TEST_EVIDENCE_LIST_MODEL) \
	$(TEST_EVIDENCE_CATEGORY_ITEM) \
	$(TEST_EVIDENCE_CATEGORY_MODEL) \
	$(TEST_EVIDENCE_TYPE) \
	$(TEST_ENTITY_TYPE) \
	$(TEST_INVESTIGATION_DAO) \
	$(TEST_EVIDENCE_DAO) \
	$(TEST_EVIDENCE_TYPE_DAO) \
	$(TEST_INVESTIGATION_SESSION) \
	$(TEST_BACKGROUND_TASK) \
	$(TEST_TASK_MANAGER) \
	$(TEST_TOOL_REGISTRY) \
	$(TEST_TOOL_PROCESS) \
	$(TEST_TOOL_TASK) \
	$(TEST_TOOL_CATALOG) \
	$(TEST_TOOL_INITIALIZER) \
	$(TEST_FILE_HASH) \
	$(TEST_EVIDENCE_INTEGRITY_VERIFIER) \
	$(TEST_EVIDENCE_COPY) \
	$(TEST_EVIDENCE_IMPORTER) \
	$(TEST_EVIDENCE_IMPORT_TASK) \
	$(TEST_EVIDENCE_IMPORT_DIALOG) \
	$(TEST_EVIDENCE_INTEGRITY_TASK) \
	$(TEST_ENTITY_RECORD) \
	$(TEST_ENTITY_TYPE_DAO) \
	$(TEST_ENTITY_DAO) \
	$(TEST_EVIDENCE_ENTITY_DAO) \
	$(TEST_RELATION_RECORD) \
	$(TEST_RELATION_DAO) \
	$(TEST_RELATION_EVIDENCE_DAO) \
	$(TEST_RELATION_SERVICE) \
	$(TEST_INVESTIGATION_GRAPH_MODEL) \
	$(TEST_INVESTIGATION_GRAPH_LOADER) \
	$(TEST_INVESTIGATION_GRAPH_LOAD_TASK) \
	$(TEST_GRAPH_NODE_POSITION_DAO) \
	$(TEST_OSINT_SELECTION_CONTEXT) \
	$(TEST_OSINT_ACTION_CATALOG) \
	$(TEST_OSINT_DNS_QUERY) \
	$(TEST_OSINT_DNS_PROPOSAL) \
	$(TEST_OSINT_DNS_INTEGRATION) \
	$(TEST_OSINT_EXECUTION_DAO) \
	$(TEST_OSINT_EXECUTION_INTEGRITY) \
	$(TEST_EVIDENCE_RECLASSIFICATION) \
	$(TEST_SOCIAL_ACCOUNT_SERVICE) \
	$(TEST_SOCIAL_PLATFORM) \
	$(TEST_PERSON_ENTITY_SERVICE) \
	$(TEST_EML_ANALYZER) \
	$(TEST_IBAN_ANALYZER) \
	$(TEST_EXIFTOOL_METADATA) \
	$(TEST_PDF_PASSWORD_RECOVERY)
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
	@$(TEST_EVIDENCE_LIST_ITEM)
	@$(TEST_EVIDENCE_LIST_MODEL)
	@$(TEST_EVIDENCE_CATEGORY_ITEM)
	@$(TEST_EVIDENCE_CATEGORY_MODEL)
	@$(TEST_EVIDENCE_RECORD)
	@$(TEST_EVIDENCE_TYPE)
	@$(TEST_ENTITY_TYPE)
	@$(TEST_INVESTIGATION_DAO)
	@$(TEST_EVIDENCE_DAO)
	@$(TEST_EVIDENCE_TYPE_DAO)
	@$(TEST_INVESTIGATION_SESSION)
	@$(TEST_BACKGROUND_TASK)
	@$(TEST_TASK_MANAGER)
	@$(TEST_TOOL_REGISTRY)
	@$(TEST_TOOL_PROCESS)
	@$(TEST_TOOL_TASK)
	@$(TEST_TOOL_CATALOG)
	@$(TEST_TOOL_INITIALIZER)
	@$(TEST_FILE_HASH)
	@$(TEST_EVIDENCE_INTEGRITY_VERIFIER)
	@$(TEST_EVIDENCE_COPY)
	@$(TEST_EVIDENCE_IMPORTER)
	@$(TEST_EVIDENCE_IMPORT_TASK)
	@$(TEST_EVIDENCE_IMPORT_DIALOG)
	@$(TEST_EVIDENCE_INTEGRITY_TASK)
	@$(TEST_ENTITY_RECORD)
	@$(TEST_ENTITY_TYPE_DAO)
	@$(TEST_ENTITY_DAO)
	@$(TEST_EVIDENCE_ENTITY_DAO)
	@$(TEST_RELATION_RECORD)
	@$(TEST_RELATION_DAO)
	@$(TEST_RELATION_EVIDENCE_DAO)
	@$(TEST_RELATION_SERVICE)
	@$(TEST_INVESTIGATION_GRAPH_MODEL)
	@$(TEST_INVESTIGATION_GRAPH_LOADER)
	@$(TEST_INVESTIGATION_GRAPH_LOAD_TASK)
	@$(TEST_GRAPH_NODE_POSITION_DAO)
	@$(TEST_OSINT_SELECTION_CONTEXT)
	@$(TEST_OSINT_ACTION_CATALOG)
	@$(TEST_OSINT_DNS_QUERY)
	@$(TEST_OSINT_DNS_PROPOSAL)
	@$(TEST_OSINT_DNS_INTEGRATION)
	@$(TEST_OSINT_EXECUTION_DAO)
	@$(TEST_OSINT_EXECUTION_INTEGRITY)
	@$(TEST_EVIDENCE_RECLASSIFICATION)
	@$(TEST_SOCIAL_ACCOUNT_SERVICE)
	@$(TEST_SOCIAL_PLATFORM)
	@$(TEST_PERSON_ENTITY_SERVICE)
	@$(TEST_EML_ANALYZER)
	@$(TEST_IBAN_ANALYZER)
	@$(TEST_EXIFTOOL_METADATA)
	@$(TEST_PDF_PASSWORD_RECOVERY)
	@echo "Tous les tests sont valides."

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(DEP) $(TARGET) \
		$(TEST_NODE) \
		$(TEST_TREE_MODEL) \
		$(TEST_TREE_BUILDER) \
		$(TEST_PROJECT) \
		$(TEST_DATABASE) \
		$(TEST_STATEMENT) \
		$(TEST_TRANSACTION) \
		$(TEST_ERROR) \
		$(TEST_INVESTIGATION_RECORD) \
		$(TEST_EVIDENCE_RECORD) \
		$(TEST_EVIDENCE_LIST_ITEM) \
		$(TEST_EVIDENCE_LIST_MODEL) \
		$(TEST_EVIDENCE_CATEGORY_ITEM) \
		$(TEST_EVIDENCE_CATEGORY_MODEL) \
		$(TEST_EVIDENCE_TYPE) \
		$(TEST_ENTITY_TYPE) \
		$(TEST_EVIDENCE_TYPE_DAO) \
		$(TEST_INVESTIGATION_DAO) \
		$(TEST_EVIDENCE_DAO) \
		$(TEST_INVESTIGATION_SESSION) \
		$(TEST_BACKGROUND_TASK) \
		$(TEST_TASK_MANAGER) \
		$(TEST_TOOL_REGISTRY) \
		$(TEST_TOOL_PROCESS) \
		$(TEST_TOOL_TASK) \
		$(TEST_TOOL_CATALOG) \
		$(TEST_TOOL_INITIALIZER) \
		$(TEST_FILE_HASH) \
		$(TEST_EVIDENCE_INTEGRITY_VERIFIER) \
		$(TEST_EVIDENCE_COPY) \
		$(TEST_EVIDENCE_IMPORTER) \
		$(TEST_EVIDENCE_IMPORT_TASK) \
		$(TEST_EVIDENCE_IMPORT_DIALOG) \
		$(TEST_EVIDENCE_INTEGRITY_TASK) \
		$(TEST_ENTITY_DAO) \
		$(TEST_EVIDENCE_ENTITY_DAO) \
		$(TEST_RELATION_RECORD) \
		$(TEST_RELATION_DAO) \
		$(TEST_RELATION_EVIDENCE_DAO) \
		$(TEST_RELATION_SERVICE) \
		$(TEST_INVESTIGATION_GRAPH_MODEL) \
		$(TEST_INVESTIGATION_GRAPH_LOADER) \
		$(TEST_INVESTIGATION_GRAPH_LOAD_TASK) \
		$(TEST_GRAPH_NODE_POSITION_DAO) \
		$(TEST_OSINT_SELECTION_CONTEXT) \
		$(TEST_OSINT_ACTION_CATALOG) \
		$(TEST_OSINT_DNS_QUERY) \
		$(TEST_OSINT_DNS_PROPOSAL) \
		$(TEST_OSINT_DNS_INTEGRATION) \
		$(TEST_OSINT_EXECUTION_DAO) \
		$(TEST_OSINT_EXECUTION_INTEGRITY) \
		$(TEST_EVIDENCE_RECLASSIFICATION) \
		$(TEST_SOCIAL_ACCOUNT_SERVICE) \
		$(TEST_SOCIAL_PLATFORM) \
		$(TEST_PERSON_ENTITY_SERVICE) \
		$(TEST_EML_ANALYZER)

-include $(DEP)

.PHONY: clean run test
