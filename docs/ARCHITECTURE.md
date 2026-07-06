# Architecture

Version : 1.0

Dernière mise à jour : 2026-07-06

Auteur : fy59

---

# Objectif

Ce document décrit l'architecture logicielle de **Labfy Investigation**.

Son objectif est de :

- définir les responsabilités de chaque composant ;
- faciliter la maintenance ;
- garantir une architecture cohérente ;
- limiter les dépendances entre les modules.

---

# Philosophie

Labfy Investigation repose sur une architecture modulaire.

Chaque composant possède une responsabilité clairement définie.

Aucun module ne doit effectuer plusieurs tâches sans justification.

---

# Architecture générale

Le projet suit une architecture inspirée du modèle MVC (Model - View - Controller).

```
                 Utilisateur
                      │
                      ▼
            +------------------+
            |    Vue (GTK4)    |
            +------------------+
                      │
                      ▼
            +------------------+
            |   Contrôleur     |
            +------------------+
                      │
                      ▼
            +------------------+
            |       DAO        |
            +------------------+
                      │
                      ▼
            +------------------+
            |     SQLite       |
            +------------------+
```

Chaque couche communique uniquement avec la couche immédiatement inférieure.

---

# Arborescence

```
labfy-investigation/

├── include/
├── src/
│
├── docs/
├── database/
├── tests/
├── tools/
├── resources/
│
├── Makefile
├── README.md
├── LICENSE
├── CHANGELOG.md
└── CONTRIBUTING.md
```

---

# Description des dossiers

## include/

Contient l'ensemble des interfaces publiques.

Aucun code métier ne doit être présent dans les fichiers d'en-tête.

---

## src/

Contient l'implémentation du logiciel.

Il est organisé en plusieurs modules :

```
src/

core/
dao/
models/
controllers/
views/
widgets/
utils/
```

---

## database/

Contient :

- le schéma SQLite ;
- les scripts d'initialisation ;
- les éventuelles migrations.

---

## docs/

Documentation technique.

---

## tests/

Tests unitaires et fonctionnels.

---

## resources/

Toutes les ressources utilisées par l'application :

- icônes ;
- feuilles CSS ;
- fichiers GtkBuilder (.ui).

---

# Description des modules

## Core

Le module Core initialise l'application.

Il gère :

- le cycle de vie du programme ;
- la configuration ;
- l'ouverture de l'enquête ;
- la fermeture propre.

---

## Models

Les modèles représentent les objets métiers.

Exemples :

- Preuve
- Entite
- Personne
- Recherche
- Source

Ils ne connaissent ni GTK ni SQLite.

---

## DAO

Les DAO sont responsables de l'accès aux données.

Ils sont les seuls autorisés à communiquer avec SQLite.

Ils réalisent :

- INSERT
- UPDATE
- DELETE
- SELECT

Aucune requête SQL ne doit apparaître ailleurs.

---

## Controllers

Les contrôleurs assurent la logique applicative.

Ils reçoivent les événements provenant de l'interface graphique.

Ils utilisent les DAO.

Ils mettent à jour les vues.

---

## Views

Les vues représentent l'interface utilisateur.

Elles ne contiennent aucun code métier.

Une vue affiche uniquement des informations.

---

## Widgets

Les widgets sont des composants GTK réutilisables.

Par exemple :

- Sidebar
- Toolbar
- Statusbar
- PropertyPanel

---

## Utils

Fonctions utilitaires :

- SHA-256
- Date
- Fichiers
- Journalisation

Les utilitaires ne doivent jamais dépendre du reste du projet.

---

# Flux de données

Lorsqu'un utilisateur ajoute une preuve :

```
Utilisateur

↓

Vue GTK

↓

Controller

↓

DAO

↓

SQLite

↓

DAO

↓

Controller

↓

Vue GTK
```

Le flux est toujours identique.

---

# Dépendances

Les dépendances suivent la règle suivante :

```
Views
 ↓
Controllers
 ↓
DAO
 ↓
SQLite
```

Les dépendances inverses sont interdites.

Par exemple :

Le DAO ne doit jamais appeler une vue.

---

# Gestion de la mémoire

Chaque module est responsable des ressources qu'il alloue.

Toute allocation possède une fonction de libération correspondante.

---

# Gestion des erreurs

Les erreurs remontent toujours vers le contrôleur.

L'interface graphique est responsable de leur affichage.

---

# Évolutivité

L'ajout d'un nouveau type d'entité ou d'une nouvelle fonctionnalité doit nécessiter le moins de modifications possibles.

L'architecture doit favoriser l'extension plutôt que la modification du code existant.

---

# Principes

L'architecture repose sur les principes suivants :

- responsabilité unique ;
- faible couplage ;
- forte cohésion ;
- séparation des responsabilités ;
- simplicité ;
- lisibilité ;
- maintenabilité.

---

# Conclusion

Toute nouvelle fonctionnalité doit s'intégrer dans cette architecture.

Si une évolution nécessite de contourner ces règles, la décision doit être documentée et justifiée.
