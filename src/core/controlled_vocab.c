/******************************************************************************
 * @file controlled_vocab.c
 * @brief Implémentation du vocabulaire contrôlé pour les domaines fermés.
 ******************************************************************************/
#include "core/controlled_vocab.h"
#include <string.h>

static const ControlledVocabItem VERIFICATION_STATUSES[] = {
    { "proposed",   "Proposé" },
    { "confirmed",  "Confirmé" },
    { "rejected",   "Rejeté" },
    { "conflicted", "Contradictoire" },
    { "invalid",    "Invalide" }
};
static const size_t VERIFICATION_STATUSES_COUNT = sizeof(VERIFICATION_STATUSES) / sizeof(VERIFICATION_STATUSES[0]);

static const ControlledVocabItem PROVENANCE_KINDS[] = {
    { "observed",  "Observé directement" },
    { "ocr",       "Extrait par OCR" },
    { "header",    "Extrait d'un en-tête" },
    { "metadata",  "Extrait des métadonnées" },
    { "derived",   "Dérivé localement" },
    { "manual",    "Saisi manuellement" }
};
static const size_t PROVENANCE_KINDS_COUNT = sizeof(PROVENANCE_KINDS) / sizeof(PROVENANCE_KINDS[0]);

static const ControlledVocabItem EMAIL_ROLES[] = {
    { "from",               "Expéditeur principal (From)" },
    { "sender",             "Expéditeur réel (Sender)" },
    { "reply_to",           "Adresse de réponse (Reply-To)" },
    { "return_path",        "Adresse de retour (Return-Path)" },
    { "to",                 "Destinataire principal (To)" },
    { "cc",                 "Copie (Cc)" },
    { "bcc",                "Copie cachée (Bcc)" },
    { "message_id_domain",  "Domaine Message-ID" },
    { "other",              "Autre rôle" }
};
static const size_t EMAIL_ROLES_COUNT = sizeof(EMAIL_ROLES) / sizeof(EMAIL_ROLES[0]);

static const ControlledVocabItem SMTP_ROLES[] = {
    { "declared_source",       "Source déclarée" },
    { "smtp_relay",            "Relais SMTP" },
    { "destination_server",    "Serveur destinataire" },
    { "private_infrastructure","Infrastructure locale/privée" },
    { "unknown",               "Rôle indéterminé" }
};
static const size_t SMTP_ROLES_COUNT = sizeof(SMTP_ROLES) / sizeof(SMTP_ROLES[0]);

static const ControlledVocabItem VALUE_TYPES[] = {
    { "text",       "Texte" },
    { "identifier", "Identifiant" },
    { "email",      "Adresse e-mail" },
    { "iban",       "IBAN" },
    { "bic",        "BIC / SWIFT" },
    { "ip_address", "Adresse IP" },
    { "domain",     "Nom de domaine" },
    { "uri",        "URI / URL" },
    { "integer",    "Nombre entier" },
    { "decimal",    "Nombre décimal" },
    { "date",       "Date" },
    { "datetime",   "Date et heure" },
    { "boolean",    "Booléen" },
    { "json",       "JSON" }
};
static const size_t VALUE_TYPES_COUNT = sizeof(VALUE_TYPES) / sizeof(VALUE_TYPES[0]);

const ControlledVocabItem *controlled_vocab_get_items(ControlledVocabCategory category,
                                                       size_t *out_count)
{
    switch (category)
    {
        case CONTROLLED_VOCAB_VERIFICATION_STATUS:
            if (out_count != NULL) *out_count = VERIFICATION_STATUSES_COUNT;
            return VERIFICATION_STATUSES;
        case CONTROLLED_VOCAB_PROVENANCE_KIND:
            if (out_count != NULL) *out_count = PROVENANCE_KINDS_COUNT;
            return PROVENANCE_KINDS;
        case CONTROLLED_VOCAB_EMAIL_ROLE:
            if (out_count != NULL) *out_count = EMAIL_ROLES_COUNT;
            return EMAIL_ROLES;
        case CONTROLLED_VOCAB_SMTP_ROLE:
            if (out_count != NULL) *out_count = SMTP_ROLES_COUNT;
            return SMTP_ROLES;
        case CONTROLLED_VOCAB_VALUE_TYPE:
            if (out_count != NULL) *out_count = VALUE_TYPES_COUNT;
            return VALUE_TYPES;
        default:
            if (out_count != NULL) *out_count = 0;
            return NULL;
    }
}

gboolean controlled_vocab_is_valid_code(ControlledVocabCategory category,
                                        const char *code)
{
    size_t count = 0;
    const ControlledVocabItem *items = NULL;

    if (code == NULL || code[0] == '\0')
        return FALSE;

    items = controlled_vocab_get_items(category, &count);
    if (items == NULL)
        return FALSE;

    for (size_t i = 0; i < count; i++)
    {
        if (g_ascii_strcasecmp(items[i].code, code) == 0)
            return TRUE;
    }

    return FALSE;
}

const char *controlled_vocab_get_label(ControlledVocabCategory category,
                                       const char *code)
{
    size_t count = 0;
    const ControlledVocabItem *items = NULL;

    if (code == NULL || code[0] == '\0')
        return NULL;

    items = controlled_vocab_get_items(category, &count);
    if (items == NULL)
        return NULL;

    for (size_t i = 0; i < count; i++)
    {
        if (g_ascii_strcasecmp(items[i].code, code) == 0)
            return items[i].label;
    }

    return NULL;
}

const char *controlled_vocab_get_code_from_label(ControlledVocabCategory category,
                                                 const char *label)
{
    size_t count = 0;
    const ControlledVocabItem *items = NULL;

    if (label == NULL || label[0] == '\0')
        return NULL;

    items = controlled_vocab_get_items(category, &count);
    if (items == NULL)
        return NULL;

    for (size_t i = 0; i < count; i++)
    {
        if (g_ascii_strcasecmp(items[i].label, label) == 0)
            return items[i].code;
    }

    return NULL;
}

gboolean controlled_vocab_is_valid_verification_status(const char *code)
{
    return controlled_vocab_is_valid_code(CONTROLLED_VOCAB_VERIFICATION_STATUS, code);
}

gboolean controlled_vocab_is_valid_provenance_kind(const char *code)
{
    return controlled_vocab_is_valid_code(CONTROLLED_VOCAB_PROVENANCE_KIND, code);
}

gboolean controlled_vocab_is_valid_email_role(const char *code)
{
    return controlled_vocab_is_valid_code(CONTROLLED_VOCAB_EMAIL_ROLE, code);
}

gboolean controlled_vocab_is_valid_smtp_role(const char *code)
{
    return controlled_vocab_is_valid_code(CONTROLLED_VOCAB_SMTP_ROLE, code);
}

gboolean controlled_vocab_is_valid_value_type(const char *code)
{
    return controlled_vocab_is_valid_code(CONTROLLED_VOCAB_VALUE_TYPE, code);
}
