# Audit du schéma SQLite V9

> [!IMPORTANT]
> Ce document audite la V9 visible sur la branche publique au commit indiqué.
> Il ne doit pas être utilisé comme audit du schéma V10 local sans une nouvelle
> vérification du code, des migrations et des tests V10.

> **Projet :** Labfy Investigation  
> **Date de l’audit :** 2026-07-24  
> **Branche auditée :** `main` publique sur Forgejo  
> **Commit audité :** `8bc3b43d63f4bd3da673ddf86a7e4b7a81e28604`  
> **Version de schéma confirmée par le code public :** **V9**  
> **Statut du document :** audit historique vérifié de la V9 publique  
> **Périmètre :** schémas SQL, mécanisme de migration, couche Database, DAO principaux, tests et tickets Forgejo associés

---

## Avertissement important concernant la V10

Le propriétaire du projet indique que le dépôt de travail local est déjà en **V10**.

Au moment de cet audit, la branche `main` publique consultée sur Forgejo expose toutefois :

- `DATABASE_SCHEMA_VERSION_CURRENT 9` dans `src/database/database.c` ;
- des scripts versionnés allant de `database/schema_v1.sql` à
  `database/schema_v9.sql` ;
- `database/schema_current.sql` explicitement présenté comme le schéma courant
  V9 ;
- aucun fichier `schema_v10.sql` dans la branche publique auditée.

Ce document décrit donc **l’état public V9 vérifiable**, et non la V10 locale
non encore accessible dans les sources consultées.

Il ne doit pas être présenté comme l’audit définitif de la V10 tant que les
fichiers suivants n’ont pas été publiés ou fournis :

- le script de migration V10 ;
- les fonctions C d’installation et de migration vers V10 ;
- la nouvelle constante de version ;
- les tests de création et de migration V10 ;
- les tickets et commits correspondants.

---

# 1. Résumé exécutif

Le schéma public de Labfy Investigation est un schéma SQLite versionné, propre
à chaque enquête et construit autour de quatre objectifs principaux :

1. préserver les preuves et leurs métadonnées ;
2. structurer les entités et leurs relations ;
3. conserver la provenance des traitements OSINT ;
4. permettre l’évolution du modèle par migrations transactionnelles.

La version publique courante est la V9.

Le mécanisme général est cohérent :

- la version est stockée dans `metadata.schema_version` ;
- une base plus récente que l’application est refusée ;
- chaque migration possède une fonction dédiée ;
- chaque migration s’exécute dans une transaction ;
- la version n’est mise à jour qu’après l’application du SQL ;
- un échec provoque un rollback ;
- une nouvelle base reçoit immédiatement V1 à V9 dans une seule transaction ;
- `schema_current.sql` ajoute des extensions idempotentes à chaque ouverture.

Les points les plus solides sont :

- activation explicite des clés étrangères ;
- usage des requêtes préparées pour les valeurs variables ;
- migration V9 conservatrice des types de relations ;
- contrôle `PRAGMA foreign_key_check` avant validation de la V9 ;
- tests de création d’une base neuve ;
- test de migration d’une ancienne base V1 ;
- test de rollback d’une migration V2 défaillante.

Les principales faiblesses documentaires ou techniques observées sont :

- la documentation principale de la base reste annoncée comme « stable V1 »
  alors que le code public est en V9 ;
- la version courante est dupliquée sous forme numérique et textuelle dans un
  fichier C privé ;
- les tests de migration ne couvrent pas systématiquement chaque version et
  chaque rollback ;
- `schema_current.sql` mélange rattrapage idempotent et migration de données de
  présentation ;
- aucune sauvegarde préalable automatique n’a été observée dans le mécanisme de
  migration consulté ;
- la V10 locale ne peut pas être auditée tant qu’elle n’est pas disponible.

---

# 2. Méthode et hiérarchie des sources

L’audit applique l’ordre de confiance suivant :

1. code présent dans la branche auditée ;
2. scripts de schéma et fonctions de migration ;
3. tests automatisés ;
4. commits associés ;
5. tickets Forgejo fermés ;
6. documentation d’architecture ;
7. anciens audits et feuilles de route.

Cette hiérarchie est nécessaire parce qu’un document historique peut décrire
une intention ou une ancienne version sans représenter l’état courant.

## 2.1 Sources principales examinées

### Schémas

- `database/schema_v1.sql`
- `database/schema_v2.sql`
- `database/schema_v3.sql`
- `database/schema_v4.sql`
- `database/schema_v5.sql`
- `database/schema_v6.sql`
- `database/schema_v7.sql`
- `database/schema_v8.sql`
- `database/schema_v9.sql`
- `database/schema_current.sql`

### Infrastructure SQLite

- `src/database/database.c`
- `src/database/schema.c`
- `src/database/statement.c`
- `src/database/transaction.c`
- `include/database/database.h`
- `include/database/schema.h`

### Accès métier et modèles

- contenu de `include/dao/`
- contenu de `src/dao/`
- contenu de `include/models/`
- modules liés aux preuves, entités, relations, provenance OSINT, extractions
  et positions du graphe

### Tests

- `tests/test_database.c`
- `tests/test_statement.c`
- `tests/test_transaction.c`
- tests des DAO et services visibles dans `tests/`
- tests des types canoniques de relations

### Documentation et tickets

- `docs/database/DATABASE_ARCHITECTURE.md`
- `docs/database/SCHEMA_AUDIT_V1.md`
- ticket Forgejo `#106` : normalisation des types de relations
- commit `8bc3b43d63` : centralisation des types de relations

## 2.2 Limites de l’audit

Cet audit est une analyse statique du dépôt public.

Il n’a pas exécuté :

- `make -j8` ;
- la suite de tests ;
- une migration réelle sur une base synthétique ;
- `PRAGMA integrity_check` sur une base V9 produite localement ;
- `PRAGMA foreign_key_check` sur une base V9 produite localement ;
- une comparaison binaire entre une base migrée et une base fraîche.

Les procédures de vérification reproductible sont proposées plus loin.

---

# 3. Source de vérité de la version

## 3.1 Constante courante

La source de vérité publique se trouve dans :

```text
src/database/database.c
```

avec les deux définitions :

```c
#define DATABASE_SCHEMA_VERSION_CURRENT 9
#define DATABASE_SCHEMA_VERSION_CURRENT_TEXT "9"
```

La première sert aux comparaisons numériques.

La seconde est enregistrée dans :

```text
metadata.schema_version
```

## 3.2 Lecture de la version

La fonction :

```c
database_read_schema_version()
```

effectue les contrôles suivants :

- prépare une requête vers `metadata` ;
- lie la clé `schema_version` ;
- refuse l’absence de valeur ;
- refuse une chaîne vide ;
- convertit la valeur en entier ;
- refuse une valeur non numérique ;
- refuse une version inférieure à 1 ;
- refuse une valeur supérieure à `G_MAXINT` ;
- vérifie qu’aucune seconde ligne n’est retournée.

## 3.3 Base plus récente que l’application

La fonction :

```c
database_migrate_to_latest()
```

refuse une base dont la version est supérieure à :

```c
DATABASE_SCHEMA_VERSION_CURRENT
```

Cela évite qu’une ancienne version de Labfy Investigation ouvre et modifie une
base créée par une version plus récente.

## 3.4 Version inconnue ou non migrable

La boucle de migration utilise un `switch` sur la version actuelle.

Une version ancienne qui ne possède aucun chemin connu produit une erreur
d’état au lieu d’essayer une transformation implicite.

## 3.5 Mise à jour de la version

La fonction :

```c
database_update_schema_version()
```

utilise une requête préparée et met à jour la clé `schema_version`.

Dans chaque migration examinée, cette mise à jour intervient après
l’installation de la nouvelle structure et avant le `COMMIT`.

En cas d’erreur, la transaction est annulée et la version ne doit pas avancer.

---

# 4. Installation d’une base neuve

La fonction :

```c
database_initialize()
```

crée une base neuve dans une transaction initiale unique.

L’ordre public V9 est le suivant :

1. ouverture de SQLite ;
2. activation de `PRAGMA foreign_keys = ON` ;
3. début de transaction ;
4. installation de V1 ;
5. installation de V2 ;
6. installation de V3 ;
7. installation de V4 ;
8. installation de V5 ;
9. installation de V6 ;
10. installation de V7 ;
11. installation de V8 ;
12. installation de V9 ;
13. application de `schema_current.sql` ;
14. insertion des métadonnées ;
15. insertion de l’enquête ;
16. commit.

Un échec provoque un rollback de l’ensemble.

## Observation

Une base neuve rejoue toute l’histoire du schéma.

Cette méthode garantit qu’une base neuve et une base migrée utilisent les mêmes
étapes. Elle impose cependant que chaque ancien script reste compatible avec
le moteur SQLite utilisé aujourd’hui.

À moyen terme, le projet pourra envisager un schéma de référence consolidé pour
les bases neuves, tout en conservant les migrations historiques pour les
anciennes bases. Ce changement ne doit être réalisé qu’avec des tests comparant
strictement les deux chemins.

---

# 5. Chaîne publique des migrations

| Passage | Script | Fonction C | Objet principal |
|---|---|---|---|
| création | `schema_v1.sql` | `schema_install_v1()` | socle métier complet |
| V1 → V2 | `schema_v2.sql` | `schema_install_v2()` | renforcement des preuves |
| V2 → V3 | `schema_v3.sql` | `schema_install_v3()` | provenance OSINT |
| V3 → V4 | `schema_v4.sql` | `schema_install_v4()` | comptes sociaux |
| V4 → V5 | `schema_v5.sql` | `schema_install_v5()` | rôles des personnes |
| V5 → V6 | `schema_v6.sql` | `schema_install_v6()` | identité usurpée |
| V6 → V7 | `schema_v7.sql` | `schema_install_v7()` | extractions |
| V7 → V8 | `schema_v8.sql` | `schema_install_v8()` | viewport du graphe |
| V8 → V9 | `schema_v9.sql` + migration C | `schema_install_v9()` | types canoniques de relations |
| complément | `schema_current.sql` | `schema_ensure_current()` | extensions idempotentes V9 |

## 5.1 V1 — Socle métier

La V1 crée notamment :

### Métadonnées

- `metadata`
- `investigation`

### Classification et référentiels

- `categories`
- `tags`
- `types_preuve`
- `types_entite`
- `types_source`
- `types_outil`

### Collecte

- `sources`
- `preuves`
- `recherches`

### Connaissance

- `entites`
- `relations`

### Raisonnement et traçabilité

- `hypotheses`
- `chronologie`
- `journal`

### Tables de liaison

- `tag_preuves`
- `tag_recherches`
- `tag_entites`
- `tag_relations`
- `tag_hypotheses`
- `tag_chronologie`
- `recherche_preuves`
- `recherche_entites`
- `preuve_entites`
- `relation_preuves`
- `recherche_relations`
- `recherche_chronologie`
- `preuve_chronologie`
- `entite_chronologie`
- `relation_chronologie`
- `hypothese_preuves`
- `hypothese_entites`
- `hypothese_relations`
- `recherche_hypotheses`

La V1 constitue un schéma étendu, et non un simple prototype à deux tables.

## 5.2 V2 — Renforcement des preuves

La V2 ajoute à `preuves` :

- `original_name` ;
- `collected_at` ;
- `source` ;
- `integrity_status`.

Elle effectue également :

- un backfill de `original_name` depuis `name` ;
- la création de `idx_preuves_imported_at` ;
- un trigger de validation à l’insertion ;
- un trigger de validation à la mise à jour.

Les triggers renforcent notamment :

- le nom original obligatoire ;
- la taille non négative ;
- le SHA-256 en minuscules sur 64 caractères ;
- la plage valide de `integrity_status`.

## 5.3 V3 — Provenance OSINT structurée

La V3 ajoute :

- `osint_executions`
- `osint_execution_entities`
- `osint_execution_relations`

La table principale conserve notamment :

- l’outil ;
- sa version ;
- l’action ;
- la sélection d’origine ;
- la cible ;
- les arguments ;
- les dates de début et de fin ;
- le code de sortie ;
- l’état final ;
- les sorties standard et erreur brutes ;
- le SHA-256 de la sortie.

Les tables de liaison indiquent si les entités ou relations ont été créées ou
réutilisées.

## 5.4 V4 — Comptes sociaux

La V4 :

- ajoute les types TikTok, X, Telegram et compte social générique ;
- crée `comptes_sociaux`.

Cette table est une extension spécialisée d’une entité et conserve :

- la plateforme ;
- l’URL du profil ;
- le pseudonyme ;
- un identifiant de plateforme facultatif ;
- la première observation ;
- l’état du compte ;
- des notes.

## 5.5 V5 — Rôles des personnes

La V5 crée `person_roles`.

Le rôle est limité à une liste contrôlée comprenant notamment :

- non catégorisé ;
- escroc présumé ;
- victime ;
- témoin ;
- suspect ;
- personne liée.

## 5.6 V6 — Identité usurpée

La V6 reconstruit `person_roles` afin d’ajouter :

```text
impersonated_identity
```

Elle :

1. renomme la table V5 ;
2. crée la nouvelle table ;
3. recopie les données ;
4. supprime l’ancienne table ;
5. recrée l’index.

Cette migration est sensible aux clés étrangères et doit rester couverte par un
test de migration réel.

## 5.7 V7 — Extractions

La V7 crée `extractions`.

Elle conserve :

- l’identifiant de l’extraction ;
- une preuve associée facultative ;
- le type de source ;
- l’identifiant de la source ;
- l’outil ;
- la date de création.

La provenance est partiellement polymorphe via :

```text
source_kind
source_id
```

SQLite ne peut pas imposer directement une clé étrangère vers plusieurs tables
possibles. La cohérence de `source_id` dépend donc aussi de la couche métier.

## 5.8 V8 — État du viewport

La V8 crée `graph_viewport`.

La table ne peut contenir qu’une ligne :

```text
id = 1
```

Elle persiste :

- le zoom ;
- le décalage horizontal ;
- le décalage vertical ;
- la date de mise à jour.

Il s’agit d’un état de présentation, pas d’une donnée métier.

## 5.9 V9 — Types canoniques de relations

La V9 crée `relation_types` avec :

- un identifiant entier ;
- un code métier stable facultatif ;
- un libellé canonique ;
- une clé normalisée unique ;
- une description facultative ;
- un indicateur système.

Elle insère dix types système initiaux, dont :

- `resolves_to`
- `aliases_to`
- `uses_name_server`
- `links_to`
- `sends`
- `uses`
- `controls`
- `owns`
- `knows`
- `redirects_to`

La migration ne se limite pas au fichier SQL.

La fonction :

```c
schema_v9_migrate_relation_types()
```

réalise également les opérations suivantes :

1. ajoute `relations.relation_type_id` si nécessaire ;
2. parcourt les anciennes relations dans un ordre déterministe ;
3. normalise l’ancien texte `type_relation` ;
4. recherche un type existant par code ou clé normalisée ;
5. crée un type personnalisé lorsqu’aucun type ne correspond ;
6. rattache chaque relation au type canonique ;
7. crée l’index d’unicité canonique ;
8. crée l’index sur `relation_type_id` ;
9. crée des triggers interdisant un type canonique nul ;
10. exécute `PRAGMA foreign_key_check`.

La colonne historique :

```text
relations.type_relation
```

reste présente pour compatibilité.

Depuis V9, l’identité logique du type doit être :

```text
relations.relation_type_id
```

et non le texte historique.

---

# 6. Rôle de `schema_current.sql`

`schema_current.sql` est présenté comme un ensemble d’extensions idempotentes
du schéma courant V9.

Il crée si nécessaire :

- `graph_node_positions`
- `extractions`
- `graph_layout_positions`
- `graph_viewport`
- `relation_types`

Il ajoute également deux triggers nettoyant les positions de graphe orphelines
après suppression d’une entité ou d’une relation.

## 6.1 Migration des positions

Le script copie les anciennes positions :

```sql
INSERT OR IGNORE INTO graph_layout_positions (...)
SELECT ... FROM graph_node_positions;
```

puis exécute :

```sql
DELETE FROM graph_node_positions;
```

L’objectif est de migrer l’ancien état limité aux entités vers une disposition
générique acceptant aussi les relations.

## 6.2 Point d’attention

Le script est exécuté après les migrations lors de l’ouverture.

Il contient donc à la fois :

- des créations idempotentes ;
- une migration de données de présentation ;
- une suppression des anciennes lignes.

Le comportement peut être légitime, mais il doit être explicitement couvert par
des tests vérifiant plusieurs exécutions successives afin de garantir :

- l’absence de perte de positions courantes ;
- l’absence de réimport d’anciennes coordonnées ;
- la stabilité après plusieurs ouvertures ;
- le nettoyage correct des positions orphelines.

---

# 7. Inventaire statique du schéma public V9

L’application statique des scripts publics produit **45 tables distinctes**.

Ce nombre est dérivé des scripts, et doit être confirmé sur une base générée par
une requête sur `sqlite_master`.

## 7.1 Métadonnées

| Table | Rôle |
|---|---|
| `metadata` | version et informations techniques |
| `investigation` | enquête unique contenue dans la base |

## 7.2 Référentiels et classification

| Table | Rôle |
|---|---|
| `categories` | classement principal |
| `tags` | annotations multiples |
| `types_preuve` | types de preuves |
| `types_entite` | types d’entités |
| `types_source` | types de sources |
| `types_outil` | types d’outils |
| `relation_types` | types canoniques de relations |

## 7.3 Données métier principales

| Table | Rôle |
|---|---|
| `sources` | origine d’une information |
| `preuves` | métadonnées des fichiers collectés |
| `recherches` | actions d’investigation |
| `entites` | objets identifiés |
| `relations` | liens orientés entre entités |
| `hypotheses` | raisonnements provisoires |
| `chronologie` | événements de l’enquête |
| `journal` | journal technique |

## 7.4 Extensions spécialisées

| Table | Version | Rôle |
|---|---:|---|
| `osint_executions` | V3 | provenance des traitements OSINT |
| `osint_execution_entities` | V3 | entités créées ou réutilisées |
| `osint_execution_relations` | V3 | relations créées ou réutilisées |
| `comptes_sociaux` | V4 | données propres aux comptes sociaux |
| `person_roles` | V5/V6 | rôle d’enquête d’une personne |
| `extractions` | V7 | provenance d’une extraction |
| `graph_viewport` | V8 | zoom et position du canevas |
| `graph_node_positions` | courant | ancien stockage des positions d’entités |
| `graph_layout_positions` | courant | positions génériques entités/relations |

## 7.5 Tables de liaison V1

| Domaine | Tables |
|---|---|
| tags | `tag_preuves`, `tag_recherches`, `tag_entites`, `tag_relations`, `tag_hypotheses`, `tag_chronologie` |
| recherches | `recherche_preuves`, `recherche_entites`, `recherche_relations`, `recherche_chronologie`, `recherche_hypotheses` |
| preuves | `preuve_entites`, `preuve_chronologie`, `relation_preuves` |
| chronologie | `entite_chronologie`, `relation_chronologie` |
| hypothèses | `hypothese_preuves`, `hypothese_entites`, `hypothese_relations` |

---

# 8. Intégrité et sécurité des données

## 8.1 Clés étrangères

`database_open()` exécute :

```sql
PRAGMA foreign_keys = ON;
```

Les stratégies observées sont :

- `CASCADE` pour les objets strictement dépendants ;
- `RESTRICT` lorsque la suppression risquerait de casser l’historique ;
- `SET NULL` pour les références facultatives.

## 8.2 Requêtes préparées

Les valeurs variables de la couche Database et des DAO utilisent les fonctions
de préparation et de liaison.

Les requêtes statiques sans donnée utilisateur peuvent être exécutées avec
`sqlite3_exec()`.

La règle à conserver est :

> aucune valeur externe ou utilisateur ne doit être concaténée dans une chaîne
> SQL.

Le caractère append-only du journal ne constitue jamais une exception à cette
règle.

## 8.3 Transactions

Les migrations publiques V1 à V9 sont pilotées par des fonctions dédiées.

Chaque passage de version :

- démarre une transaction ;
- applique le changement ;
- met à jour la version ;
- commit en cas de succès ;
- rollback en cas d’échec.

La création d’une base neuve est également atomique.

## 8.4 Preuves

Le schéma et la couche métier conservent notamment :

- UUID ;
- chemin relatif ;
- nom interne ;
- nom original ;
- taille ;
- SHA-256 ;
- type MIME ;
- date du fichier ;
- date de collecte ;
- date d’import ;
- statut d’intégrité ;
- statut logique.

Le fichier original reste dans l’arborescence de l’enquête. SQLite conserve ses
métadonnées et ses relations.

## 8.5 Suppression logique

Plusieurs objets V1 utilisent des statuts tels que :

```text
active
archived
deleted
```

Certaines suppressions physiques existent néanmoins dans les DAO, notamment
pour les relations.

Le document d’architecture doit donc éviter d’affirmer que toute suppression
est systématiquement logique. La stratégie réelle doit être documentée table
par table.

## 8.6 Journal et chronologie

Le schéma distingue :

- `chronologie` : événements significatifs de l’enquête ;
- `journal` : trace technique des actions de l’application.

Le journal est conçu comme append-only dans la documentation, mais cet audit
n’a pas identifié de trigger SQLite interdisant une mise à jour ou une
suppression. Cette propriété repose donc actuellement au moins en partie sur la
couche applicative.

---

# 9. Correspondance SQL, modèles, DAO et tests

| Domaine | Table principale | Modèle ou structure | DAO/service observé | Tests observés |
|---|---|---|---|---|
| enquête | `investigation` | `InvestigationRecord` | `InvestigationDao` | `test_investigation_record`, `test_investigation_dao` |
| preuves | `preuves` | `EvidenceRecord` | `EvidenceDao`, `EvidenceTypeDao` | nombreux tests preuve/import/intégrité |
| preuve-entité | `preuve_entites` | — | `EvidenceEntityDao` | `test_evidence_entity_dao` |
| entités | `entites` | `EntityRecord` | `EntityDao`, `EntityTypeDao` | tests modèle et DAO |
| relations | `relations` | `RelationRecord` | `RelationDao`, `RelationService` | tests DAO, modèle et service |
| types de relation | `relation_types` | `RelationType` | `RelationTypeDao`, `RelationTypeService` | normalisation et service |
| relation-preuve | `relation_preuves` | — | `RelationEvidenceDao` | `test_relation_evidence_dao` |
| provenance OSINT | `osint_executions` | `OsintExecutionRecord` | `OsintExecutionDao` | DAO et intégrité |
| extractions | `extractions` | contexte métier | `ExtractionDao`, service de dépôt | `test_extraction_drop_service` |
| positions graphe | tables de graphe | `GraphNodePosition`, layout | `GraphNodePositionDao` | `test_graph_node_position_dao` |
| comptes sociaux | `comptes_sociaux` | plateforme sociale | service compte social | `test_social_account_service` |
| personnes | `person_roles` | extension d’entité | service personne | `test_person_entity_service` |

## Observation

La V1 contient davantage de domaines que les DAO publics actuellement exposés.

Aucun DAO dédié n’a été observé dans `include/dao/` pour plusieurs tables, dont :

- `sources` ;
- `recherches` ;
- `hypotheses` ;
- `chronologie` ;
- `journal` ;
- `categories` ;
- `tags`.

Cela ne signifie pas que ces tables sont inutilisables. Cela indique seulement
qu’aucune interface DAO publique dédiée n’est visible dans le dossier audité au
commit indiqué.

---

# 10. Couverture de tests observée

## 10.1 Points couverts par `test_database.c`

Le fichier vérifie notamment :

- l’initialisation d’une base valide ;
- le rollback d’une initialisation défaillante ;
- la présence de la version `9` dans une base neuve ;
- la présence de plusieurs tables ajoutées après V1 ;
- la migration d’une base V1 vers la version courante ;
- la conservation d’une preuve V1 ;
- le backfill V2 de `original_name` ;
- l’ajout des colonnes V2 ;
- l’ajout des triggers V2 ;
- le rollback complet d’une migration V2 provoquée en échec ;
- `PRAGMA integrity_check` après le rollback.

## 10.2 Tests spécialisés observés

Le dépôt possède également des tests pour :

- les preuves et leur intégrité ;
- les entités ;
- les relations ;
- les types canoniques de relations ;
- les exécutions OSINT ;
- les extractions ;
- les positions du graphe ;
- les comptes sociaux ;
- les rôles des personnes.

## 10.3 Lacunes de couverture à traiter

Le test principal de migration ne fournit pas une fixture indépendante pour
chaque version V2 à V9.

Il manque une matrice explicite de ce type :

| Base d’entrée | Migration | Vérifications minimales |
|---|---|---|
| V1 | V1 → V9 | données, version, FK, intégrité |
| V2 | V2 → V9 | preuve V2 conservée |
| V3 | V3 → V9 | provenance OSINT conservée |
| V4 | V4 → V9 | comptes sociaux conservés |
| V5 | V5 → V9 | rôles conservés pendant reconstruction V6 |
| V6 | V6 → V9 | identité usurpée conservée |
| V7 | V7 → V9 | extractions conservées |
| V8 | V8 → V9 | viewport et positions conservés |
| V9 | réouverture | idempotence de `schema_current.sql` |

Chaque fixture devrait vérifier au minimum :

```sql
PRAGMA integrity_check;
PRAGMA foreign_key_check;
SELECT value FROM metadata WHERE key = 'schema_version';
```

---

# 11. Constats d’audit

## AUD-001 — Documentation courante restée en V1

**Sévérité : élevée**

`docs/database/DATABASE_ARCHITECTURE.md` s’annonce encore comme :

```text
Statut : Stable (V1)
Schéma : V1
```

alors que le code public utilise V9.

### Risque

- confusion des développeurs ;
- erreurs des agents locaux ;
- mauvaise interprétation de l’état du projet ;
- décisions basées sur des structures historiques ;
- oubli des tables V2 à V9.

### Action recommandée

Créer :

```text
docs/database/SCHEMA_AUDIT_CURRENT.md
```

et déplacer les audits historiques vers :

```text
docs/database/audits/SCHEMA_AUDIT_V1.md
docs/database/audits/SCHEMA_AUDIT_V9.md
```

Le document courant doit indiquer le commit exact qu’il audite.

---

## AUD-002 — V10 locale non représentée dans la branche publique

**Sévérité : bloquante pour un audit « courant »**

La branche publique s’arrête à V9 tandis que le dépôt local est annoncé en V10.

### Action recommandée

Avant de finaliser ce document :

1. publier ou fournir les fichiers V10 ;
2. vérifier la constante courante ;
3. vérifier la chaîne de migration ;
4. vérifier les tests ;
5. compléter l’inventaire ;
6. remplacer le statut V9 par V10 ;
7. enregistrer le commit audité.

---

## AUD-003 — Version courante dupliquée

**Sévérité : moyenne**

La version est définie deux fois :

```c
DATABASE_SCHEMA_VERSION_CURRENT
DATABASE_SCHEMA_VERSION_CURRENT_TEXT
```

Les tests comparent aussi directement la chaîne `"9"`.

### Risque

Une future migration peut modifier une valeur et oublier l’autre.

### Action recommandée

Conserver une seule constante numérique et produire la représentation textuelle
au moment de l’écriture, ou centraliser les deux valeurs dans un fichier
d’interface unique couvert par une assertion de test.

---

## AUD-004 — `schema_current.sql` mélange plusieurs responsabilités

**Sévérité : moyenne**

Le fichier sert à la fois à :

- créer des structures manquantes ;
- maintenir la compatibilité ;
- migrer des positions ;
- supprimer les anciennes positions ;
- installer des triggers.

### Risque

Une opération supposée idempotente peut finir par modifier des données à chaque
ouverture.

### Action recommandée

Documenter chaque opération et ajouter un test exécutant
`schema_ensure_current()` plusieurs fois sur la même fixture.

---

## AUD-005 — Contrôle des clés étrangères non généralisé

**Sévérité : moyenne**

La migration V9 exécute explicitement :

```sql
PRAGMA foreign_key_check;
```

Aucun contrôle générique similaire n’a été observé dans le pilote commun de
toutes les migrations.

### Action recommandée

Exécuter un contrôle générique avant le commit de chaque migration, ou fournir
une justification documentée lorsqu’une migration ne le nécessite pas.

---

## AUD-006 — Couverture de migration incomplète par version

**Sévérité : élevée**

Le test principal couvre :

- création d’une base neuve ;
- migration V1 vers la version courante ;
- rollback V2.

Il ne constitue pas une matrice complète de fixtures V2 à V9.

### Action recommandée

Ajouter une fixture synthétique minimale pour chaque version publiée et migrer
chacune vers la version courante.

---

## AUD-007 — Absence de sauvegarde préalable observée

**Sévérité : élevée pour des enquêtes réelles**

Aucun appel de sauvegarde SQLite ou copie préalable n’a été observé dans
`database_migrate_to_latest()`.

La transaction protège la cohérence logique, mais elle ne remplace pas une
copie de sécurité face à :

- panne disque ;
- interruption brutale ;
- défaut SQLite ou système de fichiers ;
- erreur de migration non anticipée ;
- corruption déjà présente.

### Action recommandée

Avant toute migration d’une base existante :

1. fermer ou stabiliser les accès concurrents ;
2. créer une sauvegarde cohérente ;
3. vérifier sa création ;
4. lancer la migration ;
5. conserver la sauvegarde tant que la validation n’est pas terminée.

Cette fonctionnalité doit être testée uniquement sur des bases synthétiques.

---

## AUD-008 — Ancienne et nouvelle identité du type de relation coexistent

**Sévérité : moyenne**

V9 conserve :

```text
relations.type_relation
```

et ajoute :

```text
relations.relation_type_id
```

La documentation du commit précise que la première colonne ne doit plus servir
d’identité.

### Risque

Un module ancien peut encore filtrer ou détecter les doublons par texte.

### Action recommandée

Auditer tous les producteurs et lecteurs de relations, puis ajouter un test
interdisant toute régression vers `type_relation` comme clé métier.

La colonne historique pourra être supprimée dans une future migration seulement
après validation de toutes les anciennes bases et de tous les adaptateurs.

---

## AUD-009 — Append-only non imposé par SQLite

**Sévérité : faible à moyenne**

Le journal est documenté comme append-only, mais aucun trigger interdisant
`UPDATE` ou `DELETE` n’a été identifié dans les scripts examinés.

### Action recommandée

Choisir explicitement l’une des deux politiques :

- garantie applicative documentée et testée ;
- garantie SQLite par triggers de refus.

Le document courant doit dire laquelle est réellement retenue.

---

## AUD-010 — Paramètres SQLite de durabilité non documentés dans le code audité

**Sévérité : à évaluer**

Aucun réglage explicite n’a été identifié dans `database.c` pour :

- `journal_mode` ;
- `synchronous` ;
- `busy_timeout`.

SQLite applique donc probablement ses valeurs par défaut, sauf réglage réalisé
ailleurs.

### Action recommandée

Documenter volontairement les paramètres retenus et leurs conséquences avant
un usage opérationnel.

Ne pas activer WAL ou modifier `synchronous` sans étudier la portabilité d’une
enquête et les procédures de copie de ses fichiers annexes.

---

# 12. Exigences proposées pour la V10

La V10 doit être considérée incomplète tant que les éléments suivants ne sont
pas présents et cohérents.

## 12.1 Code

- `database/schema_v10.sql`
- `schema_install_v10()`
- `database_migrate_v9_to_v10()`
- cas `9` dans `database_migrate_to_latest()`
- installation V10 dans `database_initialize()`
- mise à jour de la version numérique
- mise à jour de la version textuelle ou suppression de cette duplication
- mise à jour du commentaire de `schema_current.sql`

## 12.2 Intégrité

- migration transactionnelle ;
- conservation des UUID ;
- conservation des preuves ;
- conservation des liaisons ;
- conservation de la provenance OSINT ;
- conservation des positions du graphe ;
- `PRAGMA foreign_key_check` avant commit ;
- version mise à jour uniquement après réussite ;
- rollback complet au premier échec.

## 12.3 Tests

- création d’une base neuve V10 ;
- migration V9 vers V10 ;
- migration V1 vers V10 ;
- fixture contenant des données réelles synthétiques de chaque domaine ;
- rollback V10 provoqué volontairement ;
- deuxième ouverture sans modification inattendue ;
- `PRAGMA integrity_check` ;
- `PRAGMA foreign_key_check` ;
- comparaison des objets SQLite attendus ;
- tests des DAO concernés.

## 12.4 Documentation

- mise à jour du présent audit ;
- résumé de la migration ;
- inventaire des tables et colonnes nouvelles ;
- ticket Forgejo associé ;
- commit exact audité ;
- statut explicite : implémenté, partiel, futur, historique ou abandonné.

---

# 13. Procédure de vérification reproductible

> Utiliser uniquement une copie synthétique.  
> Ne jamais exécuter cette procédure sur une base d’enquête réelle sans
> sauvegarde et validation préalable.

## 13.1 Compilation

```bash
make clean
make -j8
make -j8 test
git diff --check
```

En cas d’échec provoqué par la parallélisation, revenir temporairement à :

```bash
make
make test
```

## 13.2 Création d’une base synthétique

Créer une enquête de test par l’API normale de l’application ou par un test
dédié, puis vérifier :

```bash
sqlite3 /chemin/test/Enquete.sqlite \
  "SELECT value FROM metadata WHERE key = 'schema_version';"

sqlite3 /chemin/test/Enquete.sqlite \
  "PRAGMA integrity_check;"

sqlite3 /chemin/test/Enquete.sqlite \
  "PRAGMA foreign_key_check;"
```

Résultats attendus :

```text
<version courante>
ok
<aucune ligne pour foreign_key_check>
```

## 13.3 Inventaire du schéma

```bash
sqlite3 /chemin/test/Enquete.sqlite \
  "SELECT type, name
   FROM sqlite_master
   WHERE name NOT LIKE 'sqlite_%'
   ORDER BY type, name;"
```

## 13.4 Vérification de l’idempotence

1. ouvrir la base ;
2. fermer la base ;
3. enregistrer un dump du schéma et les données de présentation ;
4. rouvrir la base plusieurs fois ;
5. comparer les résultats ;
6. vérifier les positions du graphe ;
7. vérifier les types de relations ;
8. exécuter à nouveau les deux PRAGMA d’intégrité.

## 13.5 Vérification de migration

Pour chaque fixture V1 à V9 :

1. calculer une copie de travail ;
2. relever la version initiale ;
3. compter les objets métier ;
4. migrer ;
5. vérifier la version finale ;
6. comparer les UUID ;
7. comparer les empreintes des preuves ;
8. comparer les liaisons ;
9. vérifier les contraintes ;
10. vérifier le rollback sur une copie volontairement invalide.

---

# 14. Règle de maintenance proposée

Toute nouvelle version du schéma doit livrer ensemble :

```text
migration SQL
+ fonction d’installation
+ fonction de migration
+ incrément de version
+ tests de base neuve
+ tests d’ancienne base
+ test de rollback
+ contrôle d’intégrité
+ mise à jour de SCHEMA_AUDIT_CURRENT.md
+ ticket Forgejo
+ commit vérifiable
```

Un ticket de migration ne doit pas être fermé tant que cette chaîne n’est pas
complète.

## Statuts documentaires obligatoires

Chaque table, colonne ou fonctionnalité mentionnée dans la documentation devrait
porter l’un des statuts suivants :

```text
IMPLÉMENTÉ
PARTIELLEMENT IMPLÉMENTÉ
DOCUMENTÉ MAIS NON IMPLÉMENTÉ
HISTORIQUE
ABANDONNÉ
```

Cette règle évitera de confondre :

- une architecture cible ;
- un ancien audit ;
- une fonctionnalité réellement présente ;
- un ticket encore ouvert ;
- une migration locale non publiée.

---

# 15. Conclusion

Le schéma public V9 repose sur une base technique sérieuse :

- modèle relationnel riche ;
- versionnement explicite ;
- migrations transactionnelles ;
- provenance OSINT ;
- contraintes sur les preuves ;
- normalisation des types de relations ;
- séparation des données métier et de l’état du graphe ;
- tests nombreux autour des DAO et services.

Le problème principal n’est plus l’absence de structure.

Le problème principal est désormais **l’écart entre le code et la
documentation de référence**.

`SCHEMA_AUDIT_V1.md` reste utile comme document historique, mais ne doit plus
servir à déterminer l’état courant.

La prochaine étape correcte est :

1. fournir ou publier la V10 ;
2. auditer le delta V9 → V10 ;
3. exécuter la procédure reproductible sur des données synthétiques ;
4. remplacer le présent statut de brouillon par un audit V10 validé ;
5. conserver l’audit V9 dans un dossier historique.

---

# 16. Références du dépôt

## Commit public audité

```text
8bc3b43d63f4bd3da673ddf86a7e4b7a81e28604
feat(relations): centraliser les types et améliorer les aperçus
```

## Ticket principal lié à V9

```text
#106 — Normaliser et centraliser les types de relations
État : fermé
```

## Documents historiques

```text
docs/database/DATABASE_ARCHITECTURE.md
docs/database/SCHEMA_AUDIT_V1.md
```

## Fichiers constituant la source de vérité technique publique

```text
database/schema_v1.sql
database/schema_v2.sql
database/schema_v3.sql
database/schema_v4.sql
database/schema_v5.sql
database/schema_v6.sql
database/schema_v7.sql
database/schema_v8.sql
database/schema_v9.sql
database/schema_current.sql

src/database/database.c
src/database/schema.c
src/database/statement.c
src/database/transaction.c

include/database/database.h
include/database/schema.h

tests/test_database.c
tests/test_statement.c
tests/test_transaction.c
```
