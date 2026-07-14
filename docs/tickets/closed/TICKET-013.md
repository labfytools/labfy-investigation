# Ticket #013

## Titre

Intégrer `InvestigationTreeView` dans la `Sidebar`.

---

## Objectif

Afficher dans la barre latérale l'arborescence complète de l'enquête ouverte.

Le composant `InvestigationTreeView` existe déjà. Ce ticket consiste uniquement à l'intégrer dans `Sidebar` et à lui transmettre le modèle métier reçu.

---

## Architecture

```text
Application
    │
    ▼
MainWindow
    │
    ▼
Sidebar
    │
    ▼
InvestigationTreeView
    │
    ▼
GtkTreeListModel
```

La `Sidebar` héberge la vue.

Elle ne parcourt jamais directement le système de fichiers.

---

## Responsabilités

### Sidebar

Le module `Sidebar` doit :

- créer un `InvestigationTreeView` ;
- intégrer son widget racine sous le titre ;
- transmettre le `InvestigationTreeModel` au composant ;
- libérer la structure `InvestigationTreeView` lors de sa propre destruction.

### InvestigationTreeView

Le composant conserve ses responsabilités actuelles :

- adapter le modèle métier à GTK4 ;
- afficher les nœuds ;
- gérer l'ouverture et le repli des dossiers.

---

## Principe de propriété

`Sidebar` est propriétaire de :

```text
InvestigationTreeView
```

Elle doit donc appeler :

```c
investigation_tree_view_free(...)
```

lors de sa destruction.

En revanche, ni `Sidebar` ni `InvestigationTreeView` ne possèdent :

```text
InvestigationTreeModel
```

Le modèle reste la propriété de `Application`.

---

## Fichiers concernés

```text
src/widgets/sidebar.c
```

Éventuellement :

```text
include/widgets/sidebar.h
```

uniquement si l'API publique doit être précisée.

Aucun changement du Core n'est nécessaire.

---

## Modifications attendues

La structure privée `Sidebar` doit contenir :

```c
InvestigationTreeView *tree_view;
```

Le constructeur doit :

1. créer le composant ;
2. récupérer son widget racine ;
3. l'ajouter sous le titre ;
4. lui permettre d'occuper l'espace disponible.

La fonction :

```c
sidebar_set_tree_model(...)
```

doit transmettre le modèle à :

```c
investigation_tree_view_set_model(...)
```

Le titre de la `Sidebar` peut continuer à afficher le nom du nœud racine.

---

## Interface graphique attendue

```text
Template

▸ 00_BaseDeDonnees
▸ 01_Preuves_Originales
▸ 02_Preuves_traitees
▸ 03_Chronologie
▸ 04_Entites
...
```

Les dossiers peuvent être développés et repliés.

Les fichiers apparaissent dans les branches développées.

---

## Hors périmètre

Ce ticket ne doit pas :

- permettre la sélection d'un nœud ;
- afficher des icônes ;
- ouvrir un fichier ;
- ajouter un menu contextuel ;
- rafraîchir automatiquement l'arbre ;
- modifier le système de fichiers ;
- communiquer avec SQLite.

---

## Contraintes techniques

- GTK4 uniquement ;
- aucun `GtkTreeView` ;
- aucune lecture directe du disque dans `Sidebar` ;
- aucun transfert de propriété du modèle métier ;
- aucun état global ;
- documentation cohérente ;
- compilation sans warning ;
- aucun `Gtk-CRITICAL`.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste entièrement valide.
- [ ] La vue est intégrée dans la `Sidebar`.
- [ ] Le titre affiche le nom de l'enquête.
- [ ] Tous les dossiers racine apparaissent.
- [ ] Les branches peuvent être développées et repliées.
- [ ] Les fichiers apparaissent dans les branches.
- [ ] L'ouverture successive de deux enquêtes reconstruit correctement l'affichage.
- [ ] La fermeture ne produit aucun warning critique.
- [ ] Le Core n'est pas modifié.

---

## Tests manuels

1. Lancer l'application.
2. Ouvrir le dossier `Template`.
3. Vérifier l'affichage des dossiers racine.
4. Développer `00_BaseDeDonnees`.
5. Vérifier la présence de `Enquete.sqlite`.
6. Développer plusieurs niveaux.
7. Fermer l'application.
8. Rouvrir avec une autre enquête.
9. Vérifier que l'ancien arbre a disparu.
10. Lancer `make test`.

---

## Commit attendu

```text
feat(gui): integrate investigation tree into sidebar
```
