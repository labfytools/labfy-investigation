# TICKET-036 — Registre des outils et dépendances externes

## Statut

À faire

## Priorité

Haute

## Objectif

Créer un module central capable de représenter les outils externes utilisés par Labfy Investigation et de déterminer s’ils sont disponibles sur la machine.

Ce registre deviendra la source unique permettant à l’application de savoir :

- quel outil est demandé par une fonctionnalité ;
- si cet outil est obligatoire ou optionnel ;
- sous quel nom il est recherché dans le `PATH` ;
- où se trouve son exécutable ;
- si sa version est connue ;
- pourquoi une fonctionnalité doit être désactivée lorsqu’une dépendance manque.

Le module doit rester indépendant de GTK.

---

## Contexte

Labfy Investigation utilisera progressivement plusieurs outils externes :

- outils DNS ;
- outils RDAP et WHOIS ;
- outils TLS ;
- outils HTTP ;
- outils de traitement de métadonnées ;
- outils de conversion ou d’analyse ;
- adaptateurs spécialisés OSINT.

Ces outils ne seront pas tous installés sur chaque machine.

La cible Ubuntu de la gendarmerie peut également disposer :

- de dépôts limités ;
- d’une liste de paquets spécifique ;
- d’outils installés dans des chemins non standards ;
- de versions différentes de celles présentes sur Arch Linux.

Le code métier ne doit donc jamais supposer qu’un exécutable est disponible.

---

## Périmètre

Ce ticket doit fournir :

1. un registre opaque `ToolRegistry` ;
2. une représentation opaque ou privée d’un outil enregistré ;
3. l’enregistrement d’un outil à partir de métadonnées stables ;
4. la détection de l’exécutable dans le `PATH` ;
5. la mémorisation du chemin détecté ;
6. la distinction entre dépendance obligatoire et optionnelle ;
7. la recherche d’un outil par identifiant ;
8. l’énumération des outils enregistrés ;
9. la détection des dépendances obligatoires manquantes ;
10. un champ permettant de mémoriser une version lorsqu’elle sera détectée ;
11. des tests unitaires sans dépendre des outils réellement installés sur la machine.

---

## Hors périmètre

Ce ticket ne doit pas encore :

- créer d’interface GTK ;
- lancer une commande OSINT réelle ;
- interpréter la sortie de `dig`, `whois`, `curl` ou `openssl` ;
- exécuter automatiquement `--version` ;
- installer des paquets ;
- modifier les dépôts du système ;
- télécharger un outil manquant ;
- utiliser une commande construite sous forme de chaîne shell ;
- contenir une liste définitive des outils OSINT de l’application.

L’exécution contrôlée des programmes externes sera traitée dans un ticket dédié.

---

## Fichiers attendus

```text
include/core/tool_registry.h
src/core/tool_registry.c
tests/test_tool_registry.c
```

Le `Makefile` devra être mis à jour pour compiler le module et son test.

---

## Contraintes générales

- Standard C17.
- Compilation avec :

```text
-Wall -Wextra -Wpedantic -Werror
```

- Utiliser GLib.
- Respecter les conventions du projet :
  - fichiers en `snake_case` ;
  - fonctions préfixées par `tool_registry_` ou `tool_info_` ;
  - noms de variables explicites ;
  - structure publique opaque ;
  - aucune variable globale mutable ;
  - aucune dépendance à GTK.
- Ne jamais construire une commande shell.
- Ne jamais appeler `system()`, `popen()` ou équivalent.
- Ne pas coder en dur un chemin comme `/usr/bin/dig`.

---

## Modèle de données

### ToolRequirement
nvim.lsp.b_248_save  BufWritePost
    <buffer=248>
              <Lua 3597: /usr/share/nvim/runtime/lua/vim/lsp.lua:943> [vim.lsp: textDocument/didSave handler]
        Modifié la dernière fois dans ~/.dotfiles/nvim/.config/nvim/init.lua (run Nvim with -V1 for more details
)
nvim.lsp.b_249_save  BufWritePost
    <buffer=249>
              <Lua 3645: /usr/share/nvim/runtime/lua/vim/lsp.lua:943> [vim.lsp: textDocument/didSave handler]
        Modifié la dernière fois dans ~/.dotfiles/nvim/.config/nvim/init.lua (run Nvim with -V1 for more details
)
nvim.lsp.b_252_save  BufWritePost
    <buffer=252>
              <Lua 3681: /usr/share/nvim/runtime/lua/vim/lsp.lua:943> [vim.lsp: textDocument/didSave handler]
        Modifié la dernière fois dans ~/.dotfiles/nvim/.config/nvim/init.lua (run Nvim with -V1 for more details
)
nvim.lsp.b_255_save  BufWritePost
    <buffer=255>
              <Lua 2223: /usr/share/nvim/runtime/lua/vim/lsp.lua:943> [vim.lsp: textDocument/didSave handler]
        Modifié la dernière fois dans ~/.dotfiles/nvim/.config/nvim/init.lua (run Nvim with -V1 for more details
)
nvim.lsp.b_259_save  BufWritePost
    <buffer=259>
              <Lua 3935: /usr/share/nvim/runtime/lua/vim/lsp.lua:943> [vim.lsp: textDocument/didSave handler]
        Modifié la dernière fois dans ~/.dotfiles/nvim/.config/nvim/init.lua (run Nvim with -V1 for more details
)
nvim.lsp.b_262_save  BufWritePost
    <buffer=262>
              <Lua 3099: /usr/share/nvim/runtime/lua/vim/lsp.lua:943> [vim.lsp: textDocument/didSave handler]
        Modifié la dernière fois dans ~/.dotfiles/nvim/.config/nvim/init.lua (run Nvim with -V1 for more details
)
nvim.lsp.b_273_save  BufWritePost
    <buffer=273>
              <Lua 4130: /usr/share/nv
Créer une énumération représentant l’importance d’une dépendance :

```c
typedef enum
{
    TOOL_REQUIREMENT_OPTIONAL,
    TOOL_REQUIREMENT_REQUIRED
} ToolRequirement;
```

### ToolAvailability

Créer une énumération représentant l’état de détection :

```c
typedef enum
{
    TOOL_AVAILABILITY_UNKNOWN,
    TOOL_AVAILABILITY_AVAILABLE,
    TOOL_AVAILABILITY_MISSING
} ToolAvailability;
```

### ToolRegistry

La structure doit rester opaque :

```c
typedef struct ToolRegistry ToolRegistry;
```

### ToolInfo

La représentation d’un outil doit également être opaque ou exposée uniquement en lecture :

```c
typedef struct ToolInfo ToolInfo;
```

Un outil enregistré doit au minimum contenir :

```text
identifier
display_name
executable_name
requirement
availability
resolved_path
detected_version
```

### Règles des champs

#### identifier

Identifiant interne stable et unique.

Exemples :

```text
dns.dig
network.curl
tls.openssl
metadata.exiftool
```

Il ne doit pas dépendre du nom du paquet de la distribution.

#### display_name

Nom compréhensible par l’utilisateur.

Exemples :

```text
dig
cURL
OpenSSL
ExifTool
```

#### executable_name

Nom recherché dans le `PATH`.

Exemples :

```text
dig
curl
openssl
exiftool
```

#### resolved_path

Chemin absolu détecté.

Exemple :

```text
/usr/bin/dig
```

Ce champ vaut `NULL` lorsque l’outil est absent ou n’a pas encore été vérifié.

#### detected_version

Version connue de l’outil.

Ce champ peut rester `NULL` dans ce ticket. Le registre doit néanmoins pouvoir la mémoriser pour les tickets suivants.

---

## Domaine d’erreur

Créer un domaine d’erreur dédié :

```c
#define TOOL_REGISTRY_ERROR \
    tool_registry_error_quark()
```

Erreurs minimales :

```c
typedef enum
{
    TOOL_REGISTRY_ERROR_INVALID_ARGUMENT,
    TOOL_REGISTRY_ERROR_DUPLICATE_IDENTIFIER,
    TOOL_REGISTRY_ERROR_NOT_FOUND
} ToolRegistryError;
```

Fonction attendue :

```c
GQuark tool_registry_error_quark(void);
```

---

## API publique attendue

L’API exacte peut être ajustée si une meilleure solution est justifiée, mais elle doit couvrir les opérations suivantes.

### Cycle de vie

```c
ToolRegistry *tool_registry_new(void);

void tool_registry_free(
    ToolRegistry *tool_registry
);
```

### Enregistrement

```c
gboolean tool_registry_register(
    ToolRegistry *tool_registry,
    const char *identifier,
    const char *display_name,
    const char *executable_name,
    ToolRequirement requirement,
    GError **error
);
```

Règles :

- tous les textes doivent être non `NULL` et non vides ;
- l’identifiant doit être unique ;
- le registre doit dupliquer les chaînes reçues ;
- l’outil nouvellement enregistré commence avec :
  - disponibilité `UNKNOWN` ;
  - chemin `NULL` ;
  - version `NULL`.

### Détection

```c
gboolean tool_registry_refresh(
    ToolRegistry *tool_registry,
    GError **error
);
```

Cette fonction doit vérifier tous les outils enregistrés.

Pour la recherche dans le `PATH`, utiliser l’API GLib adaptée, par exemple :

```c
g_find_program_in_path()
```

Règles :

- si l’exécutable est trouvé :
  - état `AVAILABLE` ;
  - chemin détecté mémorisé ;
- s’il n’est pas trouvé :
  - état `MISSING` ;
  - ancien chemin supprimé ;
- un second rafraîchissement doit remplacer proprement les anciennes valeurs ;
- aucune fuite mémoire ne doit être produite.

### Consultation

```c
gsize tool_registry_get_count(
    const ToolRegistry *tool_registry
);
```

```c
const ToolInfo *tool_registry_get_tool(
    const ToolRegistry *tool_registry,
    gsize index
);
```

```c
const ToolInfo *tool_registry_find(
    const ToolRegistry *tool_registry,
    const char *identifier
);
```

Le comportement de propriété doit être documenté clairement.

Pour une première version, les pointeurs retournés peuvent être empruntés et rester valides jusqu’à la prochaine modification du registre ou jusqu’à sa destruction.

### Version détectée

```c
gboolean tool_registry_set_version(
    ToolRegistry *tool_registry,
    const char *identifier,
    const char *detected_version,
    GError **error
);
```

Règles :

- l’outil doit exister ;
- la version peut être remplacée ;
- une chaîne vide ou `NULL` efface la version connue ;
- le registre doit dupliquer la chaîne.

Cette fonction sera utilisée plus tard par le module chargé d’interroger les exécutables.

### État global

```c
gboolean tool_registry_has_missing_required_tools(
    const ToolRegistry *tool_registry
);
```

Cette fonction retourne `TRUE` lorsqu’au moins un outil obligatoire est dans l’état `MISSING`.

Un outil obligatoire encore dans l’état `UNKNOWN` ne doit pas être considéré comme disponible.

Une seconde fonction est recommandée :

```c
gboolean tool_registry_all_required_tools_available(
    const ToolRegistry *tool_registry
);
```

Elle ne retourne `TRUE` que lorsque tous les outils obligatoires sont explicitement `AVAILABLE`.

---

## Accesseurs de ToolInfo

Prévoir au minimum :

```c
const char *tool_info_get_identifier(
    const ToolInfo *tool_info
);
```

```c
const char *tool_info_get_display_name(
    const ToolInfo *tool_info
);
```

```c
const char *tool_info_get_executable_name(
    const ToolInfo *tool_info
);
```

```c
ToolRequirement tool_info_get_requirement(
    const ToolInfo *tool_info
);
```

```c
ToolAvailability tool_info_get_availability(
    const ToolInfo *tool_info
);
```

```c
const char *tool_info_get_resolved_path(
    const ToolInfo *tool_info
);
```

```c
const char *tool_info_get_detected_version(
    const ToolInfo *tool_info
);
```

Les chaînes retournées sont empruntées et ne doivent jamais être libérées par l’appelant.

---

## Implémentation interne recommandée

Le registre peut utiliser :

```c
GPtrArray
```

pour conserver l’ordre d’enregistrement.

Une recherche linéaire par identifiant est acceptable pour cette première version, car le nombre d’outils restera faible.

Une table de hachage pourra être ajoutée plus tard seulement si elle devient utile.

Le registre devient propriétaire de tous les outils enregistrés.

Chaque outil doit libérer :

- son identifiant ;
- son nom affiché ;
- son nom d’exécutable ;
- son chemin résolu ;
- sa version ;
- sa structure.

---

## Détection testable

Les tests ne doivent pas dépendre de la présence réelle de `dig`, `curl`, `openssl` ou d’autres outils.

Le test doit créer un répertoire temporaire contenant un faux exécutable.

Exemple de stratégie :

1. créer un dossier temporaire avec GLib ;
2. créer un fichier nommé `fake_osint_tool` ;
3. lui donner les permissions d’exécution ;
4. sauvegarder la valeur actuelle de `PATH` ;
5. placer temporairement le dossier au début de `PATH` ;
6. enregistrer `fake_osint_tool` ;
7. appeler `tool_registry_refresh()` ;
8. vérifier que l’outil est disponible ;
9. restaurer impérativement le `PATH` initial ;
10. supprimer les fichiers temporaires.

Le test doit restaurer l’environnement même en cas d’échec d’une assertion intermédiaire.

Une fonction de préparation et une fonction de nettoyage de fixture sont recommandées.

---

## Tests unitaires obligatoires

### 1. Construction vide

Vérifier :

- création du registre ;
- nombre d’outils égal à zéro ;
- recherche d’un identifiant absent ;
- destruction sans erreur.

### 2. Enregistrement valide

Vérifier :

- nombre d’outils ;
- conservation de l’ordre ;
- contenu des accesseurs ;
- état initial `UNKNOWN` ;
- chemin initial `NULL` ;
- version initiale `NULL`.

### 3. Duplication des chaînes

Créer des chaînes dynamiques, enregistrer l’outil, puis libérer les chaînes originales.

Le registre doit conserver des valeurs valides.

### 4. Identifiant en double

Enregistrer deux outils avec le même identifiant.

Le second enregistrement doit échouer avec :

```text
TOOL_REGISTRY_ERROR_DUPLICATE_IDENTIFIER
```

Le premier outil ne doit pas être modifié.

### 5. Arguments invalides

Tester au minimum :

- registre `NULL` ;
- identifiant `NULL` ;
- identifiant vide ;
- nom affiché vide ;
- nom d’exécutable vide ;
- `GError` déjà initialisé si le projet applique cette règle.

### 6. Recherche par identifiant

Vérifier :

- outil existant ;
- outil absent ;
- identifiant `NULL` ;
- identifiant vide.

### 7. Outil disponible

Avec un faux exécutable placé temporairement dans le `PATH`, vérifier :

- état `AVAILABLE` ;
- chemin absolu non `NULL` ;
- chemin correspondant au faux exécutable.

### 8. Outil absent

Avec un nom improbable, vérifier :

- état `MISSING` ;
- chemin `NULL`.

Exemple :

```text
labfy_tool_that_must_not_exist_036
```

### 9. Rafraîchissement successif

Vérifier qu’un outil peut passer :

```text
MISSING → AVAILABLE
```

puis :

```text
AVAILABLE → MISSING
```

après modification contrôlée du `PATH`.

### 10. Version

Vérifier :

- ajout d’une version ;
- remplacement d’une version ;
- effacement avec `NULL` ;
- erreur sur identifiant inconnu.

### 11. Dépendances obligatoires

Tester les combinaisons :

```text
obligatoire + disponible
obligatoire + absent
obligatoire + inconnu
optionnel + absent
```

Une dépendance optionnelle absente ne doit pas rendre l’état global invalide.

### 12. Destruction complète

Exécuter les tests avec les outils de vérification mémoire du projet.

---

## Noms de tests suggérés

```text
/tool_registry/construction
/tool_registry/register
/tool_registry/string_ownership
/tool_registry/duplicate_identifier
/tool_registry/invalid_arguments
/tool_registry/find
/tool_registry/available_tool
/tool_registry/missing_tool
/tool_registry/successive_refresh
/tool_registry/version
/tool_registry/required_tools
```

---

## Makefile

Ajouter le nouveau module à la compilation principale.

Ajouter une cible :

```text
test_tool_registry
```

La cible doit compiler au minimum :

```text
tests/test_tool_registry.c
src/core/tool_registry.c
```

avec GLib.

Ajouter le test à la cible globale de tests du projet.

---

## Vérifications manuelles

Commandes attendues :

```bash
make clean
make
make test_tool_registry
./test_tool_registry
```

Puis, si une cible globale existe :

```bash
make test
```

Aucun warning de compilation ne doit être toléré.

---

## Vérification mémoire recommandée

Selon les outils déjà utilisés dans le projet :

```bash
G_DEBUG=gc-friendly \
G_SLICE=always-malloc \
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    ./test_tool_registry
```

Le test ne doit laisser aucune allocation définitivement perdue provenant du registre.

Les allocations internes conservées par GLib doivent être interprétées avec prudence.

---

## Critères d’acceptation

Le ticket est validé lorsque :

- le projet compile avec les options strictes ;
- le registre est opaque ;
- plusieurs outils peuvent être enregistrés ;
- les identifiants en double sont refusés ;
- les chaînes sont copiées ;
- les outils sont recherchés dans le `PATH` ;
- le chemin détecté est mémorisé ;
- un outil absent est marqué `MISSING` ;
- un rafraîchissement remplace correctement l’état précédent ;
- une version peut être stockée et effacée ;
- les dépendances obligatoires manquantes sont détectées ;
- les outils optionnels absents ne bloquent pas l’état global ;
- les tests ne dépendent pas des outils réellement installés ;
- le `PATH` original est restauré après chaque test ;
- aucune dépendance GTK n’est introduite ;
- aucun appel shell n’est utilisé ;
- tous les tests passent ;
- aucune erreur mémoire liée au module n’est détectée.

---

## Résultat attendu

À la fin de ce ticket, le code suivant doit être conceptuellement possible :

```c
ToolRegistry *tool_registry = NULL;
const ToolInfo *dig_tool = NULL;
GError *error = NULL;

tool_registry = tool_registry_new();

tool_registry_register(
    tool_registry,
    "dns.dig",
    "dig",
    "dig",
    TOOL_REQUIREMENT_OPTIONAL,
    &error
);

tool_registry_refresh(
    tool_registry,
    &error
);

dig_tool = tool_registry_find(
    tool_registry,
    "dns.dig"
);

if (tool_info_get_availability(dig_tool) ==
    TOOL_AVAILABILITY_AVAILABLE)
{
    g_print(
        "dig détecté dans : %s\n",
        tool_info_get_resolved_path(dig_tool)
    );
}
```

Cet exemple décrit le comportement attendu. Il ne constitue pas une obligation d’organisation interne.

---

## Suite prévue

Après validation de ce ticket :

1. catalogue initial des outils utilisés par Labfy Investigation ;
2. abstraction contrôlée autour de `GSubprocess` ;
3. interrogation des versions ;
4. adaptateurs d’outils OSINT ;
5. affichage des dépendances dans l’interface ;
6. intégration avec `BackgroundTask` et `TaskManager` ;
7. conservation des sorties brutes et de leur provenance ;
8. prise en compte du paquet Ubuntu et du script d’installation.
