# Architecture

Version : 2.0

Dernière mise à jour : 2026-07-14

Auteur : fy59

---

# Objectif

Ce document décrit l'architecture logicielle de **Labfy Investigation**.

Il constitue le document de référence du projet.

Son objectif est de :

- définir les responsabilités de chaque module ;
- garantir une architecture cohérente ;
- limiter le couplage entre les composants ;
- faciliter les évolutions futures ;
- permettre la réalisation de tests unitaires indépendants de l'interface graphique.

---

# Vision

Labfy Investigation est un logiciel libre d'investigation numérique,
développé en **C17** avec **GTK4**.

Le projet a pour objectif de fournir un environnement professionnel
permettant de gérer une enquête numérique de bout en bout :

- création d'une enquête ;
- collecte des preuves ;
- organisation des informations ;
- analyse ;
- génération de rapports.

L'application est conçue pour être utilisée aussi bien par :

- des particuliers ;
- des journalistes ;
- des analystes OSINT ;
- des experts judiciaires ;
- des forces de l'ordre.

---

# Principes fondamentaux

## Une enquête = un dossier autonome

Chaque enquête contient tout ce qui est nécessaire à son fonctionnement.

```
MonEnquete/

├── 00_BaseDeDonnees/
│   └── Enquete.sqlite
│
├── 01_Preuves_Originales/
├── 02_Preuves_Traitees/
├── 03_Chronologie/
├── 04_Entites/
├── 05_Rapports/
│
└── ...
```

Une enquête peut être :

- copiée ;
- déplacée ;
- synchronisée ;
- sauvegardée ;
- archivée ;
- transmise.

Aucune dépendance externe ne doit être nécessaire.

---

## Les preuves originales sont immuables

Les preuves originales ne doivent jamais être modifiées.

Toute opération produisant :

- une annotation ;
- une conversion ;
- une extraction ;
- une analyse ;

doit produire un nouveau fichier dans un espace dédié.

---

## Le Core est indépendant de GTK

La logique métier ne dépend jamais de l'interface graphique.

Le Core doit pouvoir être testé sans lancer GTK.

---

## Les Widgets ne connaissent pas le métier

Les widgets affichent uniquement les informations fournies.

Ils ne manipulent jamais :

- SQLite ;
- le système de fichiers ;
- les preuves.

---

## Développement incrémental

Chaque ticket doit :

- compiler sans warning ;
- être documenté ;
- être testé ;
- ne pas casser les fonctionnalités existantes.

---

# Architecture générale

```
                    Utilisateur
                          │
                          ▼
                 +-----------------+
                 |      GTK4       |
                 | Views / Widgets |
                 +-----------------+
                          │
                          ▼
                 +-----------------+
                 |   Application   |
                 +-----------------+
                          │
                          ▼
              +---------------------------+
              | InvestigationProject      |
              +---------------------------+
                   │                 │
                   ▼                 ▼
             FileSystem         Database
                   │                 │
                   └────────┬────────┘
                            ▼
                     Investigation
                            │
                            ▼
                         Models
```

Cette architecture permet une séparation claire entre :

- l'interface graphique ;
- la logique métier ;
- le stockage.

---

# Organisation du projet

```
labfy-investigation/

include/
src/

database/
docs/
resources/
tests/
tools/

README.md
LICENSE
CHANGELOG.md
CONTRIBUTING.md
Makefile
```

---

# Organisation du code

```
src/

core/
database/
models/
views/
widgets/
utils/
```

---

# Description des modules

## Core

Le Core pilote l'application.

Il ne dépend jamais de GTK.

Il gère :

- le cycle de vie de l'application ;
- l'ouverture d'une enquête ;
- la fermeture d'une enquête ;
- les interactions entre les modules.

---

## InvestigationProject

InvestigationProject est le point d'entrée métier.

Il est responsable de :

- créer une enquête ;
- ouvrir une enquête ;
- fermer une enquête ;
- valider une enquête ;
- initialiser SQLite ;
- gérer le système de fichiers.

Toute manipulation d'une enquête passe obligatoirement par ce module.

---

## Database

Le module Database est responsable du stockage.

Il gère :

- la connexion SQLite ;
- le schéma ;
- les migrations ;
- les transactions.

Aucune requête SQL ne doit apparaître ailleurs.

---

## Models

Les modèles représentent les objets métier.

Par exemple :

- InvestigationNode
- Preuve
- Entité
- Relation
- Chronologie

Ils ne connaissent ni GTK ni SQLite.

---

## Views

Les vues représentent les fenêtres de l'application.

Elles ne contiennent aucune logique métier.

---

## Widgets

Les widgets sont des composants GTK réutilisables.

Par exemple :

- Sidebar
- Workspace
- InvestigationTreeView
- Toolbar
- Statusbar

---

## Utils

Fonctions génériques.

Exemples :

- SHA-256
- Dates
- Gestion des fichiers
- Journalisation

Les utilitaires ne doivent dépendre d'aucun module métier.

---

# Flux de données

Lorsqu'un utilisateur ouvre une enquête :

```
Utilisateur

↓

GTK

↓

Application

↓

InvestigationProject

↓

FileSystem

↓

Database

↓

Investigation

↓

GUI
```

Le flux est toujours unidirectionnel.

---

# Dépendances

Les dépendances suivent toujours cette règle :

```
Widgets
    │
    ▼
Views
    │
    ▼
Application
    │
    ▼
InvestigationProject
    │
 ┌──┴───────┐
 ▼          ▼
Database  FileSystem
    │
    ▼
Models
```

Les dépendances inverses sont interdites.

---

# Gestion de la mémoire

Chaque allocation possède une fonction de libération correspondante.

Les responsabilités de libération sont clairement définies.

Aucun module ne libère une ressource qu'il n'a pas créée.

---

# Gestion des erreurs

Les erreurs remontent toujours jusqu'à Application.

L'interface graphique est uniquement responsable de leur affichage.

---

# Tests

Chaque nouveau module doit pouvoir être testé indépendamment.

Les tests unitaires ne doivent jamais dépendre de GTK.

---

# Évolutivité

L'architecture doit permettre l'ajout de nouveaux modules sans modifier les composants existants.

Les nouvelles fonctionnalités doivent privilégier l'extension plutôt que la modification.

---

# Objectifs à long terme

Le projet doit permettre :

- la gestion complète d'une enquête numérique ;
- la conservation de la chaîne de preuve ;
- l'intégration de modules OSINT ;
- l'analyse de fichiers ;
- la génération de rapports professionnels ;
- le packaging multiplateforme.

Le logiciel doit rester :

- libre ;
- documenté ;
- modulaire ;
- testable ;
- maintenable.

---

# Conclusion

Toute nouvelle fonctionnalité doit respecter cette architecture.

En cas de dérogation, celle-ci devra être documentée et justifiée.

L'objectif est de garantir la stabilité du projet tout en facilitant son évolution sur le long terme.
