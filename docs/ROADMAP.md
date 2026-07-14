# Roadmap

# Vision

Labfy Investigation est un logiciel libre d'investigation numérique,
développé en C17 avec GTK4.

Le projet a pour objectif de fournir un environnement professionnel
permettant de gérer une enquête numérique de bout en bout, depuis la
collecte des preuves jusqu'à la génération d'un rapport.

Le logiciel est conçu pour être utilisé aussi bien par des particuliers
que par des professionnels (journalistes, enquêteurs, forces de l'ordre,
experts judiciaires, analystes OSINT).

---

# Principes d'architecture

Le développement repose sur plusieurs règles fondamentales.

## Une enquête = un dossier autonome

Chaque enquête contient l'ensemble des éléments nécessaires à son
fonctionnement.

```text
MonEnquete/
│
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
- sauvegardée ;
- synchronisée ;
- archivée ;
- transmise.

Sans dépendre d'une installation particulière.

---

## Séparation des responsabilités

Le Core ne dépend jamais de GTK.

La couche graphique ne contient aucune logique métier.

```text
GUI
        │
        ▼
Application
        │
        ▼
InvestigationProject
        │
 ┌──────┴────────────┐
 │                   │
 ▼                   ▼
FileSystem        Database
        │
        ▼
Investigation
```

---

## Préservation des preuves

Les preuves originales ne sont jamais modifiées.

Toute opération produisant une version annotée,
convertie ou analysée est enregistrée dans un
espace distinct.

---

## Développement incrémental

Chaque ticket doit :

- être autonome ;
- être testé ;
- compiler sans warning ;
- ne pas introduire de régression.

Aucun ticket ne doit casser les fonctionnalités existantes.

---

# Phases du projet

## Phase 1 — Infrastructure ✅

### Architecture

- [x] Structure du projet
- [x] Architecture C17
- [x] Organisation des modules
- [x] Documentation Doxygen
- [x] Makefile
- [x] Tests unitaires

### Core

- [x] Investigation
- [x] InvestigationNode
- [x] InvestigationTreeBuilder
- [x] InvestigationTreeModel

### Interface GTK4

- [x] MainWindow
- [x] Sidebar
- [x] Workspace
- [x] GtkStack
- [x] InvestigationTreeView
- [x] Sélection des nœuds
- [x] Affichage des informations
- [x] Affichage du chemin complet
- [x] Icônes selon le type de fichier

---

# Phase 2 — Gestion des enquêtes

## TICKET-020

- [ ] Création d'une nouvelle enquête

### Objectifs

- création de l'arborescence
- création de `00_BaseDeDonnees`
- création de `Enquete.sqlite`

---

## TICKET-021

- [ ] Validation d'une enquête existante

### Objectifs

- vérifier l'arborescence
- vérifier la présence de la base
- détecter une enquête invalide
- import d'une enquête

---

## TICKET-022

- [ ] Initialisation de SQLite

### Objectifs

- création du schéma
- table metadata
- version du schéma
- UUID de l'enquête

---

## TICKET-023

- [ ] InvestigationProject

### Objectifs

- ouverture d'une enquête
- fermeture
- validation
- création

---

# Phase 3 — Consultation

- [ ] Aperçu des fichiers texte
- [ ] Aperçu des images
- [ ] Aperçu PDF
- [ ] Aperçu vidéo
- [ ] Aperçu audio

---

# Phase 4 — Base de données

- [ ] Gestion des preuves
- [ ] Gestion des entités
- [ ] Gestion des relations
- [ ] Chronologie
- [ ] Tags
- [ ] Notes

---

# Phase 5 — Analyse

- [ ] Calcul des hash
- [ ] EXIF
- [ ] OCR
- [ ] Recherche plein texte
- [ ] Analyse des métadonnées
- [ ] Import massif

---

# Phase 6 — OSINT

- [ ] Whois
- [ ] DNS
- [ ] Sous-domaines
- [ ] HTTP
- [ ] Certificats TLS
- [ ] Réseaux sociaux
- [ ] Emails
- [ ] Téléphones

---

# Phase 7 — Rapports

- [ ] Rapport HTML
- [ ] Rapport PDF
- [ ] Export ZIP
- [ ] Export dossier complet

---

# Phase 8 — Qualité

- [ ] Traductions
- [ ] Thèmes
- [ ] Tests d'intégration
- [ ] Packaging
- [ ] Installateur Linux
- [ ] Documentation utilisateur

# Objectifs à long terme

Le projet doit permettre :

- la gestion complète d'une enquête numérique ;
- la conservation de la chaîne de preuve ;
- l'intégration de modules OSINT ;
- l'analyse de fichiers et de métadonnées ;
- la génération de rapports exploitables ;
- une architecture modulaire facilitant les évolutions futures.

Le logiciel doit rester :

- libre ;
- documenté ;
- portable ;
- testable ;
- maintenable.
