# Audit du schéma SQLite courant — V20

> **Projet :** Labfy Investigation
> **Date de l’audit :** 2026-08-02
> **Branche auditée :** `main`
> **Version courante :** V20
> **Statut :** audit courant ; les audits V9 et V10 restent archivés dans
> `docs/database/audits/`.

## 1. Sources de vérité

La version 20 est définie par `DATABASE_SCHEMA_VERSION_CURRENT` et
`DATABASE_SCHEMA_VERSION_CURRENT_TEXT` dans `src/database/database.c`.
L’état courant est décrit par `database/schema_current.sql`; la migration
V18→V19 est `database/schema_v19.sql` et V19→V20 est
`database/schema_v20.sql`. Les fonctions publiques correspondantes
sont déclarées dans `include/database/schema.h` et implémentées dans
`src/database/schema.c`.

Une base neuve reçoit directement le schéma courant V20 dans la transaction
d’initialisation. Une base historique est migrée séquentiellement ; le passage
chaque migration installe son script et ne met à jour `metadata.schema_version`
qu’avant le `COMMIT`. Un échec entraîne un `ROLLBACK`.

## 2. Extensions d’identité V18 conservées

V18 ajoute les vocabulaires `identification_status_vocabulary` et
`person_role_vocabulary`, les évaluations humaines
`document_authenticity_assessments`, les relations factuelles
`person_evidence_factual_relations` et l’historique
`person_identification_assessments`.

`identity_field_observations` conserve séparément :

- `raw_value`, immuable après extraction ;
- `normalized_value` ;
- `corrected_value` ;
- `confirmed_value` ;
- `confirmation_state` ;
- `value_quality`.

Les triggers V18 refusent une confirmation sans action humaine ainsi que les
valeurs rejetées, en conflit, incertaines ou invalides. Les clés et triggers
vérifient aussi la cohérence preuve/OcrRun des évaluations d’authenticité et
des relations factuelles.

## 3. Extension V19

### `person_profile_fields`

Cette table porte les champs structurés d’une personne :

- `declared_name` ;
- `surname` ;
- `given_names` ;
- `birth_date` ;
- `birth_place` ;
- `nationality` ;
- `sex_as_printed` ;
- `address_as_printed`.

La clé primaire `(person_id, field_code)`, la liste fermée des codes, la valeur
non vide et la clé étrangère vers `entites` bornent les écritures.

### `person_ocr_field_projections`

Chaque ligne append-only conserve : personne, champ cible, valeur précédente,
nouvelle valeur, preuve, OcrRun, champ OCR, code OCR, qualité, statut de
révision, stratégie, date UTC et origine humaine. Les stratégies persistées
sont `fill_empty` et `replace_existing`; les qualités sont `complete` ou
`partial`; les statuts sont `accepted` ou `modified`.

L’index `idx_person_projection_person` sert la consultation chronologique.
Le trigger `projection_v19_consistency` relit le champ OCR lors de l’insertion
et refuse toute source devenue non projectable ou incohérente.

## 4. Atomicité et provenance

La V20 ajoute `document_identity_misuse_assessments`. Les trois états fermés
sont `indeterminate`, `presumed` et `confirmed`. Une justification est imposée
pour les deux derniers. Les triggers vérifient l’OcrRun, la chaîne précédente,
interdisent les branches ainsi que tout `UPDATE` ou `DELETE`. L’origine
persistée est exclusivement `human`.

`PersonCreationCoordinator` englobe dans une transaction la personne, ses
rôles, les preuves et rattachements, les observations OCR, les relations
factuelles explicites, les champs structurés et leur provenance V19. Les
copies définitives de preuves disposent d’une compensation si SQLite échoue.
Le service de projection peut rejoindre une transaction existante ou en ouvrir
une lorsqu’il est appelé seul.

Les historiques d’authenticité, de relations factuelles et de projections ne
sont ni remplacés ni supprimés silencieusement. Les originaux et les lignes
OCR sources restent inchangés par une projection.

## 5. Création, migration et réouverture vérifiées

Les tests couvrent :

- création directe d’une base V20 et présence des extensions V19/V20 ;
- migration historique jusqu’à V20 ;
- migration V17→V18 avec conservation OCR ;
- migration V18→V19 ;
- migration V19→V20 et rollback en cas d’échec ;
- réouverture SQLite ;
- cohérence preuve/OcrRun/champ ;
- source devenue obsolète ;
- échec au milieu d’une série ;
- rollback sans profil ni provenance partiels.

Commandes de validation :

```sh
make clean
make -j8
make check-source-size
DISPLAY="$DISPLAY" WAYLAND_DISPLAY="$WAYLAND_DISPLAY" make -j8 test
git diff --check
```

Les fixtures sont exclusivement synthétiques (`SPECIMEN`) et les bases sont
créées dans des répertoires temporaires.

## 6. Limites fonctionnelles hors schéma

La collecte des justifications des rôles sensibles et l’usage plus complet de
`person_identification_assessments` restent des renforcements architecturaux,
pas des exigences contractuelles explicites du ticket #109. Les états d’usage
d’identité présumé et confirmé sont désormais modélisés séparément en V20 et
ne sont pas confondus avec l’authenticité documentaire.
