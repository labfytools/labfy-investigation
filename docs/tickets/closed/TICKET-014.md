# Ticket #014

## Titre

Ajouter la sélection d'un nœud dans l'arborescence.

---

## Objectif

Permettre à l'utilisateur de sélectionner un dossier ou un fichier dans
`InvestigationTreeView`.

Le nœud sélectionné doit être transmis jusqu'au module `Application`, qui
reste responsable de la coordination entre l'arborescence et le reste de
l'interface.

Ce ticket ne doit encore ouvrir aucun fichier.

---

## Architecture

```text
GtkListView
    │
    ▼
InvestigationTreeView
    │
    ▼
Sidebar
    │
    ▼
MainWindow
    │
    ▼
Application
```

Le Core reste indépendant de GTK.

---

## Responsabilités

### InvestigationTreeView

Le module doit :

- remplacer `GtkNoSelection` par `GtkSingleSelection` ;
- détecter les changements de sélection ;
- retrouver le `InvestigationNode` métier correspondant ;
- appeler un callback public avec le nœud sélectionné ;
- transmettre `NULL` lorsqu'aucun nœud n'est sélectionné.

### Sidebar

Le module doit :

- recevoir un callback de sélection ;
- le transmettre à `InvestigationTreeView` ;
- ne contenir aucune logique métier liée à la sélection.

### MainWindow

Le module doit :

- recevoir le callback depuis `Application` ;
- le transmettre à `Sidebar`.

### Application

Le module doit :

- recevoir le nœud sélectionné ;
- afficher temporairement son nom et son type dans le terminal ;
- ne pas modifier ni libérer le nœud reçu.

---

## Principe de propriété

Le nœud sélectionné appartient toujours au
`InvestigationTreeModel`, lui-même possédé par `Application`.

Le callback reçoit uniquement une référence non propriétaire :

```c
const InvestigationNode *node;
```

Le code appelé ne doit jamais faire :

```c
investigation_node_free(node);
```

La référence reste valide tant que le modèle courant n'est pas remplacé ou
détruit.

---

## Interfaces publiques à ajouter

### InvestigationTreeView

```c
typedef void (*InvestigationTreeViewSelectionCallback)(
    const InvestigationNode *node,
    gpointer user_data
);
```

```c
void investigation_tree_view_set_selection_callback(
    InvestigationTreeView *tree_view,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
);
```

### Sidebar

```c
void sidebar_set_selection_callback(
    Sidebar *sidebar,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
);
```

### MainWindow

```c
void main_window_set_tree_selection_callback(
    MainWindow *main_window,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
);
```

---

## Comportement attendu

Lorsque l'utilisateur sélectionne :

```text
Enquete.sqlite
```

le terminal affiche temporairement :

```text
Nœud sélectionné : Enquete.sqlite
Type : fichier
```

Pour un dossier :

```text
Nœud sélectionné : 00_BaseDeDonnees
Type : dossier
```

Lorsqu'aucun élément n'est sélectionné, le callback reçoit `NULL`.

---

## Hors périmètre

Ce ticket ne doit pas :

- ouvrir un fichier ;
- modifier le Workspace ;
- afficher un aperçu ;
- sélectionner plusieurs nœuds ;
- ajouter un menu contextuel ;
- afficher des icônes ;
- modifier le Core ;
- modifier le système de fichiers.

---

## Contraintes techniques

- GTK4 uniquement ;
- utiliser `GtkSingleSelection` ;
- aucun `GtkTreeView` ;
- aucun état global ;
- callback documenté avec Doxygen ;
- aucune destruction du nœud sélectionné ;
- compilation sans warning ;
- aucun `Gtk-CRITICAL`.

---

## Gestion des changements de modèle

Lorsqu'un nouveau modèle est installé :

- la sélection précédente doit être supprimée ;
- aucun callback ne doit conserver une référence vers un nœud de l'ancien
  modèle ;
- la nouvelle vue doit démarrer sans sélection.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste entièrement valide.
- [ ] Un seul nœud peut être sélectionné.
- [ ] La sélection d'un dossier est détectée.
- [ ] La sélection d'un fichier est détectée.
- [ ] Le bon `InvestigationNode` est transmis au callback.
- [ ] `Application` reçoit l'événement.
- [ ] Le nom et le type sont affichés dans le terminal.
- [ ] Le changement d'enquête efface l'ancienne sélection.
- [ ] Aucun module graphique ne libère le nœud.
- [ ] Aucun `Gtk-CRITICAL`.

---

## Tests manuels

1. Lancer l'application.
2. Ouvrir le dossier `Template`.
3. Développer `00_BaseDeDonnees`.
4. Sélectionner `Enquete.sqlite`.
5. Vérifier le nom et le type dans le terminal.
6. Sélectionner un dossier.
7. Vérifier que le type affiché est `dossier`.
8. Ouvrir une autre enquête.
9. Vérifier qu'aucune ancienne sélection n'est conservée.
10. Lancer `make test`.

---

## Commit attendu

```text
feat(gui): add investigation tree selection
```
