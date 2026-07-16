# Ticket #025

## Titre

Refactoriser la couche Database et établir l'infrastructure d'accès aux données.

---

## Objectif

Mettre en place l'architecture définitive de la couche Database afin de fournir
une API interne homogène pour tous les futurs modules métier.

Ce ticket ne comprend **aucun CRUD métier**.

Son objectif est uniquement de construire les fondations techniques qui seront
réutilisées par l'ensemble de l'application.

---

## Motivations

À terme, Labfy Investigation manipulera de nombreux objets métier :

- investigations ;
- sources ;
- recherches ;
- preuves ;
- entités ;
- relations ;
- chronologie ;
- journal ;
- hypothèses ;
- catégories ;
- tags.

Sans une couche d'abstraction, chaque module devrait manipuler directement
SQLite, ce qui entraînerait :

- une forte duplication de code ;
- une gestion incohérente des erreurs ;
- des transactions difficiles à maintenir ;
- une architecture difficile à faire évoluer.

L'objectif de ce ticket est d'éviter cette dette technique.

---

## Architecture cible

La couche Database devra évoluer vers l'organisation suivante :

```text
include/database/
├── database.h
├── connection.h
├── transaction.h
├── statement.h
├── error.h
├── schema.h
```

```text
src/database/
├── database.c
├── connection.c
├── transaction.c
├── statement.c
├── error.c
├── schema.c
```

Les futurs modules métier seront ajoutés ultérieurement.

---

## Structure Database

Créer un type opaque :

```c
typedef struct Database Database;
```

L'implémentation reste privée.

La structure interne contiendra au minimum :

- le pointeur `sqlite3 *`;
- le chemin de la base ;
- l'état de la transaction ;
- la version du schéma.

Cette abstraction permettra de faire évoluer facilement l'implémentation sans
modifier l'API publique.

---

## Connexion

Créer les fonctions :

```c
Database *database_open(
    const char *database_path
);

void database_close(
    Database *database
);
```

Ces fonctions sont responsables de :

- l'ouverture de SQLite ;
- l'activation des PRAGMA nécessaires ;
- la fermeture propre de la base.

---

## Transactions

Créer les fonctions :

```c
bool database_begin(
    Database *database
);

bool database_commit(
    Database *database
);

bool database_rollback(
    Database *database
);
```

Toute gestion des transactions devra passer exclusivement par ces fonctions.

---

## Gestion des requêtes préparées

Créer une couche d'abstraction pour les instructions préparées.

L'objectif est de centraliser :

- `sqlite3_prepare_v2()`
- `sqlite3_bind_*()`
- `sqlite3_step()`
- `sqlite3_reset()`
- `sqlite3_finalize()`

Aucun futur module métier ne devra appeler directement ces fonctions SQLite.

---

## Gestion des erreurs

Créer un point d'entrée unique pour les erreurs SQLite.

Toutes les erreurs devront passer par une fonction dédiée.

Cette centralisation facilitera :

- le débogage ;
- la journalisation ;
- l'intégration future avec le journal d'audit.

---

## Responsabilités

À la fin du ticket :

- la couche Database est la seule autorisée à manipuler SQLite ;
- les autres modules ne connaissent pas `sqlite3`;
- les transactions sont centralisées ;
- les erreurs sont uniformisées ;
- les requêtes préparées sont encapsulées.

---

## Hors périmètre

Ce ticket ne doit pas :

- ajouter de nouvelles tables ;
- modifier le schéma SQL ;
- créer un CRUD métier ;
- modifier GTK ;
- implémenter des migrations ;
- ajouter des fonctionnalités d'investigation.

---

## Critères d'acceptation

- [ ] Création du type opaque `Database`.
- [ ] API publique de connexion disponible.
- [ ] API publique des transactions disponible.
- [ ] Encapsulation des requêtes préparées.
- [ ] Gestion centralisée des erreurs.
- [ ] Aucune utilisation directe de `sqlite3` en dehors du dossier `database`.
- [ ] Documentation des nouvelles API.
- [ ] Tous les tests existants restent valides.

---

## Commit attendu

```text
refactor(database): introduce database infrastructure layer
```
