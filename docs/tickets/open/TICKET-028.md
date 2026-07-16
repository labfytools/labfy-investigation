# Ticket #028 — Ajouter l’ouverture d’une enquête existante

## Contexte

Les tickets précédents ont permis de mettre en place :

- la création transactionnelle d’une base d’enquête ;
- la couche `Database` ;
- les requêtes préparées `DatabaseStatement` ;
- la gestion des transactions et des erreurs ;
- le modèle `InvestigationRecord` ;
- le DAO `InvestigationDao` permettant de charger l’unique ligne de la table `investigation`.

Le projet possède également un type `InvestigationProject` chargé de représenter les chemins du projet sur le système de fichiers, notamment :

- le dossier racine de l’enquête ;
- le chemin du fichier `Enquete.sqlite`.

Cependant, l’application ne possède pas encore d’objet représentant une enquête réellement ouverte pendant son exécution.

Les différentes ressources sont encore séparées :

```text
InvestigationProject
Database
InvestigationRecord
```

Il faut désormais les regrouper dans un contexte cohérent dont la durée de vie correspond à celle d’une enquête ouverte.

## Objectif

Créer un type opaque `InvestigationSession` chargé d’ouvrir une enquête existante et de conserver :

- son contexte de fichiers `InvestigationProject` ;
- sa connexion `Database` ;
- ses informations persistées `InvestigationRecord`.

L’ouverture doit vérifier que le dossier sélectionné correspond bien aux informations enregistrées dans la base SQLite.

La connexion SQLite doit rester ouverte pendant toute la durée de vie de la session afin de permettre les futurs appels aux DAO.

## Architecture attendue

```text
Application
    │
    ▼
InvestigationSession
    ├── InvestigationProject
    ├── Database
    └── InvestigationRecord
            ▲
            │
      InvestigationDao
            │
            ▼
      DatabaseStatement
            │
            ▼
          SQLite
```

## Travail à réaliser

### 1. Créer le type `InvestigationSession`

Créer les fichiers :

```text
include/core/investigation_session.h
src/core/investigation_session.c
```

Le type doit être opaque :

```c
typedef struct InvestigationSession InvestigationSession;
```

Sa représentation privée doit contenir au minimum :

```c
struct InvestigationSession
{
    InvestigationProject *project;
    Database *database;
    InvestigationRecord *record;
};
```

Le header public ne doit pas exposer cette structure.

### 2. Définir les erreurs d’ouverture

Créer une énumération dédiée :

```c
typedef enum
{
    INVESTIGATION_SESSION_ERROR_INVALID_ARGUMENT,
    INVESTIGATION_SESSION_ERROR_ROOT_NOT_FOUND,
    INVESTIGATION_SESSION_ERROR_DATABASE_NOT_FOUND,
    INVESTIGATION_SESSION_ERROR_PROJECT,
    INVESTIGATION_SESSION_ERROR_DATABASE,
    INVESTIGATION_SESSION_ERROR_RECORD,
    INVESTIGATION_SESSION_ERROR_ROOT_MISMATCH,
    INVESTIGATION_SESSION_ERROR_MEMORY
} InvestigationSessionError;
```

Définir un domaine d’erreur GLib :

```c
#define INVESTIGATION_SESSION_ERROR \
    investigation_session_error_quark()

GQuark investigation_session_error_quark(void);
```

Les erreurs doivent être transmises avec un paramètre :

```c
GError **error
```

Lorsqu’une erreur provenant de `Database` doit être propagée, son message doit être copié dans le `GError` avant la fermeture de la connexion.

### 3. Ajouter la fonction d’ouverture

Déclarer :

```c
InvestigationSession *investigation_session_open(
    const char *investigation_root_path,
    GError **error
);
```

La fonction doit ouvrir une enquête déjà existante à partir de son dossier racine.

Elle ne doit pas créer une nouvelle enquête.

### 4. Valider les paramètres

La fonction doit refuser :

```text
investigation_root_path == NULL
investigation_root_path vide
```

Le code d’erreur attendu est :

```c
INVESTIGATION_SESSION_ERROR_INVALID_ARGUMENT
```

Le paramètre `error` peut être `NULL`.

Si `error` n’est pas `NULL`, il doit respecter les conventions GLib :

```c
*error == NULL
```

au moment de l’appel.

### 5. Vérifier le dossier racine

Le chemin fourni doit correspondre à un dossier existant.

La fonction doit vérifier :

```c
G_FILE_TEST_IS_DIR
```

Un chemin inexistant ou qui ne représente pas un dossier doit produire :

```c
INVESTIGATION_SESSION_ERROR_ROOT_NOT_FOUND
```

Le chemin doit être normalisé avec :

```c
g_canonicalize_filename()
```

La session doit travailler à partir de ce chemin canonique.

### 6. Construire `InvestigationProject`

Réutiliser l’API existante de `InvestigationProject`.

La logique de construction du chemin de la base ne doit pas être dupliquée dans `InvestigationSession`.

Le chemin attendu reste géré par `InvestigationProject` :

```text
<racine>/00_BaseDeDonnees/Enquete.sqlite
```

Si l’API actuelle de `InvestigationProject` ne permet pas de représenter un projet existant, elle peut être étendue de manière minimale.

Aucune logique SQLite ne doit être ajoutée dans `InvestigationProject`.

### 7. Vérifier le fichier SQLite

Avant l’ouverture de la connexion, vérifier que le chemin retourné par `InvestigationProject` correspond à un fichier régulier :

```c
G_FILE_TEST_IS_REGULAR
```

Si le fichier n’existe pas, retourner :

```c
INVESTIGATION_SESSION_ERROR_DATABASE_NOT_FOUND
```

L’ouverture d’une enquête existante ne doit jamais créer silencieusement une nouvelle base vide.

### 8. Ouvrir la connexion Database

Utiliser :

```c
database_open()
```

La fonction `investigation_session_open()` ne doit pas appeler directement :

```c
sqlite3_open()
sqlite3_open_v2()
sqlite3_close()
```

Si `database_open()` échoue, retourner :

```c
INVESTIGATION_SESSION_ERROR_DATABASE
```

La connexion doit rester ouverte si la session est créée avec succès.

### 9. Charger l’enquête persistée

Utiliser :

```c
investigation_dao_load()
```

Le DAO doit retourner un `InvestigationRecord`.

Si le chargement échoue :

- récupérer le code et le message de la dernière erreur `Database` ;
- copier le message dans un `GError` ;
- retourner `NULL` ;
- fermer proprement la connexion ;
- libérer le projet ;
- ne laisser aucune ressource allouée.

Le code d’erreur de session attendu est :

```c
INVESTIGATION_SESSION_ERROR_RECORD
```

### 10. Vérifier la cohérence du chemin racine

Le chemin sélectionné doit correspondre au champ persistant :

```text
investigation.root_path
```

Comparer les versions canoniques de :

```text
chemin racine sélectionné
chemin racine enregistré dans InvestigationRecord
```

La comparaison doit être faite après normalisation avec :

```c
g_canonicalize_filename()
```

Si les chemins ne correspondent pas, l’ouverture doit échouer avec :

```c
INVESTIGATION_SESSION_ERROR_ROOT_MISMATCH
```

Cette vérification évite d’ouvrir une base copiée ou déplacée sans détecter l’incohérence.

Le déplacement volontaire d’une enquête sera traité dans un ticket distinct.

### 11. Construire la session

La session ne doit être créée qu’après validation complète :

```text
dossier racine valide
        ↓
InvestigationProject valide
        ↓
fichier SQLite présent
        ↓
Database ouverte
        ↓
InvestigationRecord chargé
        ↓
chemin racine cohérent
        ↓
InvestigationSession créée
```

En cas d’échec d’allocation, retourner :

```c
INVESTIGATION_SESSION_ERROR_MEMORY
```

### 12. Ajouter la fonction de fermeture

Déclarer :

```c
void investigation_session_close(
    InvestigationSession *session
);
```

Cette fonction doit accepter `NULL`.

Elle doit libérer toutes les ressources possédées par la session :

```text
InvestigationRecord
Database
InvestigationProject
InvestigationSession
```

La session devient propriétaire de ces trois objets dès que son ouverture réussit.

### 13. Ajouter les accesseurs

Ajouter :

```c
const InvestigationProject *investigation_session_get_project(
    const InvestigationSession *session
);

const InvestigationRecord *investigation_session_get_record(
    const InvestigationSession *session
);

Database *investigation_session_get_database(
    InvestigationSession *session
);
```

Les pointeurs retournés appartiennent à la session et ne doivent pas être libérés par l’appelant.

Les accesseurs doivent retourner `NULL` si la session reçue est `NULL`.

`Database` reste non constante car les futurs DAO auront besoin d’une connexion modifiable.

### 14. Interdire les dépendances SQLite et GTK

Le module `InvestigationSession` ne doit pas inclure :

```c
#include <sqlite3.h>
#include <gtk/gtk.h>
```

Il doit exclusivement utiliser les abstractions existantes :

```text
InvestigationProject
Database
InvestigationDao
InvestigationRecord
GLib
```

## Tests à ajouter

Créer :

```text
tests/test_investigation_session.c
```

### Test d’ouverture valide

Créer un dossier temporaire.

Initialiser une base avec :

```c
database_initialize()
```

Ouvrir ensuite l’enquête avec :

```c
investigation_session_open()
```

Vérifier :

- la session n’est pas `NULL` ;
- aucune erreur n’est produite ;
- le projet est disponible ;
- la connexion Database est disponible ;
- le record est disponible ;
- le nom de l’enquête est correct ;
- le chemin racine est correct ;
- l’UUID est valide ;
- `created_at` n’est pas vide ;
- `updated_at` n’est pas vide ;
- la connexion peut encore être utilisée par un DAO ;
- la fermeture libère correctement les ressources.

### Test des paramètres invalides

Vérifier :

```c
investigation_session_open(NULL, &error) == NULL
investigation_session_open("", &error) == NULL
```

Le code attendu est :

```c
INVESTIGATION_SESSION_ERROR_INVALID_ARGUMENT
```

Vérifier également le comportement avec :

```c
error == NULL
```

### Test d’un dossier inexistant

Utiliser un chemin inexistant.

Vérifier :

```text
résultat == NULL
erreur == INVESTIGATION_SESSION_ERROR_ROOT_NOT_FOUND
message non vide
```

### Test d’un chemin qui n’est pas un dossier

Créer un fichier temporaire et utiliser son chemin comme racine.

Vérifier :

```text
résultat == NULL
erreur == INVESTIGATION_SESSION_ERROR_ROOT_NOT_FOUND
```

### Test d’une base absente

Créer une structure de projet valide sans fichier :

```text
00_BaseDeDonnees/Enquete.sqlite
```

Vérifier :

```text
résultat == NULL
erreur == INVESTIGATION_SESSION_ERROR_DATABASE_NOT_FOUND
```

La fonction ne doit pas créer de nouveau fichier SQLite.

### Test d’une base invalide

Créer un fichier SQLite vide ou une base ne contenant pas la table :

```text
investigation
```

Vérifier :

```text
résultat == NULL
erreur == INVESTIGATION_SESSION_ERROR_RECORD
message non vide
```

### Test d’un chemin racine incohérent

Créer une base avec un chemin racine enregistré différent du dossier utilisé pour l’ouverture.

Vérifier :

```text
résultat == NULL
erreur == INVESTIGATION_SESSION_ERROR_ROOT_MISMATCH
message non vide
```

### Test des accesseurs avec NULL

Vérifier :

```c
investigation_session_get_project(NULL) == NULL
investigation_session_get_record(NULL) == NULL
investigation_session_get_database(NULL) == NULL
```

Vérifier également :

```c
investigation_session_close(NULL);
```

### Test de réutilisation du DAO

Après l’ouverture valide d’une session, appeler de nouveau :

```c
investigation_dao_load(
    investigation_session_get_database(session)
);
```

Vérifier que la connexion reste fonctionnelle pendant toute la durée de vie de la session.

## Gestion de la mémoire

Toutes les sorties d’échec de `investigation_session_open()` doivent libérer les ressources déjà créées.

Le nettoyage doit couvrir les cas suivants :

```text
échec avant création du projet
échec après création du projet
échec après ouverture de Database
échec après chargement du record
échec lors de la comparaison des chemins
échec lors de l’allocation de la session
```

Aucun objet ne doit être libéré deux fois.

Aucune ressource temporaire ne doit rester allouée :

```text
chemins canoniques
messages copiés
GError temporaires
InvestigationProject
Database
InvestigationRecord
```

## Makefile

Ajouter :

```make
TEST_INVESTIGATION_SESSION := tests/test_investigation_session
```

Ajouter une règle compilant au minimum :

```text
tests/test_investigation_session.c

src/core/investigation_session.c
src/core/investigation_project.c

src/dao/investigation_dao.c
src/models/investigation_record.c

src/database/database.c
src/database/schema.c
src/database/statement.c
src/database/transaction.c
src/database/error.c
```

Lier avec :

```text
GLib
SQLite
```

Ajouter le test aux cibles :

```text
test
clean
```

La nouvelle sortie attendue est :

```text
InvestigationSession : tous les tests sont valides.
```

## Critères d’acceptation

- [ ] Le type `InvestigationSession` est opaque.
- [ ] Une session possède un `InvestigationProject`.
- [ ] Une session possède une connexion `Database`.
- [ ] Une session possède un `InvestigationRecord`.
- [ ] Le dossier racine est validé avant l’ouverture.
- [ ] Le chemin racine est normalisé.
- [ ] Le chemin de la base provient de `InvestigationProject`.
- [ ] Le fichier SQLite doit exister avant l’appel à `database_open()`.
- [ ] L’ouverture ne crée jamais silencieusement une nouvelle base.
- [ ] Le record est chargé avec `InvestigationDao`.
- [ ] Le chemin enregistré est comparé au chemin sélectionné.
- [ ] Une incohérence de chemin empêche l’ouverture.
- [ ] La connexion reste ouverte pendant la durée de vie de la session.
- [ ] La fermeture libère toutes les ressources.
- [ ] Les accesseurs acceptent une session `NULL`.
- [ ] Les erreurs sont propagées avec `GError`.
- [ ] Aucun type SQLite n’apparaît dans l’API de la session.
- [ ] Aucune dépendance GTK n’est ajoutée.
- [ ] Les tests d’ouverture valide sont présents.
- [ ] Les tests de paramètres invalides sont présents.
- [ ] Les tests de dossier absent sont présents.
- [ ] Les tests de base absente sont présents.
- [ ] Les tests de base invalide sont présents.
- [ ] Les tests de chemin incohérent sont présents.
- [ ] Les anciens tests restent valides.
- [ ] `make` réussit sans erreur.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

## Audit attendu

Les commandes suivantes ne doivent rien afficher :

```bash
rg -n 'sqlite3_|#include <sqlite3.h>|#include <gtk' \
    include/core/investigation_session.h \
    src/core/investigation_session.c
```

Vérifier également que la logique du chemin SQLite n’est pas dupliquée :

```bash
rg -n '00_BaseDeDonnees|Enquete.sqlite' \
    src/core/investigation_session.c
```

Le résultat attendu est aucune occurrence, sauf éventuellement dans un commentaire de documentation justifié.

## Hors périmètre

Ce ticket ne doit pas ajouter :

- la création d’une nouvelle enquête ;
- le déplacement d’une enquête ;
- la réparation automatique d’un chemin racine incohérent ;
- la modification de `investigation.root_path` ;
- l’intégration dans la fenêtre GTK ;
- une boîte de dialogue de sélection ;
- l’affichage du nom de l’enquête ;
- les DAO des preuves, sources ou entités ;
- la fermeture demandée par l’interface utilisateur ;
- la sauvegarde automatique ;
- une migration de schéma ;
- un verrouillage multi-instance de la base.

## Fichiers principalement concernés

```text
include/core/investigation_session.h

src/core/investigation_session.c

tests/test_investigation_session.c

Makefile
```

Une adaptation limitée de ces fichiers est autorisée si nécessaire :

```text
include/core/investigation_project.h
src/core/investigation_project.c
```

## Résultat attendu

À la fin du ticket, le programme doit pouvoir ouvrir une enquête existante à partir de son dossier racine.

Une session valide doit conserver ensemble :

```text
le projet de fichiers
la connexion SQLite
les informations persistées de l’enquête
```

Cette session deviendra le contexte principal utilisé ultérieurement par l’interface et les futurs DAO.

## Commit attendu

Une fois tous les critères d’acceptation validés :

```text
feat(core): add investigation session loader
```

Avant le commit :

```bash
make clean
make
make test
git diff --check
git status --short
```

Préparer les fichiers :

```bash
git add \
    Makefile \
    include/core/investigation_session.h \
    src/core/investigation_session.c \
    tests/test_investigation_session.c
```

Ajouter également les fichiers `InvestigationProject` uniquement s’ils ont réellement été modifiés :

```bash
git add \
    include/core/investigation_project.h \
    src/core/investigation_project.c
```

Contrôler le contenu préparé :

```bash
git diff --cached --stat
git diff --cached
```

Créer le commit :

```bash
git commit -m "feat(core): add investigation session loader"
```

Le push ne doit être effectué qu’après validation complète de la compilation, des tests et du contenu du commit.
