# Ticket #033 — Afficher les erreurs dans l’interface GTK

## Contexte

Le ticket #032 a centralisé le chargement d’une enquête dans :

```c
application_open_and_install_investigation()
```

Cette fonction produit maintenant des `GError` précis et conserve l’ancienne session en cas d’échec.

Actuellement, les erreurs sont seulement écrites dans le terminal avec :

```c
g_warning()
```

Un utilisateur qui lance Labfy Investigation depuis son menu d’applications ne verra pas ces messages.

## Objectif

Créer un module GTK réutilisable capable d’afficher un message d’erreur compréhensible dans une fenêtre modale.

Le contrôleur doit conserver deux niveaux d’information :

```text
Terminal :
    message technique complet avec g_warning()

Interface GTK :
    titre clair + message compréhensible
```

Le module doit rester générique afin de pouvoir afficher plus tard :

- erreurs ;
- avertissements ;
- informations.

---

# Architecture attendue

Créer :

```text
include/views/application_message_dialog.h
src/views/application_message_dialog.c
```

Responsabilités :

```text
Application
    ├── décide quand afficher une erreur
    ├── fournit le titre
    └── fournit le message

ApplicationMessageDialog
    ├── construit la fenêtre GTK
    ├── affiche le contenu
    └── gère sa fermeture
```

Le module de dialogue ne doit connaître ni :

- `Application` ;
- `InvestigationSession` ;
- SQLite ;
- les règles métier de l’enquête.

---

# Travail à réaliser

## 1. Créer l’en-tête public

Créer :

```text
include/views/application_message_dialog.h
```

Contenu attendu :

```c
/******************************************************************************
 * @file application_message_dialog.h
 * @brief Fenêtre modale affichant un message à l'utilisateur.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_APPLICATION_MESSAGE_DIALOG_H
#define LABFY_INVESTIGATION_APPLICATION_MESSAGE_DIALOG_H

#include <gtk/gtk.h>

/**
 * @brief Nature du message affiché.
 */
typedef enum
{
    APPLICATION_MESSAGE_DIALOG_ERROR,
    APPLICATION_MESSAGE_DIALOG_WARNING,
    APPLICATION_MESSAGE_DIALOG_INFORMATION
} ApplicationMessageDialogType;

/**
 * @brief Affiche une fenêtre modale contenant un message.
 *
 * Les chaînes sont copiées par les widgets GTK.
 *
 * @param parent_window Fenêtre parente, ou NULL.
 * @param message_type Type de message.
 * @param title Titre de la fenêtre.
 * @param message Message principal.
 */
void application_message_dialog_present(
    GtkWindow *parent_window,
    ApplicationMessageDialogType message_type,
    const char *title,
    const char *message
);

#endif
```

---

## 2. Créer l’implémentation GTK

Créer :

```text
src/views/application_message_dialog.c
```

Le dialogue doit être construit avec des widgets GTK4 simples afin de rester indépendant d’une API de dialogue plus récente.

Structure visuelle recommandée :

```text
┌─────────────────────────────────────────────┐
│ Titre                                       │
│                                             │
│ Message pouvant occuper plusieurs lignes    │
│                                             │
│                                  [ Fermer ]  │
└─────────────────────────────────────────────┘
```

Le module doit utiliser au minimum :

- `GtkWindow` ;
- `GtkBox` ;
- `GtkLabel` ;
- `GtkButton`.

Propriétés recommandées :

```c
gtk_window_set_modal(dialog_window, TRUE);
gtk_window_set_destroy_with_parent(dialog_window, TRUE);
gtk_window_set_resizable(dialog_window, FALSE);
```

Si `parent_window` n’est pas `NULL` :

```c
gtk_window_set_transient_for(
    dialog_window,
    parent_window
);
```

Le message doit :

- revenir automatiquement à la ligne ;
- être aligné à gauche ;
- être sélectionnable pour permettre sa copie ;
- avoir une largeur raisonnable.

Exemple :

```c
gtk_label_set_wrap(
    GTK_LABEL(message_label),
    TRUE
);

gtk_label_set_selectable(
    GTK_LABEL(message_label),
    TRUE
);

gtk_label_set_xalign(
    GTK_LABEL(message_label),
    0.0F
);

gtk_label_set_max_width_chars(
    GTK_LABEL(message_label),
    70
);
```

---

## 3. Gérer les arguments invalides

La fonction doit accepter :

```text
parent_window == NULL
title == NULL
title vide
message == NULL
message vide
```

Valeurs de remplacement recommandées :

```text
Titre :
    Erreur
    Avertissement
    Information

Message :
    Aucun détail supplémentaire n'est disponible.
```

Le titre par défaut dépend de `message_type`.

Une valeur inconnue de `message_type` doit être traitée comme une information ou un avertissement, sans crash.

---

## 4. Ajouter un callback privé de fermeture

Dans l’implémentation, ajouter :

```c
static void application_message_dialog_on_close_clicked(
    GtkButton *button,
    gpointer user_data
);
```

Ce callback doit :

1. ignorer proprement `button` ;
2. vérifier le pointeur de fenêtre ;
3. appeler `gtk_window_destroy()`.

Exemple :

```c
static void application_message_dialog_on_close_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    GtkWindow *dialog_window = user_data;

    (void) button;

    if (dialog_window == NULL)
    {
        return;
    }

    gtk_window_destroy(
        dialog_window
    );
}
```

Aucune structure allouée manuellement ne doit être nécessaire pour ce premier dialogue.

---

## 5. Différencier visuellement les types

Le dialogue doit au minimum afficher un libellé distinct :

```text
Erreur
Avertissement
Information
```

Une différenciation légère peut être ajoutée avec des classes CSS GTK existantes sur le titre ou le bouton.

La couleur ne doit jamais être le seul moyen de distinguer le type.

Aucune feuille CSS spécifique n’est nécessaire dans ce ticket.

---

## 6. Ajouter la source au Makefile

Ajouter :

```text
src/views/application_message_dialog.c
```

à la liste des sources de l’application.

Le module doit être compilé avec les mêmes options strictes :

```text
-std=c17
-Wall
-Wextra
-Wpedantic
-Werror
```

---

# Intégration dans `Application`

## 7. Ajouter l’en-tête

Dans :

```text
src/core/application.c
```

ajouter :

```c
#include "views/application_message_dialog.h"
```

---

## 8. Créer un helper privé dans `application.c`

Ajouter :

```c
static void application_present_error(
    Application *application,
    const char *title,
    const char *message
);
```

Implémentation attendue :

```c
static void application_present_error(
    Application *application,
    const char *title,
    const char *message
)
{
    GtkWindow *parent_window = NULL;

    if (application != NULL &&
        application->main_window != NULL)
    {
        parent_window = main_window_get_window(
            application->main_window
        );
    }

    application_message_dialog_present(
        parent_window,
        APPLICATION_MESSAGE_DIALOG_ERROR,
        title,
        message
    );
}
```

Ce helper centralise le choix du parent GTK.

---

## 9. Afficher les erreurs d’ouverture

Dans :

```c
application_on_folder_selected()
```

conserver le `g_warning()` existant.

Ajouter ensuite :

```c
application_present_error(
    application,
    "Ouverture impossible",
    error != NULL
        ? error->message
        : "L'enquête sélectionnée n'a pas pu être ouverte."
);
```

L’erreur doit être affichée avant :

```c
g_clear_error(&error);
```

L’annulation du sélecteur ne doit toujours afficher aucun message.

---

## 10. Afficher les erreurs de création

Dans :

```c
application_on_create_investigation()
```

traiter les deux échecs.

### Échec de création physique

Conserver le `g_warning()` puis afficher :

```text
Titre :
    Création impossible

Message :
    L'enquête n'a pas pu être créée dans le dossier sélectionné.
```

Le message peut contenir :

- le nom de l’enquête ;
- le dossier parent.

### Projet créé mais impossible à ouvrir

Conserver le `g_warning()` puis afficher un message indiquant clairement :

```text
L'enquête a été créée sur le disque, mais Labfy n'a pas pu l'ouvrir.
```

Ajouter ensuite le détail du `GError`.

Le message doit éviter de laisser croire que le dossier créé a été supprimé.

Une chaîne dynamique peut être construite avec :

```c
g_strdup_printf()
```

Elle doit être libérée après l’appel au dialogue.

---

# Exemple d’intégration

```c
char *user_message = NULL;

user_message = g_strdup_printf(
    "L'enquête a été créée dans :\n%s\n\n"
    "Elle n'a cependant pas pu être ouverte :\n%s",
    created_root_path,
    error != NULL
        ? error->message
        : "erreur inconnue"
);

application_present_error(
    application,
    "Enquête créée mais non ouverte",
    user_message
);

g_free(
    user_message
);
```

---

# Gestion de plusieurs erreurs

Ce ticket ne crée pas encore de file de notifications.

Si plusieurs erreurs surviennent successivement, plusieurs fenêtres peuvent être affichées.

La gestion centralisée des tâches et notifications arrivera avec les tickets #034 et #035.

---

# Tests manuels

## Dossier invalide

1. lancer l’application ;
2. ouvrir une enquête valide A ;
3. cliquer sur `Ouvrir une enquête` ;
4. sélectionner un dossier qui n’est pas une enquête.

Vérifier :

- une fenêtre d’erreur apparaît ;
- le message explique l’échec ;
- A reste active ;
- le terminal contient encore le `g_warning()` ;
- aucun crash.

## Annulation

1. ouvrir le sélecteur ;
2. annuler.

Vérifier :

- aucun dialogue ;
- aucun warning ;
- aucun changement de session.

## Création dans un emplacement invalide

Provoquer si possible un échec de création :

- dossier non accessible en écriture ;
- nom déjà utilisé ;
- emplacement invalide.

Vérifier :

- dialogue visible ;
- message compréhensible ;
- aucune session remplacée.

## Projet créé mais ouverture échouée

Provoquer si possible un échec après la création.

Vérifier que le dialogue précise :

- que le dossier existe ;
- où il se trouve ;
- que seule l’ouverture a échoué.

## Fermeture du dialogue

Tester :

- bouton `Fermer` ;
- bouton de fermeture de la fenêtre ;
- fermeture de la fenêtre principale.

Aucun segfault ne doit apparaître.

## Répétition

Produire plusieurs erreurs successives.

Vérifier qu’une nouvelle erreur reste affichable après fermeture de la précédente.

---

# Critères d’acceptation

- [ ] Le module `application_message_dialog` existe.
- [ ] Le module ne dépend pas du cœur métier.
- [ ] Le dialogue est modal.
- [ ] Le dialogue possède une fenêtre parente lorsqu’elle existe.
- [ ] Le message revient à la ligne.
- [ ] Le message est sélectionnable.
- [ ] Les arguments `NULL` sont acceptés.
- [ ] Un bouton `Fermer` fonctionne.
- [ ] Les erreurs d’ouverture sont visibles dans GTK.
- [ ] Les erreurs de création sont visibles dans GTK.
- [ ] Les `g_warning()` techniques sont conservés.
- [ ] L’annulation ne produit aucun dialogue.
- [ ] L’ancienne session est conservée en cas d’échec.
- [ ] Aucun appel SQLite direct n’est ajouté.
- [ ] Aucun `exit()` direct n’est ajouté.
- [ ] `make` réussit.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

---

# Audit attendu

Vérifier l’utilisation du module :

```bash
rg -n \
    'application_message_dialog|application_present_error' \
    include/views/application_message_dialog.h \
    src/views/application_message_dialog.c \
    src/core/application.c \
    Makefile
```

Vérifier que les erreurs restent journalisées :

```bash
rg -n \
    'g_warning' \
    src/core/application.c
```

Vérifier l’absence de logique métier dans le dialogue :

```bash
rg -n \
    'Investigation|Database|sqlite3_' \
    include/views/application_message_dialog.h \
    src/views/application_message_dialog.c
```

Résultat attendu :

```text
aucune sortie
```

Vérifier l’absence de sortie forcée :

```bash
rg -n \
    '\bexit\s*\(' \
    src/core/application.c \
    src/views/application_message_dialog.c
```

Résultat attendu :

```text
aucune sortie
```

---

# Fichiers concernés

```text
include/views/application_message_dialog.h
src/views/application_message_dialog.c
src/core/application.c
Makefile
```

---

# Commit attendu

```bash
make clean
make
make test
git diff --check
git status --short
```

```bash
git add \
    include/views/application_message_dialog.h \
    src/views/application_message_dialog.c \
    src/core/application.c \
    Makefile
```

```bash
git diff --cached --stat
git diff --cached
```

```bash
git commit -m "feat(ui): display application errors in GTK"
git push
```

---

# Résultat attendu

Après ce ticket, l’utilisateur ne dépendra plus du terminal pour comprendre pourquoi une enquête n’a pas pu être créée ou ouverte.

Le ticket #034 pourra ensuite introduire le gestionnaire de tâches asynchrones sans mélanger la présentation des erreurs avec l’exécution des traitements longs.
