# Ticket #018

## Titre

Afficher le chemin complet du nœud sélectionné dans le Workspace.

---

## Objectif

Afficher le chemin complet du fichier ou du dossier actuellement sélectionné.

Le `Workspace` doit utiliser exclusivement le getter :

```c
investigation_node_get_path()
```

Aucune reconstruction du chemin ne doit être réalisée dans la couche graphique.

---

## Architecture

```text
InvestigationNode
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

Le Core reste propriétaire des informations.

Le `Workspace` les affiche uniquement.

---

## Responsabilités

### InvestigationNode

Aucune modification.

### Application

Aucune modification.

### MainWindow

Aucune modification.

### Workspace

Le module doit :

- afficher le chemin complet du nœud sélectionné ;
- masquer cette information lorsqu'aucun nœud n'est sélectionné.

---

## Fichiers concernés

```text
src/widgets/workspace.c
```

Aucune modification du Core.

---

## Interface publique

Aucune modification.

L'API du `Workspace` reste inchangée.

---

## Affichage attendu

### Dossier

```text
00_BaseDeDonnees

Chemin :
/home/fy59/Documents/Enquetes/Test/00_BaseDeDonnees

Type :
Dossier

Parent :
Template

Enfants :
2
```

### Fichier

```text
Enquete.sqlite

Chemin :
/home/fy59/Documents/Enquetes/Test/00_BaseDeDonnees/Enquete.sqlite

Type :
Fichier

Parent :
00_BaseDeDonnees
```

---

## Comportement attendu

Le chemin affiché doit toujours être identique à :

```c
investigation_node_get_path(node)
```

Le `Workspace` ne doit jamais construire lui-même :

```text
parent + "/" + nom
```

---

## Hors périmètre

Ce ticket ne doit pas :

- ouvrir le fichier ;
- vérifier que le chemin existe encore ;
- afficher la taille ;
- afficher les permissions ;
- afficher la date de modification ;
- ouvrir le gestionnaire de fichiers.

---

## Contraintes techniques

- GTK4 uniquement ;
- aucun état global ;
- aucune allocation mémoire supplémentaire pour le chemin ;
- utiliser directement le pointeur retourné par
  `investigation_node_get_path()`;
- documentation Doxygen ;
- compilation sans warning.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste valide.
- [ ] Le chemin du dossier est affiché.
- [ ] Le chemin du fichier est affiché.
- [ ] La page d'accueil reste inchangée.
- [ ] Aucun chemin n'est reconstruit dans la GUI.
- [ ] Aucun `Gtk-CRITICAL`.

---

## Tests

1. Lancer l'application.
2. Sélectionner un dossier.
3. Vérifier le chemin affiché.
4. Sélectionner un fichier.
5. Vérifier le chemin affiché.
6. Comparer avec :

```c
investigation_node_get_path(node)
```

7. Vérifier que `make test` reste valide.

---

## Commit attendu

```text
feat(gui): display selected node path
```
