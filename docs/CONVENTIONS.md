# Conventions de développement

Version : 1.0

---

# Philosophie

Labfy Investigation est développé comme un logiciel d'enquête numérique.

Les choix d'architecture privilégient :

- la lisibilité ;
- la maintenabilité ;
- la robustesse ;
- la traçabilité ;
- la simplicité.

Une solution simple et cohérente est toujours préférée à une solution
complexe.

---

# Langue

Le domaine métier est écrit en français.

Exemples :

- preuve
- entite
- personne
- source
- chronologie
- journal
- hypothese
- recherche

Les standards techniques conservent leur nom d'origine.

Exemples :

- uuid
- sha256
- mime_type
- sqlite
- created_at
- updated_at
- relative_path

---

# Architecture

Le projet est organisé par domaine métier.

Exemple :

```
preuve
    ↓
preuve.h
preuve.c

entite
    ↓
entite.h
entite.c
```

Chaque module possède une responsabilité unique.

---

# SQL

Une table représente un objet métier.

Une table importante possède :

- une structure C ;
- un module C ;
- des tests.

Les objets métier utilisent :

```
TEXT PRIMARY KEY
```

contenant un UUID.

Les tables de référence utilisent :

```
INTEGER PRIMARY KEY
```

---

# UUID

Tous les objets métier utilisent un UUID.

Exemple :

```
preuve
entite
personne
source
chronologie
journal
```

Il n'existe pas simultanément :

```
id
uuid
```

La colonne :

```
id
```

contient directement l'UUID.

---

# Dates

Toutes les dates sont enregistrées en UTC.

Format :

```
YYYY-MM-DDTHH:MM:SSZ
```

---

# Chemins

Les chemins sont toujours relatifs à la racine de l'enquête.

Autorisé :

```
01_Preuves_Originales/Documents/facture.pdf
```

Interdit :

```
/home/user/Documents/...
```

---

# Suppression

Les objets métier importants ne sont jamais supprimés immédiatement.

Une suppression est une modification d'état.

Exemple :

```
active
archived
deleted
```

Une purge physique éventuelle devra être explicite.

---

# Nommage

Le SQL utilise :

- snake_case
- minuscules
- aucun accent
- aucun espace

---

# Code C

Norme :

```
C17
```

Variables :

Toujours explicites.

Autorisé :

```c
relative_path
description
commentaire
created_at
```

Interdit :

```c
path
desc
comm
crt
```

Les fonctions utilisent le nom du module.

Exemple :

```c
preuve_create()
preuve_load()
preuve_update()
preuve_delete()
```

---

# Structures

Une structure représente un objet métier.

Exemple :

```c
typedef struct Preuve Preuve;
```

Les structures publiques sont opaques lorsque cela est possible.

---

# Tests

Toute nouvelle fonctionnalité importante possède un test.

Le développement suit toujours l'ordre :

```
Conception

↓

Implémentation

↓

Tests

↓

Commit
```

---

# Git

Un ticket terminé correspond à un commit.

Le message de commit est rédigé en anglais.

Exemple :

```
feat(database): initialize investigation database
```

---

# Documentation

Les décisions importantes sont documentées.

Les changements de schéma SQLite sont précédés d'un audit.

Aucune décision importante ne doit exister uniquement dans le code.

---

# Objectif

Le projet doit rester compréhensible plusieurs années après sa création.

La priorité est donnée à la qualité du code plutôt qu'à la rapidité de
développement.
