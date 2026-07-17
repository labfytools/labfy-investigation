# Ticket #035 — File de tâches et panneau d’activité

## Contexte

Le ticket #034 a introduit `BackgroundTask`, une primitive asynchrone capable de :

- lancer un worker dans un thread GLib ;
- suivre son état ;
- signaler sa progression ;
- gérer l’annulation ;
- conserver un résultat ou une erreur ;
- revenir sur le contexte principal à la fin ;
- rester vivante grâce au comptage de références.

Cette primitive ne gère cependant pas encore plusieurs tâches ni leur présentation dans l’interface.

## Objectif

Créer un gestionnaire opaque :

```c
TaskManager
```

chargé de suivre les tâches actives et terminées, puis ajouter un panneau GTK permettant de les consulter.

Le ticket doit permettre de voir :

```text
Titre
État
Progression
Message courant
Bouton Annuler
```

Le gestionnaire reste indépendant de GTK.

---

# Architecture attendue

Créer :

```text
include/core/task_manager.h
src/core/task_manager.c
tests/test_task_manager.c

include/widgets/task_panel.h
src/widgets/task_panel.c
```

Relations :

```text
Application
    ├── possède TaskManager
    └── transmet TaskManager à MainWindow

MainWindow
    └── possède TaskPanel

TaskPanel
    └── observe TaskManager

TaskManager
    └── conserve des références vers BackgroundTask
```

---

# Phase A — `TaskManager`

## 1. Type opaque

Dans :

```text
include/core/task_manager.h
```

déclarer :

```c
typedef struct TaskManager TaskManager;
```

Le type doit rester indépendant de GTK, SQLite et des enquêtes.

---

## 2. Callback de changement

Définir :

```c
typedef void (*TaskManagerChangedCallback)(
    TaskManager *task_manager,
    gpointer user_data
);
```

Le callback signale qu’un changement visible a eu lieu :

- tâche ajoutée ;
- progression modifiée ;
- tâche terminée ;
- tâche retirée ;
- annulation demandée.

Le callback est une notification globale. Il ne transmet pas directement une tâche particulière.

---

## 3. API publique

### Construction

```c
TaskManager *task_manager_new(void);

void task_manager_free(
    TaskManager *task_manager
);
```

### Ajouter une tâche

```c
gboolean task_manager_add(
    TaskManager *task_manager,
    BackgroundTask *task,
    GError **error
);
```

Règles :

- `task_manager` devient propriétaire d’une référence supplémentaire ;
- l’appelant conserve sa propre référence ;
- une même tâche ne peut pas être ajoutée deux fois ;
- une tâche déjà terminée peut être ajoutée, mais elle est immédiatement visible comme terminée ;
- `NULL` est refusé.

### Consulter les tâches

```c
gsize task_manager_get_count(
    const TaskManager *task_manager
);

BackgroundTask *task_manager_get_task(
    const TaskManager *task_manager,
    gsize index
);
```

`task_manager_get_task()` retourne une nouvelle référence que l’appelant doit libérer.

### Retirer une tâche

```c
gboolean task_manager_remove(
    TaskManager *task_manager,
    BackgroundTask *task
);
```

La tâche n’est pas annulée automatiquement.

### Supprimer les tâches terminées

```c
gsize task_manager_remove_finished(
    TaskManager *task_manager
);
```

États concernés :

```text
COMPLETED
FAILED
CANCELLED
```

### Annuler toutes les tâches actives

```c
void task_manager_cancel_all(
    TaskManager *task_manager
);
```

### Callback de changement

```c
void task_manager_set_changed_callback(
    TaskManager *task_manager,
    TaskManagerChangedCallback callback,
    gpointer user_data,
    GDestroyNotify user_data_destroy
);
```

Le manager possède `user_data` après l’appel.

Remplacer le callback existant doit détruire les anciennes données exactement une fois.

---

## 4. Domaine d’erreur

Définir :

```c
typedef enum
{
    TASK_MANAGER_ERROR_INVALID_ARGUMENT,
    TASK_MANAGER_ERROR_ALREADY_ADDED
} TaskManagerError;
```

Puis :

```c
#define TASK_MANAGER_ERROR \
    task_manager_error_quark()

GQuark task_manager_error_quark(void);
```

---

## 5. Structure interne recommandée

```c
struct TaskManager
{
    GMutex mutex;
    GPtrArray *tasks;

    TaskManagerChangedCallback changed_callback;
    gpointer changed_user_data;
    GDestroyNotify changed_user_data_destroy;
};
```

`tasks` doit contenir des références `BackgroundTask *`.

Configurer le `GPtrArray` avec :

```c
background_task_unref
```

comme fonction de destruction.

---

## 6. Notification périodique

`BackgroundTask` ne possède pas encore de callback de progression.

Pour ce ticket, `TaskPanel` peut rafraîchir périodiquement l’affichage avec :

```c
g_timeout_add()
```

fréquence recommandée :

```text
200 à 300 ms
```

`TaskManager` notifie immédiatement les changements structurels.

La progression sera relue par le panneau.

Une API d’observation plus fine pourra être ajoutée plus tard si nécessaire.

---

# Phase B — Tests de `TaskManager`

Créer :

```text
tests/test_task_manager.c
```

## Tests minimaux

### Construction

Vérifier :

```text
manager non NULL
compteur initial à zéro
```

### Ajout

Ajouter une tâche et vérifier :

```text
compteur à un
callback déclenché
tâche récupérable
référence indépendante
```

### Doublon

Ajouter deux fois la même tâche :

```text
FALSE
TASK_MANAGER_ERROR_ALREADY_ADDED
compteur inchangé
```

### Retrait

Retirer une tâche :

```text
TRUE
compteur décrémenté
callback déclenché
```

### Retrait inconnu

Retirer une tâche absente :

```text
FALSE
aucun crash
```

### Nettoyage des tâches terminées

Ajouter :

- une tâche en attente ;
- une tâche terminée ;
- une tâche échouée ;
- une tâche annulée.

Vérifier que seules les tâches terminées sont supprimées.

### Annulation globale

Ajouter plusieurs tâches actives et appeler :

```c
task_manager_cancel_all()
```

Vérifier que chaque `GCancellable` reçoit une demande d’annulation.

### Durée de vie

Vérifier que :

- le manager conserve ses références ;
- la destruction du manager libère toutes les tâches ;
- le remplacement du callback détruit les anciennes données une seule fois.

---

# Phase C — `TaskPanel`

## 7. Type opaque

Dans :

```text
include/widgets/task_panel.h
```

déclarer :

```c
typedef struct TaskPanel TaskPanel;
```

API :

```c
TaskPanel *task_panel_new(
    TaskManager *task_manager
);

GtkWidget *task_panel_get_widget(
    const TaskPanel *task_panel
);

void task_panel_refresh(
    TaskPanel *task_panel
);

void task_panel_free(
    TaskPanel *task_panel
);
```

`TaskPanel` ne devient pas propriétaire de `TaskManager`.

`TaskPanel` doit rester valide tant que le manager existe.

---

## 8. Interface recommandée

Premier rendu simple :

```text
┌──────────────────────────────────────────────────────────┐
│ Activité                                  [ Nettoyer ]    │
├──────────────────────────────────────────────────────────┤
│ Extraction des métadonnées                              │
│ En cours — 45 %                                         │
│ [████████░░░░░░░░░░]                     [ Annuler ]     │
├──────────────────────────────────────────────────────────┤
│ Calcul SHA-256                                          │
│ Terminé                                                 │
│ [████████████████████]                                  │
└──────────────────────────────────────────────────────────┘
```

Widgets GTK possibles :

- `GtkBox` ;
- `GtkLabel` ;
- `GtkProgressBar` ;
- `GtkButton` ;
- `GtkScrolledWindow`.

Ne pas utiliser encore de `GtkListView` si cela complexifie inutilement le ticket.

---

## 9. État visuel

Créer une fonction privée traduisant les états :

```text
PENDING   → En attente
RUNNING   → En cours
COMPLETED → Terminée
FAILED    → Échouée
CANCELLED → Annulée
```

Une tâche en erreur doit afficher son message d’erreur sous forme courte.

Une tâche en cours doit afficher son message de progression lorsqu’il existe.

---

## 10. Bouton d’annulation

Le bouton `Annuler` doit être visible uniquement pour :

```text
RUNNING
```

Son callback appelle :

```c
background_task_cancel(task);
```

Le bouton ne doit pas retirer la tâche.

---

## 11. Bouton de nettoyage

Ajouter :

```text
Nettoyer
```

Il appelle :

```c
task_manager_remove_finished()
```

Les tâches actives restent visibles.

---

## 12. Rafraîchissement périodique

`TaskPanel` doit enregistrer une source GLib :

```c
g_timeout_add()
```

Elle appelle :

```c
task_panel_refresh()
```

Lors de `task_panel_free()` :

- retirer la source avec `g_source_remove()` ;
- empêcher tout callback après destruction ;
- ne pas détruire `TaskManager`.

---

# Phase D — Intégration GTK

## 13. Ajouter le panneau à `MainWindow`

Modifier :

```text
include/views/main_window.h
src/views/main_window.c
```

Changer la construction :

```c
MainWindow *main_window_new(
    GtkApplication *application,
    TaskManager *task_manager
);
```

`MainWindow` doit créer :

```c
TaskPanel *task_panel;
```

Le panneau peut être placé :

- sous la zone de travail ;
- dans un volet inférieur ;
- ou temporairement dans une colonne latérale secondaire.

Pour ce ticket, un volet inférieur sous `GtkPaned` est acceptable.

---

## 14. Ajouter `TaskManager` à `Application`

Dans la structure privée :

```c
TaskManager *task_manager;
```

Dans `application_new()` :

```c
application->task_manager =
    task_manager_new();
```

En cas d’échec, nettoyer l’application.

Dans `application_on_activate()` :

```c
application->main_window = main_window_new(
    gtk_application,
    application->task_manager
);
```

Dans `application_free()` :

1. fermer/détruire `MainWindow` ;
2. libérer `TaskManager` après le panneau ;
3. poursuivre le nettoyage existant.

L’ordre doit éviter que `TaskPanel` lise un manager déjà détruit.

---

# Phase E — Tâche de démonstration

## 15. Ajouter temporairement une tâche de test

Pour valider l’interface, ajouter un bouton temporaire :

```text
Tâche de test
```

Il lance une `BackgroundTask` qui :

- dure environ deux secondes ;
- progresse de 0 à 100 % ;
- accepte l’annulation ;
- retourne un résultat simple.

Cette tâche doit être ajoutée au `TaskManager`.

Le bouton pourra être supprimé lorsque le premier véritable traitement asynchrone sera disponible.

Le code de démonstration doit rester clairement identifié :

```c
/* Temporary demonstration task for ticket #035. */
```

---

# Hors périmètre

Ne pas ajouter encore :

- de limite de concurrence ;
- de priorité ;
- de persistance SQLite ;
- de reprise après redémarrage ;
- d’historique permanent ;
- d’exécution de commandes ;
- de recherche DNS ;
- d’ExifTool ;
- de notifications système ;
- de tri avancé ;
- de pagination.

---

# Critères d’acceptation

- [ ] `TaskManager` est opaque.
- [ ] `TaskManager` ne dépend pas de GTK.
- [ ] `TaskManager` protège sa collection avec `GMutex`.
- [ ] Le manager conserve une référence par tâche.
- [ ] Une tâche ne peut pas être ajoutée deux fois.
- [ ] Une tâche peut être retirée.
- [ ] Les tâches terminées peuvent être nettoyées.
- [ ] Toutes les tâches actives peuvent être annulées.
- [ ] Le callback de changement fonctionne.
- [ ] `TaskPanel` affiche toutes les tâches.
- [ ] La progression est visible.
- [ ] L’état est lisible.
- [ ] Une tâche active peut être annulée.
- [ ] Les tâches terminées peuvent être supprimées.
- [ ] Le rafraîchissement périodique est correctement détruit.
- [ ] `MainWindow` ne devient pas propriétaire du manager.
- [ ] L’ordre de destruction ne provoque aucun crash.
- [ ] La tâche de démonstration fonctionne.
- [ ] `make` réussit.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

---

# Audit attendu

Vérifier l’indépendance du manager :

```bash
rg -n \
    '#include <gtk|sqlite3_|Database|Investigation' \
    include/core/task_manager.h \
    src/core/task_manager.c
```

Résultat attendu :

```text
aucune sortie
```

Vérifier les références :

```bash
rg -n \
    'background_task_ref|background_task_unref' \
    src/core/task_manager.c
```

Vérifier le rafraîchissement GTK :

```bash
rg -n \
    'g_timeout_add|g_source_remove|task_panel_refresh' \
    src/widgets/task_panel.c
```

Vérifier l’absence de threads bruts :

```bash
rg -n \
    'pthread_|pthread.h' \
    include/core/task_manager.h \
    src/core/task_manager.c \
    src/widgets/task_panel.c
```

Résultat attendu :

```text
aucune sortie
```

---

# Fichiers concernés

```text
include/core/task_manager.h
src/core/task_manager.c
tests/test_task_manager.c

include/widgets/task_panel.h
src/widgets/task_panel.c

include/views/main_window.h
src/views/main_window.c

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
    include/core/task_manager.h \
    src/core/task_manager.c \
    tests/test_task_manager.c \
    include/widgets/task_panel.h \
    src/widgets/task_panel.c \
    include/views/main_window.h \
    src/views/main_window.c \
    src/core/application.c \
    Makefile
```

```bash
git diff --cached --stat
git diff --cached
```

```bash
git commit -m "feat(ui): add task manager and activity panel"
git push
```

---

# Résultat attendu

Après ce ticket, Labfy possédera une infrastructure visible pour tous les futurs traitements longs :

```text
Utilisateur
    ↓
Action GTK
    ↓
BackgroundTask
    ↓
TaskManager
    ↓
TaskPanel
```

Le ticket suivant pourra ajouter le registre des dépendances et lancer les premiers contrôles d’outils externes sans bloquer l’interface.
