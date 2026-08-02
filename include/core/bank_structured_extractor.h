#ifndef LABFY_INVESTIGATION_BANK_STRUCTURED_EXTRACTOR_H
#define LABFY_INVESTIGATION_BANK_STRUCTURED_EXTRACTOR_H

#include "models/bank_analysis.h"

G_BEGIN_DECLS

/* Analyse pure et déterministe. Le résultat possède toutes ses données.
 * NULL et le texte vide produisent un résultat vide. Une valeur factuelle
 * dépassant BANK_ANALYSIS_MAX_VALUE_BYTES fait échouer l'analyse. */
/* Un montant exige un marqueur explicite ou une devise adjacente. Sa séquence
 * numérique est délimitée sans copie et la ponctuation périphérique est
 * exclue. Les groupes par espaces ou séparateurs ponctués doivent respecter
 * 1-3 chiffres puis des groupes de 3. Les formes à séparateur unique suivi de
 * trois chiffres (1,234 et 1.234) restent ambiguës. Les formes incohérentes
 * restent entières, invalides et sans normalisation. Le contexte puis toutes
 * les limites sont contrôlés avant toute allocation proportionnelle. */
BankAnalysisResult *bank_structured_extractor_analyze(const char *text,
    GError **error);

G_END_DECLS
#endif
