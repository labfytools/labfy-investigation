# TICKET-038 — Exécution d’un outil externe dans une BackgroundTask

## Statut

À faire

## Priorité

Haute

## Objectif

Créer une couche d’intégration permettant d’exécuter un outil externe dans une `BackgroundTask`, puis de suivre cette exécution avec le `TaskManager` et le panneau d’activité.

Ce ticket doit relier proprement les modules déjà validés :

```text
ToolRegistry
ToolProcess
BackgroundTask
TaskManager
TaskPanel
```

Le résultat attendu est une abstraction réutilisable capable de :

- sélectionner un outil enregistré ;
- vérifier sa disponibilité ;
- préparer une exécution structurée ;
- lancer cette exécution dans un thread secondaire ;
- transmettre l’annulation ;
- publier un statut compréhensible ;
- conserver le résultat brut de l’outil ;
- signaler proprement les erreurs ;
- rester indépendante de GTK.

---

## Contexte

Les tickets précédents ont fourni :

### Ticket #034

```text
BackgroundTask
```

pour exécuter un travail asynchrone, publier une progression et gérer l’annulation.

### Ticket #035

```text
TaskManager
TaskPanel
```

pour conserver, afficher et annuler les tâches.

### Ticket #036

```text
ToolRegistry
ToolInfo
```

pour détecter les exécutables disponibles sur la machine.

### Ticket #037

```text
ToolProcess
ToolProcessResult
```

pour lancer un exécutable sans shell, capturer ses sorties et gérer son annulation.

Il manque maintenant une couche métier qui assemble ces briques sans obliger `Application`, les futurs adaptateurs OSINT ou les widgets GTK à connaître tous les détails de leur fonctionnement interne.

---

## Nom proposé du module

```text
ToolTask
```

Fichiers attendus :

```text
include/core/tool_task.h
src/core/tool_task.c
tests/test_tool_task.c
```

Le nom peut être ajusté avant implémentation s’il existe une meilleure proposition cohérente avec le projet.

---

## Responsabilité du module

`ToolTask` doit représenter une exécution externe préparée pour être lancée dans une `BackgroundTask`.

Le module devra :

1. recevoir un `ToolRegistry` ;
2. rechercher un outil par identifiant ;
3. vérifier que l’outil est disponible ;
4. copier les arguments reçus ;
5. copier le dossier de travail facultatif ;
6. créer une `BackgroundTask` ;
7. utiliser `ToolProcess` dans son worker ;
8. transmettre le `GCancellable` fourni par `BackgroundTask` ;
9. publier un statut avant le lancement ;
10. publier un statut après la fin ;
11. transférer un résultat structuré au propriétaire de la tâche ;
12. convertir les erreurs de préparation en erreurs `ToolTask` ;
13. laisser les erreurs d’exécution de l’outil disponibles dans le résultat ou dans la tâche selon leur nature.

---

## Hors périmètre

Ce ticket ne doit pas encore :

- ajouter un outil OSINT réel à l’interface ;
- créer un adaptateur DNS ;
- analyser la sortie d’un outil ;
- enregistrer une preuve ;
- écrire les sorties dans SQLite ;
- sauvegarder les résultats bruts sur disque ;
- afficher directement les sorties dans GTK ;
- gérer une limite de temps ;
- gérer stdin ;
- relancer automatiquement une commande ;
- gérer un pipeline entre plusieurs outils ;
- déterminer automatiquement les arguments d’un outil ;
- modifier le `ToolRegistry`.

Le premier adaptateur OSINT concret viendra après ce ticket.

---

## Contraintes générales

- C17.
- GLib et GIO.
- Aucune dépendance GTK.
- Compilation stricte :

```text
-Wall -Wextra -Wpedantic -Werror
```

- Aucun shell.
- Aucune concaténation de commande.
- Les arguments doivent rester séparés.
- Les structures publiques doivent être opaques.
- Les fonctions doivent être préfixées par :

```text
tool_task_
tool_task_result_
```

- Aucun état global mutable.
- Les règles de propriété doivent être documentées.
- Le module doit être testable sans dépendre d’un outil réellement installé.

---

## Modèle public

### ToolTaskResult

Créer une structure opaque :

```c
typedef struct ToolTaskResult ToolTaskResult;
```

Cette structure doit conserver au minimum :

```text
tool_identifier
executable_path
arguments
working_directory
process_result
```

Le champ `process_result` doit contenir le `ToolProcessResult` produit par `ToolProcess`.

Le résultat doit être indépendant du `ToolRegistry` après sa création.

### ToolTask

Deux approches sont possibles.

#### Approche recommandée

Ne pas créer une structure persistante `ToolTask`.

Créer directement une `BackgroundTask` configurée à partir d’une requête.

Exemple :

```c
BackgroundTask *tool_task_create(
    const ToolRegistry *tool_registry,
    const char *tool_identifier,
    const char *task_title,
    const char *const arguments[],
    const char *working_directory,
    GError **error
);
```

Le `BackgroundTask` retourné n’est pas encore démarré.

Une seconde fonction démarre la tâche :

```c
gboolean tool_task_start(
    BackgroundTask *background_task,
    BackgroundTaskCompletedCallback completed_callback,
    gpointer completed_user_data,
    GDestroyNotify completed_user_data_destroy,
    GError **error
);
```

#### Approche alternative

Créer une structure opaque `ToolTask` possédant sa `BackgroundTask`.

Cette approche n’est acceptable que si elle simplifie clairement la propriété et les tests.

---

## Domaine d’erreur

Créer :

```c
#define TOOL_TASK_ERROR \
    tool_task_error_quark()
```

Énumération minimale :

```c
typedef enum
{
    TOOL_TASK_ERROR_INVALID_ARGUMENT,
    TOOL_TASK_ERROR_TOOL_NOT_FOUND,
    TOOL_TASK_ERROR_TOOL_NOT_CHECKED,
    TOOL_TASK_ERROR_TOOL_MISSING,
    TOOL_TASK_ERROR_TASK_CREATION,
    TOOL_TASK_ERROR_TASK_START,
    TOOL_TASK_ERROR_PROCESS
} ToolTaskError;
```

Fonction :

```c
GQuark tool_task_error_quark(void);
```

---

## Règles de préparation

### Outil inconnu

Si l’identifiant n’existe pas dans le registre :

```text
retour : NULL
erreur : TOOL_TASK_ERROR_TOOL_NOT_FOUND
```

### Outil non vérifié

Si l’état vaut :

```text
TOOL_AVAILABILITY_UNKNOWN
```

la création doit échouer avec :

```text
TOOL_TASK_ERROR_TOOL_NOT_CHECKED
```

Le module ne doit pas appeler automatiquement `tool_registry_refresh()`.

Cette décision garde les responsabilités séparées.

### Outil absent

Si l’état vaut :

```text
TOOL_AVAILABILITY_MISSING
```

la création doit échouer avec :

```text
TOOL_TASK_ERROR_TOOL_MISSING
```

### Outil disponible sans chemin

Un outil marqué disponible mais sans chemin résolu représente un état incohérent.

La création doit échouer.

### Arguments

Le tableau d’arguments :

```c
const char *const arguments[]
```

peut être `NULL`.

Lorsqu’il est fourni :

- chaque chaîne doit être non `NULL` jusqu’au terminateur ;
- les chaînes doivent être dupliquées ;
- leur ordre doit être conservé ;
- aucune interprétation ne doit être faite ;
- le tableau interne doit se terminer par `NULL`.

### Titre

Le titre de la tâche doit être non vide.

Exemple :

```text
Interrogation DNS de example.org
```

Le titre ne doit pas être fabriqué automatiquement par le module.

---

## Cycle de vie recommandé

### Création

```c
BackgroundTask *tool_task_create(
    const ToolRegistry *tool_registry,
    const char *tool_identifier,
    const char *task_title,
    const char *const arguments[],
    const char *working_directory,
    GError **error
);
```

Comportement :

1. valider les arguments ;
2. rechercher `ToolInfo` ;
3. vérifier l’état ;
4. récupérer le chemin résolu ;
5. dupliquer :
   - identifiant ;
   - chemin ;
   - arguments ;
   - dossier de travail ;
6. créer les données du worker ;
7. créer une `BackgroundTask` ;
8. attacher les données au futur lancement.

### Démarrage

```c
gboolean tool_task_start(
    BackgroundTask *background_task,
    BackgroundTaskCompletedCallback completed_callback,
    gpointer completed_user_data,
    GDestroyNotify completed_user_data_destroy,
    GError **error
);
```

Cette fonction doit :

1. vérifier que la tâche provient bien de `ToolTask` ;
2. appeler `background_task_start()` ;
3. transmettre :
   - le worker ;
   - les données du worker ;
   - leur destructeur ;
   - le destructeur du résultat ;
   - le callback de fin ;
   - ses données utilisateur.

La fonction ne doit pas ajouter elle-même la tâche dans un `TaskManager`.

Cette responsabilité reste à l’appelant.

---

## Worker

Le worker doit respecter la signature actuelle de `BackgroundTask`.

Comportement attendu :

1. publier :

```text
Préparation de l’exécution
```

2. publier :

```text
Exécution de <nom de l’outil>
```

3. appeler `tool_process_run()` avec :
   - le chemin détecté ;
   - les arguments copiés ;
   - le dossier de travail ;
   - le `GCancellable` reçu ;
4. si l’appel échoue :
   - propager une erreur exploitable ;
   - ne produire aucun résultat ;
5. si l’appel réussit :
   - créer `ToolTaskResult` ;
   - y transférer `ToolProcessResult` ;
   - publier un statut final ;
6. placer le résultat dans :

```c
*result
```

### Progression

Une commande externe ne fournit pas toujours une progression mesurable.

Pour ce ticket, utiliser une progression qualitative :

```text
0.0  Préparation
0.1  Lancement
0.9  Exécution terminée, traitement du résultat
1.0  Terminé
```

Il ne faut pas simuler une progression régulière avec un minuteur.

---

## Code de sortie non nul

Un programme lancé correctement mais terminant avec un code non nul ne doit pas faire échouer la `BackgroundTask`.

Dans ce cas :

```text
BackgroundTask : COMPLETED
ToolTaskResult : disponible
ToolProcessResult : is_success == FALSE
```

Le résultat doit conserver :

- le code de sortie ;
- stdout ;
- stderr.

Cette distinction est essentielle pour les futurs adaptateurs.

Exemple :

```text
dig retourne 9
```

La tâche technique est terminée, mais le résultat fonctionnel signale un échec.

---

## Annulation

Le `GCancellable` reçu par le worker doit être transmis directement à :

```c
tool_process_run()
```

Si `ToolProcess` retourne :

```text
TOOL_PROCESS_ERROR_CANCELLED
```

le worker doit permettre à `BackgroundTask` d’aboutir à l’état :

```text
BACKGROUND_TASK_STATE_CANCELLED
```

Il ne doit pas transformer l’annulation en erreur métier générique.

Aucun processus enfant ne doit rester actif.

---

## Résultat

### Cycle de vie

```c
void tool_task_result_free(
    ToolTaskResult *result
);
```

La fonction doit accepter `NULL`.

### Identifiant

```c
const char *tool_task_result_get_tool_identifier(
    const ToolTaskResult *result
);
```

Chaîne empruntée.

### Chemin de l’exécutable

```c
const char *tool_task_result_get_executable_path(
    const ToolTaskResult *result
);
```

Chaîne empruntée.

### Arguments

```c
gsize tool_task_result_get_argument_count(
    const ToolTaskResult *result
);
```

```c
const char *tool_task_result_get_argument(
    const ToolTaskResult *result,
    gsize index
);
```

Chaînes empruntées.

### Dossier de travail

```c
const char *tool_task_result_get_working_directory(
    const ToolTaskResult *result
);
```

Retourne `NULL` si aucun dossier n’a été défini.

### Résultat du processus

```c
const ToolProcessResult *tool_task_result_get_process_result(
    const ToolTaskResult *result
);
```

Pointeur emprunté.

Une fonction de transfert ou de référence n’est pas nécessaire pour ce ticket.

---

## Propriété des données

### Données de préparation

Le module doit copier toutes les données nécessaires avant le démarrage.

Il ne doit pas dépendre de la durée de vie :

- du `ToolRegistry` ;
- du `ToolInfo` ;
- du tableau d’arguments d’origine ;
- du titre d’origine ;
- du dossier de travail d’origine.

### Résultat

`ToolTaskResult` devient propriétaire de :

- l’identifiant copié ;
- le chemin copié ;
- la copie des arguments ;
- la copie du dossier de travail ;
- `ToolProcessResult`.

Le destructeur doit tout libérer.

### BackgroundTask

La propriété de la `BackgroundTask` reste conforme au ticket #034.

---

## API publique suggérée

```c
typedef struct ToolTaskResult ToolTaskResult;

typedef enum
{
    TOOL_TASK_ERROR_INVALID_ARGUMENT,
    TOOL_TASK_ERROR_TOOL_NOT_FOUND,
    TOOL_TASK_ERROR_TOOL_NOT_CHECKED,
    TOOL_TASK_ERROR_TOOL_MISSING,
    TOOL_TASK_ERROR_TASK_CREATION,
    TOOL_TASK_ERROR_TASK_START,
    TOOL_TASK_ERROR_PROCESS
} ToolTaskError;

#define TOOL_TASK_ERROR \
    tool_task_error_quark()

GQuark tool_task_error_quark(void);

BackgroundTask *tool_task_create(
    const ToolRegistry *tool_registry,
    const char *tool_identifier,
    const char *task_title,
    const char *const arguments[],
    const char *working_directory,
    GError **error
);

gboolean tool_task_start(
    BackgroundTask *background_task,
    BackgroundTaskCompletedCallback completed_callback,
    gpointer completed_user_data,
    GDestroyNotify completed_user_data_destroy,
    GError **error
);

void tool_task_result_free(
    ToolTaskResult *result
);

const char *tool_task_result_get_tool_identifier(
    const ToolTaskResult *result
);

const char *tool_task_result_get_executable_path(
    const ToolTaskResult *result
);

gsize tool_task_result_get_argument_count(
    const ToolTaskResult *result
);

const char *tool_task_result_get_argument(
    const ToolTaskResult *result,
    gsize index
);

const char *tool_task_result_get_working_directory(
    const ToolTaskResult *result
);

const ToolProcessResult *tool_task_result_get_process_result(
    const ToolTaskResult *result
);
```

L’API peut évoluer si l’implémentation de `BackgroundTask` rend une autre forme plus sûre.

---

## Marquage d’une BackgroundTask

`tool_task_start()` doit pouvoir vérifier que la tâche reçue a été créée par `tool_task_create()`.

Approches possibles :

### Structure privée associée

Conserver les données du worker dans une structure privée connue uniquement de `ToolTask`.

### Extension de BackgroundTask

Ajouter un pointeur de contexte privé à `BackgroundTask` uniquement si cela reste générique et justifié.

### Wrapper opaque

Créer un `ToolTask` opaque contenant la `BackgroundTask`.

Cette solution peut être préférable si la vérification devient fragile.

Le choix final doit privilégier :

- sécurité de propriété ;
- lisibilité ;
- testabilité ;
- absence de cast dangereux.

---

## Tests unitaires obligatoires

Les tests doivent créer un faux outil temporaire.

Ils ne doivent pas dépendre d’un outil réel.

---

### 1. Arguments invalides

Tester au minimum :

- registre `NULL` ;
- identifiant `NULL` ;
- identifiant vide ;
- titre `NULL` ;
- titre vide ;
- dossier de travail vide ;
- `GError` déjà initialisé si la convention est vérifiée.

Résultat attendu :

```text
TOOL_TASK_ERROR_INVALID_ARGUMENT
```

---

### 2. Outil inconnu

Registre valide mais identifiant absent.

Résultat :

```text
TOOL_TASK_ERROR_TOOL_NOT_FOUND
```

---

### 3. Outil non vérifié

Enregistrer un outil sans appeler `tool_registry_refresh()`.

Résultat :

```text
TOOL_TASK_ERROR_TOOL_NOT_CHECKED
```

---

### 4. Outil absent

Enregistrer un faux outil absent, puis rafraîchir.

Résultat :

```text
TOOL_TASK_ERROR_TOOL_MISSING
```

---

### 5. Création valide

Créer un faux outil dans un `PATH` temporaire.

Vérifier :

- registre rafraîchi ;
- création de la `BackgroundTask` ;
- titre correct ;
- état initial `PENDING`.

---

### 6. Copie des arguments

Créer les arguments avec des chaînes dynamiques.

Créer la tâche, puis libérer les chaînes originales.

Démarrer la tâche.

Le résultat doit toujours contenir les bonnes valeurs.

---

### 7. Exécution réussie

Le faux outil écrit dans stdout et retourne zéro.

Vérifier :

- tâche terminée ;
- état `COMPLETED` ;
- résultat non `NULL` ;
- identifiant correct ;
- chemin correct ;
- arguments corrects ;
- `ToolProcessResult` disponible ;
- `is_success == TRUE`.

---

### 8. Code de sortie non nul

Le faux outil retourne :

```text
exit 7
```

Vérifier :

- tâche `COMPLETED` ;
- résultat disponible ;
- code `7` ;
- succès fonctionnel `FALSE`.

---

### 9. Erreur de lancement

Créer la tâche avec un exécutable détecté, puis supprimer le fichier avant le démarrage.

Vérifier :

- tâche `FAILED` ;
- erreur exploitable ;
- aucun résultat.

---

### 10. Annulation

Créer un faux outil long.

Démarrer la tâche puis l’annuler.

Vérifier :

- état final `CANCELLED` ;
- aucun résultat ;
- aucun processus enfant actif.

---

### 11. Dossier de travail

Le faux outil affiche son dossier courant.

Vérifier que le résultat correspond au dossier demandé.

---

### 12. Plusieurs tâches simultanées

Créer deux tâches à partir du même outil avec des arguments différents.

Les lancer simultanément.

Vérifier :

- deux résultats indépendants ;
- aucun mélange des arguments ;
- aucun partage incorrect des sorties ;
- aucun état global mutable.

---

### 13. Destruction avant démarrage

Créer une tâche puis la libérer sans la lancer.

Vérifier :

- aucune fuite ;
- destruction des copies d’arguments ;
- destruction des données privées.

---

### 14. Destruction après exécution

Exécuter une tâche, lire le résultat, puis libérer la tâche.

Vérifier la libération complète.

---

## Noms de tests suggérés

```text
/tool_task/invalid_arguments
/tool_task/tool_not_found
/tool_task/tool_not_checked
/tool_task/tool_missing
/tool_task/create
/tool_task/argument_ownership
/tool_task/success
/tool_task/nonzero_exit
/tool_task/spawn_failure
/tool_task/cancellation
/tool_task/working_directory
/tool_task/concurrent_tasks
/tool_task/free_before_start
/tool_task/free_after_completion
```

---

## Synchronisation des tests

Les tests doivent utiliser une `GMainLoop` pour attendre la fin des `BackgroundTask`.

Ils ne doivent pas utiliser une attente active infinie.

Prévoir un timeout de sécurité afin qu’un test défectueux ne bloque pas toute la suite.

Exemple :

```text
5 secondes maximum pour un test normal
```

Le test d’annulation doit se terminer rapidement.

---

## Makefile

Ajouter :

```make
TEST_TOOL_TASK := tests/test_tool_task
```

Règle attendue :

```make
$(TEST_TOOL_TASK): \
	tests/test_tool_task.c \
	src/core/tool_task.c \
	src/core/tool_registry.c \
	src/core/tool_process.c \
	src/core/background_task.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)
```

Ajouter la cible à :

```text
make test
make clean
```

---

## Vérifications manuelles

```bash
make clean
make
make tests/test_tool_task
./tests/test_tool_task
make test
```

---

## Vérification mémoire

```bash
G_DEBUG=gc-friendly \
G_SLICE=always-malloc \
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    ./tests/test_tool_task
```

Surveiller particulièrement :

- création sans démarrage ;
- échec de démarrage ;
- annulation ;
- résultat avec code non nul ;
- tâches simultanées.

---

## Critères d’acceptation

Le ticket est validé lorsque :

- le module compile en C17 strict ;
- aucune dépendance GTK n’est ajoutée ;
- un outil est recherché par identifiant dans `ToolRegistry` ;
- les états `UNKNOWN` et `MISSING` sont correctement refusés ;
- le chemin résolu est copié ;
- les arguments sont copiés ;
- le dossier de travail est copié ;
- la tâche utilise `ToolProcess` dans un thread secondaire ;
- le `GCancellable` est transmis ;
- l’annulation aboutit à `CANCELLED` ;
- un code de sortie non nul produit une tâche `COMPLETED` ;
- le résultat brut est accessible ;
- les résultats sont indépendants du registre après création ;
- plusieurs tâches peuvent fonctionner simultanément ;
- aucun shell n’est utilisé ;
- aucun processus enfant n’est abandonné ;
- aucune fuite mémoire n’est détectée ;
- tous les tests passent ;
- le test est intégré au `Makefile`.

---

## Démonstration finale prévue

Après validation du module, remplacer temporairement le bouton actuel :

```text
Tâche de test
```

par une vraie démonstration basée sur un faux outil ou un outil stable détecté.

Cette démonstration devra passer par :

```text
ToolRegistry
→ ToolTask
→ BackgroundTask
→ TaskManager
→ TaskPanel
```

Le bouton temporaire sera supprimé dès que le premier adaptateur OSINT réel sera disponible.

---

## Suite prévue

Après ce ticket :

1. ticket #039 — catalogue initial des outils externes ;
2. interrogation de leurs versions ;
3. ticket d’adaptateur DNS ;
4. conservation structurée des exécutions ;
5. stockage des sorties brutes ;
6. création d’observations et de preuves ;
7. affichage dans le workspace ;
8. packaging Ubuntu avec dépendances optionnelles.
