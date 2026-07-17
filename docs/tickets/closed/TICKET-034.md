# Ticket #034 — Modèle et exécuteur de tâches asynchrones

## Contexte

Labfy Investigation va bientôt exécuter des traitements potentiellement longs :

- calcul d’empreintes ;
- copie de fichiers ;
- extraction de métadonnées ;
- lancement d’outils externes ;
- recherches DNS et réseau ;
- analyse de résultats ;
- génération de rapports.

Ces opérations ne doivent jamais bloquer la boucle principale GTK.

Le ticket #035 ajoutera une file de tâches et un panneau d’activité. Avant cela, il faut créer une abstraction asynchrone fiable, indépendante de GTK et réutilisable par tous les futurs modules.

## Objectif

Créer un type opaque :

```c
BackgroundTask
```

capable de :

- exécuter une fonction de travail dans un thread GLib ;
- conserver son état ;
- suivre sa progression ;
- accepter une demande d’annulation ;
- conserver un résultat ;
- conserver une erreur ;
- enregistrer ses dates de début et de fin ;
- appeler un callback de fin sur le contexte principal ;
- garantir une gestion correcte de sa durée de vie.

Le module doit s’appuyer sur :

```text
GTask
GCancellable
GMutex
gatomicrefcount
```

Il ne doit dépendre ni de GTK, ni de SQLite, ni d’une enquête particulière.

---

# Architecture attendue

Créer :

```text
include/core/background_task.h
src/core/background_task.c
tests/test_background_task.c
```

Le flux général doit être :

```text
background_task_new()
        ↓
background_task_start()
        ↓
GTask exécute le worker dans un thread
        ↓
le worker signale sa progression
        ↓
succès / erreur / annulation
        ↓
callback de fin sur le contexte principal
        ↓
background_task_unref()
```

---

# 1. Définir les états publics

Dans :

```text
include/core/background_task.h
```

définir :

```c
typedef enum
{
    BACKGROUND_TASK_STATE_PENDING,
    BACKGROUND_TASK_STATE_RUNNING,
    BACKGROUND_TASK_STATE_COMPLETED,
    BACKGROUND_TASK_STATE_FAILED,
    BACKGROUND_TASK_STATE_CANCELLED
} BackgroundTaskState;
```

Transitions autorisées :

```text
PENDING → RUNNING
RUNNING → COMPLETED
RUNNING → FAILED
RUNNING → CANCELLED
```

Une tâche terminée ne peut jamais être redémarrée.

---

# 2. Définir le type opaque

```c
typedef struct BackgroundTask BackgroundTask;
```

La structure interne ne doit jamais apparaître dans le header.

---

# 3. Définir le worker

Ajouter :

```c
typedef gboolean (*BackgroundTaskWorker)(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
);
```

Contrat :

```text
succès :
    retourne TRUE
    error reste NULL
    result peut être NULL ou contenir un résultat

échec :
    retourne FALSE
    error doit normalement être renseignée
    result doit rester NULL

annulation :
    retourne FALSE
    error appartient au domaine G_IO_ERROR
    code G_IO_ERROR_CANCELLED
```

Le worker s’exécute dans un thread secondaire.

Il ne doit jamais :

- manipuler directement GTK ;
- accéder à un widget ;
- modifier une structure non protégée ;
- appeler le callback final lui-même.

Le worker peut appeler :

```c
background_task_report_progress()
```

depuis son thread.

---

# 4. Définir le callback final

Ajouter :

```c
typedef void (*BackgroundTaskCompletionCallback)(
    BackgroundTask *task,
    gpointer user_data
);
```

Le callback doit être appelé après la mise à jour de l’état final.

Lorsqu’une tâche est lancée depuis le thread principal GTK, le callback doit revenir sur ce contexte principal grâce au comportement de `GTask`.

---

# 5. Définir le domaine d’erreur

Ajouter :

```c
typedef enum
{
    BACKGROUND_TASK_ERROR_INVALID_ARGUMENT,
    BACKGROUND_TASK_ERROR_ALREADY_STARTED,
    BACKGROUND_TASK_ERROR_WORKER_PROTOCOL
} BackgroundTaskError;
```

Puis :

```c
#define BACKGROUND_TASK_ERROR \
    background_task_error_quark()

GQuark background_task_error_quark(void);
```

Utilisation :

- argument invalide ;
- seconde tentative de démarrage ;
- worker retournant `FALSE` sans fournir de `GError` ;
- worker retournant `TRUE` tout en fournissant une erreur.

---

# 6. API publique attendue

## Construction et références

```c
BackgroundTask *background_task_new(
    const char *title
);

BackgroundTask *background_task_ref(
    BackgroundTask *task
);

void background_task_unref(
    BackgroundTask *task
);
```

`background_task_new()` doit refuser :

```text
title == NULL
title vide
```

La tâche utilise un comptage de références atomique.

Ne pas exposer une fonction `background_task_free()`.

Cette décision est importante : la tâche doit pouvoir rester vivante pendant l’exécution même si son propriétaire visuel disparaît.

---

## Démarrage

```c
gboolean background_task_start(
    BackgroundTask *task,
    BackgroundTaskWorker worker,
    gpointer worker_data,
    GDestroyNotify worker_data_destroy,
    GDestroyNotify result_destroy,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
);
```

### Propriété des paramètres

Si le démarrage réussit :

```text
BackgroundTask prend en charge worker_data
BackgroundTask prend en charge completion_data
BackgroundTask prend en charge le futur result
```

Les fonctions de destruction correspondantes seront appelées au moment approprié.

Si le démarrage échoue :

```text
l’appelant conserve worker_data
l’appelant conserve completion_data
```

### Contraintes

La fonction doit :

1. vérifier `task` ;
2. vérifier `worker` ;
3. vérifier la convention `GError` ;
4. refuser une tâche qui n’est plus `PENDING` ;
5. créer un `GCancellable` ;
6. passer l’état à `RUNNING` ;
7. enregistrer la date de début ;
8. lancer le worker avec `g_task_run_in_thread()` ;
9. conserver une référence interne jusqu’au callback final.

---

## Annulation

```c
void background_task_cancel(
    BackgroundTask *task
);

gboolean background_task_is_cancelled(
    const BackgroundTask *task
);
```

`background_task_cancel()` exprime une demande.

Le worker reste responsable de vérifier régulièrement :

```c
g_cancellable_set_error_if_cancelled()
```

ou :

```c
g_cancellable_is_cancelled()
```

L’annulation ne doit jamais tuer brutalement un thread.

---

## Progression

```c
void background_task_report_progress(
    BackgroundTask *task,
    double progress,
    const char *status_message
);
```

Règles :

- utilisable depuis le worker ;
- protégée par `GMutex` ;
- valeur limitée entre `0.0` et `1.0` ;
- ignorée si la tâche n’est pas `RUNNING` ;
- `status_message` est copié ;
- `status_message == NULL` est accepté.

Le ticket #035 pourra lire régulièrement ces valeurs pour mettre à jour le panneau d’activité.

---

## Accesseurs

```c
const char *background_task_get_title(
    const BackgroundTask *task
);

BackgroundTaskState background_task_get_state(
    const BackgroundTask *task
);

double background_task_get_progress(
    const BackgroundTask *task
);

char *background_task_dup_status_message(
    const BackgroundTask *task
);

gint64 background_task_get_started_at_us(
    const BackgroundTask *task
);

gint64 background_task_get_finished_at_us(
    const BackgroundTask *task
);

GError *background_task_dup_error(
    const BackgroundTask *task
);

gpointer background_task_get_result(
    const BackgroundTask *task
);
```

### Propriété des valeurs

```text
get_title() :
    pointeur emprunté
    valide pendant la durée de vie de task
    titre immuable

dup_status_message() :
    nouvelle chaîne
    l’appelant doit appeler g_free()

dup_error() :
    nouvelle copie
    l’appelant doit appeler g_error_free()

get_result() :
    pointeur emprunté
    ne doit jamais être libéré par l’appelant
```

`get_result()` ne doit être considéré comme valide qu’après l’état :

```text
BACKGROUND_TASK_STATE_COMPLETED
```

---

# 7. Structure interne recommandée

Dans :

```text
src/core/background_task.c
```

la structure peut contenir :

```c
struct BackgroundTask
{
    gatomicrefcount reference_count;
    GMutex mutex;

    char *title;
    char *status_message;

    BackgroundTaskState state;
    double progress;

    gint64 started_at_us;
    gint64 finished_at_us;

    GCancellable *cancellable;

    gpointer result;
    GDestroyNotify result_destroy;

    GError *error;

    BackgroundTaskCompletionCallback
        completion_callback;

    gpointer completion_data;
    GDestroyNotify completion_data_destroy;
};
```

Les champs mutables doivent être protégés par `mutex`.

Le titre est immuable après construction.

---

# 8. Contexte interne d’exécution

Créer une structure privée, par exemple :

```c
typedef struct
{
    BackgroundTask *task;

    BackgroundTaskWorker worker;

    gpointer worker_data;
    GDestroyNotify worker_data_destroy;

    GDestroyNotify result_destroy;
} BackgroundTaskRunContext;
```

Le `worker_data` doit être détruit lorsque le contexte `GTask` est libéré.

Le pointeur `task` peut être non propriétaire si une référence interne distincte garantit sa durée de vie jusqu’au callback final.

---

# 9. Fonction exécutée dans le thread

Créer un trampoline privé compatible avec :

```c
GTaskThreadFunc
```

Il doit :

1. récupérer le contexte ;
2. appeler le worker ;
3. vérifier le contrat de retour ;
4. retourner le résultat avec `g_task_return_pointer()` ;
5. retourner l’erreur avec `g_task_return_error()` ;
6. créer une erreur `BACKGROUND_TASK_ERROR_WORKER_PROTOCOL` si le worker viole son contrat.

Cas à traiter :

```text
FALSE + error valide :
    échec normal

FALSE + error NULL :
    erreur de protocole

TRUE + error non NULL :
    erreur de protocole

TRUE + result quelconque :
    succès
```

Si un résultat a été produit alors que l’exécution échoue, il doit être détruit avec `result_destroy`.

---

# 10. Callback interne de fin

Créer un callback privé compatible avec :

```c
GAsyncReadyCallback
```

Il doit :

1. appeler `g_task_propagate_pointer()` ;
2. déterminer le nouvel état ;
3. conserver le résultat ou l’erreur ;
4. enregistrer la date de fin ;
5. forcer la progression à `1.0` en cas de succès ;
6. appeler le callback utilisateur ;
7. détruire les données du callback utilisateur ;
8. libérer la référence interne de la tâche.

Détermination de l’état :

```text
aucune erreur :
    COMPLETED

G_IO_ERROR_CANCELLED :
    CANCELLED

autre erreur :
    FAILED
```

Le callback utilisateur doit observer un objet déjà entièrement finalisé.

---

# 11. Destruction de la tâche

Quand la dernière référence est libérée :

1. vérifier qu’aucune référence interne d’exécution ne subsiste ;
2. détruire `result` avec `result_destroy` ;
3. libérer `error` ;
4. libérer les chaînes ;
5. libérer `GCancellable` ;
6. libérer les éventuelles données de callback restantes ;
7. nettoyer `GMutex` ;
8. libérer la structure.

L’appel suivant doit être accepté :

```c
background_task_unref(NULL);
```

---

# 12. Sécurité des threads

Les opérations suivantes doivent utiliser `GMutex` :

- lecture et écriture de l’état ;
- progression ;
- message de progression ;
- dates ;
- résultat ;
- erreur ;
- accès au cancellable si nécessaire.

Ne jamais conserver le mutex verrouillé pendant :

- l’appel du worker ;
- l’appel du callback utilisateur ;
- une fonction de destruction fournie par l’appelant ;
- un appel potentiellement bloquant.

---

# 13. Tests unitaires

Créer :

```text
tests/test_background_task.c
```

Les tests doivent utiliser un `GMainLoop` pour attendre le callback final.

## Test de construction

Vérifier :

```c
background_task_new(NULL) == NULL
background_task_new("") == NULL
```

Vérifier qu’une tâche valide commence avec :

```text
PENDING
progression 0.0
date de début 0
date de fin 0
aucune erreur
aucun résultat
```

## Test de succès

Créer un worker qui :

1. signale plusieurs progressions ;
2. renvoie une chaîne allouée ;
3. retourne `TRUE`.

Vérifier dans le callback :

```text
état COMPLETED
progression 1.0
résultat correct
erreur NULL
date de début > 0
date de fin >= date de début
callback appelé une seule fois
```

Vérifier que `result_destroy` est appelé lors du dernier `unref`.

## Test d’échec

Créer un worker qui retourne :

```c
FALSE
```

avec une erreur :

```text
G_IO_ERROR_FAILED
```

Vérifier :

```text
état FAILED
résultat NULL
erreur conservée
message conservé
```

## Test de protocole invalide

Créer un worker qui retourne :

```c
FALSE
```

sans renseigner `GError`.

Vérifier :

```text
état FAILED
domaine BACKGROUND_TASK_ERROR
code BACKGROUND_TASK_ERROR_WORKER_PROTOCOL
```

## Test d’annulation

Créer un worker qui travaille par petites étapes et vérifie régulièrement le `GCancellable`.

Programmer :

```c
background_task_cancel()
```

depuis le contexte principal avec `g_timeout_add()`.

Vérifier :

```text
état CANCELLED
erreur G_IO_ERROR_CANCELLED
callback final appelé
aucun crash
```

## Test de double démarrage

Démarrer une tâche puis rappeler immédiatement :

```c
background_task_start()
```

Vérifier :

```text
FALSE
BACKGROUND_TASK_ERROR_ALREADY_STARTED
```

La première exécution doit continuer normalement.

## Test des données utilisateur

Vérifier que :

- `worker_data_destroy` est appelé exactement une fois ;
- `completion_data_destroy` est appelé exactement une fois ;
- aucune donnée n’est détruite lorsque `background_task_start()` échoue avant transfert de propriété.

## Test du comptage de références

Démarrer une tâche puis libérer immédiatement la référence de l’appelant.

Vérifier que :

- la tâche reste vivante jusqu’au callback ;
- aucun accès mémoire invalide n’a lieu ;
- la destruction finale intervient après la fin de l’exécution.

---

# 14. Makefile

Le code de production est déjà découvert automatiquement si le Makefile utilise :

```make
SRC := $(shell find src -name "*.c")
```

Ajouter toutefois une cible de test dédiée :

```make
TEST_BACKGROUND_TASK := tests/test_background_task
```

La cible doit compiler au minimum :

```text
tests/test_background_task.c
src/core/background_task.c
```

Lier avec les paquets GLib/GIO déjà utilisés par le projet.

Ajouter le binaire aux cibles :

```text
test
clean
```

Sortie attendue :

```text
BackgroundTask : tous les tests sont valides.
```

---

# 15. Hors périmètre

Ce ticket ne doit pas encore ajouter :

- de file de tâches ;
- de limite de concurrence ;
- de panneau GTK ;
- de persistance SQLite ;
- de tâche associée à une enquête ;
- d’exécution de commande externe ;
- d’adaptateur ExifTool ;
- de recherche DNS ;
- de système de notifications ;
- de reprise après redémarrage ;
- de priorité entre tâches.

Ces fonctions viendront dans les tickets suivants.

---

# 16. Critères d’acceptation

- [ ] `BackgroundTask` est opaque.
- [ ] Le module ne dépend pas de GTK.
- [ ] Le module ne dépend pas de SQLite.
- [ ] Le module utilise `GTask`.
- [ ] Le module utilise `GCancellable`.
- [ ] Le module utilise un comptage de références.
- [ ] Le module protège son état avec `GMutex`.
- [ ] Une tâche ne peut être démarrée qu’une fois.
- [ ] Le worker s’exécute dans un thread secondaire.
- [ ] Le callback final revient sur le contexte principal.
- [ ] La progression est comprise entre `0.0` et `1.0`.
- [ ] L’annulation est coopérative.
- [ ] Le résultat est conservé jusqu’à la destruction.
- [ ] L’erreur est conservée jusqu’à la destruction.
- [ ] Les dates de début et de fin sont enregistrées.
- [ ] Les données utilisateur sont détruites exactement une fois.
- [ ] La tâche reste vivante pendant son exécution.
- [ ] Aucun callback utilisateur n’est appelé sous mutex.
- [ ] Tous les tests unitaires passent.
- [ ] Les anciens tests restent valides.
- [ ] `make` réussit.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

---

# 17. Audit attendu

Vérifier l’absence de GTK et SQLite :

```bash
rg -n \
    '#include <gtk|sqlite3_|Database|Investigation' \
    include/core/background_task.h \
    src/core/background_task.c
```

Résultat attendu :

```text
aucune sortie
```

Vérifier les primitives asynchrones :

```bash
rg -n \
    'GTask|GCancellable|GMutex|g_atomic_ref_count' \
    include/core/background_task.h \
    src/core/background_task.c
```

Vérifier qu’aucun thread POSIX brut n’est ajouté :

```bash
rg -n \
    'pthread_|pthread.h' \
    include/core/background_task.h \
    src/core/background_task.c
```

Résultat attendu :

```text
aucune sortie
```

Vérifier l’absence de sortie forcée :

```bash
rg -n \
    '\bexit\s*\(' \
    src/core/background_task.c \
    tests/test_background_task.c
```

Résultat attendu :

```text
aucune sortie
```

---

# 18. Fichiers concernés

```text
include/core/background_task.h
src/core/background_task.c
tests/test_background_task.c
Makefile
```

Aucune modification de `Application` ou de GTK n’est attendue.

---

# 19. Commit attendu

```bash
make clean
make
make test
git diff --check
git status --short
```

```bash
git add \
    include/core/background_task.h \
    src/core/background_task.c \
    tests/test_background_task.c \
    Makefile
```

```bash
git diff --cached --stat
git diff --cached
```

```bash
git commit -m "feat(core): add asynchronous background task"
git push
```

---

# Résultat attendu

Après ce ticket, Labfy disposera d’une primitive générique pour exécuter proprement les futurs traitements longs :

```text
SHA-256
copie de preuve
ExifTool
dig
RDAP
TLS
HTTP
recherche Web
génération de rapport
```

Le ticket #035 pourra ensuite construire une file de tâches et un panneau d’activité au-dessus de cette abstraction.
