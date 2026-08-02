CC = gcc

PKG_CONFIG = pkg-config
REQUIRED_PACKAGES = gtk4 sqlite3 libheif poppler-glib
ifeq ($(shell $(PKG_CONFIG) --exists $(REQUIRED_PACKAGES) && echo yes),)
$(error Dépendances de compilation manquantes : $(REQUIRED_PACKAGES). Voir docs/DEPENDENCE.md)
endif

.DEFAULT_GOAL := all

SOURCE_SIZE_LIMIT := 2000
# Exceptions historiques : plafonds constatés avant la tranche V18.
SOURCE_SIZE_EXCEPTIONS := \
	src/core/application.c:9325 \
	src/widgets/investigation_graph_view.c:4901 \
	src/widgets/workspace.c:4071 \
	src/views/main_window.c:2239 \
	src/views/create_relation_dialog.c:2043

.PHONY: check-source-size
check-source-size:
	@limit=$(SOURCE_SIZE_LIMIT); failed=0; \
	files="$$(git ls-files --cached --others --exclude-standard -- \
		'src/*.c' 'src/**/*.c' | sort -u)"; \
	for file in $$files; do \
		lines=$$(wc -l < "$$file"); allowed=$$limit; historical=0; \
		for exception in $(SOURCE_SIZE_EXCEPTIONS); do \
			case "$$exception" in "$$file":*) \
				allowed=$${exception##*:}; historical=1;; esac; \
		done; \
		if [ $$historical -eq 1 ]; then \
			echo "EXCEPTION HISTORIQUE $$file : $$lines/$$allowed lignes (limite normale $$limit)"; \
		fi; \
		if [ $$lines -gt $$allowed ]; then \
			echo "ERREUR taille source $$file : $$lines lignes, maximum $$allowed"; \
			failed=1; \
		fi; \
	done; \
	exit $$failed

CFLAGS = -std=c17 \
          -Wall \
          -Wextra \
          -Werror \
          -g \
          -Iinclude \
          -MMD \
          -MP \
          $(shell $(PKG_CONFIG) --cflags gtk4 sqlite3 libheif poppler-glib)

LDFLAGS = $(shell $(PKG_CONFIG) --libs gtk4 sqlite3 libheif poppler-glib) -ljpeg

TEST_CFLAGS = -std=c17 \
              -Wall \
              -Wextra \
              -Werror \
              -Iinclude \
              $(shell $(PKG_CONFIG) --cflags glib-2.0 gio-2.0)

TEST_LDFLAGS = src/core/relation_type_normalizer.c \
               $(shell $(PKG_CONFIG) --libs glib-2.0 gio-2.0)

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
TEST_EVIDENCE_PREVIEW := tests/test_evidence_preview
TEST_EVIDENCE_VIDEO_PREVIEW_CONTROLLER := tests/test_evidence_video_preview_controller
TEST_EVIDENCE_SELECTION_MODEL := tests/test_evidence_selection_model
TEST_PERSON_CREATION_GUARD := tests/test_person_creation_guard
TEST_PERSON_DIALOG_LIFECYCLE := tests/test_person_dialog_lifecycle
TEST_CREATE_PERSON_DIALOG_GTK := tests/test_create_person_dialog_gtk
TEST_CREATE_PERSON_DIALOG_OCR_GTK := tests/test_create_person_dialog_ocr_gtk
TEST_EVIDENCE_PREVIEW_WIDGET_GTK := tests/test_evidence_preview_widget_gtk
TEST_PERSON_CONFIRMATION_SUMMARY := tests/test_person_confirmation_summary
TEST_PERSON_ROLE_ASSIGNMENT_DAO := tests/test_person_role_assignment_dao
TEST_PERSON_EVIDENCE_SELECTION := tests/test_person_evidence_selection
TEST_EVIDENCE_STAGING := tests/test_evidence_staging
TEST_PERSON_CREATION_COORDINATOR := tests/test_person_creation_coordinator
TEST_EML_ANALYZER := tests/test_eml_analyzer
TEST_EML_INTEGRATION := tests/test_eml_integration
TEST_IBAN_ANALYZER := tests/test_iban_analyzer
TEST_FINANCIAL_FOUNDATION := tests/test_financial_foundation
TEST_BANK_STRUCTURED_EXTRACTOR := tests/test_bank_structured_extractor
TEST_EXIFTOOL_METADATA := tests/test_exiftool_metadata
TEST_PDF_PASSWORD_RECOVERY := tests/test_pdf_password_recovery
TEST_EXTRACTION_DROP_SERVICE := tests/test_extraction_drop_service
TEST_RELATION_TYPE_NORMALIZER := tests/test_relation_type_normalizer
TEST_RELATION_TYPE_SERVICE := tests/test_relation_type_service
TEST_CONTROLLED_VOCAB := tests/test_controlled_vocab
TEST_BANK_PROPOSAL := tests/test_bank_proposal
TEST_EML_PIPELINE_TASK := tests/test_eml_pipeline_task
TEST_EML_MIME_EXTRACTOR := tests/test_eml_mime_extractor
TEST_EXIFTOOL_ANALYSIS := tests/test_exiftool_analysis
TEST_OCR_ANALYSIS := tests/test_ocr_analysis
TEST_PDF_ANALYSIS := tests/test_pdf_analysis
TEST_DOCUMENT_TOOL_RUNNER := tests/test_document_tool_runner
TEST_IDENTITY_OCR := tests/test_identity_ocr
TEST_IDENTITY_OCR_PREPROCESSOR := tests/test_identity_ocr_preprocessor
TEST_IDENTITY_TRACEABILITY := tests/test_identity_traceability
TEST_OCR_PROVENANCE_OVERLAY_GTK := tests/test_ocr_provenance_overlay_gtk
TEST_EVIDENCE_METADATA_DIALOG_GTK := tests/test_evidence_metadata_dialog_gtk
TEST_EVIDENCE_IDENTITY_IMPORT_GTK := tests/test_evidence_identity_import_gtk
TEST_WORKSPACE_IDENTITY_OCR_GTK := tests/test_workspace_identity_ocr_gtk
TEST_DIALOG_GEOMETRY_GTK := tests/test_dialog_geometry_gtk
TEST_PERSON_FACTUAL_RELATION_EDITOR_GTK := tests/test_person_factual_relation_editor_gtk
TEST_DOCUMENT_AUTHENTICITY_EDITOR_GTK := tests/test_document_authenticity_editor_gtk
TEST_DOCUMENT_IDENTITY_MISUSE_EDITOR_GTK := tests/test_document_identity_misuse_editor_gtk
TEST_PERSON_DETAILS_TRACEABILITY_GTK := tests/test_person_details_traceability_gtk
TEST_PERSON_OCR_PROJECTION := tests/test_person_ocr_projection
TEST_PERSON_OCR_PROJECTION_EDITOR_GTK := tests/test_person_ocr_projection_editor_gtk
FAKE_DOCUMENT_TOOL := tests/fake_document_tool

DOCUMENT_ANALYSIS_TEST_SOURCES := \
	src/core/document_analysis.c \
	src/core/document_tool_runner.c \
	src/core/tool_process.c \
	src/core/file_hash.c

$(TEST_DOCUMENT_TOOL_RUNNER): tests/test_document_tool_runner.c \
	$(DOCUMENT_ANALYSIS_TEST_SOURCES) $(FAKE_DOCUMENT_TOOL)
	$(CC) $(TEST_CFLAGS) -Wpedantic \
		tests/test_document_tool_runner.c \
		$(DOCUMENT_ANALYSIS_TEST_SOURCES) -o $@ $(TEST_LDFLAGS)

$(TEST_DIALOG_GEOMETRY_GTK): tests/test_dialog_geometry_gtk.c \
	src/views/dialog_geometry.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_PERSON_FACTUAL_RELATION_EDITOR_GTK): \
	tests/test_person_factual_relation_editor_gtk.c \
	src/views/person_factual_relation_editor.c \
	src/views/person_vocabulary_adapter.c src/models/identity_ocr.c \
	src/models/identity_traceability.c src/models/person_role_assignment.c \
	src/dao/identity_traceability_dao.c src/database/database.c \
	src/database/schema.c src/database/statement.c src/database/transaction.c \
	src/database/error.c src/core/relation_type_normalizer.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_DOCUMENT_AUTHENTICITY_EDITOR_GTK): \
	tests/test_document_authenticity_editor_gtk.c \
	src/views/document_authenticity_editor.c \
	src/core/document_authenticity_service.c src/models/identity_traceability.c \
	src/models/identity_ocr.c src/dao/identity_ocr_dao.c \
	src/dao/identity_traceability_dao.c src/database/database.c \
	src/database/schema.c src/database/statement.c src/database/transaction.c \
	src/database/error.c src/core/relation_type_normalizer.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_DOCUMENT_IDENTITY_MISUSE_EDITOR_GTK): \
	tests/test_document_identity_misuse_editor_gtk.c \
	src/views/document_identity_misuse_editor.c \
	src/core/document_identity_misuse_service.c src/models/identity_traceability.c \
	src/models/identity_ocr.c src/dao/identity_ocr_dao.c \
	src/dao/identity_traceability_dao.c src/database/database.c \
	src/database/schema.c src/database/statement.c src/database/transaction.c \
	src/database/error.c src/core/relation_type_normalizer.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_PERSON_DETAILS_TRACEABILITY_GTK): \
	tests/test_person_details_traceability_gtk.c \
	src/widgets/entity_details_panel.c src/models/entity_record.c \
	src/models/evidence_record.c src/models/identity_traceability.c \
	src/models/person_role_assignment.c src/views/person_vocabulary_adapter.c
	$(CC) $(CFLAGS) $^ src/dao/identity_traceability_dao.c \
		src/database/database.c src/database/schema.c src/database/statement.c \
		src/database/transaction.c src/database/error.c \
		src/core/relation_type_normalizer.c -o $@ $(LDFLAGS)

PERSON_OCR_PROJECTION_SOURCES := src/models/person_ocr_projection.c \
	src/core/person_ocr_projection_mapping.c src/core/person_ocr_projection_service.c \
	src/dao/person_ocr_projection_dao.c src/dao/identity_ocr_dao.c \
	src/models/identity_ocr.c src/models/identity_traceability.c \
	src/dao/entity_dao.c src/models/entity_record.c src/database/database.c \
	src/database/schema.c src/database/statement.c src/database/transaction.c \
	src/database/error.c src/core/relation_type_normalizer.c
$(TEST_PERSON_OCR_PROJECTION): tests/test_person_ocr_projection.c $(PERSON_OCR_PROJECTION_SOURCES)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
$(TEST_PERSON_OCR_PROJECTION_EDITOR_GTK): tests/test_person_ocr_projection_editor_gtk.c \
	src/views/person_ocr_projection_editor.c src/models/person_ocr_projection.c \
	src/core/person_ocr_projection_mapping.c src/models/identity_ocr.c \
	src/models/identity_traceability.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

all: $(TARGET)

$(TEST_IDENTITY_OCR): tests/test_identity_ocr.c \
	src/models/identity_ocr.c src/core/identity_field_extractor.c \
	src/core/mrz_parser.c src/core/ocr_region_geometry.c \
	src/core/ocr_analysis.c $(DOCUMENT_ANALYSIS_TEST_SOURCES)
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_IDENTITY_OCR_PREPROCESSOR): tests/test_identity_ocr_preprocessor.c \
	src/core/identity_ocr_preprocessor.c src/core/evidence_preview.c \
	src/core/eml_analyzer.c src/core/eml_mime_extractor.c \
	src/core/evidence_integrity_verifier.c src/core/file_hash.c \
	src/core/ocr_analysis.c src/core/document_analysis.c \
	src/core/document_tool_runner.c src/core/tool_process.c \
	src/core/tool_registry.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_IDENTITY_TRACEABILITY): tests/test_identity_traceability.c \
	src/models/identity_traceability.c src/dao/identity_traceability_dao.c \
	src/core/document_authenticity_service.c \
	src/core/document_identity_misuse_service.c \
	src/views/person_vocabulary_adapter.c \
	src/views/person_factual_relation_editor.c \
	src/models/person_role_assignment.c \
	src/models/identity_ocr.c src/dao/identity_ocr_dao.c \
	src/core/relation_type_normalizer.c \
	src/database/database.c src/database/schema.c src/database/statement.c \
	src/database/transaction.c src/database/error.c \
	src/core/relation_type_normalizer.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_OCR_PROVENANCE_OVERLAY_GTK): \
	tests/test_ocr_provenance_overlay_gtk.c \
	src/widgets/ocr_provenance_overlay.c src/core/ocr_region_geometry.c \
	src/models/identity_ocr.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_EVIDENCE_METADATA_DIALOG_GTK): \
	tests/test_evidence_metadata_dialog_gtk.c \
	$(filter-out src/main.c,$(SRC))
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

$(TEST_EVIDENCE_IDENTITY_IMPORT_GTK): \
	tests/test_evidence_identity_import_gtk.c \
	$(filter-out src/main.c,$(SRC)) $(FAKE_DOCUMENT_TOOL)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

$(TEST_WORKSPACE_IDENTITY_OCR_GTK): \
	tests/test_workspace_identity_ocr_gtk.c \
	$(filter-out src/main.c,$(SRC))
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

$(TEST_BANK_PROPOSAL): \
	tests/test_bank_proposal.c \
	src/core/bank_proposal.c \
	src/core/iban_analyzer.c \
	src/core/controlled_vocab.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ \
		$(shell $(PKG_CONFIG) --libs glib-2.0)

$(TEST_CONTROLLED_VOCAB): \
	tests/test_controlled_vocab.c \
	src/core/controlled_vocab.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ \
		$(shell $(PKG_CONFIG) --libs glib-2.0)

$(TEST_EML_PIPELINE_TASK): \
	tests/test_eml_pipeline_task.c \
	src/core/eml_pipeline_task.c src/core/eml_mime_extractor.c \
	src/core/eml_analyzer.c src/core/bank_proposal.c \
	src/core/controlled_vocab.c src/core/iban_analyzer.c \
	src/core/file_hash.c src/core/background_task.c \
	src/core/document_analysis.c src/core/document_tool_runner.c \
	src/core/exiftool_analysis.c src/core/ocr_analysis.c \
	src/core/pdf_analysis.c src/core/document_file_analysis.c \
	src/core/tool_process.c $(FAKE_DOCUMENT_TOOL)
	$(CC) $(TEST_CFLAGS) -Wpedantic \
		tests/test_eml_pipeline_task.c \
		src/core/eml_pipeline_task.c src/core/eml_mime_extractor.c \
		src/core/eml_analyzer.c src/core/bank_proposal.c \
		src/core/controlled_vocab.c src/core/iban_analyzer.c \
		src/core/file_hash.c src/core/background_task.c \
		src/core/document_analysis.c src/core/document_tool_runner.c \
		src/core/exiftool_analysis.c src/core/ocr_analysis.c \
		src/core/pdf_analysis.c src/core/document_file_analysis.c \
		src/core/tool_process.c -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_EML_MIME_EXTRACTOR): \
	tests/test_eml_mime_extractor.c \
	src/core/eml_mime_extractor.c \
	src/core/file_hash.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(FAKE_DOCUMENT_TOOL): tests/fake_document_tool.c
	$(CC) -std=c17 -Wall -Wextra -Werror -Wpedantic $< -o $@

$(TEST_EXIFTOOL_ANALYSIS): tests/test_exiftool_analysis.c \
	src/core/exiftool_analysis.c $(DOCUMENT_ANALYSIS_TEST_SOURCES) \
	$(FAKE_DOCUMENT_TOOL)
	$(CC) $(TEST_CFLAGS) -Wpedantic \
		tests/test_exiftool_analysis.c src/core/exiftool_analysis.c \
		$(DOCUMENT_ANALYSIS_TEST_SOURCES) -o $@ $(TEST_LDFLAGS)

$(TEST_OCR_ANALYSIS): tests/test_ocr_analysis.c \
	src/core/ocr_analysis.c $(DOCUMENT_ANALYSIS_TEST_SOURCES) \
	$(FAKE_DOCUMENT_TOOL)
	$(CC) $(TEST_CFLAGS) -Wpedantic \
		tests/test_ocr_analysis.c src/core/ocr_analysis.c \
		$(DOCUMENT_ANALYSIS_TEST_SOURCES) -o $@ $(TEST_LDFLAGS)

$(TEST_PDF_ANALYSIS): tests/test_pdf_analysis.c \
	src/core/pdf_analysis.c src/core/ocr_analysis.c \
	$(DOCUMENT_ANALYSIS_TEST_SOURCES) $(FAKE_DOCUMENT_TOOL)
	$(CC) $(TEST_CFLAGS) -Wpedantic \
		tests/test_pdf_analysis.c src/core/pdf_analysis.c \
		src/core/ocr_analysis.c $(DOCUMENT_ANALYSIS_TEST_SOURCES) \
		-o $@ $(TEST_LDFLAGS)



$(TEST_RELATION_TYPE_NORMALIZER): \
	tests/test_relation_type_normalizer.c \
	src/core/relation_type_normalizer.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ \
		$(shell $(PKG_CONFIG) --libs glib-2.0)

$(TEST_RELATION_TYPE_SERVICE): \
	tests/test_relation_type_service.c \
	src/core/relation_type_service.c \
	src/dao/relation_type_dao.c \
	src/models/relation_type.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

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

$(TEST_EXTRACTION_DROP_SERVICE): \
	tests/test_extraction_drop_service.c \
	src/core/extraction_drop_service.c \
	src/core/file_hash.c \
	src/dao/evidence_dao.c \
	src/dao/entity_dao.c \
	src/dao/evidence_entity_dao.c \
	src/models/evidence_record.c \
	src/models/evidence_observation.c \
	src/models/entity_record.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

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
	src/views/dialog_geometry.c \
	src/models/evidence_type.c \
	src/widgets/evidence_preview_widget.c \
	src/core/evidence_preview_task.c src/core/evidence_preview.c \
	src/core/evidence_video_preview_controller.c \
	src/core/evidence_integrity_verifier.c src/core/file_hash.c \
	src/core/background_task.c src/core/task_manager.c \
	src/core/eml_analyzer.c src/core/eml_mime_extractor.c
	$(CC) $(CFLAGS) $^ \
		-o $@ \
		$(LDFLAGS)

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
	src/models/evidence_observation.c \
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
	src/dao/relation_type_dao.c \
	src/models/relation_type.c \
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
	src/dao/relation_type_dao.c \
	src/models/relation_type.c \
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
	src/dao/relation_type_dao.c \
	src/models/relation_type.c \
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
	src/dao/relation_type_dao.c \
	src/models/relation_type.c \
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
	src/dao/relation_type_dao.c \
	src/models/relation_type.c \
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
	src/models/evidence_observation.c \
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
	src/dao/person_role_assignment_dao.c \
	src/dao/entity_dao.c \
	src/dao/evidence_entity_dao.c \
	src/models/entity_record.c \
	src/models/evidence_observation.c \
	src/models/person_role_assignment.c \
	src/database/database.c \
	src/database/schema.c \
	src/database/statement.c \
	src/database/transaction.c \
	src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_EVIDENCE_PREVIEW): tests/test_evidence_preview.c \
	src/core/evidence_preview.c \
	src/core/evidence_preview_task.c src/core/background_task.c \
	src/core/task_manager.c \
	src/core/evidence_integrity_verifier.c \
	src/core/file_hash.c src/core/eml_analyzer.c \
	src/core/eml_mime_extractor.c src/models/evidence_record.c
	$(CC) $(CFLAGS) -Wpedantic $^ -o $@ $(LDFLAGS) -ljpeg

$(TEST_EVIDENCE_VIDEO_PREVIEW_CONTROLLER): \
	tests/test_evidence_video_preview_controller.c \
	src/core/evidence_video_preview_controller.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_SELECTION_MODEL): tests/test_evidence_selection_model.c \
	src/models/evidence_selection_model.c src/models/evidence_record.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_PERSON_CREATION_GUARD): tests/test_person_creation_guard.c \
	src/core/person_creation_guard.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_PERSON_DIALOG_LIFECYCLE): tests/test_person_dialog_lifecycle.c \
	src/core/person_dialog_lifecycle.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_CREATE_PERSON_DIALOG_GTK): tests/test_create_person_dialog_gtk.c \
	src/views/create_person_dialog.c src/views/dialog_geometry.c \
	src/views/person_vocabulary_adapter.c \
	src/views/person_factual_relation_editor.c \
	src/views/person_ocr_projection_editor.c src/views/person_creation_confirmation.c \
	src/models/person_ocr_projection.c \
	src/core/person_ocr_projection_mapping.c \
	src/views/identity_ocr_option_adapter.c \
	src/core/person_dialog_lifecycle.c \
	src/core/person_confirmation_summary.c \
	src/core/evidence_staging.c src/core/evidence_staging_task.c \
	src/core/evidence_preview_task.c src/core/evidence_preview.c \
	src/core/eml_analyzer.c src/core/eml_mime_extractor.c \
	src/core/evidence_video_preview_controller.c \
	src/core/identity_ocr_preprocessor.c \
	src/core/identity_ocr_workflow.c \
	src/core/identity_field_extractor.c src/core/ocr_analysis.c \
	src/core/document_analysis.c src/core/document_tool_runner.c \
	src/core/tool_registry.c \
	src/models/identity_ocr.c \
	src/widgets/ocr_provenance_overlay.c src/core/ocr_region_geometry.c \
	src/widgets/evidence_preview_widget.c \
	src/core/evidence_integrity_verifier.c src/core/file_hash.c \
	src/core/background_task.c src/core/task_manager.c \
	src/models/evidence_selection_model.c src/models/evidence_record.c \
	src/models/person_role_assignment.c \
	src/models/person_evidence_selection.c \
	src/models/identity_traceability.c src/dao/identity_traceability_dao.c \
	src/database/database.c src/database/schema.c src/database/statement.c \
	src/database/transaction.c src/database/error.c \
	src/core/relation_type_normalizer.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CREATE_PERSON_DIALOG_OCR_GTK): \
	tests/test_create_person_dialog_ocr_gtk.c \
	src/views/create_person_dialog.c src/views/dialog_geometry.c \
	src/views/person_vocabulary_adapter.c \
	src/views/person_factual_relation_editor.c \
	src/views/person_ocr_projection_editor.c src/views/person_creation_confirmation.c \
	src/views/identity_ocr_option_adapter.c \
	src/core/person_dialog_lifecycle.c \
	src/core/person_confirmation_summary.c \
	src/core/evidence_staging.c src/core/evidence_staging_task.c \
	src/core/evidence_preview_task.c src/core/evidence_preview.c \
	src/core/eml_analyzer.c src/core/eml_mime_extractor.c \
	src/core/evidence_video_preview_controller.c \
	src/core/identity_ocr_preprocessor.c \
	src/core/identity_ocr_workflow.c \
	src/core/identity_field_extractor.c src/core/ocr_analysis.c \
	src/core/person_creation_coordinator.c \
	src/models/person_ocr_projection.c src/core/person_ocr_projection_mapping.c \
	src/core/person_ocr_projection_service.c src/dao/person_ocr_projection_dao.c \
	src/core/document_analysis.c src/core/document_tool_runner.c \
	src/core/tool_registry.c src/models/identity_ocr.c \
	src/dao/identity_ocr_dao.c src/dao/entity_dao.c \
	src/dao/evidence_dao.c src/dao/evidence_entity_dao.c \
	src/dao/person_role_assignment_dao.c \
	src/models/entity_record.c src/models/evidence_observation.c \
	src/widgets/ocr_provenance_overlay.c src/core/ocr_region_geometry.c \
	src/widgets/evidence_preview_widget.c \
	src/core/evidence_integrity_verifier.c src/core/file_hash.c \
	src/core/background_task.c src/core/task_manager.c \
	src/models/evidence_selection_model.c src/models/evidence_record.c \
	src/models/person_role_assignment.c \
	src/models/person_evidence_selection.c \
	src/models/identity_traceability.c src/dao/identity_traceability_dao.c \
	src/database/database.c src/database/schema.c src/database/statement.c \
	src/database/transaction.c src/database/error.c \
	src/core/relation_type_normalizer.c $(FAKE_DOCUMENT_TOOL)
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDFLAGS)

$(TEST_EVIDENCE_PREVIEW_WIDGET_GTK): \
	tests/test_evidence_preview_widget_gtk.c \
	src/widgets/evidence_preview_widget.c \
	src/core/evidence_preview_task.c src/core/evidence_preview.c \
	src/core/eml_analyzer.c src/core/eml_mime_extractor.c \
	src/core/evidence_video_preview_controller.c \
	src/core/evidence_integrity_verifier.c src/core/file_hash.c \
	src/core/background_task.c src/core/task_manager.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -ljpeg

$(TEST_PERSON_CONFIRMATION_SUMMARY): \
	tests/test_person_confirmation_summary.c \
	src/core/person_confirmation_summary.c src/models/evidence_record.c \
	src/models/person_evidence_selection.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_PERSON_ROLE_ASSIGNMENT_DAO): \
	tests/test_person_role_assignment_dao.c \
	src/dao/person_role_assignment_dao.c src/models/person_role_assignment.c \
	src/dao/entity_dao.c src/models/entity_record.c \
	src/dao/evidence_dao.c src/models/evidence_record.c \
	src/database/database.c src/database/schema.c src/database/statement.c \
	src/database/transaction.c src/database/error.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_PERSON_EVIDENCE_SELECTION): \
	tests/test_person_evidence_selection.c \
	src/models/person_evidence_selection.c src/models/evidence_record.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EVIDENCE_STAGING): tests/test_evidence_staging.c \
	src/core/evidence_staging.c src/core/file_hash.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_PERSON_CREATION_COORDINATOR): \
	tests/test_person_creation_coordinator.c \
	src/core/person_creation_coordinator.c src/core/evidence_staging.c \
	src/models/person_ocr_projection.c src/core/person_ocr_projection_mapping.c \
	src/core/person_ocr_projection_service.c src/dao/person_ocr_projection_dao.c \
	src/core/file_hash.c \
	src/models/identity_ocr.c src/dao/identity_ocr_dao.c \
	src/models/identity_traceability.c src/dao/identity_traceability_dao.c \
	src/models/person_evidence_selection.c src/models/evidence_record.c \
	src/models/person_role_assignment.c src/models/entity_record.c \
	src/models/evidence_observation.c \
	src/dao/entity_dao.c src/dao/evidence_dao.c \
	src/dao/evidence_entity_dao.c src/dao/person_role_assignment_dao.c \
	src/database/database.c src/database/schema.c src/database/statement.c \
	src/database/transaction.c src/database/error.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_EML_ANALYZER): tests/test_eml_analyzer.c src/core/eml_analyzer.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_EML_INTEGRATION): tests/test_eml_integration.c \
	src/core/eml_integration.c src/core/controlled_vocab.c \
	src/dao/entity_dao.c src/dao/evidence_entity_dao.c \
	src/models/entity_record.c src/models/evidence_observation.c \
	src/database/database.c src/database/schema.c \
	src/database/statement.c src/database/transaction.c src/database/error.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lsqlite3

$(TEST_IBAN_ANALYZER): tests/test_iban_analyzer.c src/core/iban_analyzer.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_FINANCIAL_FOUNDATION): tests/test_financial_foundation.c \
	src/models/financial_foundation.c src/core/bank_text_extractor.c \
	src/core/bic_validator.c src/core/french_bban.c \
	src/core/iban_analyzer.c src/core/bank_proposal.c src/core/controlled_vocab.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BANK_STRUCTURED_EXTRACTOR): tests/test_bank_structured_extractor.c \
	src/models/bank_analysis.c src/models/financial_foundation.c \
	src/core/bank_structured_extractor.c src/core/bank_analysis_revision.c \
	src/core/bank_text_extractor.c src/core/bic_validator.c \
	src/core/iban_analyzer.c
	$(CC) $(TEST_CFLAGS) -Wpedantic $^ -o $@ $(TEST_LDFLAGS)

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
	src/dao/relation_type_dao.c \
	src/models/relation_type.c \
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
	$(TEST_EVIDENCE_PREVIEW) \
	$(TEST_EVIDENCE_VIDEO_PREVIEW_CONTROLLER) \
	$(TEST_EVIDENCE_SELECTION_MODEL) \
	$(TEST_PERSON_CREATION_GUARD) \
	$(TEST_PERSON_DIALOG_LIFECYCLE) \
	$(TEST_CREATE_PERSON_DIALOG_GTK) \
	$(TEST_CREATE_PERSON_DIALOG_OCR_GTK) \
	$(TEST_EVIDENCE_PREVIEW_WIDGET_GTK) \
	$(TEST_PERSON_CONFIRMATION_SUMMARY) \
	$(TEST_PERSON_ROLE_ASSIGNMENT_DAO) \
	$(TEST_PERSON_EVIDENCE_SELECTION) \
	$(TEST_EVIDENCE_STAGING) \
	$(TEST_PERSON_CREATION_COORDINATOR) \
	$(TEST_EML_ANALYZER) \
	$(TEST_EML_INTEGRATION) \
	$(TEST_IBAN_ANALYZER) \
	$(TEST_FINANCIAL_FOUNDATION) \
	$(TEST_BANK_STRUCTURED_EXTRACTOR) \
	$(TEST_EXIFTOOL_METADATA) \
	$(TEST_PDF_PASSWORD_RECOVERY) \
	$(TEST_EXTRACTION_DROP_SERVICE) \
	$(TEST_RELATION_TYPE_NORMALIZER) \
	$(TEST_RELATION_TYPE_SERVICE) \
	$(TEST_CONTROLLED_VOCAB) \
	$(TEST_BANK_PROPOSAL) \
	$(TEST_EML_PIPELINE_TASK) \
	$(TEST_EML_MIME_EXTRACTOR) \
	$(TEST_EXIFTOOL_ANALYSIS) \
	$(TEST_OCR_ANALYSIS) \
	$(TEST_PDF_ANALYSIS) \
	$(TEST_DOCUMENT_TOOL_RUNNER) \
	$(TEST_IDENTITY_OCR) \
	$(TEST_IDENTITY_OCR_PREPROCESSOR) \
	$(TEST_IDENTITY_TRACEABILITY) \
	$(TEST_OCR_PROVENANCE_OVERLAY_GTK) \
	$(TEST_EVIDENCE_METADATA_DIALOG_GTK) \
	$(TEST_EVIDENCE_IDENTITY_IMPORT_GTK) \
	$(TEST_WORKSPACE_IDENTITY_OCR_GTK) \
	$(TEST_DIALOG_GEOMETRY_GTK) \
	$(TEST_PERSON_FACTUAL_RELATION_EDITOR_GTK) \
	$(TEST_DOCUMENT_AUTHENTICITY_EDITOR_GTK) \
	$(TEST_DOCUMENT_IDENTITY_MISUSE_EDITOR_GTK) \
	$(TEST_PERSON_DETAILS_TRACEABILITY_GTK) \
	$(TEST_PERSON_OCR_PROJECTION) \
	$(TEST_PERSON_OCR_PROJECTION_EDITOR_GTK)
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
	@$(TEST_EVIDENCE_PREVIEW)
	@$(TEST_EVIDENCE_VIDEO_PREVIEW_CONTROLLER)
	@$(TEST_EVIDENCE_SELECTION_MODEL)
	@$(TEST_PERSON_CREATION_GUARD)
	@$(TEST_PERSON_DIALOG_LIFECYCLE)
	@$(TEST_CREATE_PERSON_DIALOG_GTK)
	@$(TEST_CREATE_PERSON_DIALOG_OCR_GTK)
	@$(TEST_EVIDENCE_PREVIEW_WIDGET_GTK)
	@$(TEST_PERSON_CONFIRMATION_SUMMARY)
	@$(TEST_PERSON_ROLE_ASSIGNMENT_DAO)
	@$(TEST_PERSON_EVIDENCE_SELECTION)
	@$(TEST_EVIDENCE_STAGING)
	@$(TEST_PERSON_CREATION_COORDINATOR)
	@$(TEST_EML_ANALYZER)
	@$(TEST_EML_INTEGRATION)
	@$(TEST_IBAN_ANALYZER)
	@$(TEST_FINANCIAL_FOUNDATION)
	@$(TEST_BANK_STRUCTURED_EXTRACTOR)
	@$(TEST_EXIFTOOL_METADATA)
	@$(TEST_PDF_PASSWORD_RECOVERY)
	@$(TEST_EXTRACTION_DROP_SERVICE)
	@$(TEST_RELATION_TYPE_NORMALIZER)
	@$(TEST_RELATION_TYPE_SERVICE)
	@$(TEST_CONTROLLED_VOCAB)
	@$(TEST_BANK_PROPOSAL)
	@$(TEST_EML_PIPELINE_TASK)
	@$(TEST_EML_MIME_EXTRACTOR)
	@$(TEST_EXIFTOOL_ANALYSIS)
	@$(TEST_OCR_ANALYSIS)
	@$(TEST_PDF_ANALYSIS)
	@$(TEST_DOCUMENT_TOOL_RUNNER)
	@$(TEST_IDENTITY_OCR)
	@$(TEST_IDENTITY_OCR_PREPROCESSOR)
	@$(TEST_IDENTITY_TRACEABILITY)
	@$(TEST_OCR_PROVENANCE_OVERLAY_GTK)
	@$(TEST_EVIDENCE_METADATA_DIALOG_GTK)
	@$(TEST_EVIDENCE_IDENTITY_IMPORT_GTK)
	@$(TEST_WORKSPACE_IDENTITY_OCR_GTK)
	@$(TEST_DIALOG_GEOMETRY_GTK)
	@$(TEST_PERSON_FACTUAL_RELATION_EDITOR_GTK)
	@$(TEST_DOCUMENT_AUTHENTICITY_EDITOR_GTK)
	@$(TEST_DOCUMENT_IDENTITY_MISUSE_EDITOR_GTK)
	@$(TEST_PERSON_DETAILS_TRACEABILITY_GTK)
	@$(TEST_PERSON_OCR_PROJECTION)
	@$(TEST_PERSON_OCR_PROJECTION_EDITOR_GTK)
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
		$(TEST_EVIDENCE_PREVIEW) \
		$(TEST_EVIDENCE_VIDEO_PREVIEW_CONTROLLER) \
		$(TEST_EVIDENCE_SELECTION_MODEL) \
		$(TEST_PERSON_CREATION_GUARD) \
		$(TEST_PERSON_DIALOG_LIFECYCLE) \
		$(TEST_CREATE_PERSON_DIALOG_GTK) \
		$(TEST_CREATE_PERSON_DIALOG_OCR_GTK) \
		$(TEST_EVIDENCE_PREVIEW_WIDGET_GTK) \
		$(TEST_PERSON_CONFIRMATION_SUMMARY) \
		$(TEST_PERSON_ROLE_ASSIGNMENT_DAO) \
		$(TEST_PERSON_EVIDENCE_SELECTION) \
		$(TEST_EVIDENCE_STAGING) \
		$(TEST_PERSON_CREATION_COORDINATOR) \
		$(TEST_EML_ANALYZER) \
		$(TEST_EML_INTEGRATION) \
		$(TEST_EXTRACTION_DROP_SERVICE) \
		$(TEST_RELATION_TYPE_NORMALIZER) \
		$(TEST_RELATION_TYPE_SERVICE) \
		$(TEST_CONTROLLED_VOCAB) \
		$(TEST_BANK_PROPOSAL) \
		$(TEST_FINANCIAL_FOUNDATION) \
		$(TEST_BANK_STRUCTURED_EXTRACTOR) \
		$(TEST_EML_PIPELINE_TASK) \
		$(TEST_EML_MIME_EXTRACTOR) \
		$(TEST_EXIFTOOL_ANALYSIS) \
		$(TEST_OCR_ANALYSIS) \
		$(TEST_PDF_ANALYSIS) \
		$(TEST_DOCUMENT_TOOL_RUNNER) \
	$(TEST_IDENTITY_OCR) \
	$(TEST_IDENTITY_OCR_PREPROCESSOR) \
	$(TEST_IDENTITY_TRACEABILITY) \
	$(TEST_OCR_PROVENANCE_OVERLAY_GTK) \
	$(TEST_EVIDENCE_METADATA_DIALOG_GTK) \
	$(TEST_EVIDENCE_IDENTITY_IMPORT_GTK) \
	$(TEST_WORKSPACE_IDENTITY_OCR_GTK) \
	$(TEST_DIALOG_GEOMETRY_GTK) \
	$(TEST_PERSON_FACTUAL_RELATION_EDITOR_GTK) \
	$(TEST_DOCUMENT_AUTHENTICITY_EDITOR_GTK) \
	$(TEST_DOCUMENT_IDENTITY_MISUSE_EDITOR_GTK) \
	$(TEST_PERSON_DETAILS_TRACEABILITY_GTK) \
	$(TEST_PERSON_OCR_PROJECTION) \
	$(TEST_PERSON_OCR_PROJECTION_EDITOR_GTK) \
	$(FAKE_DOCUMENT_TOOL)



-include $(DEP)

.PHONY: clean run test
