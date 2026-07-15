# Ticket #022

## Titre

Initialiser la base SQLite d'une enquête.

---

## Objectif

Créer et initialiser correctement le fichier :

```text
00_BaseDeDonnees/Enquete.sqlite
```

avec un premier schéma SQLite versionné.

La base doit contenir les métadonnées minimales permettant d'identifier
l'enquête et la version du schéma.

---

## Architecture

```text
InvestigationProject
        │
        ▼
Database
        │
        ▼
SQLite
```

`InvestigationProject` orchestre la création de l'enquête.

Le module `Database` est seul responsable de l'ouverture de SQLite,
de l'exécution du schéma et de la fermeture de la base.

---

## Responsabilités

### InvestigationProject

Le module doit :

- créer l'arborescence ;
- demander au module `Database` d'initialiser `Enquete.sqlite` ;
- considérer la création comme échouée si l'initialisation SQLite échoue ;
- nettoyer toute l'enquête créée en cas d'échec.

### Database

Le module doit :

- ouvrir ou créer le fichier SQLite ;
- démarrer une transaction ;
- créer le schéma initial ;
- insérer les métadonnées ;
- valider la transaction ;
- annuler la transaction en cas d'erreur ;
- fermer proprement la connexion.

---

## Nouveaux fichiers

```text
include/database/database.h
src/database/database.c
```

Éventuellement :

```text
include/database/schema.h
src/database/schema.c
```

si le schéma devient trop volumineux pour rester dans `database.c`.

Pour ce ticket, un seul module `database.c` est acceptable.

---

## Interface publique attendue

```c
bool database_initialize(
    const char *database_path
);
```

La fonction retourne :

```c
true
```

si la base a été correctement initialisée.

Elle retourne :

```c
false
```

en cas d'erreur.

---

## Schéma initial

### Table `metadata`

```sql
CREATE TABLE metadata
(
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
```

### Métadonnées obligatoires

```text
schema_version
application
created_at
investigation_uuid
```

Valeurs attendues :

```text
schema_version     = 1
application        = Labfy Investigation
created_at         = date UTC ISO 8601
investigation_uuid = UUID unique
```

Exemple :

```text
2026-07-14T18:42:15Z
```

---

## Table `investigation`

Créer également une table minimale représentant l'enquête :

```sql
CREATE TABLE investigation
(
    id          INTEGER PRIMARY KEY CHECK (id = 1),
    name        TEXT NOT NULL,
    root_path   TEXT NOT NULL,
    created_at  TEXT NOT NULL
);
```

La table contient une seule ligne correspondant à l'enquête courante.

---

## Données nécessaires

`database_initialize()` doit recevoir suffisamment d'informations pour
initialiser correctement la base.

L'interface pourra donc évoluer vers :

```c
bool database_initialize(
    const char *database_path,
    const char *investigation_name,
    const char *investigation_root_path
);
```

Cette signature est préférée pour éviter que le module `Database`
reconstruise ou devine des informations métier.

---

## UUID

L'UUID doit être généré avec GLib :

```c
g_uuid_string_random()
```

La chaîne retournée doit être libérée avec :

```c
g_free(uuid);
```

---

## Date de création

La date doit être produite en UTC avec GLib.

Format attendu :

```text
YYYY-MM-DDTHH:MM:SSZ
```

La date doit être enregistrée à la fois :

- dans `metadata.created_at` ;
- dans `investigation.created_at`.

---

## Transaction

Toute l'initialisation doit se dérouler dans une transaction :

```sql
BEGIN IMMEDIATE;
```

Puis :

```sql
COMMIT;
```

En cas d'erreur :

```sql
ROLLBACK;
```

Une base partiellement initialisée ne doit jamais être considérée comme valide.

---

## Intégration avec InvestigationProject

`investigation_project_create()` ne doit plus créer un fichier vide avec :

```c
g_file_set_contents(...)
```

Il doit construire le chemin de la base puis appeler :

```c
database_initialize(
    database_path,
    investigation_name,
    investigation_path
);
```

Si l'appel échoue :

- le fichier SQLite éventuel est supprimé ;
- tous les dossiers créés sont supprimés ;
- `investigation_project_create()` retourne `NULL`.

---

## Validation

À partir de ce ticket, `investigation_project_validate()` doit toujours
vérifier la présence du fichier SQLite, mais pas encore son contenu SQL.

La validation du schéma sera ajoutée dans un prochain ticket dédié.

---

## Hors périmètre

Ce ticket ne doit pas :

- créer les tables Preuves ;
- créer les tables Entites ;
- créer les relations ;
- gérer les migrations ;
- ouvrir une enquête existante ;
- modifier GTK ;
- exposer directement `sqlite3 *` hors du module Database.

---

## Contraintes techniques

- C17 ;
- SQLite3 ;
- GLib ;
- aucune dépendance GTK ;
- aucun état global ;
- requêtes SQL centralisées dans `database.c` ;
- fermeture garantie de la connexion ;
- transaction obligatoire ;
- documentation Doxygen ;
- compilation sans warning.

---

## Tests

Créer :

```text
tests/test_database.c
```

Le test doit :

- créer un dossier temporaire ;
- initialiser une base SQLite ;
- vérifier que le fichier existe ;
- ouvrir la base en lecture ;
- vérifier la présence de la table `metadata` ;
- vérifier la présence de la table `investigation` ;
- vérifier `schema_version = 1` ;
- vérifier `application = Labfy Investigation` ;
- vérifier que `created_at` n'est pas vide ;
- vérifier que l'UUID n'est pas vide ;
- vérifier la ligne unique de la table `investigation` ;
- vérifier le nom et le chemin racine ;
- vérifier qu'une initialisation sur un chemin invalide échoue ;
- nettoyer complètement les fichiers temporaires.

Faire également évoluer :

```text
tests/test_investigation_project.c
```

pour vérifier que la base créée n'est plus vide.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste entièrement valide.
- [ ] `Enquete.sqlite` est une vraie base SQLite.
- [ ] La table `metadata` existe.
- [ ] La table `investigation` existe.
- [ ] `schema_version` vaut `1`.
- [ ] Une date UTC est enregistrée.
- [ ] Un UUID est généré.
- [ ] L'enquête est enregistrée dans la base.
- [ ] L'initialisation est transactionnelle.
- [ ] Toute erreur provoque un nettoyage complet.
- [ ] Aucun type `sqlite3 *` n'est exposé publiquement.
- [ ] Aucune dépendance GTK.

---

## Commit attendu

```text
feat(database): initialize investigation database
```
