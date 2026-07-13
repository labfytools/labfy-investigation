# Ticket #004

## Titre

Créer la fenêtre principale (`MainWindow`).

---

## Objectif

Créer la fenêtre principale de Labfy Investigation.

Cette fenêtre deviendra le point central de l'interface utilisateur.

À ce stade, elle ne contient qu'une structure vide destinée à accueillir les futurs composants.

---

## Responsabilités

Le module `MainWindow` doit :

- créer la fenêtre principale ;
- définir son titre ;
- définir sa taille par défaut ;
- afficher une zone centrale vide ;
- afficher une barre d'état.

---

## Hors périmètre

Ce ticket ne doit pas :

- afficher l'arborescence des fichiers ;
- ouvrir une enquête ;
- afficher les preuves ;
- communiquer avec SQLite ;
- gérer les menus.

---

## Architecture

Le module appartient à la couche **Views**.

Il est responsable uniquement de l'interface graphique.

Aucune logique métier ne doit être présente dans ce module.

---

## Dépendances

- GTK4

Aucune dépendance vers SQLite.

---

## Fichiers concernés

```text
include/views/main_window.h
src/views/main_window.c
```

---

## Interface attendue

```c
MainWindow *main_window_new(GtkApplication *application);

GtkWindow *main_window_get_window(
    const MainWindow *main_window
);

void main_window_present(MainWindow *main_window);

void main_window_free(MainWindow *main_window);
```

---

## Interface graphique attendue

```text
┌───────────────────────────────────────────────────────────────┐
│ Labfy Investigation                                          │
├───────────────────────────────────────────────────────────────┤
│                                                               │
│                                                               │
│                       Zone de travail                         │
│                                                               │
│                                                               │
├───────────────────────────────────────────────────────────────┤
│ Aucune enquête ouverte                                       │
└───────────────────────────────────────────────────────────────┘
```

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] Une fenêtre s'ouvre correctement.
- [ ] La fenêtre est créée par le module `MainWindow`.
- [ ] `Application` n'appelle plus directement `gtk_application_window_new()`.
- [ ] Aucun état global.
- [ ] Documentation Doxygen.
- [ ] Gestion mémoire correcte.

---

## Tests

- Lancement de l'application.
- Fermeture de la fenêtre.
- Vérification de l'absence de fuite mémoire.

---

## Commit attendu

```text
feat(view): create main window
```
