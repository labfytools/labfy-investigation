# Ticket #031 — Ouvrir une enquête existante depuis GTK

## Contexte

Les tickets #029 et #030 ont permis de :

- créer une enquête depuis GTK ;
- initialiser son arborescence et sa base SQLite ;
- ouvrir une `InvestigationSession` ;
- construire le `InvestigationTreeModel` ;
- installer la session dans `Application` ;
- afficher le nom et le chemin de l’enquête dans `MainWindow`.

L’application ne permet cependant pas encore d’ouvrir explicitement une enquête existante.

L’ancien sélecteur automatique au démarrage a été supprimé afin de ne pas forcer l’utilisateur à choisir un dossier à chaque lancement.

## Objectif

Ajouter un bouton :

```text
Ouvrir une enquête
```

Ce bouton doit permettre de sélectionner le dossier racine d’une enquête existante, puis de l’ouvrir avec le flux déjà présent :

```text
FolderDialog
    ↓
investigation_session_open()
    ↓
investigation_tree_builder_build()
    ↓
application_install_session()
    ↓
MainWindow mise à jour
```

La logique d’installation d’une session ne doit pas être dupliquée.

## Travail à réaliser

### 1. Ajouter le bouton dans `MainWindow`

Modifier :

```text
include/views/main_window.h
src/views/main_window.c
```

Ajouter un bouton visible dans la barre d’actions :

```text
Ouvrir une enquête
```

La barre doit contenir au minimum :

```text
[ Nouvelle enquête ] [ Ouvrir une enquête ]
```

Ajouter dans la structure privée :

```c
GtkWidget *open_investigation_button;
```

### 2. Ajouter le type de callback

Dans `include/views/main_window.h`, ajouter :

```c
typedef void (*MainWindowOpenInvestigationCallback)(
    gpointer user_data
);
```

Puis déclarer :

```c
void main_window_set_open_investigation_callback(
    MainWindow *main_window,
    MainWindowOpenInvestigationCallback callback,
    gpointer user_data
);
```

### 3. Conserver le callback dans `MainWindow`

Ajouter dans la structure privée :

```c
MainWindowOpenInvestigationCallback
    open_investigation_callback;

gpointer
    open_investigation_user_data;
```

Ajouter un callback privé :

```c
static void main_window_on_open_investigation_clicked(
    GtkButton *button,
    gpointer user_data
);
```

Il doit appeler le callback configuré uniquement s’il existe.

`MainWindow` ne doit pas ouvrir elle-même la session.

### 4. Relier le bouton à `Application`

Dans `src/core/application.c`, réactiver l’utilisation de :

```c
#include "views/folder_dialog.h"
```

Ajouter :

```c
static void application_on_open_investigation_requested(
    gpointer user_data
);
```

Cette fonction doit ouvrir le sélecteur avec :

```c
folder_dialog_select_folder(
    main_window_get_window(application->main_window),
    application_on_folder_selected,
    application
);
```

### 5. Réutiliser `application_on_folder_selected()`

La fonction doit :

1. accepter l’annulation ;
2. ouvrir la session avec `investigation_session_open()` ;
3. récupérer le chemin racine depuis `InvestigationProject` ;
4. construire l’arbre avec `investigation_tree_builder_build()` ;
5. installer les objets avec `application_install_session()`.

En cas d’échec :

```text
ancienne session conservée
ancien arbre conservé
nouvelle session libérée
nouvel arbre libéré
warning explicite
```

### 6. Enregistrer le callback dans `application_on_activate()`

Après la création de `MainWindow`, appeler :

```c
main_window_set_open_investigation_callback(
    application->main_window,
    application_on_open_investigation_requested,
    application
);
```

Le sélecteur ne doit pas être lancé automatiquement au démarrage.

## Tests manuels

### Ouverture valide

1. lancer l’application ;
2. cliquer sur `Ouvrir une enquête` ;
3. sélectionner une enquête créée avec le ticket #030 ;
4. vérifier l’affichage de l’arborescence ;
5. vérifier le titre ;
6. vérifier la barre d’état ;
7. vérifier l’absence d’erreur SQLite.

### Annulation

Ouvrir le sélecteur puis annuler.

Vérifier :

```text
aucun crash
aucun changement de session
aucun changement d’arbre
```

### Dossier invalide

Sélectionner un dossier sans base SQLite.

Vérifier :

```text
warning explicite
aucune base créée
ancienne session conservée
```

### Remplacement

1. ouvrir une enquête A ;
2. ouvrir une enquête B ;
3. vérifier que B remplace A.

### Échec pendant le remplacement

1. ouvrir une enquête valide A ;
2. tenter d’ouvrir un dossier invalide ;
3. vérifier que A reste active.

## Critères d’acceptation

- [ ] Le bouton `Ouvrir une enquête` est visible.
- [ ] Le bouton ouvre un sélecteur de dossier.
- [ ] Le sélecteur ne s’ouvre pas automatiquement au démarrage.
- [ ] Une enquête valide peut être ouverte.
- [ ] L’arborescence est affichée.
- [ ] Le titre est mis à jour.
- [ ] La barre d’état est mise à jour.
- [ ] L’annulation ne modifie aucun état.
- [ ] Un dossier invalide est refusé.
- [ ] Une base absente n’est pas créée.
- [ ] L’ancienne session reste active en cas d’échec.
- [ ] L’ancien arbre reste actif en cas d’échec.
- [ ] `application_install_session()` est réutilisée.
- [ ] Aucun appel SQLite direct n’est ajouté.
- [ ] `make` réussit.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

## Audit attendu

```bash
rg -n     'open_investigation|Ouvrir une enquête'     include/views/main_window.h     src/views/main_window.c     src/core/application.c
```

```bash
rg -n     'folder_dialog_select_folder'     src/core/application.c
```

```bash
rg -n     'sqlite3_|#include <sqlite3.h>'     src/core/application.c     src/views/main_window.c
```

Résultat attendu pour la dernière commande :

```text
aucune sortie
```

## Fichiers principalement concernés

```text
include/views/main_window.h
src/views/main_window.c
src/core/application.c
```

## Résultat attendu

À la fin du ticket, l’application doit proposer deux actions explicites :

```text
Nouvelle enquête
Ouvrir une enquête
```

L’utilisateur doit pouvoir créer une enquête, fermer l’application, puis la rouvrir plus tard depuis GTK.

## Commit attendu

```bash
make clean
make
make test
git diff --check
git status --short
```

```bash
git add     include/views/main_window.h     src/views/main_window.c     src/core/application.c
```

```bash
git commit -m "feat(ui): add investigation opening action"
git push
```

