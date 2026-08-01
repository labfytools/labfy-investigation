## [0.1.0-dev]

### Added

- Saisie et consultation, depuis la fiche preuve, d’un historique immuable
  d’appréciations humaines d’authenticité documentaire avec justification et
  `OcrRun` facultatif appartenant à la preuve, sans verdict automatique.
- OCR d’identité contrôlé et révisable dans la création d’une personne,
  l’import normal et la fiche de preuve, avec texte brut immuable,
  transcription corrigée persistante, `manual_override`, `manual_entry` et
  notes documentaires factuelles.
- Consultation complète des données OCR persistées, sélection explicite d’un
  `OcrRun`, révision sans relance de Tesseract et création d’un nouveau run
  lors d’une nouvelle analyse.
- Aperçu partagé avec zoom, ajustement, défilement bidirectionnel, navigation
  PDF multipage, compteur de pages et provenance OCR synchronisée.
- Politique commune des dialogues GTK métiers complexes : parent transitoire,
  géométrie responsive, répartition initiale 2/3–1/3, formulaire défilable et
  barre d’actions fixe.
- Préparation du pivot e-mail : pipeline EML asynchrone, extraction MIME
  sécurisée, propositions bancaires IBAN/BIC et vocabulaire contrôlé.

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

- Rafraîchissement immédiat de Workspace après import ou révision OCR et
  persistance des données sur l’UUID définitif de la preuve, y compris après
  fermeture et réouverture de SQLite.
- Fermeture sûre du dialogue après enregistrement d’une révision OCR, avec
  protection contre le double clic, maintien ouvert en cas d’échec et absence
  de relance de Tesseract.
- Conservation du cadrage du graphe pendant les rechargements.
- Persistance SQLite du zoom et de la position du graphe entre deux sessions.
- Réduction de la fenêtre d’intégration des extractions pour maintenir les
  actions accessibles sur les petits écrans.
