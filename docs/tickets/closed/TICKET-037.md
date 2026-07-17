# TICKET-037 — Exécution sécurisée des outils externes avec GSubprocess

## Statut

À faire

## Priorité

Haute

## Objectif

Créer une abstraction C centralisée permettant à Labfy Investigation d’exécuter un outil externe de manière contrôlée avec `GSubprocess`, sans passer par un shell.

Le module devra :

- recevoir le chemin d’un exécutable ;
- recevoir une liste d’arguments séparés ;
- lancer directement le programme ;
- capturer intégralement `stdout` et `stderr` ;
- conserver le code de sortie ;
- distinguer une fin normale d’une terminaison par signal ;
- permettre l’annulation avec `GCancellable` ;
- restituer un résultat structuré ;
- rester indépendant de GTK ;
- pouvoir être utilisé dans un worker `BackgroundTask`.

---

## Contexte

Le ticket #036 fournit désormais un registre capable de détecter les outils présents sur la machine et de mémoriser leur chemin.

Le prochain besoin est d’exécuter ces outils sans introduire de vulnérabilité liée à une commande shell.

Labfy Investigation devra progressivement lancer des outils tels que :

```text
dig
curl
openssl
whois
exiftool
```

Les arguments pourront contenir :

- des noms de domaines ;
- des chemins de fichiers ;
- des URL ;
- des options ;
- des espaces ;
- des caractères normalement interprétés par un shell.

Ces valeurs ne doivent jamais être concaténées dans une commande textuelle.

---

## Principe de sécurité

Le module doit lancer l’exécutable et ses arguments sous forme de tableau.

Exemple conceptuel correct :

```c
const char *arguments[] =
{
    "+short",
    "example.org",
    NULL
};
```

Exemple interdit :

```c
char *command = g_strdup_printf(
    "dig +short %s",
    domain_name
);

system(command);
```

Les API suivantes sont interdites dans le code de production :

```text
system()
popen()
execl("/bin/sh", ...)
sh -c
bash -c
g_spawn_command_line_sync()
g_spawn_command_line_async()
```

Le module doit utiliser `GSubprocess` ou `GSubprocessLauncher` avec un vecteur d’arguments.

---

## Périmètre

Ce ticket doit fournir :

1. un module autonome `tool_process` ;
2. une structure opaque `ToolProcessResult` ;
3. une fonction synchrone et annulable d’exécution ;
4. la capture brute de `stdout` ;
5. la capture brute de `stderr` ;
6. le code de sortie ;
7. l’état de fin normale ou par signal ;
8. la conservation du signal de terminaison lorsqu’il est disponible ;
9. la gestion d’un dossier de travail facultatif ;
10. une distinction claire entre :
    - une erreur de lancement ;
    - une erreur de communication ;
    - une annulation ;
    - un programme ayant simplement retourné un code différent de zéro ;
11. des tests unitaires avec de faux outils temporaires ;
12. l’intégration au `Makefile`.

---

## Hors périmètre

Ce ticket ne doit pas encore :

- créer une interface GTK ;
- lancer automatiquement les outils du registre ;
- analyser la sortie de `dig`, `curl`, `openssl` ou d’un autre outil ;
- déterminer la version d’un outil ;
- ajouter une limite de durée automatique ;
- gérer un pool de processus ;
- gérer une file d’attente ;
- enregistrer les sorties dans SQLite ;
- créer une preuve ou une observation OSINT ;
- modifier l’environnement complet du processus ;
- afficher la progression détaillée d’une commande ;
- ajouter un pseudo-terminal ;
- exécuter une commande à travers un shell.

L’intégration avec `BackgroundTask` sera effectuée après validation de cette abstraction.

---

## Fichiers attendus

```text
include/core/tool_process.h
src/core/tool_process.c
tests/test_tool_process.c
```

Le `Makefile` devra compiler le module dans l’application principale et fournir une cible :

```text
tests/test_tool_process
```

---

## Contraintes générales

- C17.
- GLib et GIO.
- Aucune dépendance GTK.
- Compilation stricte :

```text
-Wall -Wextra -Wpedantic -Werror
```

- Structure publique opaque.
- Fonctions préfixées par :

```text
tool_process_
tool_process_result_
```

- Aucun état global mutable.
- Aucun shell.
- Aucun chemin d’outil codé en dur dans le module.
- Les arguments doivent être transmis séparément.
- Les sorties doivent être conservées sous forme d’octets bruts.
- Les données reçues doivent être copiées ou référencées avec une propriété clairement documentée.

---

## Modèle public

### ToolProcessResult

```c
typedef struct ToolProcessResult ToolProcessResult;
```

La structure doit rester opaque.

Elle doit contenir au minimum :

```text
stdout_bytes
stderr_bytes
exited_normally
exit_status
was_signaled
termination_signal
```

### Propriété

Le résultat retourné appartient à l’appelant.

Il doit être libéré avec :

```c
void tool_process_result_free(
    ToolProcessResult *result
);
```

Les sorties doivent être conservées avec `GBytes`.

---

## Domaine d’erreur

Créer :

```c
#define TOOL_PROCESS_ERROR \
    tool_process_error_quark()
```

Énumération minimale :

```c
typedef enum
{
    TOOL_PROCESS_ERROR_INVALID_ARGUMENT,
    TOOL_PROCESS_ERROR_SPAWN,
    TOOL_PROCESS_ERROR_COMMUNICATION,
    TOOL_PROCESS_ERROR_CANCELLED,
    TOOL_PROCESS_ERROR_INVALID_RESULT
} ToolProcessError;
```

Fonction :

```c
GQuark tool_process_error_quark(void);
```

### Règle importante

Un programme qui se lance correctement puis retourne :

```text
exit 1
exit 2
exit 127
```

ne constitue pas une erreur de l’API.

Dans ce cas :

```text
tool_process_run() retourne TRUE
```

et le code de sortie est conservé dans `ToolProcessResult`.

La fonction ne retourne `FALSE` que lorsque le module n’a pas pu mener l’exécution jusqu’à l’obtention d’un résultat exploitable.

---

## API publique attendue

L’API exacte peut être ajustée si une meilleure conception est justifiée, mais elle doit couvrir les opérations suivantes.

### Exécution

```c
gboolean tool_process_run(
    const char *executable_path,
    const char *const arguments[],
    const char *working_directory,
    GCancellable *cancellable,
    ToolProcessResult **out_result,
    GError **error
);
```

### Paramètres

#### executable_path

Chemin de l’exécutable.

Le module doit accepter un chemin absolu détecté par `ToolRegistry`.

Le chemin doit être :

- non `NULL` ;
- non vide.

Le module ne doit pas reconstruire une commande textuelle.

#### arguments

Tableau terminé par `NULL`.

Ce tableau contient uniquement les arguments placés après `argv[0]`.

Exemple :

```c
const char *arguments[] =
{
    "+short",
    "example.org",
    NULL
};
```

`arguments` peut être `NULL`, ce qui signifie aucun argument supplémentaire.

Le module doit construire en interne un vecteur de ce type :

```text
argv[0] = executable_path
argv[1] = arguments[0]
argv[2] = arguments[1]
...
argv[n] = NULL
```

Chaque argument doit rester un élément distinct.

#### working_directory

Dossier de travail facultatif.

- `NULL` signifie utiliser le dossier courant hérité ;
- une chaîne non vide demande l’utilisation de ce dossier ;
- une chaîne vide est invalide.

Utiliser `GSubprocessLauncher` lorsqu’un dossier de travail est fourni.

#### cancellable

Objet d’annulation facultatif.

- `NULL` signifie que l’appel n’est pas annulable ;
- un objet déjà annulé doit provoquer un échec avec :
  - `TOOL_PROCESS_ERROR_CANCELLED`.

#### out_result

Doit être non `NULL`.

Avant l’appel :

```c
*out_result == NULL
```

En cas de succès :

```c
*out_result != NULL
```

En cas d’échec :

```c
*out_result == NULL
```

#### error

Respecter la convention GLib :

```c
error == NULL || *error == NULL
```

---

## Utilisation de GSubprocess

Créer le processus avec les drapeaux :

```c
G_SUBPROCESS_FLAGS_STDOUT_PIPE
G_SUBPROCESS_FLAGS_STDERR_PIPE
```

La capture recommandée est :

```c
g_subprocess_communicate()
```

Cette API permet de récupérer des `GBytes` et évite de supposer que les sorties sont valides en UTF-8.

Le module ne doit pas utiliser exclusivement :

```c
g_subprocess_communicate_utf8()
```

car une sortie brute peut contenir des octets non UTF-8.

---

## Données envoyées à stdin

Dans ce ticket, aucun contenu n’est envoyé à l’entrée standard.

La communication doit transmettre :

```c
stdin_buf = NULL
```

Une gestion explicite de stdin pourra être ajoutée plus tard.

---

## Construction du vecteur d’arguments

L’implémentation peut utiliser :

```c
GPtrArray
```

Exemple conceptuel :

```c
GPtrArray *argv_builder = NULL;

argv_builder = g_ptr_array_new();

g_ptr_array_add(
    argv_builder,
    (gpointer) executable_path
);

/* Ajout de chaque argument séparément. */

g_ptr_array_add(
    argv_builder,
    NULL
);
```

Le module doit décider clairement s’il duplique les chaînes ou s’il les emprunte uniquement pendant l’appel.

Comme `g_subprocess_launcher_spawnv()` consomme le tableau pendant l’appel de création, les chaînes peuvent être empruntées si leur durée de vie couvre cet appel.

Aucune fonction ne doit concaténer les arguments.

---

## Résultat d’exécution

### Cycle de vie

```c
void tool_process_result_free(
    ToolProcessResult *result
);
```

La fonction doit accepter `NULL`.

### Sortie standard

```c
GBytes *tool_process_result_ref_stdout(
    const ToolProcessResult *result
);
```

Retourne une nouvelle référence.

L’appelant doit la libérer avec :

```c
g_bytes_unref()
```

### Sortie d’erreur

```c
GBytes *tool_process_result_ref_stderr(
    const ToolProcessResult *result
);
```

Retourne une nouvelle référence.

### Fin normale

```c
gboolean tool_process_result_exited_normally(
    const ToolProcessResult *result
);
```

### Code de sortie

```c
int tool_process_result_get_exit_status(
    const ToolProcessResult *result
);
```

Règles recommandées :

- retourne le code réel si le programme s’est terminé normalement ;
- retourne `-1` si le résultat est `NULL` ou si le processus ne s’est pas terminé normalement.

### Terminaison par signal

```c
gboolean tool_process_result_was_signaled(
    const ToolProcessResult *result
);
```

```c
int tool_process_result_get_termination_signal(
    const ToolProcessResult *result
);
```

Règles recommandées :

- retourne le signal réel si disponible ;
- retourne `0` dans les autres cas.

### Succès fonctionnel

Ajouter une fonction pratique :

```c
gboolean tool_process_result_is_success(
    const ToolProcessResult *result
);
```

Elle retourne `TRUE` uniquement lorsque :

```text
exited_normally == TRUE
exit_status == 0
```

Cette fonction ne doit pas confondre le succès de l’API et le succès du programme lancé.

---

## Gestion des sorties vides

Même lorsqu’un programme n’écrit rien, le résultat doit rester exploitable.

Les choix acceptables sont :

```text
GBytes vide
```

ou :

```text
NULL documenté comme sortie vide
```

La solution recommandée est de conserver un `GBytes` vide afin de simplifier les appelants.

Les deux flux doivent être indépendants.

---

## Gestion de l’annulation

Lorsqu’un `GCancellable` est annulé pendant :

```c
g_subprocess_communicate()
```

le module doit :

1. interrompre l’attente ;
2. forcer l’arrêt du processus si nécessaire avec :
   ```c
   g_subprocess_force_exit()
   ```
3. attendre ou finaliser proprement l’objet `GSubprocess` selon le comportement GLib ;
4. ne retourner aucun résultat partiel ;
5. retourner `FALSE` ;
6. produire :
   ```text
   TOOL_PROCESS_ERROR_CANCELLED
   ```

Le module ne doit pas laisser de processus enfant actif après l’annulation.

---

## Nettoyage obligatoire

Tous les chemins d’erreur doivent libérer :

- le tableau d’arguments ;
- le `GSubprocessLauncher` ;
- le `GSubprocess` ;
- les `GBytes` temporaires ;
- le résultat partiellement construit ;
- toute erreur intermédiaire.

L’implémentation doit utiliser lorsque cela améliore la lisibilité :

```c
g_autoptr()
g_clear_object()
g_clear_pointer()
```

Le style doit rester cohérent avec le reste du projet.

---

## Comportement attendu

### Exécutable introuvable

```text
retour : FALSE
résultat : NULL
erreur : TOOL_PROCESS_ERROR_SPAWN
```

### Exécutable valide, code zéro

```text
retour : TRUE
résultat : non NULL
is_success : TRUE
exit_status : 0
```

### Exécutable valide, code non nul

```text
retour : TRUE
résultat : non NULL
is_success : FALSE
exit_status : code réel
```

### Annulation

```text
retour : FALSE
résultat : NULL
erreur : TOOL_PROCESS_ERROR_CANCELLED
```

### Sortie invalide UTF-8

```text
retour : TRUE
stdout brut conservé dans GBytes
```

---

## Tests unitaires obligatoires

Les tests doivent créer leurs propres faux exécutables temporaires.

Ils ne doivent pas dépendre de :

```text
dig
curl
openssl
whois
```

Ils peuvent utiliser des scripts temporaires avec un shebang, car le projet cible Linux.

Le code du module ne doit cependant jamais lancer un shell explicitement.

---

### 1. Arguments invalides

Tester au minimum :

- `executable_path == NULL` ;
- chemin vide ;
- `out_result == NULL` ;
- `*out_result != NULL` ;
- dossier de travail vide ;
- `GError` déjà initialisé si cette convention est vérifiée par le projet.

Résultat attendu :

```text
TOOL_PROCESS_ERROR_INVALID_ARGUMENT
```

---

### 2. Exécution sans argument

Créer un faux outil qui affiche un texte fixe.

Vérifier :

- retour `TRUE` ;
- fin normale ;
- code zéro ;
- sortie capturée ;
- `stderr` vide ;
- `is_success == TRUE`.

---

### 3. Transmission exacte des arguments

Le faux outil doit afficher chaque argument reçu sur une ligne séparée.

Tester des arguments comme :

```text
example.org
nom avec espaces
$(touch /tmp/labfy-injection)
; echo danger
*
```

Vérifier que les valeurs sont restituées littéralement.

Le test doit également vérifier qu’aucun fichier d’injection n’a été créé.

Ce test prouve qu’aucun shell n’interprète les arguments.

---

### 4. Capture séparée de stdout et stderr

Le faux outil écrit :

```text
message-out
```

dans `stdout`, et :

```text
message-err
```

dans `stderr`.

Vérifier que les deux sorties ne sont pas mélangées.

---

### 5. Code de sortie non nul

Le faux outil doit :

```text
exit 7
```

Vérifier :

- `tool_process_run() == TRUE` ;
- fin normale ;
- code de sortie `7` ;
- `is_success == FALSE`.

---

### 6. Exécutable inexistant

Utiliser un chemin temporaire inexistant.

Vérifier :

- retour `FALSE` ;
- résultat `NULL` ;
- erreur `TOOL_PROCESS_ERROR_SPAWN`.

---

### 7. Dossier de travail

Créer un dossier temporaire et un outil affichant son répertoire courant.

Exécuter avec `working_directory`.

Vérifier que la sortie correspond au chemin demandé.

---

### 8. Annulation avant le lancement

Créer un `GCancellable`, l’annuler avant l’appel, puis exécuter le faux outil.

Vérifier :

- retour `FALSE` ;
- résultat `NULL` ;
- erreur `TOOL_PROCESS_ERROR_CANCELLED`.

---

### 9. Annulation pendant l’exécution

Créer un faux outil long, par exemple :

```sh
#!/bin/sh
sleep 10
```

Lancer l’annulation depuis un second thread après un court délai.

Vérifier :

- retour `FALSE` ;
- erreur d’annulation ;
- aucun résultat ;
- durée du test nettement inférieure à dix secondes ;
- aucun processus enfant laissé actif.

Le test ne doit pas dépendre d’une boucle principale GTK.

---

### 10. Sortie vide

Créer un outil qui ne produit aucune sortie.

Vérifier que les accesseurs de sortie restent sûrs.

---

### 11. Octets non UTF-8

Créer un outil qui écrit au moins un octet non UTF-8 dans `stdout`.

Vérifier :

- retour `TRUE` ;
- récupération des octets avec `GBytes` ;
- taille exacte ;
- contenu exact.

Ce test confirme que le module n’utilise pas une API limitée au texte UTF-8.

---

### 12. Terminaison par signal

Sur Linux, créer un faux outil qui se termine lui-même par un signal.

Vérifier lorsque la plateforme le permet :

- `exited_normally == FALSE` ;
- `was_signaled == TRUE` ;
- signal non nul ;
- `exit_status == -1` ;
- `is_success == FALSE`.

Le test peut être conditionné avec les macros de plateforme appropriées.

---

## Noms de tests suggérés

```text
/tool_process/invalid_arguments
/tool_process/no_arguments
/tool_process/literal_arguments
/tool_process/stdout_stderr
/tool_process/nonzero_exit
/tool_process/missing_executable
/tool_process/working_directory
/tool_process/cancelled_before_start
/tool_process/cancelled_during_run
/tool_process/empty_output
/tool_process/non_utf8_output
/tool_process/signaled
```

---

## Fixtures de test

Une fixture peut contenir :

```c
typedef struct
{
    char *temporary_directory;
    char *tool_path;
} ToolProcessFixture;
```

Le `setup` doit :

- créer un dossier temporaire ;
- préparer les chemins.

Le `teardown` doit :

- supprimer tous les scripts ;
- supprimer les fichiers produits par les tests ;
- supprimer le dossier ;
- libérer toutes les chaînes.

Chaque test doit rester isolé.

---

## Makefile

Ajouter :

```make
TEST_TOOL_PROCESS := tests/test_tool_process
```

Règle attendue :

```make
$(TEST_TOOL_PROCESS): \
	tests/test_tool_process.c \
	src/core/tool_process.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)
```

Ajouter la cible dans :

```make
test
```

et dans :

```make
clean
```

`TEST_LDFLAGS` contient déjà `gio-2.0`, nécessaire pour `GSubprocess`.

---

## Vérifications manuelles

```bash
make clean
make
make tests/test_tool_process
./tests/test_tool_process
make test
```

Aucun avertissement de compilation ne doit être accepté.

---

## Vérification mémoire

Exécuter :

```bash
G_DEBUG=gc-friendly \
G_SLICE=always-malloc \
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    ./tests/test_tool_process
```

Vérifier particulièrement les chemins :

- lancement impossible ;
- code de sortie non nul ;
- annulation ;
- sortie vide ;
- erreur de communication.

---

## Vérification des processus enfants

Pendant le test d’annulation, contrôler si nécessaire qu’aucun faux outil ne reste actif après la fin du test.

Le module ne doit créer ni zombie ni processus abandonné.

---

## Critères d’acceptation

Le ticket est validé lorsque :

- le module compile en C17 strict ;
- il n’a aucune dépendance GTK ;
- il utilise `GSubprocess` ou `GSubprocessLauncher` ;
- il n’utilise aucun shell ;
- l’exécutable et les arguments sont transmis séparément ;
- `stdout` et `stderr` sont capturés séparément ;
- les sorties sont conservées dans des `GBytes` ;
- un code de sortie non nul produit tout de même un résultat ;
- l’échec de lancement produit une erreur ;
- le dossier de travail facultatif fonctionne ;
- l’annulation avant le lancement fonctionne ;
- l’annulation pendant l’exécution arrête le processus ;
- aucune sortie partielle n’est retournée après annulation ;
- les arguments contenant des caractères de shell restent littéraux ;
- les octets non UTF-8 sont conservés ;
- la terminaison par signal est représentée ;
- aucun processus enfant ne reste actif ;
- aucune fuite mémoire liée au module n’est détectée ;
- tous les tests passent ;
- le test est intégré à `make test` et `make clean`.

---

## Exemple d’utilisation attendu

```c
ToolProcessResult *result = NULL;
GBytes *stdout_bytes = NULL;
GError *error = NULL;

const char *arguments[] =
{
    "+short",
    "example.org",
    NULL
};

if (!tool_process_run(
        "/usr/bin/dig",
        arguments,
        NULL,
        NULL,
        &result,
        &error
    ))
{
    g_warning(
        "Impossible d’exécuter dig : %s",
        error != NULL
            ? error->message
            : "erreur inconnue"
    );

    g_clear_error(
        &error
    );

    return;
}

stdout_bytes = tool_process_result_ref_stdout(
    result
);

if (!tool_process_result_is_success(
        result
    ))
{
    g_warning(
        "dig a retourné le code %d",
        tool_process_result_get_exit_status(
            result
        )
    );
}

g_bytes_unref(
    stdout_bytes
);

tool_process_result_free(
    result
);
```

L’exemple illustre le comportement attendu. Il ne constitue pas une obligation d’organisation interne.

---

## Intégration future

Après validation de ce ticket :

1. création d’une tâche `BackgroundTask` exécutant `ToolProcess` ;
2. interrogation de la version des outils enregistrés ;
3. premier adaptateur OSINT, probablement DNS ;
4. conservation de la commande structurée :
   - exécutable ;
   - arguments ;
   - date ;
   - code de sortie ;
   - sortie brute ;
   - erreur brute ;
5. enregistrement de la provenance ;
6. affichage des résultats dans le panneau de travail ;
7. gestion ultérieure d’une durée maximale ;
8. ajout des dépendances facultatives aux installateurs Ubuntu.

