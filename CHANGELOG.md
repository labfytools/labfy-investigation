## [0.1.0-dev] — en préparation, non publiée

### Pivots locaux J7

- Index C typé des observations persistées, rapprochements explicables
  e-mail/domaine/IP, connexions locales bornées et corpus Web SPECIMEN
  multi-preuves.
- Annulation active propagée jusqu’au groupe d’outil, annulation d’attente sans
  interruption du worker actif et temporaires d’export concurrents uniques.

### Jobs locaux J5

- Ajout du JobStore V1 séparé, du worker mono-propriétaire, des contrôles CLI,
  de la reprise après publication et de l’export Web en lecture seule.
- Durcissement du runner et du replay ExifTool typé, y compris la vérification
  d’un résultat historique lorsque l’outil courant est indisponible.

### Poste Web local J6

- Ajout d’une session loopback protégée, d’un bridge JSON vers le service C,
  des actions EML/ExifTool depuis le graphe et des contrôles de file.
- Actualisation automatique des jobs et résultats persistés, avec conservation
  du contexte graphique et tests Firefox de bout en bout.

### Foundation

- Repositionnement canonique de Labfy comme environnement local-first
  d'investigation numérique centré sur le graphe interactif.
- Adoption de « THE GRAPH IS THE INVESTIGATION » comme principe UX et métier :
  Identity, Infrastructure, Finance, Timeline, Geo et Evidence deviennent des
  projections d'un même réseau logique.
- Choix du Web comme future interface principale, sans sélection prématurée du
  framework, du protocole d'événements ou du serveur HTTP local ; GTK reste
  l'interface fonctionnelle existante pendant la transition.
- Architecture cible du Toolkit Registry, des adapters, du Job Engine, du
  Pivot Engine, du Correlation Engine, du planner et des contrôles de scope.
- Séparation canonique des entités, artefacts, preuves, observations, claims,
  événements, transactions et hypothèses dans une projection graphique
  unifiée.
- Définition d'une autonomie progressive, de budgets multidimensionnels et
  d'une confiance qualitative explicable.
- Adoption de Catppuccin Mocha avec Lavender comme direction visuelle de la
  future interface Web et création d'une bannière dédiée au README.
- Refonte du README et remplacement de la roadmap par une séquence V2 ordonnée,
  sans annoncer les fonctionnalités cibles comme déjà disponibles.
- Ajout de l'index documentaire, du contrat conceptuel du graphe, des décisions
  Web/graph-first/polyglotte, du registre Toolkit, du modèle de sécurité, du
  design system et du backlog local préparé.
- Ajout d'une source SVG éditable pour la bannière et structuration de la
  roadmap en jalons J0 à J9 avec portes de validation et non-objectifs.

### Added

- Premier lot J3 expérimental en lecture seule : ouverture SQLite V20 sans
  création/migration, projection C unifiée des entités, relations, preuves,
  observations et exécutions OSINT, snapshot JSON v2 borné/atomique et mode de
  démonstration Web alimenté par une fixture créée avec les DAO de production.
- Extension J3 EML synthétique : analyse native réellement exécutée, dérivé
  versionné, extraction et observations proposées publiés atomiquement en V20,
  replay idempotent vérifié après réouverture et projection Web snapshot v3.
- Lot J4 local : registre de capabilities consommé, runner POSIX borné,
  adapters EML/ExifTool, publication idempotente V20 et démonstration Web sur
  fixtures SPECIMEN avec ExifTool réel.

- Assistant de création de personne en sept étapes séparant la révision OCR,
  la projection facultative et les relations factuelles avant confirmation.
- Projection contrôlée des champs OCR confirmés vers les attributs structurés
  d’une personne, avec provenance append-only, conflits explicites et rollback.
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

- Schéma de création directe aligné sur la V19, avec champs structurés,
  provenance des projections et garde SQLite identique à la migration V19.
- Migration V20 pour l’évaluation humaine append-only de l’usage d’identité,
  distincte de l’authenticité et sans automatisme OCR.
- Consultation V20 sur la fiche preuve et test GTK des relations et champs
  structurés affichés sur la fiche Person.
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
# Changements locaux non publiés — J8

- Ajout du planner C explicable, des plans/budgets persistants JobStore V3 et de
  l'admission Web atomique pour le corpus synthétique local.
- Fiabilisation J7 des révisions, plafonds, références, transactions de lecture
  et normalisation prudente.
- Finalisation JobStore V3 : budgets opposables au claim, temps monotone,
  réserve conservée après crash, états de plan exportés et scénario Firefox J8.

# Changements locaux non publiés — J9

## Espace local et import navigateur (non publié)

- création Web explicite d’une enquête V20 et d’un JobStore V3 réellement vides ;
- réception privée EML/PNG/JPEG, limites serveur, contrôle de signature et
  confirmation idempotente par UUID réservé ;
- raccord aux projections, planner, analyses et rapports existants ;
- matrice Python/C et scénario Firefox canonique depuis zéro.
- fermeture d’intégrité de l’import local : création non destructive, intention
  et reçus structurés, rejeu revalidé sur l’original V20, états terminaux
  immuables, réservations concurrentes atomiques et timeout de corps opposable ;
- durcissement P01–P06 du cycle de vie, des réservations, du rejeu et du
  framing HTTP ; refus des corps tronqués ou ambigus sans réouverture d’un état
  terminal ;
- aperçu Web EML/PNG/JPEG produit par le bridge C après intégrité et bornes,
  sans servir l’original ni son chemin ; l’identité de cache ne crée aucun cache
  disque persistant ;
- revue transactionnelle d’observations avec journal V20 non signé, corrections
  distinctes et rejeu strict ; la confirmation décrit la revue d’une
  observation, jamais une identité ;
- promotion volontaire par création ou rattachement d’un indicateur e-mail,
  domaine ou IP, et retrait non destructif préservant observation, entités et
  rattachements indépendants ;
- republication réessayable du graphe, des corrélations et du planner après
  import ou revue, sans relancer d’analyse, nouveau claim ni consommation de
  budget.

- Ajout des projections Web Preuves, Chronologie et Infrastructure locale sur
  le graphe cœur, avec dates persistées et prudence temporelle.
- Ajout de la sélection explicite, de la prévisualisation révisionnée et du
  dossier hors ligne HTML/JSON/PDF avec manifeste, hashes et vérificateur.
- Ajout des limites, de la minimisation, de l'idempotence et des reprises après
  interruption, ainsi que des tests C, API, Python et Firefox correspondants.
- Renforcement de l’intégrité du rapport : PDF Cairo/Pango Unicode fidèle,
  sections effectives, fermeture de provenance orientée, refus de troncature,
  vérificateur hostile strict et réconciliation durable après interruption.

# Changements locaux non publiés — poste Web UX V1

- Réorganisation graph-first en barre compacte, projections, inspecteur à
  onglets et tiroir Tâches/Actions/Plans/Rapports responsive.
- Ajout d’actions ancrées près des nœuds, avec clic droit, `Maj+F10`, fermeture
  `Échap`, alternative visible et motifs issus du registre de capabilities.
- Conservation du brouillon de rapport pendant une actualisation, validation
  Firefox des cinq téléchargements réels et ouverture du HTML reçu hors ligne.
- Prise en charge de dates ISO 8601 à la minute avec `Z` ou offset explicite,
  sans promotion artificielle de leur précision à la seconde.
- Clôture sur graphe alimenté : classification par `object_kind`, disposition
  bornée par étages, navigation cohérente et provenance interactive.
- Application des snapshots modifiés à UUID constants, aperçu de rapport
  invalidé dès l’édition et tiroir libérant réellement le graphe.
- Scénario Firefox lançant six analyses depuis l’UI puis validant le snapshot C
  observé à 62 nœuds/100 arêtes, sans relation ajoutée côté JavaScript.
