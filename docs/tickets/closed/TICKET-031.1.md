# Ticket #031.1 — Ajouter une fermeture propre de l’application

## Contexte

L’application permet désormais :

- de créer une enquête ;
- d’ouvrir une enquête existante ;
- de remplacer proprement la session active.

La fermeture se fait encore avec :

```text
Ctrl+C
```

Cette méthode interrompt brutalement le processus et ne constitue pas un parcours utilisateur normal.

## Objectif

Ajouter un bouton :

```text
Quitter
```

permettant de fermer proprement l’application GTK.

La fermeture doit déclencher le cycle normal de nettoyage :

```text
GtkApplication
    ↓
fin de la boucle principale
    ↓
application_free()
    ↓
InvestigationTreeModel libéré
    ↓
InvestigationSession fermée
    ↓
Database fermée
    ↓
MainWindow libérée
```

## Travail à réaliser

### 1. Ajouter le bouton dans `MainWindow`

Modifier :

```text
include/views/main_window.h
src/views/main_window.c
```

La barre d’actions doit devenir :

```text
[ Nouvelle enquête ] [ Ouvrir une enquête ] [ Quitter ]
```

Ajouter dans la structure privée :

```c
GtkWidget *quit_button;
```

### 2. Ajouter le type de callback

Dans `include/views/main_window.h`, ajouter :

```c
typedef void (*MainWindowQuitCallback)(
    gpointer user_data
);
```

Puis déclarer :

```c
void main_window_set_quit_callback(
    MainWindow *main_window,
    MainWindowQuitCallback callback,
    gpointer user_data
);
```

### 3. Conserver le callback dans `MainWindow`

Ajouter dans la structure privée :

```c
MainWindowQuitCallback
    quit_callback;

gpointer
    quit_user_data;
```

### 4. Ajouter le callback privé du bouton

Dans `src/views/main_window.c`, ajouter :

```c
static void main_window_on_quit_clicked(
    GtkButton *button,
    gpointer user_data
);
```

Cette fonction doit :

- accepter `button == NULL` ;
- accepter `main_window == NULL` ;
- ne rien faire si aucun callback n’est défini ;
- appeler le callback configuré sinon.

### 5. Créer le bouton

Dans `main_window_new()` :

```c
main_window->quit_button =
    gtk_button_new_with_label(
        "Quitter"
    );
```

Ajouter le bouton à `action_bar`.

Relier son signal `clicked` à :

```c
main_window_on_quit_clicked
```

### 6. Ajouter le setter public

Implémenter :

```c
void main_window_set_quit_callback(
    MainWindow *main_window,
    MainWindowQuitCallback callback,
    gpointer user_data
)
{
    if (main_window == NULL)
    {
        return;
    }

    main_window->quit_callback = callback;
    main_window->quit_user_data = user_data;
}
```

### 7. Ajouter le contrôleur dans `Application`

Dans `src/core/application.c`, ajouter :

```c
static void application_on_quit_requested(
    gpointer user_data
);
```

Cette fonction doit utiliser :

```c
g_application_quit(
    G_APPLICATION(
        application->gtk_application
    )
);
```

Aucun appel direct à `exit()` ne doit être ajouté.

### 8. Relier le callback dans `application_on_activate()`

Ajouter :

```c
main_window_set_quit_callback(
    application->main_window,
    application_on_quit_requested,
    application
);
```

## Tests manuels

### Fermeture sans enquête

1. lancer l’application ;
2. cliquer sur `Quitter` ;
3. vérifier que le processus se termine normalement.

### Fermeture avec une enquête ouverte

1. ouvrir ou créer une enquête ;
2. cliquer sur `Quitter` ;
3. vérifier que la fenêtre se ferme ;
4. vérifier qu’aucun crash n’apparaît ;
5. relancer l’application ;
6. rouvrir la même enquête.

### Fermeture après remplacement de session

1. ouvrir une enquête A ;
2. ouvrir une enquête B ;
3. cliquer sur `Quitter` ;
4. vérifier l’absence de crash ou de double libération.

## Critères d’acceptation

- [ ] Le bouton `Quitter` est visible.
- [ ] Le bouton ferme l’application.
- [ ] Aucun `exit()` direct n’est utilisé.
- [ ] `g_application_quit()` est utilisé.
- [ ] `MainWindow` ne connaît pas `GtkApplication`.
- [ ] Le nettoyage reste géré par `Application`.
- [ ] La fermeture fonctionne sans enquête ouverte.
- [ ] La fermeture fonctionne avec une enquête ouverte.
- [ ] La fermeture fonctionne après un changement d’enquête.
- [ ] `make` réussit.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

## Audit attendu

```bash
rg -n \
    'quit_button|quit_callback|g_application_quit' \
    include/views/main_window.h \
    src/views/main_window.c \
    src/core/application.c
```

La commande suivante ne doit rien afficher :

```bash
rg -n \
    '\bexit\s*\(' \
    src/core/application.c \
    src/views/main_window.c
```

## Fichiers concernés

```text
include/views/main_window.h
src/views/main_window.c
src/core/application.c
```

## Commit attendu

```bash
make clean
make
make test
git diff --check
git status --short
```

```bash
git add \
    include/views/main_window.h \
    src/views/main_window.c \
    src/core/application.c
```

```bash
git commit -m "feat(ui): add clean application quit action"
git push
```
