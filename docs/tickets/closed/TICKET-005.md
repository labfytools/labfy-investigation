# Ticket #005

## Titre

Créer le panneau de navigation latéral (`Sidebar`).

---

## Objectif

Créer la structure principale de l'interface avec :

- un panneau latéral à gauche ;
- une zone de travail à droite ;
- une séparation redimensionnable entre les deux zones.

Le panneau latéral servira plus tard à afficher l'arborescence du dossier d'enquête.

---

## Responsabilités

Le module `Sidebar` doit :

- créer un panneau GTK réutilisable ;
- posséder une largeur initiale raisonnable ;
- afficher temporairement un titre ;
- exposer son widget racine en lecture seule.

Le module `MainWindow` doit :

- intégrer le panneau latéral ;
- conserver la zone de travail principale ;
- permettre le redimensionnement des deux zones.

---

## Hors périmètre

Ce ticket ne doit pas :

- afficher l'arborescence réelle des fichiers ;
- lire le contenu d'un dossier ;
- surveiller les changements du système de fichiers ;
- créer ou supprimer des fichiers ;
- ajouter le bouton permettant de masquer le panneau ;
- communiquer avec SQLite.

---

## Architecture

```text
MainWindow
├── Sidebar
├── Workspace
└── StatusLabel
```

Le module `Sidebar` appartient à la couche `widgets`.

Le module ne contient aucune logique métier.

---

## Fichiers concernés

```text
include/widgets/sidebar.h
src/widgets/sidebar.c

include/views/main_window.h
src/views/main_window.c
```

---

## Interface publique attendue

```c
Sidebar *sidebar_new(void);

GtkWidget *sidebar_get_widget(
    const Sidebar *sidebar
);

void sidebar_free(Sidebar *sidebar);
```

---

## Interface graphique attendue

```text
┌───────────────────────┬──────────────────────────────────────┐
│ Dossier d'enquête     │                                      │
│                       │                                      │
│                       │                                      │
│                       │          Zone de travail             │
│                       │                                      │
│                       │                                      │
├───────────────────────┴──────────────────────────────────────┤
│ Aucune enquête ouverte                                      │
└──────────────────────────────────────────────────────────────┘
```

La séparation entre le panneau gauche et la zone centrale doit pouvoir être déplacée horizontalement.

---

## Contraintes techniques

- utiliser GTK4 ;
- utiliser un `GtkPaned` horizontal dans `MainWindow` ;
- ne pas utiliser de variable globale ;
- conserver une structure `Sidebar` opaque ;
- respecter les conventions de nommage du projet ;
- compiler en C17 sans warning.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] Le panneau latéral apparaît à gauche.
- [ ] La zone de travail apparaît à droite.
- [ ] La séparation peut être déplacée avec la souris.
- [ ] Le panneau est créé par le module `Sidebar`.
- [ ] `MainWindow` ne construit pas directement le contenu interne du panneau.
- [ ] Aucun état global.
- [ ] Les fonctions publiques sont documentées avec Doxygen.
- [ ] La fermeture de l'application ne provoque aucune erreur GTK.

---

## Tests

- lancer l'application ;
- sélectionner un dossier ;
- déplacer la séparation ;
- agrandir et réduire la fenêtre ;
- fermer l'application ;
- vérifier l'absence de warning ou de message critique.

---

## Commit attendu

```text
feat(widget): create navigation sidebar
```
