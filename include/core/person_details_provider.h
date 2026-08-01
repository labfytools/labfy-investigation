#ifndef PERSON_DETAILS_PROVIDER_H
#define PERSON_DETAILS_PROVIDER_H

#include <glib.h>
#include "database/database.h"

GPtrArray *person_details_provider_get_evidences(Database *database, const char *person_identifier, GError **error);
GPtrArray *person_details_provider_get_factual_relations(Database *database, const char *person_identifier, GError **error);

#endif
