# Ticket #027 — Ajouter le modèle de lecture et le DAO de l’enquête

## Contexte

Les tickets précédents ont permis de mettre en place :

- le schéma SQLite V1 ;
- l’infrastructure `Database` ;
- les requêtes préparées `DatabaseStatement` ;
- les transactions ;
- la gestion centralisée des erreurs ;
- l’initialisation transactionnelle d’une nouvelle enquête.

La table SQLite :

```text
investigation
```
contient les informations principales de l’enquête courante :

```text
id
name
root_path
created_at
updated_at
```

Le projet possède déjà un type :

```C
Investigation
```

dans le module `core`.

Cet objet représente actuellement le contexte de travail sur le système de
fichiers :

- chemin racine de l’enquête ;
- chemin du fichier SQLite.

Il ne doit pas être transformé directement en représentation d’une ligne SQL.

L’architecture du projet prévoit une séparation entre :

```text
Core
Models
DAO
Database
SQLite
```

Le DAO doit être responsable des requêtes SQL métier.

## Objectif

Créer un modèle de données en lecture seule représentant la ligne de la table
investigation, ainsi qu’un DAO permettant de charger cette ligne depuis une
connexion Database.

Le ticket doit permettre de lire les informations persistées d’une enquête
sans utiliser directement l’API SQLite hors de la couche Database.

## Architecture attendue

```
InvestigationRecord
        ↑
        │ construit par
        │
InvestigationDao
        │
        ▼
DatabaseStatement
        │
        ▼
Database
        │
        ▼
SQLite
```

## Travail à réaliser

### Créer le modèle InvestigationRecord

Créer les fichiers :
```text
include/models/investigation_record.h
src/models/investigation_record.c
```

Le modèle doit être opaque :

```C
typedef struct InvestigationRecord InvestigationRecord;
```

Sa représentation privée doit contenir :

```C
struct InvestigationRecord
{
    char *id;
    char *name;
    char *root_path;
    char *created_at;
    char *updated_at;
};
```

Le modèle ne doit dépendre ni de SQLite, ni de GTK.

### Ajouter le constructeur

Créer une fonction :

```C
InvestigationRecord *investigation_record_new(
    const char *id,
    const char *name,
    const char *root_path,
    const char *created_at,
    const char *updated_at
);
```

Le constructeur doit :

- refuser les pointeurs `NULL` ;
- refuser les chaînes vides ;
- allouer une copie de chaque chaîne ;
- nettoyer correctement toutes les allocations en cas d’échec ;
- retourner `NULL` si les données sont invalides.

### Ajouter la fonction de libération

Créer :

```C
void investigation_record_free(
    InvestigationRecord *record
);
```

Cette fonction doit :

- accepter `NULL` ;
- libérer toutes les chaînes ;
- libérer la structure.

### Ajouter les accesseurs

Créer les accesseurs en lecture seule :

```C
const char *investigation_record_get_id(
    const InvestigationRecord *record
);

const char *investigation_record_get_name(
    const InvestigationRecord *record
);

const char *investigation_record_get_root_path(
    const InvestigationRecord *record
);

const char *investigation_record_get_created_at(
    const InvestigationRecord *record
);

const char *investigation_record_get_updated_at(
    const InvestigationRecord *record
);
```

Les pointeurs retournés appartiennent au modèle et ne doivent pas être libérés.

Les accesseurs doivent retourner `NULL` si le modèle reçu est `NULL`.

### Créer le DAO de l’enquête

Créer les fichiers :

```text
include/dao/investigation_dao.h
src/dao/investigation_dao.c
```

Déclarer la fonction :

```C
InvestigationRecord *investigation_dao_load(
    Database *database
);
```

Cette fonction doit charger l’unique ligne de la table :

```text
investigation
```

La requête doit lire :

```SQL
SELECT
    id,
    name,
    root_path,
    created_at,
    updated_at
FROM investigation;
```

### Utiliser exclusivement DatabaseStatement

Le DAO doit utiliser :

```C
database_statement_prepare()
database_statement_step()
database_statement_column_text()
database_statement_finalize()
```

Le DAO ne doit pas utiliser directement :

```C
sqlite3_prepare_v2()
sqlite3_step()
sqlite3_column_text()
sqlite3_finalize()
```

Aucun type SQLite ne doit apparaître dans l’interface publique du DAO.

### Vérifier le nombre de lignes

La base d’une enquête doit contenir exactement une ligne dans la table
`investigation`.

Le DAO doit gérer les cas suivants :

Une ligne
```
ROW
DONE
```
Résultat :

```
succès
```

Le DAO construit et retourne un `InvestigationRecord`.

#### Aucune ligne

`DONE` dès le premier step

Résultat :

```
échec
```

Le DAO retourne `NULL` et enregistre :

```
DATABASE_ERROR_INVALID_STATE
```

#### Plusieurs lignes

```
ROW
ROW
```

Résultat :

`échec`

Le DAO libère le modèle temporaire, retourne `NULL` et enregistre :
```C
DATABASE_ERROR_INVALID_STATE
```

### Propager les erreurs

Les erreurs de préparation ou d’exécution provenant de `DatabaseStatement`
doivent rester disponibles dans le contexte `Database`.

Le DAO doit enregistrer une erreur explicite pour :

- une table vide ;
- plusieurs lignes dans investigation ;
- une colonne obligatoire invalide ;
- une allocation impossible ;
- un état incohérent.

Les codes prévus sont :

```C
DATABASE_ERROR_INVALID_ARGUMENT
DATABASE_ERROR_INVALID_STATE
DATABASE_ERROR_MEMORY
DATABASE_ERROR_SQLITE
```

Une lecture réussie doit laisser :

```C
DATABASE_ERROR_NONE
```

### Gérer proprement la mémoire

Le DAO doit libérer toutes les chaînes temporaires retournées par :

```C
database_statement_column_text()
```

après la construction du modèle.

Toutes les sorties d’échec doivent :

- finaliser la requête préparée ;
- libérer les chaînes déjà lues ;
- libérer le modèle partiellement construit ;
- ne provoquer aucune fuite mémoire.
- Tests à ajouter

#### Test du modèle

Créer :

```
tests/test_investigation_record.c
```

Vérifier :

- la création avec des données valides ;
- la copie des chaînes ;
- les cinq accesseurs ;
- le refus des paramètres `NULL` ;
- le refus des chaînes vides ;
- `investigation_record_free(NULL)`.

#### Test du DAO

Créer :

```
tests/test_investigation_dao.c
Chargement valide
```

Créer une base temporaire avec :

```C
database_initialize()
```

Puis :

- ouvrir la base avec `database_open()` ;
- charger l’enquête avec `investigation_dao_load()` ;
- vérifier que le modèle n’est pas `NULL` ;
- vérifier l’UUID ;
- vérifier le nom ;
- vérifier le chemin racine ;
- vérifier `created_at` ;
- vérifier `updated_at` ;
- vérifier que l’erreur Database vaut `DATABASE_ERROR_NONE`.

#### Paramètre invalide

Vérifier :

```C
investigation_dao_load(NULL) == NULL
```

#### Table vide

Créer une base contenant la table investigation sans ligne.

Vérifier :

```
résultat == NULL
DATABASE_ERROR_INVALID_STATE
message non vide
```

#### Plusieurs lignes

Créer une base contenant deux lignes dans la table investigation.

Vérifier :

```
résultat == NULL
DATABASE_ERROR_INVALID_STATE
message non vide
```

#### Données invalides

Créer une situation dans laquelle une colonne obligatoire ne peut pas être
correctement lue.

Vérifier :

```
résultat == NULL
erreur enregistrée
aucune fuite de mémoire
```

## Makefile

Ajouter les exécutables :

```
tests/test_investigation_record
tests/test_investigation_dao
```

Ajouter leurs règles de compilation.

Les tests du DAO devront notamment compiler avec :

```
src/models/investigation_record.c
src/dao/investigation_dao.c
src/database/database.c
src/database/schema.c
src/database/statement.c
src/database/transaction.c
src/database/error.c
```

Ajouter les nouveaux tests aux cibles :

```
test
clean
```
### Critères d’acceptation

- [ ] Le type `InvestigationRecord` est opaque.
- [ ] Le modèle ne dépend ni de SQLite ni de GTK.
- [ ] Le constructeur valide tous les champs obligatoires.
- [ ] Toutes les chaînes sont copiées.
- [ ] Une fonction de libération complète est disponible.
- [ ] Les cinq accesseurs en lecture seule sont disponibles.
- [ ] `investigation_dao_load()` charge l’enquête courante.
- [ ] Le DAO utilise uniquement l’API `DatabaseStatement`.
- [ ] Aucun appel direct à SQLite n’apparaît dans le DAO.
- [ ] Une base contenant exactement une enquête est acceptée.
- [ ] Une table vide est refusée.
- [ ] Plusieurs lignes sont refusées.
- [ ] Les erreurs sont enregistrées dans `Database`.
- [ ] Toutes les ressources temporaires sont libérées.
- [ ] Les tests du modèle sont valides.
- [ ] Les tests du DAO sont valides.
- [ ] Tous les anciens tests restent valides.
- [ ] `make` réussit sans erreur.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

### Hors périmètre

Ce ticket ne doit pas ajouter :

- la création d’une enquête par le DAO ;
- la modification du nom de l’enquête ;
- la modification du chemin racine ;
- la suppression d’une enquête ;
- un CRUD complet ;
- l’ouverture automatique dans l’interface GTK ;
- l’affichage des informations dans la fenêtre ;
- les DAO des preuves, sources ou entités ;
- les migrations de schéma ;
- une nouvelle version du schéma SQLite.

### Fichiers principalement concernés

```
include/models/investigation_record.h
include/dao/investigation_dao.h

src/models/investigation_record.c
src/dao/investigation_dao.c

tests/test_investigation_record.c
tests/test_investigation_dao.c

Makefile
```
## Résultat attendu

À la fin du ticket, le projet doit être capable de charger les informations
persistées de l’enquête courante dans un modèle C indépendant de SQLite.

Le DAO devient le premier point d’accès métier structuré à la base de données.

### Commit attendu

Une fois tous les critères validés :

`feat(dao): add investigation record loader`

Avant le commit :

```bash
make clean
make
make test
git diff --check
git status --short
git add . 
git diff --cached --stat
git diff --cached
git commit -m "feat(dao): add investigation record loader"
git push
```
