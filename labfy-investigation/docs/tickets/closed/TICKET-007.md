# Ticket #007

## Titre

Créer le module `InvestigationNode`.

---

## Objectif

Créer une structure métier représentant un élément de l'arborescence d'une enquête.

Un nœud peut représenter :

- un dossier ;
- un fichier.

Ce module servira de base au futur modèle d'arborescence.

---

## Responsabilités

Le module `InvestigationNode` doit :

- mémoriser le nom d'un élément ;
- mémoriser son type ;
- gérer sa propre mémoire ;
- exposer ses informations en lecture seule.

---

## Hors périmètre

Ce ticket ne doit pas :

- parcourir le système de fichiers ;
- construire une arborescence complète ;
- stocker des enfants ;
- afficher quoi que ce soit avec GTK ;
- communiquer avec SQLite ;
- ouvrir une enquête.

---

## Architecture

```text
InvestigationTreeModel
        │
        └── InvestigationNode
```

Le module appartient à la couche `core`.

Il ne doit contenir aucune dépendance vers GTK.

---

## Fichiers concernés

```text
include/core/investigation_node.h
src/core/investigation_node.c
```

---

## Type attendu

```c
typedef enum
{
    INVESTIGATION_NODE_DIRECTORY,
    INVESTIGATION_NODE_FILE
} InvestigationNodeType;
```

---

## Interface publique attendue

```c
InvestigationNode *investigation_node_new(
    const char *name,
    InvestigationNodeType type
);

void investigation_node_free(
    InvestigationNode *node
);

const char *investigation_node_get_name(
    const InvestigationNode *node
);

InvestigationNodeType investigation_node_get_type(
    const InvestigationNode *node
);
```

---

## Comportement attendu

Exemple d'utilisation :

```c
InvestigationNode *node = NULL;

node = investigation_node_new(
    "01_Preuves_Originales",
    INVESTIGATION_NODE_DIRECTORY
);

if (node == NULL)
{
    /* Gestion de l'erreur */
}

const char *name = investigation_node_get_name(node);
InvestigationNodeType type = investigation_node_get_type(node);

investigation_node_free(node);
```

---

## Cas à gérer

- nom valide ;
- nom vide ;
- pointeur `NULL` ;
- type dossier ;
- type fichier ;
- libération avec `NULL`.

---

## Contraintes techniques

- C17 ;
- structure opaque ;
- aucun état global ;
- aucune dépendance GTK ;
- documentation Doxygen ;
- compilation sans warning ;
- noms conformes à `DEVELOPMENT.md`.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] La structure `InvestigationNode` est opaque.
- [ ] Un nœud peut représenter un fichier.
- [ ] Un nœud peut représenter un dossier.
- [ ] Le nom est copié et possédé par le nœud.
- [ ] Les getters retournent des données en lecture seule.
- [ ] `investigation_node_free(NULL)` est accepté.
- [ ] Aucun code GTK.
- [ ] Aucun code SQLite.
- [ ] Les fonctions publiques sont documentées avec Doxygen.

---

## Tests

- créer un nœud dossier ;
- créer un nœud fichier ;
- lire le nom ;
- lire le type ;
- tester un nom vide ;
- tester un nom `NULL` ;
- libérer un nœud valide ;
- appeler `investigation_node_free(NULL)`.

---

## Commit attendu

```text
feat(core): create investigation node
```
