# Ticket #026 — Migrer l’initialisation vers la couche Database

## Contexte

Le ticket #025 a introduit l’infrastructure principale de la couche Database :

- contexte opaque `Database` ;
- ouverture et fermeture des connexions ;
- requêtes préparées avec `DatabaseStatement` ;
- bindings et lecture typée ;
- gestion des transactions ;
- infrastructure d’erreurs.

Cependant, `database_initialize()` utilise encore directement plusieurs fonctions de l’API SQLite :

```c
sqlite3_open_v2()
sqlite3_prepare_v2()
sqlite3_bind_text()
sqlite3_step()
sqlite3_reset()
sqlite3_clear_bindings()
sqlite3_finalize()
sqlite3_close()

La fonction exécute également directement les commandes SQL suivantes :

```sql

BEGIN IMMEDIATE;
COMMIT;
ROLLBACK;
```

De son côté, `schema_install_v1()` reçoit toujours directement un `sqlite3 *`, ce qui contourne l’abstraction `Database`.

## Objectif

Réécrire l’initialisation d’une nouvelle enquête afin qu’elle utilise l’infrastructure créée au ticket #025.

L’initialisation doit rester atomique :

- soit la base est entièrement initialisée ;
- soit aucune donnée partielle n’est conservée.

## Travail à réaliser

### Adapter l’installation du schéma

Modifier la signature actuelle :

```C 
bool schema_install_v1(
    sqlite3 *database
);
```

afin qu’elle reçoive un contexte Database :

```C 
bool schema_install_v1(
    Database *database
);
```

L’installation du fichier SQL complet pourra continuer à utiliser `sqlite3_exec()` en interne, car le schéma contient plusieurs instructions SQL.

L’accès au handle SQLite devra passer par l’API interne :

```C
database_get_handle()
```

La fonction ne devra réaliser ni `COMMIT` ni `ROLLBACK`.

### Migrer l’insertion des métadonnées

Réécrire les fonctions responsables de l’insertion dans la table `metadata` avec `DatabaseStatement`.

Les opérations devront utiliser l’API suivante :

```C
database_statement_prepare()
database_statement_bind_text()
database_statement_step()
database_statement_reset()
database_statement_clear_bindings()
database_statement_finalize()
```

Les métadonnées obligatoires restent :

```
schema_version
application
created_at
investigation_uuid
```

Une seule requête préparée devra pouvoir être réutilisée pour insérer les quatre métadonnées.

### Migrer l’insertion de l’enquête

Réécrire l’insertion dans la table `investigation` avec `DatabaseStatement`.

Les champs insérés restent :

```C
database_open()
database_transaction_begin()
schema_install_v1()
database_transaction_commit()
database_transaction_rollback()
database_close()
```

Le déroulement attendu est :

```
validation des paramètres
        ↓
création du timestamp UTC
        ↓
création de l’UUID
        ↓
ouverture de Database
        ↓
début de transaction
        ↓
installation du schéma V1
        ↓
insertion des métadonnées
        ↓
insertion de l’enquête
        ↓
commit
        ↓
fermeture de Database
```

Tout échec après le début de la transaction doit provoquer un rollback.

### Utiliser l’infrastructure d’erreurs

Les erreurs rencontrées pendant :

- l’installation du schéma ;
- la préparation d’une requête ;
- le binding d’un paramètre ;
- l’exécution d’une requête ;
- le début d’une transaction ;
- le commit ;
- le rollback ;

doivent être enregistrées dans le contexte Database lorsque celui-ci est disponible.

L’infrastructure interne existante pourra être utilisée :

```C
database_set_error()
database_clear_error_internal()
```

```C
DATABASE_ERROR_SQLITE
```

```C
DATABASE_ERROR_INVALID_ARGUMENT
```

```C
DATABASE_ERROR_INVALID_STATE
```

### Nettoyer l’ancien code SQLite

Supprimer de la logique d’initialisation les appels directs à :

```C
sqlite3_open_v2()
sqlite3_prepare_v2()
sqlite3_bind_text()
sqlite3_step()
sqlite3_reset()
sqlite3_clear_bindings()
sqlite3_finalize()
sqlite3_close()
```

Supprimer également les commandes manuelles :

```SQL
BEGIN IMMEDIATE;
COMMIT;
ROLLBACK;
```

La fonction utilitaire `database_execute_sql()` pourra être conservée uniquement si elle reste nécessaire à `database_open()` pour l’activation des clés étrangères.

### Mettre à jour la documentation publique

Dans `include/database/database.h`, supprimer la mention temporaire :

```
Cette fonction conserve temporairement son rôle actuel pendant le
refactoring du ticket #025.
```

La documentation de `database_initialize()` devra préciser que :

- l’initialisation est transactionnelle ;
- un échec provoque un rollback ;
- la base est fermée avant le retour de la fonction.

## Tests à ajouter ou adapter

### Initialisation réussie

Vérifier qu’une initialisation valide crée :

- le schéma V1 ;
- les tables attendues ;
- les métadonnées obligatoires ;
- une seule ligne dans investigation ;
- le bon nom d’enquête ;
- le bon chemin racine ;
- un UUID non vide ;
- une date de création non vide ;
- une date de modification non vide.

### Paramètres invalides

Vérifier le refus des cas suivants :

```
database_path == NULL
database_path vide
investigation_name == NULL
investigation_name vide
investigation_root_path == NULL
investigation_root_path vide
```

### Rollback

Provoquer un échec après le démarrage de la transaction.

Une seconde tentative d’initialisation sur une base déjà initialisée pourra être utilisée pour provoquer une erreur de contrainte ou de création de schéma.

Après l’échec, vérifier que :

- aucune donnée partielle supplémentaire n’est conservée ;
- aucune seconde enquête n’est ajoutée ;
- les métadonnées existantes ne sont pas dupliquées ;
- la transaction est annulée ;
- la base reste lisible ;
- la connexion peut être fermée proprement.

### Régression

Tous les tests existants doivent rester valides :

```
InvestigationNode
InvestigationTreeModel
InvestigationTreeBuilder
InvestigationProject
Database
DatabaseStatement
DatabaseTransaction
DatabaseError
```

## Critères d’acceptation

- [ ] `database_initialize()` n’appelle plus directement `sqlite3_open_v2()`.
- [ ] `database_initialize()` utilise `database_open()`.
- [ ] `database_initialize()` utilise `database_close()`.
- [ ] Les transactions utilisent le module `transaction`.
- [ ] Les insertions utilisent `DatabaseStatement`.
- [ ] `InvestigationNode` Investigation `schema_install_v1()` reçoit un `Database *`.
InvestigationNode
Investigation `schema_install_v1()` reçoit un `Database *`.
- [ ] `schema_install_v1()` récupère le handle avec l’API interne.
- [ ] Aucun `sqlite3_stmt *` n’est manipulé dans le code d’initialisation.
- [ ] Aucun `BEGIN`, `COMMIT` ou `ROLLBACK` manuel ne reste dans `database.c`.
- [ ] Tout échec après le début de la transaction provoque un rollback.
- [ ] Les erreurs sont enregistrées dans la couche Database.
- [ ] Les tests de réussite sont valides.
- [ ] Les tests de paramètres invalides sont valides.
- [ ] Les tests de rollback sont valides.
- [ ] Les tests de régression sont valides.
- [ ] `make` réussit sans erreur.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

## Hors périmètre

Ce ticket ne doit pas ajouter :

- de CRUD métier ;
- de dépôt ou repository métier ;
- de migration entre plusieurs versions de schéma ;
- de système GResource pour embarquer `schema_v1.sql` ;
- de création automatique des dossiers parents ;
- de modification du schéma SQL V1 ;
- de nouvelles tables ;
- de logique d’interface graphique ;
- de nouvelles fonctionnalités utilisateur.

## Fichiers principalement concernés

```text
include/database/database.h
include/database/schema.h

src/database/database.c
src/database/database_internal.h
src/database/schema.c

tests/test_database.c

Makefile
```

## Résultat attendu

À la fin du ticket, `database_initialize()` doit être un utilisateur normal de la couche Database.

La fonction ne doit plus contourner cette couche avec des appels SQLite directs.

L’initialisation d’une enquête doit être entièrement transactionnelle, testée et cohérente avec l’architecture mise en place au ticket #025.

## Commit attendu

Une fois tous les critères d’acceptation validés, le ticket pourra être enregistré avec le commit suivant :

```text
refactor(database): migrate initialization to database layer
```

Le commit doit principalement contenir :

```
include/database/database.h
include/database/schema.h

src/database/database.c
src/database/database_internal.h
src/database/schema.c

tests/test_database.c

Makefile
```
Avant le commit, exécuter :

```bash
make clean
make
make test
git diff --check
git status --short
```

