# Ticket #030 — Créer une enquête depuis l’interface GTK

## Contexte

Le ticket #029 a intégré `InvestigationSession` au cycle de vie de l’application.

L’application sait désormais :

- ouvrir une enquête existante ;
- conserver sa connexion SQLite ;
- charger ses métadonnées persistées ;
- construire son arborescence ;
- afficher son nom et son chemin dans la fenêtre principale ;
- conserver l’ancienne enquête si une nouvelle ouverture échoue.

Cependant, lorsqu’aucune enquête n’existe encore, l’utilisateur ne peut pas en créer une depuis l’interface.

Le dossier sélectionné est actuellement toujours interprété comme une enquête existante. Si la base suivante est absente :

```text
00_BaseDeDonnees/Enquete.sqlite
```

l’ouverture échoue, ce qui est volontairement sûr.

La création d’une nouvelle enquête doit être une action explicite et séparée de l’ouverture.

---

# Objectif

Ajouter une première fonctionnalité GTK réellement utilisable :

```text
Créer une nouvelle enquête
```

Le flux attendu est :

```text
clic sur « Nouvelle enquête »
        ↓
sélection du dossier parent
        ↓
saisie du nom de l’enquête
        ↓
validation des paramètres
        ↓
investigation_project_create()
        ↓
investigation_session_open()
        ↓
construction de l’arborescence
        ↓
installation dans Application
        ↓
mise à jour de MainWindow
```

L’utilisateur ne doit jamais avoir à créer manuellement :

```text
00_BaseDeDonnees
Enquete.sqlite
01_Preuves_Originales
...
09_Hash
```

---

# Architecture attendue

```text
MainWindow
    │
    └── bouton « Nouvelle enquête »
             │
             ▼
CreateInvestigationDialog
             │
             ├── dossier parent
             ├── nom de l’enquête
             └── validation
                     │
                     ▼
Application
    │
    ├── investigation_project_create()
    ├── investigation_session_open()
    ├── investigation_tree_builder_build()
    └── main_window_set_investigation()
```

---

# Travail à réaliser

## 1. Créer un module de dialogue dédié

Créer :

```text
include/views/create_investigation_dialog.h
src/views/create_investigation_dialog.c
```

Le module doit rester indépendant de SQLite.

Il ne doit pas inclure :

```c
#include <sqlite3.h>
```

Il ne doit pas appeler :

```text
database_initialize
investigation_project_create
investigation_session_open
```

Son rôle est uniquement de recueillir les informations saisies par l’utilisateur.

---

## 2. Définir le callback public

Dans :

```text
include/views/create_investigation_dialog.h
```

déclarer :

```c
typedef void (*CreateInvestigationDialogCallback)(
    const char *parent_directory,
    const char *investigation_name,
    gpointer user_data
);
```

Puis :

```c
void create_investigation_dialog_present(
    GtkWindow *parent_window,
    CreateInvestigationDialogCallback callback,
    gpointer user_data
);
```

Le callback reçoit :

```text
parent_directory
investigation_name
user_data
```

En cas d’annulation :

```text
parent_directory == NULL
investigation_name == NULL
```

Les chaînes transmises au callback ne restent valides que pendant l’appel.

Le callback doit les copier s’il souhaite les conserver.

---

## 3. Concevoir le dialogue GTK

Le dialogue doit contenir au minimum :

```text
Titre : Nouvelle enquête

Dossier parent :
[ chemin sélectionné                     ] [ Parcourir ]

Nom de l’enquête :
[                                             ]

[ Annuler ] [ Créer ]
```

Le bouton `Créer` doit être désactivé tant que :

```text
aucun dossier parent valide n’est sélectionné
ou
le nom est vide
```

Le dialogue peut utiliser :

```text
GtkWindow
GtkBox
GtkLabel
GtkEntry
GtkButton
GtkFileDialog
```

Ne pas utiliser les anciennes API GTK3 synchrones.

---

## 4. Sélectionner le dossier parent

Le bouton :

```text
Parcourir
```

doit ouvrir un sélecteur de dossier GTK4.

Le dossier choisi représente le parent dans lequel le nouveau dossier d’enquête sera créé.

Exemple :

```text
Dossier parent :
/home/fy59/Documents/Enquetes

Nom :
Arnaque_Billets
```

Résultat attendu :

```text
/home/fy59/Documents/Enquetes/Arnaque_Billets
```

Le dossier parent doit déjà exister.

---

## 5. Valider le nom dans le dialogue

Le nom doit être refusé s’il est :

```text
NULL
vide
uniquement composé d’espaces
```

Il doit aussi être refusé s’il contient un séparateur de chemin :

```text
/
```

et, pour rester portable :

```text
\
```

Exemples invalides :

```text
Enquetes/Test
Enquetes\Test
```

Les espaces en début et fin doivent être supprimés avant l’envoi au callback.

Utiliser :

```c
g_strstrip()
```

sur une copie allouée.

Le dialogue ne doit pas modifier directement le contenu interne de `GtkEntry`.

---

## 6. Ajouter le bouton dans `MainWindow`

Modifier :

```text
include/views/main_window.h
src/views/main_window.c
```

Ajouter un bouton visible :

```text
Nouvelle enquête
```

Il peut être placé dans une barre horizontale au-dessus du `GtkPaned`.

Organisation attendue :

```text
MainWindow
└── main_box
    ├── action_bar
    │   └── bouton Nouvelle enquête
    ├── main_paned
    └── status_label
```

Ajouter dans la structure privée :

```c
GtkWidget *action_bar;
GtkWidget *new_investigation_button;
```

---

## 7. Ajouter un callback de fenêtre

Définir dans :

```text
include/views/main_window.h
```

un type de callback :

```c
typedef void (*MainWindowNewInvestigationCallback)(
    gpointer user_data
);
```

Ajouter :

```c
void main_window_set_new_investigation_callback(
    MainWindow *main_window,
    MainWindowNewInvestigationCallback callback,
    gpointer user_data
);
```

`MainWindow` ne doit pas créer elle-même l’enquête.

Elle ne doit faire que transmettre le clic au contrôleur `Application`.

---

## 8. Conserver les données de callback

Dans la structure privée de `MainWindow`, ajouter :

```c
MainWindowNewInvestigationCallback
    new_investigation_callback;

gpointer
    new_investigation_user_data;
```

Le bouton GTK doit être relié à un callback privé :

```c
static void main_window_on_new_investigation_clicked(
    GtkButton *button,
    gpointer user_data
);
```

Ce callback doit appeler :

```c
main_window->new_investigation_callback(
    main_window->new_investigation_user_data
);
```

uniquement si le callback est défini.

---

## 9. Ajouter le contrôleur dans `Application`

Modifier :

```text
src/core/application.c
```

Ajouter :

```c
static void application_on_new_investigation_requested(
    gpointer user_data
);
```

Cette fonction doit ouvrir :

```c
create_investigation_dialog_present()
```

en utilisant :

```c
main_window_get_window(
    application->main_window
);
```

---

## 10. Traiter le résultat du dialogue

Ajouter :

```c
static void application_on_create_investigation(
    const char *parent_directory,
    const char *investigation_name,
    gpointer user_data
);
```

En cas d’annulation :

```c
parent_directory == NULL
investigation_name == NULL
```

la fonction doit simplement retourner.

Aucun état existant ne doit être modifié.

---

## 11. Créer le projet

Appeler :

```c
char *created_root_path = NULL;
```

puis :

```c
created_root_path = investigation_project_create(
    parent_directory,
    investigation_name
);
```

Si la création échoue :

```text
ancienne session conservée
ancien arbre conservé
aucune modification de MainWindow
warning explicite
```

Exemple :

```c
g_warning(
    "Impossible de créer l'enquête '%s' dans '%s'.",
    investigation_name,
    parent_directory
);
```

---

## 12. Ouvrir immédiatement la nouvelle enquête

Après une création valide, ouvrir :

```c
InvestigationSession *new_session = NULL;
GError *error = NULL;
```

avec :

```c
new_session = investigation_session_open(
    created_root_path,
    &error
);
```

La nouvelle enquête doit être utilisable sans redémarrer l’application.

---

## 13. Construire son arbre

Récupérer :

```c
const InvestigationProject *project = NULL;
const char *root_path = NULL;
```

Puis construire :

```c
InvestigationTreeModel *new_tree_model = NULL;
```

avec :

```c
new_tree_model = investigation_tree_builder_build(
    root_path
);
```

---

## 14. Factoriser l’installation d’une session

Le ticket #029 contient déjà une logique de remplacement dans :

```c
application_on_folder_selected()
```

Cette logique ne doit pas être dupliquée.

Créer une fonction privée :

```c
static gboolean application_install_session(
    Application *application,
    InvestigationSession *new_session,
    InvestigationTreeModel *new_tree_model
);
```

Cette fonction doit :

1. valider ses paramètres ;
2. récupérer le projet ;
3. récupérer le record ;
4. récupérer le chemin racine ;
5. récupérer le nom ;
6. libérer l’ancien arbre ;
7. fermer l’ancienne session ;
8. installer les nouveaux objets ;
9. mettre à jour la sidebar ;
10. mettre à jour le titre et la barre d’état.

Elle retourne :

```text
TRUE en cas de succès
FALSE en cas d’échec
```

---

## 15. Propriété des objets dans la fonction factorisée

Avant l’appel réussi à :

```c
application_install_session()
```

le code appelant possède :

```text
new_session
new_tree_model
```

En cas de succès, `Application` devient propriétaire des deux objets.

En cas d’échec, le code appelant reste propriétaire et doit les libérer.

Cette règle doit être documentée clairement.

---

## 16. Adapter l’ouverture existante

Modifier :

```c
application_on_folder_selected()
```

pour utiliser également :

```c
application_install_session()
```

Le flux devient :

```text
investigation_session_open()
        ↓
investigation_tree_builder_build()
        ↓
application_install_session()
```

Cela garantit que l’ouverture et la création utilisent exactement le même mécanisme d’installation.

---

## 17. Gérer un échec après création du projet

Si le dossier et la base ont été créés avec succès mais que :

```text
investigation_session_open()
```

ou :

```text
investigation_tree_builder_build()
```

échouent, ne pas supprimer automatiquement le nouveau projet.

Raison :

```text
la création SQLite a pu réussir
le dossier contient potentiellement déjà des informations utiles
une suppression automatique après création complète serait risquée
```

Afficher un warning explicite indiquant que le projet a été créé mais n’a pas pu être ouvert.

Exemple :

```text
L’enquête a été créée dans '<chemin>', mais son ouverture a échoué.
```

Le chemin doit être laissé à l’utilisateur pour diagnostic.

---

## 18. Mettre à jour le statut pendant la création

Optionnel mais recommandé :

Avant la création :

```text
Création de l’enquête…
```

Après succès :

```text
Enquête ouverte : <nom> — <chemin>
```

En cas d’échec, le statut précédent doit être restauré ou conservé.

Ne pas laisser le statut bloqué sur :

```text
Création de l’enquête…
```

si l’opération échoue.

---

## 19. Ajouter une fonction de statut générique

Pour éviter que `Application` manipule directement `GtkLabel`, ajouter dans :

```text
include/views/main_window.h
```

```c
void main_window_set_status(
    MainWindow *main_window,
    const char *status_text
);
```

Implémenter dans :

```text
src/views/main_window.c
```

La fonction doit :

- accepter `main_window == NULL` ;
- accepter `status_text == NULL` ;
- afficher une chaîne sûre ;
- utiliser `gtk_label_set_text()`.

Pour `status_text == NULL`, afficher :

```text
Aucune enquête ouverte
```

---

# Tests manuels

## 20. Création valide

Lancer :

```bash
make
make run
```

Cliquer sur :

```text
Nouvelle enquête
```

Sélectionner :

```text
/home/fy59/Documents/Enquetes
```

Saisir par exemple :

```text
Test_Enquete
```

Vérifier la création de :

```text
/home/fy59/Documents/Enquetes/Test_Enquete/
```

Vérifier la présence de :

```text
00_BaseDeDonnees/Enquete.sqlite
01_Preuves_Originales
02_Preuves_Traitees
03_Chronologie
04_Entites
05_Rapports
06_Exports
07_Notes
08_Sources
09_Hash
```

Vérifier aussi :

```text
arborescence visible
titre mis à jour
barre d’état mise à jour
aucune erreur SQLite
```

---

## 21. Nom vide

Laisser le nom vide.

Le bouton `Créer` doit rester désactivé.

Aucun dossier ne doit être créé.

---

## 22. Nom composé d’espaces

Saisir uniquement :

```text

```

Le bouton `Créer` doit rester désactivé ou la validation doit refuser l’opération.

Aucun dossier ne doit être créé.

---

## 23. Nom contenant un séparateur

Tester :

```text
Test/Enquete
```

puis :

```text
Test\Enquete
```

La création doit être refusée.

---

## 24. Dossier déjà existant

Créer une première fois :

```text
Test_Enquete
```

Puis tenter de recréer le même nom dans le même dossier parent.

Vérifier :

```text
aucun écrasement
aucune modification du projet existant
warning explicite
ancienne session conservée
```

---

## 25. Annulation du dialogue

Ouvrir le dialogue puis cliquer sur :

```text
Annuler
```

Vérifier :

```text
aucun crash
aucun dossier créé
ancienne session conservée
fenêtre toujours utilisable
```

---

## 26. Création après ouverture d’une enquête

Lorsque l’action « Nouvelle enquête » est disponible pendant une session active :

1. ouvrir une enquête A ;
2. créer une enquête B ;
3. vérifier que B remplace A uniquement après création et ouverture complètes.

Si la création de B échoue, A doit rester active.

---

# Gestion de la mémoire

## Dialogue

Le dialogue possède :

```text
sa fenêtre GTK
ses widgets
ses chaînes temporaires
```

Il doit être détruit après :

```text
création
annulation
fermeture de la fenêtre
```

## Application

`Application` possède après succès :

```text
InvestigationSession
InvestigationTreeModel
```

## Variables temporaires

Le callback de création possède temporairement :

```text
created_root_path
new_session
new_tree_model
GError
```

Tous les chemins d’échec doivent libérer les ressources qu’ils possèdent encore.

---

# Critères d’acceptation

- [ ] Un bouton `Nouvelle enquête` est visible.
- [ ] Le bouton ouvre un dialogue dédié.
- [ ] Le dialogue permet de sélectionner un dossier parent.
- [ ] Le dialogue permet de saisir un nom.
- [ ] Le nom vide est refusé.
- [ ] Le nom composé d’espaces est refusé.
- [ ] Les séparateurs `/` et `\` sont refusés.
- [ ] Le bouton `Créer` n’est actif que lorsque les données sont valides.
- [ ] La création utilise `investigation_project_create()`.
- [ ] La nouvelle enquête est ouverte avec `investigation_session_open()`.
- [ ] L’arborescence est construite automatiquement.
- [ ] La session est installée dans `Application`.
- [ ] Le nom et le chemin sont affichés.
- [ ] L’ancienne session est conservée en cas d’échec.
- [ ] L’ancien arbre est conservé en cas d’échec.
- [ ] La logique d’installation n’est pas dupliquée.
- [ ] Le dialogue ne connaît ni SQLite ni `Database`.
- [ ] Aucun dossier existant n’est écrasé.
- [ ] L’annulation ne modifie aucun état.
- [ ] Les anciens tests restent valides.
- [ ] `make` réussit sans warning.
- [ ] `make test` réussit.
- [ ] Le test manuel de création valide réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

---

# Audit attendu

Le dialogue ne doit contenir aucune dépendance métier :

```bash
rg -n \
    'sqlite3_|database_|investigation_project_create|investigation_session_open' \
    include/views/create_investigation_dialog.h \
    src/views/create_investigation_dialog.c
```

Résultat attendu :

```text
aucune sortie
```

Vérifier la factorisation :

```bash
rg -n \
    'application_install_session' \
    src/core/application.c
```

Vérifier que les deux flux l’utilisent :

```text
application_on_folder_selected
application_on_create_investigation
```

Vérifier l’absence de SQLite dans `Application` :

```bash
rg -n \
    'sqlite3_|#include <sqlite3.h>' \
    src/core/application.c
```

Résultat attendu :

```text
aucune sortie
```

---

# Hors périmètre

Ce ticket ne doit pas ajouter :

- une barre de menu complète ;
- un raccourci clavier ;
- la suppression d’une enquête ;
- le renommage d’une enquête ;
- le déplacement d’une enquête ;
- la restauration de la dernière enquête ;
- une liste des enquêtes récentes ;
- une confirmation de fermeture ;
- l’import de preuves ;
- les DAO des preuves ;
- une boîte d’erreur avancée ;
- la gestion de modèles d’enquête personnalisés.

---

# Fichiers principalement concernés

```text
include/views/create_investigation_dialog.h
src/views/create_investigation_dialog.c

include/views/main_window.h
src/views/main_window.c

src/core/application.c
```

Le fichier suivant ne devrait pas nécessiter de modification :

```text
include/core/application.h
```

Le `Makefile` de production détecte automatiquement le nouveau fichier `.c` avec :

```make
SRC := $(shell find src -name "*.c")
```

Aucune nouvelle cible de test n’est obligatoire pour ce ticket GTK.

---

# Résultat attendu

À la fin du ticket, l’utilisateur doit pouvoir lancer l’application sans disposer d’une enquête préalable.

Il doit pouvoir :

```text
ouvrir l’application
        ↓
cliquer sur « Nouvelle enquête »
        ↓
choisir ~/Documents/Enquetes
        ↓
saisir un nom
        ↓
créer l’enquête
        ↓
voir immédiatement son arborescence
```

Cette fonctionnalité constitue la première opération complète utilisable depuis GTK.

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
    include/views/create_investigation_dialog.h \
    src/views/create_investigation_dialog.c \
    include/views/main_window.h \
    src/views/main_window.c \
    src/core/application.c
```

Contrôler :

```bash
git diff --cached --stat
git diff --cached
```

Créer le commit :

```bash
git commit -m "feat(ui): add investigation creation workflow"
```

Puis pousser après validation complète :

```bash
git push
```
