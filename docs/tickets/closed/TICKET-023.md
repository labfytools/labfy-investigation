# Ticket #023

## Titre

Consolider le schéma métier initial de l'enquête.

---

## Objectif

Transformer le schéma SQLite préparé lors des premières phases du projet en
un schéma métier officiel, cohérent, documenté et testable.

La base existante sert de fondation.

Le travail ne consiste pas à repartir de zéro, mais à :

- conserver les concepts métier déjà définis ;
- corriger les incohérences ;
- renforcer les contraintes ;
- uniformiser les conventions ;
- intégrer le schéma au module `Database`.

---

## Base de travail

La base historique contient notamment les tables suivantes :

```text
preuves
entites
personnes
chronologie
sources
recherche
hypotheses
journal
tags
types_preuve
types_entite
types_source
types_outils
associations
personnes_entites
entite_tags
preuves_tag
```

Les tables créées au ticket #022 restent également obligatoires :

```text
metadata
investigation
```

Le schéma officiel doit donc préserver ces concepts tout en les consolidant.

---

## Architecture

```text
InvestigationProject
        │
        ▼
Database
        │
        ▼
Schéma SQLite versionné
        │
        ├── Métadonnées
        ├── Enquête
        ├── Preuves
        ├── Entités
        ├── Personnes
        ├── Sources
        ├── Chronologie
        ├── Recherches
        ├── Hypothèses
        ├── Journal
        └── Tags et relations
```

Le module `Database` reste le seul autorisé à exécuter du SQL.

---

## Décisions de conception à officialiser

### Identifiants

Les entités métier utiliseront des identifiants texte de type UUID.

Exemple :

```text
550e8400-e29b-41d4-a716-446655440000
```

Les identifiants sont générés par l'application avec :

```c
g_uuid_string_random()
```

Les tables de référence simples peuvent utiliser des identifiants entiers.

---

### Dates

Les dates techniques sont stockées en UTC au format ISO 8601 :

```text
YYYY-MM-DDTHH:MM:SSZ
```

Exemple :

```text
2026-07-15T08:42:17Z
```

---

### Nommage

Les noms SQL utilisent uniquement :

- des minuscules ;
- du `snake_case` ;
- aucun accent ;
- aucun espace.

Les noms des tables de liaison sont uniformisés.

Exemple :

```text
preuve_tags
entite_tags
personne_entites
```

Aucune forme singulier/pluriel incohérente ne doit subsister.

---

### Clés étrangères

Toutes les relations métier doivent utiliser des clés étrangères explicites.

Le module `Database` doit activer :

```sql
PRAGMA foreign_keys = ON;
```

à chaque ouverture de connexion.

Les comportements `ON DELETE` doivent être décidés pour chaque relation.

---

### Tables de liaison

Chaque table de liaison doit empêcher les doublons grâce à :

- une clé primaire composite ;
- ou une contrainte `UNIQUE`.

Exemple :

```sql
PRIMARY KEY (preuve_id, tag_id)
```

---

## Tables métier à consolider

### `preuves`

Responsabilité :

- représenter un élément de preuve ;
- mémoriser son nom ;
- son chemin relatif ;
- son type ;
- sa description ;
- ses dates ;
- son hash éventuel ;
- son statut.

Contraintes minimales :

- identifiant obligatoire ;
- nom obligatoire ;
- chemin relatif obligatoire ;
- type obligatoire ;
- date de création obligatoire ;
- chemin unique au sein d'une enquête.

---

### `entites`

Responsabilité :

- représenter une entité identifiée pendant l'enquête.

Exemples :

- adresse email ;
- compte bancaire ;
- profil social ;
- pseudonyme ;
- téléphone ;
- document d'identité.

Contraintes minimales :

- identifiant obligatoire ;
- type obligatoire ;
- valeur obligatoire ;
- date de création obligatoire.

---

### `personnes`

Responsabilité :

- représenter une personne physique ou un profil humain étudié.

Le modèle ne doit pas obliger à connaître l'identité civile complète.

Une personne peut n'avoir qu'un pseudonyme ou une désignation temporaire.

---

### `sources`

Responsabilité :

- représenter l'origine d'une information ou d'une preuve.

Exemples :

- site web ;
- email ;
- réseau social ;
- document ;
- témoignage ;
- outil OSINT.

---

### `chronologie`

Responsabilité :

- mémoriser les événements importants de l'enquête ;
- relier éventuellement un événement à une preuve, une entité ou une source.

---

### `recherches`

Responsabilité :

- documenter une opération de recherche ;
- mémoriser la requête ;
- l'outil employé ;
- la date ;
- le résultat ;
- les observations.

---

### `hypotheses`

Responsabilité :

- enregistrer une hypothèse de travail ;
- mémoriser son statut ;
- sa justification ;
- les éléments qui la soutiennent ou la contredisent.

---

### `journal`

Responsabilité :

- conserver la trace des actions significatives réalisées dans l'enquête.

Exemples :

- création d'une preuve ;
- modification d'une entité ;
- calcul d'un hash ;
- génération d'un rapport ;
- import d'un fichier.

---

### `tags`

Responsabilité :

- permettre une classification libre des preuves et des entités.

---

## Tables de référence

Les tables suivantes doivent être consolidées :

```text
types_preuve
types_entite
types_source
types_outils
```

Elles doivent comporter au minimum :

```sql
id          INTEGER PRIMARY KEY
code        TEXT NOT NULL UNIQUE
label       TEXT NOT NULL
description TEXT
```

Les valeurs initiales sont insérées lors de la création de la base.

---

## Tables de liaison

Les relations suivantes doivent être prévues :

```text
preuve_tags
entite_tags
personne_entites
```

Le schéma existant contient également une table générique :

```text
associations
```

Cette table doit être auditée avant conservation.

Elle ne doit être gardée que si son rôle est clairement défini et ne fait pas
doublon avec des relations spécialisées.

---

## Audit obligatoire

Avant d'écrire le schéma définitif, chaque table de la base historique doit
être classée dans l'une des catégories suivantes :

```text
CONSERVER
MODIFIER
RENOMMER
FUSIONNER
SUPPRIMER
```

La décision doit être documentée dans :

```text
docs/database/SCHEMA_AUDIT_V1.md
```

Le document doit contenir au minimum :

```text
Nom historique
Décision
Nom final
Justification
Modifications prévues
```

---

## Organisation du module Database

Le code SQL est organisé par domaine métier.

```
include/
└── database/
    ├── database.h
    ├── schema.h
    ├── preuve.h
    ├── entite.h
    ├── personne.h
    ├── source.h
    ├── chronologie.h
    ├── recherche.h
    ├── hypothese.h
    ├── journal.h
    ├── tag.h
    ├── type_preuve.h
    ├── type_entite.h
    ├── type_source.h
    └── type_outil.h

src/
└── database/
    ├── database.c
    ├── schema.c
    ├── preuve.c
    ├── entite.c
    ├── personne.c
    ├── source.c
    ├── chronologie.c
    ├── recherche.c
    ├── hypothese.c
    ├── journal.c
    ├── tag.c
    ├── type_preuve.c
    ├── type_entite.c
    ├── type_source.c
    └── type_outil.c

database/
├── schema_v1.sql
├── indexes.sql
├── triggers.sql
└── reference_data.sql
```

---

### Responsabilités

`database.c`

- ouverture de SQLite ;
- fermeture ;
- transactions ;
- erreurs ;
- point d'entrée du module Database.

`schema.c`

- installation du schéma ;
- migrations ;
- exécution des scripts SQL.

Chaque module métier est responsable exclusivement de son objet métier.

Exemple :

```
preuve.c
```

gère uniquement :

- création ;
- lecture ;
- modification ;
- suppression logique ;
- gestion des tags de la preuve.

Les tables de liaison simples ne possèdent pas de module dédié.

Exemples :

```
preuve_tags
personne_entites
entite_tags
```

Leur manipulation est réalisée par le module métier correspondant.

---

## Intégration dans `Database`

Le module `database_initialize()` doit exécuter le schéma officiel contenu
dans :

```text
database/schema_v1.sql
```

Deux approches sont acceptables :

1. intégrer le schéma au binaire via une ressource GLib ;
2. générer une chaîne C depuis le fichier SQL lors de la compilation.

Le logiciel installé ne doit pas dépendre d'un chemin relatif fragile vers le
dépôt source.

---

## Principe d'organisation

Une table métier importante implique systématiquement :

- une structure C ;
- un module C ;
- des tests ;
- une documentation.

La correspondance est la suivante :

| SQL | C |
|-----|---|
| preuves | preuve.c |
| entites | entite.c |
| personnes | personne.c |
| sources | source.c |
| chronologie | chronologie.c |
| journal | journal.c |
| recherches | recherche.c |
| hypotheses | hypothese.c |

---

## Version du schéma

Le numéro de version reste :

```text
1
```

tant que Labfy Investigation n'a pas produit de base officiellement publiée.

Une fois le schéma V1 considéré comme stable et distribué, toute modification
incompatible nécessitera :

- une nouvelle version ;
- une migration dédiée ;
- une mise à jour de `schema_version`.

---

## Hors périmètre

Ce ticket ne doit pas :

- développer les fonctions CRUD ;
- créer les structures C `Preuve`, `Entite` ou `Personne` ;
- connecter les tables à GTK ;
- importer les anciennes données automatiquement ;
- créer les migrations V2 ;
- produire un rapport ;
- ajouter la recherche plein texte.

---

## Contraintes techniques

- C17 ;
- SQLite3 ;
- GLib ;
- aucune dépendance GTK ;
- toutes les tables utilisent des contraintes explicites ;
- toutes les clés étrangères sont documentées ;
- toutes les tables de liaison empêchent les doublons ;
- aucun SQL métier hors du module `Database` ;
- aucun identifiant généré par SQLite pour les objets métier en UUID ;
- compilation sans warning.

---

## Tests

Faire évoluer :

```text
tests/test_database.c
```

Les tests doivent vérifier :

- la présence de toutes les tables officielles ;
- la présence des tables de référence ;
- l'insertion des valeurs initiales ;
- l'activation des clés étrangères ;
- le refus d'une clé étrangère invalide ;
- le refus d'un doublon dans une table de liaison ;
- le refus des colonnes obligatoires manquantes ;
- l'unicité des codes des tables de référence ;
- la valeur `schema_version = 1` ;
- l'intégrité SQLite avec :

```sql
PRAGMA integrity_check;
```

---

## Critères d'acceptation

- [ ] Le schéma historique a été audité.
- [ ] Chaque table possède une décision documentée.
- [ ] Un schéma V1 officiel existe.
- [ ] Les conventions de nommage sont uniformes.
- [ ] Toutes les clés primaires sont explicites.
- [ ] Toutes les clés étrangères sont explicites.
- [ ] Les relations empêchent les doublons.
- [ ] Les colonnes obligatoires utilisent `NOT NULL`.
- [ ] Les tables de référence sont initialisées.
- [ ] `database_initialize()` installe le schéma complet.
- [ ] `PRAGMA integrity_check` retourne `ok`.
- [ ] Tous les tests restent valides.
- [ ] Aucune dépendance GTK.

---

## Livrables

```text
docs/database/SCHEMA_AUDIT_V1.md
database/schema_v1.sql
include/database/database.h
src/database/database.c
tests/test_database.c
```

---

## Commit attendu

```text
feat(database): consolidate initial investigation schema
```
