# Ticket #017

## Titre

Ajouter le chemin complet à `InvestigationNode`.

---

## Objectif

Faire évoluer `InvestigationNode` afin que chaque nœud connaisse le chemin
complet qu'il représente sur le système de fichiers.

Cette information permettra ensuite au `Workspace` et aux futurs visualiseurs
de savoir exactement quel fichier ou dossier est sélectionné.

---

## Responsabilités

Le module `InvestigationNode` doit :

- mémoriser un chemin complet ;
- posséder sa propre copie de ce chemin ;
- exposer le chemin en lecture seule ;
- libérer correctement cette chaîne lors de sa destruction.

Le module `InvestigationTreeBuilder` doit :

- fournir le chemin complet lors de la création de chaque nœud ;
- construire correctement le chemin des enfants ;
- ne jamais reconstruire le chemin dans la GUI.

---

## Architecture

```text
InvestigationTreeBuilder
        │
        ▼
InvestigationNode
├── name
├── path
├── type
├── parent
└── children
```

Le chemin appartient au Core.

La couche graphique ne fait que le lire.

---

## Fichiers concernés

```text
include/core/investigation_node.h
src/core/investigation_node.c

src/core/investigation_tree_builder.c

tests/test_investigation_node.c
tests/test_investigation_tree_builder.c
```

Aucune modification graphique dans ce ticket.

---

## API publique à faire évoluer

Le constructeur devient :

```c
InvestigationNode *investigation_node_new(
    const char *name,
    const char *path,
    InvestigationNodeType type
);
```

Un nouveau getter est ajouté :

```c
const char *investigation_node_get_path(
    const InvestigationNode *node
);
```

---

## Principe de propriété

Le nœud copie le chemin reçu avec une allocation dédiée.

Après :

```c
node = investigation_node_new(
    "Enquete.sqlite",
    "/home/fy59/Enquetes/Test/00_BaseDeDonnees/Enquete.sqlite",
    INVESTIGATION_NODE_FILE
);
```

le code appelant peut modifier ou libérer ses propres chaînes.

Le nœud reste propriétaire de ses copies internes.

---

## Structure interne attendue

```c
struct InvestigationNode
{
    char *name;
    char *path;
    InvestigationNodeType type;
    InvestigationNode *parent;
    GPtrArray *children;
};
```

La structure reste opaque.

---

## Comportement attendu

Pour le dossier racine :

```text
Nom :
Test

Chemin :
/home/fy59/Enquetes/Test
```

Pour un fichier enfant :

```text
Nom :
Enquete.sqlite

Chemin :
/home/fy59/Enquetes/Test/00_BaseDeDonnees/Enquete.sqlite
```

---

## Règles de validation

Le constructeur doit refuser :

- `name == NULL` ;
- un nom vide ;
- `path == NULL` ;
- un chemin vide.

Le getter doit retourner :

```c
NULL
```

si le nœud vaut `NULL`.

---

## Hors périmètre

Ce ticket ne doit pas :

- afficher le chemin dans le `Workspace` ;
- ouvrir un fichier ;
- normaliser les permissions ;
- résoudre les liens symboliques ;
- calculer un chemin relatif ;
- modifier le système de fichiers ;
- ajouter des métadonnées.

---

## Contraintes techniques

- C17 ;
- GLib autorisée ;
- aucune dépendance GTK ;
- structure opaque ;
- aucun état global ;
- documentation Doxygen ;
- compilation sans warning ;
- aucune fuite mémoire.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `InvestigationNode` possède un chemin.
- [ ] Le chemin est copié.
- [ ] Le getter retourne le bon chemin.
- [ ] Les chemins racine et enfants sont corrects.
- [ ] Les noms invalides sont refusés.
- [ ] Les chemins invalides sont refusés.
- [ ] Le builder construit correctement tous les chemins.
- [ ] Tous les tests existants sont adaptés.
- [ ] `make test` reste entièrement valide.
- [ ] Aucun code GTK n'est modifié.

---

## Tests

### InvestigationNode

- créer un nœud avec un chemin valide ;
- lire le chemin ;
- tester `path == NULL` ;
- tester un chemin vide ;
- vérifier que la chaîne est copiée ;
- tester `investigation_node_get_path(NULL)`.

### InvestigationTreeBuilder

- vérifier le chemin de la racine ;
- vérifier le chemin de `DirectoryA` ;
- vérifier le chemin de `FileA.txt` ;
- vérifier le chemin de `RootFile.md`.

---

## Commit attendu

```text
feat(core): add path to investigation nodes
```
