# Ticket #012

## Titre

Créer le composant `InvestigationTreeView`.

---

## Objectif

Créer un composant graphique spécialisé chargé d'afficher un
`InvestigationTreeModel` à l'aide des widgets modernes de GTK4.

Ce composant servira d'adaptateur entre le modèle métier et les widgets GTK.

À ce stade, il n'est pas encore intégré dans la `Sidebar`.

---

## Architecture

```text
InvestigationTreeModel
          │
          ▼
InvestigationTreeView
          │
          ▼
GtkTreeListModel
          │
          ▼
GtkListView
```

Le composant appartient à la couche graphique.

Le Core ne dépend jamais de GTK.

---

## Responsabilités

Le module doit :

- recevoir un `InvestigationTreeModel` ;
- créer un `GtkTreeListModel` ;
- créer un `GtkListView` ;
- utiliser un `GtkSignalListItemFactory` ;
- utiliser un `GtkTreeExpander` ;
- afficher le nom des nœuds ;
- permettre le développement et le repli des dossiers ;
- fournir un widget GTK réutilisable.

---

## Hors périmètre

Ce ticket ne doit pas :

- intégrer le composant dans la Sidebar ;
- permettre la sélection d'un nœud ;
- afficher des icônes ;
- ouvrir des fichiers ;
- modifier l'arborescence ;
- communiquer avec SQLite ;
- rafraîchir automatiquement le modèle.

---

## Principe de propriété

Le composant ne devient jamais propriétaire du
`InvestigationTreeModel`.

Il reçoit uniquement une référence.

En revanche, il est propriétaire de tous les objets GTK qu'il crée.

---

## Fichiers concernés

```text
include/widgets/investigation_tree_view.h
src/widgets/investigation_tree_view.c
```

Aucun autre module ne doit être modifié.

---

## Interface publique

```c
typedef struct InvestigationTreeView InvestigationTreeView;
```

```c
InvestigationTreeView *investigation_tree_view_new(void);
```

```c
GtkWidget *investigation_tree_view_get_widget(
    const InvestigationTreeView *tree_view
);
```

```c
void investigation_tree_view_set_model(
    InvestigationTreeView *tree_view,
    const InvestigationTreeModel *tree_model
);
```

```c
void investigation_tree_view_free(
    InvestigationTreeView *tree_view
);
```

---

## Dépendances GTK

Le composant doit utiliser exclusivement :

- GtkTreeListModel
- GtkListView
- GtkTreeExpander
- GtkSignalListItemFactory

Aucun widget GTK3.

---

## Contraintes techniques

- C17
- GTK4 uniquement
- aucune variable globale
- aucune dépendance vers SQLite
- aucune lecture directe du système de fichiers
- documentation Doxygen
- compilation sans warning
- aucune fuite mémoire

---

## Critères d'acceptation

- [ ] Le projet compile.
- [ ] Le composant peut être créé.
- [ ] Le composant fournit un widget GTK.
- [ ] Un modèle peut être associé.
- [ ] Les dossiers peuvent être développés.
- [ ] Les fichiers apparaissent.
- [ ] Le composant ne possède pas le modèle.
- [ ] Aucun Gtk-CRITICAL.
- [ ] Aucun warning.

---

## Commit attendu

```text
feat(gui): create investigation tree view
```
