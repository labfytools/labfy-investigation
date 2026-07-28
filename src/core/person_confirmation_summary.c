#include "core/person_confirmation_summary.h"

static const char *optional(const char *value, const char *fallback)
{
    return value != NULL && value[0] != '\0' ? value : fallback;
}

static const char *integrity(EvidenceIntegrityStatus status)
{
    switch (status) {
        case EVIDENCE_INTEGRITY_STATUS_VALID: return "Valide";
        case EVIDENCE_INTEGRITY_STATUS_MISSING: return "Fichier absent";
        case EVIDENCE_INTEGRITY_STATUS_MODIFIED: return "Invalide";
        case EVIDENCE_INTEGRITY_STATUS_ERROR: return "Erreur";
        default: return "Non vérifiée";
    }
}

char *person_confirmation_summary_build(const char *designation,
    const char *declared_name, const char *pseudonym,
    const char *identification_status, gint confidence, const char *notes,
    const GPtrArray *role_labels, const EvidenceRecord *evidence)
{
    GString *summary = g_string_new(NULL);
    char *size = NULL, *short_sha = NULL;
    g_string_append_printf(summary,
        "PERSONNE\nDésignation : %s\nNom déclaré : %s\nPseudonyme : %s\n"
        "Statut d’identification : %s\nConfiance : %d %%\n"
        "Notes factuelles : %s\n\nRÔLES\n",
        optional(designation, "Non renseignée"),
        optional(declared_name, "Non renseigné"),
        optional(pseudonym, "Non renseigné"),
        optional(identification_status, "Non renseigné"),
        confidence, optional(notes, "Aucune"));
    if (role_labels == NULL || role_labels->len == 0)
        g_string_append(summary, "Aucun rôle sélectionné\n");
    else
        for (guint i = 0; i < role_labels->len; i++)
            g_string_append_printf(summary, "• %s — preuve : %s\n",
                optional(g_ptr_array_index((GPtrArray *) role_labels, i),
                    "Rôle non renseigné"),
                evidence != NULL
                    ? optional(evidence_record_get_original_name(evidence),
                        "Non renseignée")
                    : "Aucune");
    g_string_append(summary, "\nPREUVE PRINCIPALE\n");
    if (evidence == NULL)
        g_string_append(summary, "Aucune preuve principale associée.\n");
    else {
        size = g_format_size(evidence_record_get_size_bytes(evidence));
        short_sha = g_strndup(evidence_record_get_sha256(evidence), 12);
        g_string_append_printf(summary,
            "Nom original : %s\nType métier : %s\nType MIME : %s\n"
            "Taille : %s\nDate d’import : %s\nSHA-256 : %s…\n"
            "Intégrité : %s\nDescription : %s\n",
            optional(evidence_record_get_original_name(evidence),
                "Non renseigné"),
            optional(evidence_record_get_type_label(evidence),
                evidence_record_get_type_identifier(evidence)),
            optional(evidence_record_get_mime_type(evidence),
                "Non renseigné"),
            size, optional(evidence_record_get_imported_at(evidence),
                "Non renseignée"),
            optional(short_sha, "Non renseigné"),
            integrity(evidence_record_get_integrity_status(evidence)),
            optional(evidence_record_get_description(evidence), "Aucune"));
    }
    g_string_append(summary,
        "\nAVERTISSEMENTS\n"
        "Aucune écriture n’a encore été effectuée.\n"
        "Les informations seront créées seulement après un clic sur "
        "« Créer la personne ».\n"
        "Aucune identité réelle ni relation avec un auteur n’est inférée "
        "automatiquement.");
    g_free(size);
    g_free(short_sha);
    return g_string_free(summary, FALSE);
}
