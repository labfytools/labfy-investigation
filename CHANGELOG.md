## [0.1.0-dev]

### Added

- Référentiel persistant et normalisé des types de relations, avec codes
  système stables, types personnalisés, renommage et fusion transactionnelle.
- Sélecteur canonique dans les formulaires de création et de modification des
  relations.
- Glisser-déposer confirmé des extractions texte vers le graphe, avec
  persistance de leur association à une entité.
- Ouverture de l’aperçu des pièces jointes et preuves depuis les fiches
  d’entités.
- Initialisation du cycle de vie de l'application.
- Création du module `Application`.
- Intégration de GTK4.
- Affichage de la première fenêtre.
- Module `MainWindow`.
- Séparation de la fenêtre principale du module `Application`.
- Première architecture de la fenêtre principale.

### Fixed

- Conservation du cadrage du graphe pendant les rechargements.
- Persistance SQLite du zoom et de la position du graphe entre deux sessions.
- Réduction de la fenêtre d’intégration des extractions pour maintenir les
  actions accessibles sur les petits écrans.
