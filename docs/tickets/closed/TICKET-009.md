# Ticket #009

## Titre

Ajouter les relations parent/enfants à `InvestigationNode`.

---

## Objectif

Faire évoluer `InvestigationNode` afin qu'un nœud puisse appartenir à une arborescence.

Chaque nœud pourra :

- connaître son parent ;
- posséder plusieurs enfants ;
- exposer ses enfants en lecture seule via une API publique.

---

## Responsabilités

Le module `InvestigationNode` doit :

- mémoriser un pointeur vers son parent ;
- posséder un tableau dynamique d'enfants ;
- ajouter un enfant ;
- retourner un enfant par son index ;
- retourner le nombre d'enfants ;
- retourner son parent ;
- détruire récursivement les enfants qu'il possède.

---

## Hors périmètre

Ce ticket ne doit pas :

- parcourir le système de fichiers ;
- construire automatiquement une arborescence depuis un dossier ;
- afficher quoi que ce soit avec GTK ;
- communiquer avec SQLite ;
- gérer le tri des enfants ;
- gérer la suppression individuelle d'un enfant ;
- gérer le déplacement d'un nœud entre deux parents.

---

## Architecture

```text
InvestigationTreeModel
        │
        ▼
InvestigationNode
        │
        ├── InvestigationNode
        ├── InvestigationNode
        └── InvestigationNode
```

Le module appartient à la couche `core`.

Il peut dépendre de GLib, mais jamais de GTK.

---

## Gestion de la propriété

Un nœud devient propriétaire de chaque enfant ajouté avec :

```c
investigation_node_add_child(parent, child);
```

Après un ajout réussi :

- `parent` possède `child` ;
- le code appelant ne doit plus libérer `child` directement ;
- `child` conserve une référence non propriétaire vers `parent`.

Lors de la destruction du parent, tous ses enfants sont détruits récursivement.

---

## Structure interne attendue

```c
struct InvestigationNode
{
    char *name;
    InvestigationNodeType type;
    InvestigationNode *parent;
    GPtrArray *children;
};
```

La structure reste privée dans `investigation_node.c`.

---

## Interface publique attendue

Les fonctions existantes sont conservées :

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

Les fonctions suivantes sont ajoutées :

```c
bool investigation_node_add_child(
    InvestigationNode *parent,
    InvestigationNode *child
);

const InvestigationNode *investigation_node_get_child(
    const InvestigationNode *node,
    size_t index
);

size_t investigation_node_get_children_count(
    const InvestigationNode *node
);

const InvestigationNode *investigation_node_get_parent(
    const InvestigationNode *node
);
```

---

## Comportement attendu

Exemple :

```c
InvestigationNode *root = NULL;
InvestigationNode *child = NULL;

root = investigation_node_new(
    "Template",
    INVESTIGATION_NODE_DIRECTORY
);

child = investigation_node_new(
    "00_BaseDeDonnees",
    INVESTIGATION_NODE_DIRECTORY
);

if (!investigation_node_add_child(root, child))
{
    investigation_node_free(child);
    investigation_node_free(root);
    return;
}
```

Après l'ajout :

```text
Template
└── 00_BaseDeDonnees
```

Le nœud `root` devient propriétaire de `child`.

Le nettoyage correct est uniquement :

```c
investigation_node_free(root);
```

---

## Règles d'ajout

`investigation_node_add_child()` doit refuser :

- un parent `NULL` ;
- un enfant `NULL` ;
- un parent qui représente un fichier ;
- l'ajout d'un nœud comme enfant de lui-même ;
- un enfant possédant déjà un parent.

En cas d'échec, la propriété de l'enfant reste au code appelant.

---

## Dépendances

- C17 ;
- GLib.

Aucune dépendance GTK ou SQLite.

---

## Contraintes techniques

- structure opaque ;
- aucun état global ;
- utilisation de `GPtrArray` ;
- tableau créé avec une fonction de destruction ;
- documentation Doxygen ;
- compilation sans warning ;
- respect des conventions de nommage ;
- aucune modification directe du tableau en dehors du module.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] Un dossier peut posséder plusieurs enfants.
- [ ] Un fichier ne peut pas recevoir d'enfant.
- [ ] Un enfant connaît son parent.
- [ ] Le nombre d'enfants est correct.
- [ ] Un enfant peut être récupéré par son index.
- [ ] Un index invalide retourne `NULL`.
- [ ] Un enfant ne peut pas avoir deux parents.
- [ ] Un nœud ne peut pas être son propre enfant.
- [ ] La destruction d'un parent détruit récursivement ses enfants.
- [ ] Aucun code GTK.
- [ ] Aucun code SQLite.

---

## Tests

- créer un parent dossier ;
- ajouter un enfant dossier ;
- ajouter un enfant fichier ;
- vérifier le nombre d'enfants ;
- récupérer chaque enfant ;
- vérifier le parent d'un enfant ;
- tester un index invalide ;
- refuser l'ajout à un fichier ;
- refuser un parent `NULL` ;
- refuser un enfant `NULL` ;
- refuser l'auto-référence ;
- refuser un enfant possédant déjà un parent ;
- détruire un arbre complet ;
- appeler les getters avec `NULL`.

---

## Commit attendu

```text
feat(core): add investigation node hierarchy
```
