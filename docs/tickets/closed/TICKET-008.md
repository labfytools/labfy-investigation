# Ticket #008

## Titre

Créer le module `InvestigationTreeModel`.

---

## Objectif

Créer une structure métier représentant le modèle de l'arborescence d'une enquête.

Le modèle sera propriétaire du nœud racine et servira de base au futur explorateur d'enquête.

---

## Responsabilités

Le module `InvestigationTreeModel` doit :

- posséder un nœud racine ;
- gérer le cycle de vie du modèle ;
- exposer le nœud racine en lecture seule.

---

## Hors périmètre

Ce ticket ne doit pas :

- parcourir le système de fichiers ;
- créer les enfants d'un nœud ;
- afficher une interface GTK ;
- communiquer avec SQLite ;
- charger une enquête.

---

## Architecture

```text
Investigation
        │
        ▼
InvestigationTreeModel
        │
        ▼
InvestigationNode
```

Le module appartient à la couche **Core**.

Aucune dépendance vers GTK.

---

## Principe de propriété

Le module est **propriétaire** du nœud racine.

À partir du moment où un `InvestigationNode` est transmis au constructeur :

```c
investigation_tree_model_new(root_node);
```

le modèle devient responsable de sa destruction.

Le code appelant ne doit plus appeler :

```c
investigation_node_free(root_node);
```

La destruction du modèle doit automatiquement détruire son nœud racine.

---

## Fichiers concernés

```text
include/core/investigation_tree_model.h
src/core/investigation_tree_model.c
```

---

## Interface publique attendue

```c
InvestigationTreeModel *investigation_tree_model_new(
    InvestigationNode *root_node
);

void investigation_tree_model_free(
    InvestigationTreeModel *tree_model
);

const InvestigationNode *investigation_tree_model_get_root(
    const InvestigationTreeModel *tree_model
);
```

---

## Comportement attendu

Le modèle contient uniquement un nœud racine.

Exemple :

```text
Template
```

Les enfants seront ajoutés dans un ticket ultérieur.

---

## Contraintes techniques

- C17
- Structure opaque
- Aucun état global
- Aucune dépendance GTK
- Documentation Doxygen
- Compilation sans warning
- Respect des conventions du projet

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] Le modèle est opaque.
- [ ] Le modèle possède un nœud racine.
- [ ] Le getter retourne le nœud racine.
- [ ] La destruction du modèle détruit également le nœud racine.
- [ ] Aucun code GTK.
- [ ] Aucun code SQLite.

---

## Tests

- création d'un modèle valide ;
- lecture du nœud racine ;
- destruction du modèle ;
- création avec un nœud NULL ;
- destruction avec NULL.

---

## Commit attendu

```text
feat(core): create investigation tree model
```
