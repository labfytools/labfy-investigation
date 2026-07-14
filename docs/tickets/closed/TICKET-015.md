# Ticket #015

## Titre

Afficher le nœud sélectionné dans le `Workspace`.

---

## Objectif

Afficher dans la zone de travail les informations principales du nœud sélectionné dans l'arborescence.

Lorsqu'un dossier ou un fichier est sélectionné, le `Workspace` doit afficher :

- son nom ;
- son type ;
- son parent éventuel ;
- son nombre d'enfants s'il s'agit d'un dossier.

Aucun fichier ne doit encore être ouvert.

---

## Architecture

```text
InvestigationTreeView
        │
        ▼
Application
        │
        ▼
MainWindow
        │
        ▼
Workspace
```

`Application` reste le coordinateur.

`InvestigationTreeView` détecte la sélection.

`Workspace` affiche les informations reçues.

---

## Responsabilités

### Application

Le module doit :

- recevoir le nœud sélectionné ;
- transmettre ce nœud à `MainWindow` ;
- ne pas modifier ni libérer le nœud.

### MainWindow

Le module doit :

- recevoir un nœud en lecture seule ;
- le transmettre au `Workspace`.

### Workspace

Le module doit :

- recevoir un `InvestigationNode` en lecture seule ;
- afficher son nom ;
- afficher son type ;
- afficher le nom de son parent si disponible ;
- afficher le nombre d'enfants pour un dossier ;
- restaurer la page d'accueil si le nœud vaut `NULL`.

---

## Principe de propriété

Le nœud sélectionné reste la propriété du `InvestigationTreeModel`.

`Application`, `MainWindow` et `Workspace` ne reçoivent qu'une référence non propriétaire :

```c
const InvestigationNode *node;
```

Aucun de ces modules ne doit appeler :

```c
investigation_node_free(node);
```

---

## Fichiers concernés

```text
include/widgets/workspace.h
src/widgets/workspace.c

include/views/main_window.h
src/views/main_window.c

src/core/application.c
```

Aucune modification du Core métier n'est nécessaire.

---

## Interfaces publiques à ajouter

### Workspace

```c
void workspace_set_selected_node(
    Workspace *workspace,
    const InvestigationNode *node
);
```

### MainWindow

```c
void main_window_set_selected_node(
    MainWindow *main_window,
    const InvestigationNode *node
);
```

---

## Comportement attendu

### Aucun nœud sélectionné

```text
Labfy Investigation

Aucune enquête ouverte

Sélectionnez ou créez une enquête
```

### Dossier sélectionné

```text
00_BaseDeDonnees

Type : dossier
Parent : Template
Enfants : 2
```

### Fichier sélectionné

```text
Enquete.sqlite

Type : fichier
Parent : 00_BaseDeDonnees
```

---

## Hors périmètre

Ce ticket ne doit pas :

- ouvrir le contenu d'un fichier ;
- afficher un aperçu ;
- lire les métadonnées du système de fichiers ;
- afficher la taille d'un fichier ;
- communiquer avec SQLite ;
- modifier l'arborescence ;
- gérer plusieurs sélections.

---

## Contraintes techniques

- C17 ;
- GTK4 uniquement ;
- aucun état global ;
- aucune propriété du nœud transférée au `Workspace` ;
- documentation Doxygen ;
- compilation sans warning ;
- aucun `Gtk-CRITICAL`.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste valide.
- [ ] La sélection d'un dossier met à jour le `Workspace`.
- [ ] La sélection d'un fichier met à jour le `Workspace`.
- [ ] Le nom est correct.
- [ ] Le type est correct.
- [ ] Le parent est correct.
- [ ] Le nombre d'enfants est correct pour un dossier.
- [ ] Une sélection `NULL` restaure la page d'accueil.
- [ ] Aucun module graphique ne libère le nœud.
- [ ] Aucun `Gtk-CRITICAL`.

---

## Tests manuels

1. Ouvrir une enquête.
2. Sélectionner un dossier.
3. Vérifier les informations affichées.
4. Sélectionner un fichier.
5. Vérifier les informations affichées.
6. Changer d'enquête.
7. Vérifier que l'ancien nœud n'est plus affiché.
8. Lancer `make test`.

---

## Commit attendu

```text
feat(gui): display selected node in workspace
```
