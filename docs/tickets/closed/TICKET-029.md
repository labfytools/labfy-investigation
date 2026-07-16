# Ticket #029 — Intégrer `InvestigationSession` au cycle de vie GTK

## Contexte

Le ticket #028 a ajouté `InvestigationSession`, qui permet d’ouvrir une enquête existante de manière contrôlée.

Une session valide possède désormais :

- un `InvestigationProject` ;
- une connexion `Database` ouverte ;
- un `InvestigationRecord` chargé depuis SQLite.

L’application GTK utilise encore l’ancien objet `Investigation` pour représenter le dossier sélectionné.

Son fonctionnement actuel est approximativement le suivant :

```text
FolderDialog
    ↓
Investigation
    ↓
InvestigationTreeBuilder
    ↓
InvestigationTreeModel
    ↓
MainWindow
```

Cette organisation ne conserve pas la connexion SQLite et n’utilise pas les informations persistées dans la table `investigation`.

Il faut désormais remplacer l’ancien objet détenu par `Application` par une véritable `InvestigationSession`.

## Objectif

Faire de `InvestigationSession` le contexte actif de l’application.

Après la sélection d’un dossier :

1. ouvrir une nouvelle `InvestigationSession` ;
2. récupérer le chemin racine depuis son `InvestigationProject` ;
3. construire le nouvel `InvestigationTreeModel` ;
4. ne remplacer l’ancienne enquête qu’après validation complète ;
5. mettre à jour la fenêtre principale ;
6. conserver la session jusqu’à la fermeture de l’application.

L’application ne doit posséder qu’une seule session active à la fois.

## Architecture attendue

```text
FolderDialog
    │
    ▼
Application
    │
    ├── InvestigationSession
    │       ├── InvestigationProject
    │       ├── Database
    │       └── InvestigationRecord
    │
    ├── InvestigationTreeModel
    │
    └── MainWindow
            ├── Sidebar
            ├── Workspace
            └── Barre d’état
```

## Principe de remplacement transactionnel

L’ouverture d’une nouvelle enquête doit suivre cet ordre :

```text
ancienne session toujours active
        ↓
ouvrir la nouvelle session
        ↓
construire le nouvel arbre
        ↓
vérifier que les deux objets sont valides
        ↓
remplacer l’ancien arbre
        ↓
fermer l’ancienne session
        ↓
installer la nouvelle session
        ↓
mettre à jour MainWindow
```

En cas d’échec avant le remplacement :

```text
ancienne session conservée
ancienne arborescence conservée
nouvelle ressource libérée
message d’erreur produit
```

L’application ne doit jamais perdre une enquête déjà ouverte simplement parce qu’une nouvelle sélection est invalide.

---

# Travail à réaliser

## 1. Remplacer l’ancien objet dans `Application`

Modifier :

```text
src/core/application.c
```

La structure privée actuelle contient notamment :

```c
Investigation *investigation;
InvestigationTreeModel *tree_model;
```

Remplacer le premier champ par :

```c
InvestigationSession *session;
```

La structure doit devenir au minimum :

```c
struct Application
{
    GtkApplication *gtk_application;
    MainWindow *main_window;
    InvestigationSession *session;
    InvestigationTreeModel *tree_model;
};
```

Ne pas exposer cette structure dans le header public.

## 2. Modifier les dépendances de `application.c`

Supprimer :

```c
#include "core/investigation.h"
```

Ajouter :

```c
#include "core/investigation_session.h"
#include "core/investigation_project.h"
#include "models/investigation_record.h"
```

Conserver les dépendances nécessaires à :

```text
FolderDialog
MainWindow
InvestigationTreeBuilder
InvestigationTreeModel
GTK
```

`application.c` ne doit pas inclure directement :

```c
#include <sqlite3.h>
```

et ne doit appeler aucune fonction `sqlite3_*`.

## 3. Ouvrir la session après la sélection du dossier

Dans :

```c
application_on_folder_selected()
```

remplacer la création de l’ancien objet :

```c
investigation_new(folder_path)
```

par :

```c
investigation_session_open(
    folder_path,
    &error
);
```

Utiliser des variables temporaires :

```c
InvestigationSession *new_session = NULL;
InvestigationTreeModel *new_tree_model = NULL;
GError *error = NULL;
```

Ne jamais écrire directement dans :

```c
application->session
application->tree_model
```

avant que toute l’ouverture soit validée.

## 4. Gérer une sélection annulée

Lorsque :

```c
folder_path == NULL
```

la fonction doit simplement retourner.

L’enquête déjà ouverte doit rester active.

Aucune session ne doit être fermée.

Aucune erreur ne doit être affichée.

Un message de diagnostic avec `g_print()` reste acceptable :

```text
Sélection annulée.
```

## 5. Gérer l’échec d’ouverture de session

Si :

```c
new_session == NULL
```

la fonction doit :

- afficher un avertissement contenant le message du `GError` ;
- libérer le `GError` ;
- conserver l’ancienne session ;
- conserver l’ancien arbre ;
- ne pas modifier la fenêtre principale.

Exemple de diagnostic acceptable :

```c
g_warning(
    "Impossible d'ouvrir l'enquête : %s",
    error != NULL ? error->message : "erreur inconnue"
);
```

Le dialogue graphique d’erreur est hors périmètre de ce ticket.

## 6. Construire l’arborescence depuis la session

Récupérer le projet de la nouvelle session :

```c
const InvestigationProject *project = NULL;
```

avec :

```c
project = investigation_session_get_project(
    new_session
);
```

Récupérer ensuite son chemin racine :

```c
const char *root_path = NULL;
```

avec :

```c
root_path = investigation_project_get_root_path(
    project
);
```

Construire le nouvel arbre avec :

```c
new_tree_model = investigation_tree_builder_build(
    root_path
);
```

La logique de construction du chemin ne doit pas être dupliquée dans `application.c`.

## 7. Gérer l’échec de construction de l’arbre

Si `InvestigationTreeBuilder` échoue après l’ouverture de la session :

```text
new_session valide
new_tree_model == NULL
```

la fonction doit :

```c
investigation_session_close(new_session);
```

puis retourner.

L’ancienne session et l’ancien arbre doivent rester actifs.

Le message suivant est acceptable :

```text
Impossible de construire l'arborescence de l'enquête.
```

## 8. Remplacer les anciens objets uniquement après validation

Lorsque `new_session` et `new_tree_model` sont tous deux valides :

```c
investigation_tree_model_free(
    application->tree_model
);

investigation_session_close(
    application->session
);

application->tree_model = new_tree_model;
application->session = new_session;
```

Après le transfert :

```c
new_tree_model = NULL;
new_session = NULL;
```

Cette remise à `NULL` n’est pas obligatoire si la fonction retourne immédiatement, mais elle est recommandée pour rendre la propriété explicite.

## 9. Conserver la connexion SQLite ouverte

Après une ouverture réussie, la session doit rester stockée dans :

```c
application->session
```

Il est interdit de fermer la session à la fin de :

```c
application_on_folder_selected()
```

La connexion `Database` doit rester utilisable pendant toute la durée d’ouverture de l’enquête.

## 10. Ajouter un accesseur interne ou public

Ajouter dans :

```text
include/core/application.h
```

la déclaration suivante :

```c
const InvestigationSession *application_get_session(
    const Application *application
);
```

Implémenter dans :

```text
src/core/application.c
```

```c
const InvestigationSession *application_get_session(
    const Application *application
)
{
    if (application == NULL)
    {
        return NULL;
    }

    return application->session;
}
```

Le pointeur retourné appartient à `Application`.

Le code appelant ne doit pas fermer cette session.

Cet accesseur servira aux futurs contrôleurs et DAO.

## 11. Mettre à jour la fenêtre principale

Ajouter dans :

```text
include/views/main_window.h
```

la fonction :

```c
void main_window_set_investigation(
    MainWindow *main_window,
    const char *investigation_name,
    const char *investigation_root_path
);
```

Implémenter dans :

```text
src/views/main_window.c
```

Cette fonction doit :

- accepter `main_window == NULL` ;
- accepter des chaînes `NULL` sans planter ;
- mettre à jour le titre de la fenêtre ;
- mettre à jour la barre d’état.

### Titre attendu

Lorsque le nom est valide :

```text
Labfy Investigation — <nom>
```

Exemple :

```text
Labfy Investigation — Enquete_Session
```

Lorsque le nom est absent :

```text
Labfy Investigation
```

Construire le titre avec GLib :

```c
g_strdup_printf()
```

puis le libérer avec :

```c
g_free()
```

### Barre d’état attendue

Lorsque l’enquête est ouverte :

```text
Enquête ouverte : <nom> — <chemin racine>
```

Lorsque certaines valeurs sont absentes, utiliser une formulation sûre sans déréférencer `NULL`.

La barre d’état ne doit pas conserver un pointeur vers une chaîne temporaire.

`gtk_label_set_text()` copie le texte fourni.

## 12. Récupérer le nom persistant

Dans `application_on_folder_selected()`, récupérer :

```c
const InvestigationRecord *record = NULL;
const char *investigation_name = NULL;
```

avec :

```c
record = investigation_session_get_record(
    application->session
);

investigation_name = investigation_record_get_name(
    record
);
```

Utiliser les données du `InvestigationRecord`.

Ne pas reconstruire le nom à partir du dernier composant du chemin.

## 13. Mettre à jour la fenêtre après le modèle

Après l’installation de la nouvelle session et du nouvel arbre :

```c
main_window_set_tree_model(
    application->main_window,
    application->tree_model
);
```

puis :

```c
main_window_set_investigation(
    application->main_window,
    investigation_name,
    root_path
);
```

L’ordre attendu est :

```text
nouvelle session installée
nouvel arbre installé
sidebar mise à jour
titre et statut mis à jour
```

## 14. Fermer la session dans `application_free()`

Remplacer :

```c
investigation_free(application->investigation);
```

par :

```c
investigation_session_close(
    application->session
);
```

L’ordre de nettoyage conseillé est :

```c
investigation_tree_model_free(
    application->tree_model
);

investigation_session_close(
    application->session
);

main_window_free(
    application->main_window
);
```

Puis libérer `GtkApplication` et `Application` comme actuellement.

`application_free(NULL)` doit rester valide.

## 15. Ne plus utiliser l’ancien objet dans l’application

À la fin du ticket, les symboles suivants ne doivent plus apparaître dans `application.c` :

```text
Investigation *
investigation_new
investigation_free
investigation_get_root_path
investigation_get_database_path
```

Le module historique `investigation.c` n’est pas supprimé dans ce ticket.

Sa suppression éventuelle sera effectuée séparément après vérification qu’aucun autre composant ne l’utilise.

---

# Tests et validations

## 16. Tests automatisés existants

Aucun nouveau test GTK automatisé n’est obligatoire dans ce ticket.

Tous les tests existants doivent rester valides :

```bash
make test
```

En particulier :

```text
InvestigationProject
InvestigationSession
InvestigationTreeBuilder
InvestigationTreeModel
InvestigationDao
```

## 17. Test manuel d’ouverture valide

Créer ou utiliser une enquête valide.

Lancer :

```bash
make
make run
```

Sélectionner le dossier racine de l’enquête.

Vérifier :

- la fenêtre reste ouverte ;
- l’arborescence apparaît dans la sidebar ;
- le titre contient le nom persistant de l’enquête ;
- la barre d’état contient le nom et le chemin racine ;
- aucune erreur SQLite n’apparaît ;
- la session reste active après la fin du callback.

## 18. Test manuel d’annulation

Relancer l’application et annuler la sélection.

Vérifier :

- aucun crash ;
- aucun warning critique ;
- la fenêtre reste ouverte ;
- la barre d’état reste sur :

```text
Aucune enquête ouverte
```

## 19. Test manuel d’un dossier invalide

Sélectionner un dossier qui ne contient pas :

```text
00_BaseDeDonnees/Enquete.sqlite
```

Vérifier :

- aucun crash ;
- un warning explicite est affiché ;
- aucun fichier SQLite n’est créé ;
- la fenêtre reste utilisable ;
- aucune fausse enquête n’apparaît dans la sidebar.

## 20. Test manuel de remplacement

Si l’interface permet une seconde sélection pendant la même exécution :

1. ouvrir une enquête valide A ;
2. tenter d’ouvrir un dossier invalide ;
3. vérifier que A reste affichée ;
4. ouvrir une enquête valide B ;
5. vérifier que B remplace A.

Si une seconde sélection n’est pas encore accessible dans l’interface, cette validation sera complétée lors de l’ajout de l’action « Ouvrir ».

La logique du callback doit néanmoins déjà préserver l’ancienne session.

---

# Gestion de la mémoire

Les propriétaires doivent être clairement définis.

## `Application` possède

```text
GtkApplication
MainWindow
InvestigationSession
InvestigationTreeModel
```

## `InvestigationSession` possède

```text
InvestigationProject
Database
InvestigationRecord
```

## Variables temporaires du callback

```text
new_session
new_tree_model
GError
```

En cas d’échec :

```text
new_session fermée si elle existe
new_tree_model libéré si nécessaire
GError libéré
ancienne session conservée
ancien tree model conservé
```

En cas de succès :

```text
anciens objets libérés
nouveaux objets transférés dans Application
aucun double free
```

---

# Critères d’acceptation

- [ ] `Application` possède un `InvestigationSession *`.
- [ ] `Application` ne possède plus d’`Investigation *`.
- [ ] Le callback ouvre une session avec `investigation_session_open()`.
- [ ] Le chemin racine provient de `InvestigationProject`.
- [ ] Le nom provient d’`InvestigationRecord`.
- [ ] L’arbre est construit depuis le chemin de la session.
- [ ] L’ancienne session reste active si l’ouverture échoue.
- [ ] L’ancien arbre reste actif si l’ouverture échoue.
- [ ] La nouvelle session est fermée si la construction de l’arbre échoue.
- [ ] Les anciens objets ne sont remplacés qu’après validation complète.
- [ ] La session reste ouverte après le callback.
- [ ] `application_free()` ferme la session.
- [ ] `application_get_session(NULL)` retourne `NULL`.
- [ ] `main_window_set_investigation()` accepte `NULL`.
- [ ] Le titre affiche le nom de l’enquête.
- [ ] La barre d’état affiche le nom et le chemin racine.
- [ ] `application.c` n’appelle aucune fonction SQLite.
- [ ] Aucun chemin SQLite n’est reconstruit dans `application.c`.
- [ ] Les anciens tests restent valides.
- [ ] `make` réussit sans warning.
- [ ] `make test` réussit.
- [ ] Le test manuel d’ouverture valide réussit.
- [ ] Le test manuel d’annulation réussit.
- [ ] Le test manuel de dossier invalide réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

---

# Audit attendu

La commande suivante ne doit rien afficher :

```bash
rg -n \
    'investigation_new|investigation_free|investigation_get_root_path|investigation_get_database_path|Investigation \*' \
    src/core/application.c
```

La commande suivante ne doit rien afficher :

```bash
rg -n \
    'sqlite3_|#include <sqlite3.h>|00_BaseDeDonnees|Enquete.sqlite' \
    src/core/application.c
```

Vérifier la présence de la nouvelle session :

```bash
rg -n \
    'InvestigationSession|investigation_session_' \
    src/core/application.c \
    include/core/application.h
```

Vérifier la mise à jour de la fenêtre :

```bash
rg -n \
    'main_window_set_investigation' \
    include/views/main_window.h \
    src/views/main_window.c \
    src/core/application.c
```

---

# Hors périmètre

Ce ticket ne doit pas ajouter :

- une action de menu « Ouvrir » ;
- un raccourci clavier ;
- un dialogue graphique détaillé pour les erreurs ;
- la création d’une enquête ;
- la fermeture manuelle d’une enquête ;
- la réparation d’un chemin racine incohérent ;
- la modification de la base SQLite ;
- les DAO des preuves ou des entités ;
- la restauration automatique de la dernière enquête ;
- la persistance des préférences utilisateur ;
- le verrouillage multi-instance ;
- la suppression de l’ancien module `Investigation`.

---

# Fichiers principalement concernés

```text
include/core/application.h
src/core/application.c

include/views/main_window.h
src/views/main_window.c
```

Fichiers utilisés sans modification attendue :

```text
include/core/investigation_session.h
src/core/investigation_session.c

include/core/investigation_project.h
src/core/investigation_project.c

include/models/investigation_record.h
src/models/investigation_record.c

include/core/investigation_tree_builder.h
src/core/investigation_tree_builder.c
```

Le `Makefile` ne devrait pas nécessiter de nouvelle cible, car les sources de production sont détectées automatiquement avec :

```make
SRC := $(shell find src -name "*.c")
```

---

# Résultat attendu

À la fin du ticket, la sélection d’un dossier doit ouvrir une véritable session d’enquête.

L’application doit conserver ensemble :

```text
la connexion SQLite
les métadonnées persistées
le contexte de fichiers
l’arborescence
l’état visuel de la fenêtre
```

Le flux final doit être :

```text
sélection du dossier
        ↓
InvestigationSession ouverte
        ↓
InvestigationTreeModel construit
        ↓
session installée dans Application
        ↓
arbre affiché dans MainWindow
        ↓
nom et chemin affichés
```

---

# Commit attendu

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
    include/core/application.h \
    src/core/application.c \
    include/views/main_window.h \
    src/views/main_window.c
```

Contrôler :

```bash
git diff --cached --stat
git diff --cached
```

Créer le commit :

```bash
git commit -m "feat(app): integrate investigation session"
```

Puis pousser après validation complète :

```bash
git push
```

