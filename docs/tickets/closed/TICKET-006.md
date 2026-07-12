# Ticket #006

## Titre

Créer le module `Workspace`.

---

## Objectif

Créer un composant graphique représentant la zone principale de travail de Labfy Investigation.

Le module doit être indépendant du reste de l'interface afin de pouvoir évoluer sans modifier `MainWindow`.

---

## Responsabilités

Le module `Workspace` doit :

- créer la zone centrale de l'application ;
- afficher une page d'accueil lorsqu'aucune enquête n'est ouverte ;
- fournir un widget racine réutilisable ;
- gérer uniquement son interface graphique.

---

## Hors périmètre

Ce ticket ne doit pas :

- afficher les preuves ;
- afficher les entités ;
- afficher un PDF ;
- afficher une image ;
- communiquer avec SQLite ;
- charger une enquête.

---

## Architecture

```text
MainWindow
├── Sidebar
├── Workspace
└── StatusBar
```

Le module appartient à la couche **Widgets**.

Aucune logique métier.

---

## Fichiers concernés

```text
include/widgets/workspace.h
src/widgets/workspace.c
```

---

## Interface publique

```c
Workspace *workspace_new(void);

GtkWidget *workspace_get_widget(
    const Workspace *workspace
);

void workspace_free(
    Workspace *workspace
);
```

---

## Interface graphique attendue

```text
+-------------------------------------------------------------+
|                                                             |
|                                                             |
|                  Labfy Investigation                        |
|                                                             |
|             Aucune enquête ouverte                          |
|                                                             |
|         Sélectionnez ou créez une enquête                   |
|                                                             |
|                                                             |
+-------------------------------------------------------------+
```

---

## Évolutions prévues

Cette zone accueillera plus tard :

- visualiseur d'image ;
- lecteur PDF ;
- chronologie ;
- fiche personne ;
- fiche IBAN ;
- fiche email ;
- fiche téléphone ;
- rapports ;
- cartes ;
- résultats OSINT.

Le module doit donc rester générique.

---

## Contraintes

- GTK4 uniquement ;
- structure opaque ;
- aucune variable globale ;
- documentation Doxygen ;
- C17 ;
- compilation sans warning.

---

## Critères d'acceptation

- [ ] Le projet compile.
- [ ] Le Workspace apparaît dans MainWindow.
- [ ] Le texte d'accueil est centré.
- [ ] Sidebar et Workspace sont totalement indépendants.
- [ ] Aucun warning.
- [ ] Aucun Gtk-CRITICAL.

---

## Tests

- lancement ;
- redimensionnement de la fenêtre ;
- fermeture ;
- vérification visuelle.

---

## Commit attendu

```text
feat(widget): create workspace
```
