# TICKET-039 — Catalogue initial des outils externes et détection de leurs versions

## Statut

À faire

## Priorité

Haute

## Objectif

Créer le catalogue initial des outils externes utilisés par Labfy Investigation et fournir une détection fiable de leur version.

Ce ticket complète les modules déjà validés :

```text
ToolRegistry
ToolProcess
ToolTask
```

Le catalogue décrit les outils connus par l’application. Le registre décrit leur état sur la machine courante.

```text
ToolCatalog
    description statique des outils connus
            ↓
ToolRegistry
    disponibilité, chemin résolu, version détectée
            ↓
ToolProcess
    exécution sécurisée de la commande de version
```

## Contexte

`ToolRegistry` permet déjà d’enregistrer un outil, de rechercher son exécutable dans le `PATH`, de conserver son chemin résolu et sa version détectée, et de distinguer les états `UNKNOWN`, `AVAILABLE` et `MISSING`.

Il manque une source centrale définissant :

- les outils reconnus par l’application ;
- leur identifiant interne ;
- leur nom affiché ;
- leur exécutable ;
- leur importance ;
- les arguments permettant de demander leur version ;
- la manière de normaliser la sortie obtenue.

Sans catalogue central, chaque fonctionnalité finirait par enregistrer elle-même ses dépendances.

## Modules attendus

```text
include/core/tool_catalog.h
src/core/tool_catalog.c
tests/test_tool_catalog.c
```

Le ticket ne doit pas modifier l’API publique de `ToolRegistry`, sauf nécessité démontrée pendant l’implémentation.

## Responsabilités de ToolCatalog

Le module doit :

1. conserver une liste statique et immuable des outils connus ;
2. garantir l’unicité de leurs identifiants ;
3. exposer leurs informations descriptives ;
4. enregistrer l’ensemble du catalogue dans un `ToolRegistry` ;
5. définir les arguments de détection de version ;
6. exécuter ces arguments avec `ToolProcess` ;
7. normaliser la sortie de version ;
8. retourner la version détectée sans modifier directement le registre ;
9. gérer l’annulation ;
10. rester indépendant de GTK.

Le module ne doit pas installer de dépendance, modifier le `PATH`, utiliser un shell, analyser un résultat OSINT ni modifier l’interface graphique.

## Catalogue initial

| Identifiant interne | Nom affiché | Exécutable | Importance | Arguments de version |
|---|---|---|---|---|
| `dns.dig` | `dig` | `dig` | optionnel | `-v` |
| `dns.host` | `host` | `host` | optionnel | `-V` |
| `network.whois` | `whois` | `whois` | optionnel | `--version` |
| `http.curl` | `curl` | `curl` | optionnel | `--version` |
| `tls.openssl` | `OpenSSL` | `openssl` | optionnel | `version` |

Tous ces outils sont optionnels au niveau de l’application entière.

Ne pas ajouter encore `nslookup`, `traceroute`, `jq`, `file`, `exiftool`, `nmap`, `subfinder`, `amass`, les outils Python ou les outils installés depuis GitHub.

## Structure ToolCatalogEntry

Créer une structure opaque :

```c
typedef struct ToolCatalogEntry ToolCatalogEntry;
```

Elle conserve au minimum :

```text
identifier
display_name
executable_name
requirement
version_arguments
```

Les données du catalogue sont statiques et immuables. Elles ne doivent jamais être libérées par l’appelant.

## Domaine d’erreur

```c
#define TOOL_CATALOG_ERROR \
    tool_catalog_error_quark()
```

```c
typedef enum
{
    TOOL_CATALOG_ERROR_INVALID_ARGUMENT,
    TOOL_CATALOG_ERROR_ENTRY_NOT_FOUND,
    TOOL_CATALOG_ERROR_REGISTRATION,
    TOOL_CATALOG_ERROR_TOOL_NOT_REGISTERED,
    TOOL_CATALOG_ERROR_TOOL_NOT_CHECKED,
    TOOL_CATALOG_ERROR_TOOL_MISSING,
    TOOL_CATALOG_ERROR_INVALID_TOOL_STATE,
    TOOL_CATALOG_ERROR_PROCESS,
    TOOL_CATALOG_ERROR_VERSION_COMMAND,
    TOOL_CATALOG_ERROR_VERSION_OUTPUT,
    TOOL_CATALOG_ERROR_CANCELLED
} ToolCatalogError;
```

```c
GQuark tool_catalog_error_quark(void);
```

## API publique proposée

### Consultation

```c
gsize tool_catalog_get_count(void);
```

```c
const ToolCatalogEntry *tool_catalog_get_entry(
    gsize index
);
```

```c
const ToolCatalogEntry *tool_catalog_find(
    const char *identifier
);
```

### Accesseurs

```c
const char *tool_catalog_entry_get_identifier(
    const ToolCatalogEntry *entry
);
```

```c
const char *tool_catalog_entry_get_display_name(
    const ToolCatalogEntry *entry
);
```

```c
const char *tool_catalog_entry_get_executable_name(
    const ToolCatalogEntry *entry
);
```

```c
ToolRequirement tool_catalog_entry_get_requirement(
    const ToolCatalogEntry *entry
);
```

```c
gsize tool_catalog_entry_get_version_argument_count(
    const ToolCatalogEntry *entry
);
```

```c
const char *tool_catalog_entry_get_version_argument(
    const ToolCatalogEntry *entry,
    gsize index
);
```

### Enregistrement

```c
gboolean tool_catalog_register_defaults(
    ToolRegistry *tool_registry,
    GError **error
);
```

### Détection d’une version

```c
gboolean tool_catalog_detect_version(
    const ToolRegistry *tool_registry,
    const char *identifier,
    GCancellable *cancellable,
    char **out_version,
    GError **error
);
```

`out_version` reçoit une chaîne nouvellement allouée, à libérer avec `g_free()`.

La fonction ne doit pas appeler `tool_registry_set_version()`.

## Règles d’enregistrement

`tool_catalog_register_defaults()` doit vérifier avant toute insertion qu’aucun identifiant du catalogue n’existe déjà dans le registre.

En cas de doublon :

- aucune entrée ne doit être ajoutée ;
- la fonction retourne `FALSE` ;
- l’erreur vaut `TOOL_CATALOG_ERROR_REGISTRATION`.

Cette prévalidation évite une insertion partielle, car `ToolRegistry` ne fournit pas de suppression.

## Validation de la détection

La fonction doit refuser :

- un registre `NULL` ;
- un identifiant `NULL` ou vide ;
- un `out_version` égal à `NULL` ;
- un `out_version` pointant déjà vers une chaîne ;
- un `GError` déjà initialisé.

Au début d’un appel valide :

```c
*out_version = NULL;
```

Cas d’erreur :

```text
entrée absente du catalogue     → TOOL_CATALOG_ERROR_ENTRY_NOT_FOUND
outil absent du registre        → TOOL_CATALOG_ERROR_TOOL_NOT_REGISTERED
état UNKNOWN                    → TOOL_CATALOG_ERROR_TOOL_NOT_CHECKED
état MISSING                    → TOOL_CATALOG_ERROR_TOOL_MISSING
AVAILABLE sans chemin résolu    → TOOL_CATALOG_ERROR_INVALID_TOOL_STATE
```

## Exécution de la commande de version

Utiliser exclusivement :

```c
tool_process_run()
```

avec :

```text
executable_path   = chemin résolu du ToolRegistry
arguments         = arguments statiques du ToolCatalogEntry
working_directory = NULL
cancellable       = paramètre reçu
```

Interdictions :

- `/bin/sh -c` ;
- `system()` ;
- `popen()` ;
- concaténation d’une ligne de commande ;
- redirections interprétées ;
- ajout ou transformation des arguments.

## Annulation

Si `ToolProcess` retourne `TOOL_PROCESS_ERROR_CANCELLED`, la fonction doit produire :

```text
G_IO_ERROR
G_IO_ERROR_CANCELLED
```

Aucune version partielle ne doit être retournée.

## Code de sortie

Pour une commande de version, un code de sortie non nul est un échec :

```text
TOOL_CATALOG_ERROR_VERSION_COMMAND
```

Une terminaison par signal produit la même catégorie d’erreur.

La sortie ne doit pas être interprétée lorsque le processus n’a pas terminé normalement avec le code zéro.

## Sélection de la sortie

Règle générique :

1. prendre la première ligne non vide de stdout ;
2. si stdout ne contient rien d’exploitable, essayer stderr ;
3. si aucun flux ne contient de texte exploitable, échouer.

Cette règle évite une logique spéciale par outil pendant ce premier ticket.

## Normalisation

Le catalogue ne doit pas extraire seulement un numéro sémantique.

La version conservée est la première ligne descriptive exploitable, par exemple :

```text
DiG 9.20.8
curl 8.14.1 (x86_64-pc-linux-gnu)
OpenSSL 3.5.1 1 Jul 2025
```

Algorithme :

1. obtenir les octets du flux ;
2. inspecter au maximum 4096 octets par flux ;
3. convertir les octets invalides avec `g_utf8_make_valid()` ;
4. séparer les lignes ;
5. ignorer les lignes vides ;
6. choisir la première ligne non vide ;
7. retirer espaces et tabulations en début et fin ;
8. retirer `\r` et `\n` ;
9. refuser un résultat vide ;
10. retourner une nouvelle chaîne.

## Propriété des données

Les entrées et chaînes du catalogue sont empruntées et statiques.

La chaîne placée dans `*out_version` appartient à l’appelant et doit être libérée avec `g_free()`.

`ToolCatalog` ne devient jamais propriétaire du registre.

## Contraintes de thread

Le catalogue statique est immuable.

`tool_catalog_detect_version()` :

- ne modifie pas le catalogue ;
- ne modifie pas le registre ;
- copie le chemin nécessaire avant l’exécution ;
- ne conserve aucun pointeur vers `ToolInfo` après la préparation.

Le stockage avec `tool_registry_set_version()` reste la responsabilité du propriétaire du registre, idéalement dans le thread principal.

## Tests unitaires obligatoires

Les tests utilisent uniquement de faux exécutables temporaires.

Ils ne doivent pas dépendre des outils réellement installés.

### Tests du catalogue

1. nombre exact d’entrées ;
2. contenu exact des cinq entrées ;
3. index invalide ;
4. recherche par identifiant ;
5. identifiant inconnu ;
6. unicité des identifiants ;
7. arguments de version ;
8. enregistrement complet dans un registre vide ;
9. état initial `UNKNOWN` ;
10. prévalidation d’un doublon sans insertion partielle.

### Tests de détection

11. arguments invalides ;
12. entrée inconnue ;
13. outil non enregistré ;
14. outil non vérifié ;
15. outil absent ;
16. version sur stdout ;
17. version sur stderr ;
18. première ligne non vide ;
19. normalisation des espaces et fins de ligne ;
20. sortie non UTF-8 ;
21. sortie vide ;
22. code de sortie non nul ;
23. terminaison par signal ;
24. annulation ;
25. indépendance du registre ;
26. plusieurs détections successives.

## Noms de tests suggérés

```text
/tool_catalog/entries
/tool_catalog/invalid_index
/tool_catalog/find
/tool_catalog/unique_entries
/tool_catalog/register_defaults
/tool_catalog/register_duplicate
/tool_catalog/detect_invalid_arguments
/tool_catalog/detect_unknown_entry
/tool_catalog/detect_unregistered_tool
/tool_catalog/detect_unchecked_tool
/tool_catalog/detect_missing_tool
/tool_catalog/version_stdout
/tool_catalog/version_stderr
/tool_catalog/version_first_nonempty_line
/tool_catalog/version_normalization
/tool_catalog/version_non_utf8
/tool_catalog/version_empty
/tool_catalog/version_nonzero_exit
/tool_catalog/version_signaled
/tool_catalog/version_cancelled
/tool_catalog/version_registry_independence
/tool_catalog/version_successive_runs
```

## Makefile

```make
TEST_TOOL_CATALOG := tests/test_tool_catalog
```

```make
$(TEST_TOOL_CATALOG): \
	tests/test_tool_catalog.c \
	src/core/tool_catalog.c \
	src/core/tool_registry.c \
	src/core/tool_process.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)
```

Ajouter le binaire à `make test` et `make clean`.

## Vérifications

```bash
make clean
make
make tests/test_tool_catalog
./tests/test_tool_catalog
make test
git diff --check
```

## Vérification mémoire

```bash
G_DEBUG=gc-friendly \
G_SLICE=always-malloc \
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    ./tests/test_tool_catalog
```

## Critères d’acceptation

Le ticket est validé lorsque :

- le catalogue contient exactement les cinq outils prévus ;
- toutes les entrées sont immuables ;
- tous les identifiants sont uniques ;
- le catalogue s’enregistre dans un registre vide ;
- un doublon empêche toute insertion partielle ;
- la détection utilise `ToolProcess` ;
- aucun shell n’est utilisé ;
- stdout et stderr sont pris en charge ;
- la première ligne non vide est normalisée ;
- une sortie invalide devient un UTF-8 valide ;
- l’inspection est limitée à 4096 octets par flux ;
- un code non nul ou un signal est refusé ;
- l’annulation produit `G_IO_ERROR_CANCELLED` ;
- la fonction ne modifie pas directement le registre ;
- les versions successives sont indépendantes ;
- les tests ne dépendent d’aucun outil installé ;
- tous les tests passent ;
- aucune fuite mémoire n’est détectée ;
- le test est intégré au `Makefile`.

## Démonstration finale

Après validation :

1. créer un `ToolRegistry` ;
2. appeler `tool_catalog_register_defaults()` ;
3. appeler `tool_registry_refresh()` ;
4. afficher les outils présents et absents ;
5. détecter la version d’un outil disponible ;
6. stocker explicitement la version avec `tool_registry_set_version()`.

Cette démonstration restera hors de l’interface GTK principale.

## Suite prévue

```text
#040 — Initialisation des dépendances et analyse asynchrone au démarrage
```

Ce ticket devra créer le registre global de l’application, enregistrer le catalogue, rafraîchir les disponibilités hors du thread GTK, détecter les versions et publier une tâche dans `TaskManager`.
