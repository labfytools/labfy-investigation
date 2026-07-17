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

# Feuille de route repensée — Labfy Investigation

## Point de départ

Le ticket **#031.1 — Fermeture propre de l’application** termine le premier socle GTK.

À ce stade, Labfy Investigation sait :

- créer une enquête ;
- ouvrir une enquête existante ;
- remplacer une session active ;
- afficher son arborescence ;
- fermer proprement l’application ;
- ouvrir et initialiser la base SQLite ;
- charger l’identité persistante de l’enquête ;
- gérer les transactions et les erreurs de base.

La suite ne doit plus être pensée comme un simple gestionnaire de fichiers.

## Vision du logiciel

Labfy Investigation doit devenir un **poste de travail OSINT orienté enquête** capable de :

- conserver les preuves originales ;
- extraire et rechercher des métadonnées ;
- créer des entités ;
- relier les données entre elles ;
- afficher les relations sous forme de graphe ;
- construire une chronologie ;
- effectuer des recherches locales ;
- lancer des outils OSINT externes ;
- interroger des services Internet ;
- conserver les résultats bruts ;
- normaliser les résultats ;
- documenter la provenance de chaque information ;
- produire un rapport exploitable.

Architecture générale :

```text
Interface GTK
    ↓
Contrôleurs de l’application
    ↓
Services métier
    ├── preuves
    ├── entités
    ├── relations
    ├── recherches
    ├── tâches
    └── rapports
    ↓
Adaptateurs
    ├── SQLite
    ├── système de fichiers
    ├── outils CLI
    ├── bibliothèques
    └── API Internet
```

---

# Principes non négociables

## SQLite reste la source de vérité

Le graphe, les tableaux, la chronologie et les résultats de recherche sont des vues différentes des mêmes données.

## Résultat brut et résultat normalisé sont séparés

Chaque outil doit produire :

```text
Sortie brute conservée et horodatée
        +
Données normalisées utilisables dans Labfy
```

## Toute information doit avoir une provenance

Une donnée exploitable doit pouvoir répondre à ces questions :

```text
Qu’a-t-on trouvé ?
Quand ?
Avec quel outil ?
Avec quelle version ?
À partir de quelle requête ?
Quelle était la réponse brute ?
Quelle preuve ou source justifie l’interprétation ?
```

## Les outils externes sont optionnels

Une dépendance absente ne doit pas empêcher Labfy de démarrer.

Chaque capacité peut être :

```text
Disponible
Absente
Trop ancienne
Non configurée
Désactivée
En erreur
```

## L’interface ne doit jamais être bloquée

Les opérations longues doivent fonctionner en arrière-plan :

- calcul d’empreinte ;
- copie de fichiers ;
- extraction de métadonnées ;
- requêtes réseau ;
- analyse de résultats ;
- génération de rapports.

## Aucune commande shell construite avec une chaîne

Les outils externes doivent être lancés avec `GSubprocess` et une liste d’arguments.

## Les données déduites restent distinctes des données observées

Labfy doit distinguer :

```text
Observation directe
Résultat produit par un outil
Donnée importée
Déduction de l’enquêteur
Hypothèse
```

---

# Phase 1 — Consolider le noyau d’application

## Ticket #032 — Factoriser le chargement d’une enquête

Créer un service interne unique qui :

1. ouvre une `InvestigationSession` ;
2. récupère le projet ;
3. construit l’arborescence ;
4. installe la session dans `Application` ;
5. conserve l’ancienne session en cas d’échec.

Ce flux sera réutilisé par :

- création d’une enquête ;
- ouverture manuelle ;
- enquêtes récentes ;
- arguments de ligne de commande ;
- restauration de session.

## Ticket #033 — Afficher les erreurs dans GTK

Créer un module commun d’affichage :

- erreur ;
- avertissement ;
- confirmation ;
- information.

Les erreurs techniques restent journalisées avec GLib, mais l’utilisateur doit recevoir un message graphique compréhensible.

## Ticket #034 — Gestionnaire de tâches asynchrones

Créer un modèle de tâche capable de gérer :

- état ;
- progression ;
- annulation ;
- résultat ;
- erreur ;
- date de début ;
- date de fin.

États recommandés :

```text
En attente
En cours
Terminée
Échouée
Annulée
```

L’interface GTK doit rester fluide pendant l’exécution.

## Ticket #035 — File de tâches et panneau d’activité

Ajouter une file de tâches et une vue GTK permettant de suivre :

- imports ;
- calculs d’empreintes ;
- recherches OSINT ;
- extractions de métadonnées ;
- exports.

## Ticket #036 — Configuration de l’application

Créer une configuration persistante pour :

- chemins d’outils ;
- délais d’exécution ;
- activation des modules ;
- préférences d’interface ;
- paramètres réseau ;
- comportement des imports.

Les secrets et clés API ne doivent pas être stockés en clair dans la base de l’enquête.

---

# Phase 2 — Système d’outils externes

## Ticket #037 — Registre des capacités et dépendances

Créer un registre central capable de détecter :

- présence d’un exécutable ;
- chemin réel ;
- version ;
- compatibilité minimale ;
- statut ;
- message d’erreur.

Premiers outils candidats :

```text
dig
host
whois
curl
openssl
file
exiftool
ffprobe
strings
```

## Ticket #038 — Interface commune des adaptateurs OSINT

Définir les concepts opaques :

```c
OsintTool
OsintRequest
OsintExecution
OsintResult
```

Un adaptateur devra pouvoir :

- valider une requête ;
- vérifier sa disponibilité ;
- exécuter l’outil ;
- appliquer un délai maximal ;
- capturer `stdout` ;
- capturer `stderr` ;
- récupérer le code de retour ;
- analyser la sortie ;
- retourner un résultat normalisé.

## Ticket #039 — Exécuteur sécurisé `GSubprocess`

Créer un module générique qui lance un programme sans passer par un shell.

Il doit gérer :

- tableau d’arguments ;
- environnement contrôlé ;
- délai maximal ;
- annulation ;
- taille maximale des sorties ;
- code de retour ;
- erreurs de lancement.

## Ticket #040 — Conservation des exécutions brutes

Ajouter les tables et fichiers nécessaires pour conserver :

- outil ;
- version ;
- cible ;
- paramètres ;
- date ;
- durée ;
- sortie standard ;
- sortie d’erreur ;
- code de retour ;
- SHA-256 de la sortie brute ;
- statut d’analyse.

Exemple de stockage :

```text
02_Preuves_Traitees/
└── Resultats_Outils/
    └── <date>_<outil>_<identifiant>.json
```

## Ticket #041 — Vue des dépendances et capacités

Créer une page GTK affichant :

```text
ExifTool             Disponible
dig                   Disponible
ffprobe               Absent
Recherche sociale     Non configurée
```

Une capacité absente doit expliquer comment l’activer sans bloquer l’application.

---

# Phase 3 — Preuves et fichiers

## Ticket #042 — Modèle opaque `EvidenceRecord`

Champs initiaux :

- identifiant ;
- nom original ;
- nom interne ;
- chemin relatif ;
- type ;
- taille ;
- SHA-256 ;
- date d’importation ;
- date de collecte déclarée ;
- source ;
- description ;
- statut d’intégrité.

## Ticket #043 — Schéma SQLite et DAO des preuves

Créer :

- table `evidence` ;
- table `evidence_types` ;
- index ;
- contraintes ;
- DAO d’insertion ;
- DAO de lecture ;
- DAO de mise à jour autorisée.

## Ticket #044 — Calcul SHA-256 par blocs

Créer un module indépendant capable de traiter les gros fichiers sans les charger entièrement en mémoire.

Ce ticket peut être synchronisé avec le cours de C.

## Ticket #045 — Copie sûre et contrôlée d’un fichier

Créer un service de copie qui :

- valide le fichier source ;
- refuse les chemins dangereux ;
- évite les collisions ;
- copie vers une destination temporaire ;
- synchronise la copie ;
- recalcule son empreinte ;
- renomme atomiquement le fichier final.

Ce ticket peut être synchronisé avec le cours Unix.

## Ticket #046 — Import transactionnel d’une preuve

Pipeline :

```text
Validation
→ SHA-256 source
→ Copie temporaire
→ SHA-256 destination
→ Écriture SQLite
→ Validation de transaction
→ Renommage final
```

En cas d’échec, aucune preuve partiellement importée ne doit rester active.

## Ticket #047 — Dialogue GTK d’import

Ajouter :

```text
Importer une preuve
```

Le formulaire doit permettre :

- choix du fichier ;
- type ;
- source ;
- date de collecte ;
- description ;
- confirmation.

## Ticket #048 — Liste et fiche détaillée des preuves

Afficher sous forme de tableau :

- nom ;
- type ;
- taille ;
- date ;
- source ;
- empreinte ;
- état d’intégrité.

Une fiche détaillée doit afficher toutes les informations disponibles.

## Ticket #049 — Vérification d’intégrité

Recalculer les empreintes et détecter :

- fichier absent ;
- taille modifiée ;
- empreinte modifiée ;
- chemin invalide ;
- doublon.

---

# Phase 4 — Métadonnées et analyse locale

## Ticket #050 — Premier adaptateur externe : ExifTool

Ce sera le premier adaptateur complet validant toute l’architecture :

```text
EvidenceRecord
→ tâche asynchrone
→ ExifTool
→ JSON brut
→ résultat stocké
→ métadonnées normalisées
```

## Ticket #051 — Modèle de métadonnées normalisées

Créer un modèle générique :

- clé ;
- valeur textuelle ;
- type de valeur ;
- namespace ;
- source ;
- outil ;
- confiance ;
- date d’extraction.

## Ticket #052 — Extracteurs spécialisés

Ajouter progressivement :

- `file` pour le type réel ;
- `ffprobe` pour audio et vidéo ;
- extraction PDF ;
- extraction de documents bureautiques ;
- analyse EML ;
- analyse d’archives.

## Ticket #053 — Recherche d’indicateurs dans les fichiers

Détecter dans les métadonnées et contenus extraits :

- emails ;
- domaines ;
- URL ;
- adresses IP ;
- téléphones ;
- pseudonymes ;
- noms d’utilisateurs ;
- coordonnées GPS.

Les résultats doivent être proposés à l’enquêteur avant création d’entités.

## Ticket #054 — Vue des métadonnées

Afficher :

- métadonnées brutes ;
- métadonnées normalisées ;
- filtres ;
- recherche ;
- provenance ;
- création d’entité depuis une valeur.

---

# Phase 5 — Entités, observations et relations

## Ticket #055 — Modèle opaque `EntityRecord`

Types initiaux :

```text
Personne
Pseudonyme
Adresse email
Téléphone
Domaine
Adresse IP
URL
Compte social
Organisation
IBAN
Cryptomonnaie
Document
Lieu
```

## Ticket #056 — Schéma et DAO des entités

Créer :

- `entity_types` ;
- `entities` ;
- `entity_attributes` ;
- contraintes ;
- index ;
- DAO.

## Ticket #057 — Modèle `ObservationRecord`

Une observation décrit une information découverte avec :

- valeur ;
- date ;
- source ;
- méthode ;
- outil ;
- confiance ;
- statut de validation ;
- notes.

## Ticket #058 — Interface de création et consultation des entités

Permettre :

- création manuelle ;
- modification ;
- recherche ;
- fusion contrôlée ;
- ajout d’attributs ;
- consultation des preuves associées.

## Ticket #059 — Modèle opaque `RelationRecord`

Une relation doit contenir :

- source ;
- cible ;
- type ;
- direction ;
- date ;
- confiance ;
- provenance ;
- preuve associée ;
- note ;
- statut de validation.

## Ticket #060 — Schéma et DAO des relations

Créer :

- `relation_types` ;
- `relations` ;
- contraintes ;
- index ;
- DAO.

## Ticket #061 — Création et validation des relations

Permettre :

- création manuelle ;
- proposition automatique ;
- acceptation ;
- rejet ;
- modification ;
- justification.

Aucune relation déduite automatiquement ne doit devenir définitive sans provenance.

---

# Phase 6 — Recherche locale et requêtes

## Ticket #062 — Index de recherche SQLite FTS5

Indexer :

- preuves ;
- descriptions ;
- notes ;
- entités ;
- attributs ;
- observations ;
- métadonnées ;
- résultats OSINT ;
- événements.

## Ticket #063 — Langage de filtres interne

Créer un service de requêtes combinant :

- texte ;
- type ;
- date ;
- source ;
- confiance ;
- outil ;
- statut ;
- relations.

Exemples :

```text
type:email source:exiftool
domain:example.org
confidence:<50
evidence:"capture écran"
```

## Ticket #064 — Interface de recherche globale

Ajouter :

- barre de recherche ;
- filtres ;
- tri ;
- regroupement ;
- résultats paginés ;
- ouverture de la fiche correspondante.

## Ticket #065 — Requêtes enregistrées

Permettre d’enregistrer et de rejouer une recherche.

## Ticket #066 — Recherche transversale et pivots

Depuis une valeur, proposer :

```text
Rechercher partout
Afficher les preuves liées
Afficher les entités liées
Afficher les relations
Lancer un enrichissement OSINT
```

---

# Phase 7 — Enrichissement OSINT réseau

## Ticket #067 — Adaptateur DNS

Fonctions initiales :

- A ;
- AAAA ;
- MX ;
- NS ;
- TXT ;
- CNAME ;
- SOA ;
- résolution inverse.

Stocker :

- requête ;
- serveur utilisé ;
- date ;
- réponse brute ;
- réponses normalisées ;
- erreurs.

## Ticket #068 — Adaptateur RDAP et WHOIS

Extraire :

- registraire ;
- dates ;
- serveurs de noms ;
- statuts ;
- contacts publics ;
- réseau IP ;
- ASN lorsque disponible.

## Ticket #069 — Adaptateur TLS

Analyser :

- certificat ;
- sujet ;
- émetteur ;
- SAN ;
- dates ;
- chaîne ;
- empreintes ;
- protocoles observés.

## Ticket #070 — Adaptateur HTTP

Collecter :

- statut ;
- redirections ;
- en-têtes ;
- titre ;
- type de contenu ;
- serveur déclaré ;
- empreinte de réponse ;
- liens principaux.

## Ticket #071 — Recherche de sous-domaines

Agrégation contrôlée de plusieurs sources :

- DNS ;
- certificats ;
- données passives disponibles ;
- outils externes facultatifs.

Les sources doivent rester identifiables séparément.

## Ticket #072 — Archives du Web

Ajouter un fournisseur permettant de rechercher :

- captures anciennes ;
- dates disponibles ;
- URL historiques ;
- changements visibles.

## Ticket #073 — Moteurs de recherche et recherche Web

Créer une interface fournisseur pouvant utiliser :

- API officielle ;
- service configuré ;
- ouverture assistée dans le navigateur ;
- import manuel de résultats.

Labfy ne doit pas contourner les protections des moteurs.

## Ticket #074 — Réseaux sociaux

Créer un cadre générique pour :

- comptes publics ;
- pseudonymes ;
- URL de profils ;
- publications publiques ;
- observations manuelles ;
- fournisseurs autorisés.

Chaque plateforme pourra avoir :

- un adaptateur API ;
- un adaptateur CLI ;
- un import manuel ;
- aucune automatisation si les règles l’interdisent.

## Ticket #075 — Cache, quotas et limitation de débit

Ajouter :

- cache ;
- date d’expiration ;
- quotas ;
- pauses ;
- reprises ;
- erreurs temporaires ;
- délais entre requêtes.

## Ticket #076 — Pivots OSINT

Depuis une entité, proposer les recherches compatibles :

```text
Domaine → DNS, RDAP, TLS, HTTP, archives
IP → reverse DNS, RDAP, ASN
Email → domaines, occurrences locales, fournisseurs configurés
Pseudonyme → moteurs, réseaux sociaux, dépôts publics
URL → HTTP, TLS, archives, métadonnées
```

## Ticket #077 — Révision des résultats avant intégration

Une recherche OSINT doit produire une liste de propositions :

- créer une entité ;
- compléter une entité ;
- créer une observation ;
- créer une relation ;
- ignorer.

L’enquêteur garde le contrôle.

---

# Phase 8 — Affichage analytique et graphe

## Ticket #078 — Service de projection graphique

Transformer les données SQLite en représentation graphique sans faire du graphe la source de vérité.

Types de nœuds :

- preuves ;
- entités ;
- événements ;
- résultats OSINT.

Types d’arêtes :

- relations validées ;
- relations proposées ;
- liens de provenance.

## Ticket #079 — Première vue graphique

Afficher un graphe simple avec :

- nœuds ;
- liens ;
- libellés ;
- sélection ;
- zoom ;
- déplacement.

Le premier moteur pourra s’appuyer sur Graphviz ou une bibliothèque compatible GTK.

## Ticket #080 — Fiche contextuelle du graphe

Cliquer sur un nœud ou une relation doit afficher :

- identité ;
- attributs ;
- source ;
- confiance ;
- preuve justificative ;
- actions possibles.

## Ticket #081 — Filtres du graphe

Filtrer par :

- type ;
- date ;
- source ;
- confiance ;
- statut ;
- profondeur ;
- enquêteur ;
- outil.

## Ticket #082 — Organisation et regroupement

Permettre :

- regroupement manuel ;
- regroupement par type ;
- regroupement par domaine ;
- regroupement par période ;
- masquage de branches ;
- expansion d’un voisinage.

## Ticket #083 — Vues graphiques enregistrées

Enregistrer :

- position des nœuds ;
- filtres ;
- regroupements ;
- annotations ;
- titre de la vue.

## Ticket #084 — Tableaux et statistiques

Créer des vues tabulaires et graphiques pour :

- types de preuves ;
- entités ;
- domaines ;
- outils utilisés ;
- volume de résultats ;
- dates ;
- intégrité ;
- relations.

---

# Phase 9 — Chronologie et notes

## Ticket #085 — Modèle d’événement

Champs :

- date et heure ;
- précision ;
- fuseau ;
- description ;
- source ;
- entités ;
- preuves ;
- confiance.

## Ticket #086 — Vue chronologique

Afficher :

- événements ;
- filtres ;
- regroupement ;
- preuves associées ;
- périodes sans date exacte.

## Ticket #087 — Notes d’enquête

Permettre :

- notes globales ;
- notes liées à une preuve ;
- notes liées à une entité ;
- notes liées à une relation ;
- notes liées à un événement.

## Ticket #088 — Hypothèses et pistes

Créer un espace distinct pour :

- hypothèses ;
- questions ouvertes ;
- pistes à vérifier ;
- statut ;
- priorité ;
- éléments favorables ;
- éléments contradictoires.

---

# Phase 10 — Traçabilité et reproductibilité

## Ticket #089 — Journal d’audit

Tracer les opérations importantes :

- création ;
- import ;
- modification ;
- suppression autorisée ;
- recherche OSINT ;
- création de relation ;
- validation ;
- export.

## Ticket #090 — Rejouer une recherche

Permettre de relancer une recherche avec :

- même outil ;
- même version si disponible ;
- mêmes paramètres ;
- comparaison des résultats.

## Ticket #091 — Manifeste de l’enquête

Générer un manifeste contenant :

- fichiers ;
- tailles ;
- empreintes ;
- versions d’outils ;
- base SQLite ;
- résultats bruts ;
- date de génération.

## Ticket #092 — Contrôle de cohérence

Détecter :

- référence orpheline ;
- fichier absent ;
- résultat brut absent ;
- empreinte invalide ;
- relation sans provenance ;
- entité dupliquée.

---

# Phase 11 — Rapports et exports

## Ticket #093 — Rapport Markdown

Générer :

- identité de l’enquête ;
- résumé ;
- méthodologie ;
- preuves ;
- entités ;
- relations ;
- chronologie ;
- résultats OSINT ;
- empreintes ;
- limites.

## Ticket #094 — Export PDF

Transformer le rapport en document PDF transmissible.

## Ticket #095 — Export du graphe

Exporter une vue :

- image ;
- SVG ;
- PDF ;
- annexe de rapport.

## Ticket #096 — Archive autonome

Créer une archive contenant :

- rapport ;
- base ;
- manifeste ;
- résultats bruts ;
- preuves sélectionnées ;
- graphe ;
- journal d’audit.

## Ticket #097 — Rédaction et anonymisation

Permettre de produire une copie avec :

- données masquées ;
- preuves exclues ;
- identifiants remplacés ;
- rapport adapté à la diffusion.

---

# Phase 12 — Installation et distribution Ubuntu

## Ticket #098 — Classification des dépendances

Classer :

```text
Obligatoires
Optionnelles
Recommandées
Fournies par une API
Indisponibles dans certains dépôts
```

## Ticket #099 — Script de compilation et installation

Créer un dossier autonome avec :

- détection de la distribution ;
- vérification des dépendances ;
- tentative d’installation ;
- compilation ;
- installation locale ;
- rapport clair des capacités indisponibles.

## Ticket #100 — Paquet Debian

Créer un paquet `.deb` contenant :

- binaire ;
- icône ;
- fichier `.desktop` ;
- licence ;
- schémas ;
- dépendances obligatoires ;
- recommandations optionnelles.

## Ticket #101 — Fonctionnement avec dépôts restreints

Prévoir :

- dépendances minimales ;
- modules facultatifs ;
- désactivation propre ;
- paquetage séparé si nécessaire ;
- documentation d’installation hors ligne.

## Ticket #102 — Tests Ubuntu

Tester :

- Ubuntu LTS ;
- Wayland ;
- X11 ;
- machine sans outils de développement ;
- environnement hors ligne ;
- dépôts restreints ;
- utilisateur sans droits administrateur.

---

# Jalons du projet

## Jalon A — Socle professionnel

Tickets :

```text
#032 à #041
```

Résultat :

- contrôleur propre ;
- erreurs GTK ;
- tâches asynchrones ;
- registre d’outils ;
- exécution externe sécurisée ;
- résultats bruts conservés.

## Jalon B — Gestion fiable des preuves

Tickets :

```text
#042 à #054
```

Résultat :

- import sûr ;
- SHA-256 ;
- intégrité ;
- métadonnées ;
- extraction d’indicateurs.

## Jalon C — Enquête structurée

Tickets :

```text
#055 à #066
```

Résultat :

- entités ;
- observations ;
- relations ;
- recherche locale ;
- filtres ;
- pivots.

## Jalon D — Poste de travail OSINT

Tickets :

```text
#067 à #077
```

Résultat :

- DNS ;
- RDAP ;
- TLS ;
- HTTP ;
- archives ;
- moteurs ;
- réseaux sociaux ;
- enrichissements contrôlés.

## Jalon E — Analyse visuelle

Tickets :

```text
#078 à #088
```

Résultat :

- graphe ;
- tableaux ;
- chronologie ;
- notes ;
- hypothèses.

## Jalon F — Transmission

Tickets :

```text
#089 à #102
```

Résultat :

- audit ;
- reproductibilité ;
- rapports ;
- exports ;
- installateurs Ubuntu.

---

# MVP recommandé

Le premier MVP réellement utile est atteint après le ticket **#066**.

L’utilisateur pourra alors :

1. créer ou ouvrir une enquête ;
2. importer des preuves ;
3. vérifier leur intégrité ;
4. extraire leurs métadonnées ;
5. créer des entités ;
6. relier les objets ;
7. rechercher dans toute l’enquête ;
8. effectuer des pivots locaux.

Le second MVP, orienté OSINT réseau, est atteint après le ticket **#077**.

---

# Ordre immédiat

Ne pas commencer directement par le graphe ou les réseaux sociaux.

Ordre recommandé :

```text
#032 — Factoriser le chargement d’une enquête
#033 — Afficher les erreurs dans GTK
#034 — Gestionnaire de tâches asynchrones
#035 — File de tâches et panneau d’activité
#037 — Registre des dépendances
#038 — Interface commune des adaptateurs
#039 — Exécuteur GSubprocess
#040 — Conservation des résultats bruts
#042 — EvidenceRecord
```

Le ticket #036 sur la configuration peut être placé juste avant la première API nécessitant une clé.

---

# Synchronisation avec les autres projets

## C

Tickets particulièrement adaptés au cours de C :

- #034 gestion des tâches ;
- #039 exécution de processus ;
- #044 SHA-256 ;
- #045 copie robuste ;
- #053 extraction d’indicateurs ;
- #062 indexation ;
- #078 projection graphique.

## Unix

Tickets adaptés au cours système Unix :

- #039 processus et signaux ;
- #045 fichiers et renommage atomique ;
- #049 intégrité ;
- #075 quotas et temporisation ;
- #089 journalisation ;
- #099 installation ;
- #100 paquet Debian.

## OSINT

Chaque nouveau fournisseur devra être précédé d’un apprentissage manuel :

```text
Comprendre la technique
→ réaliser un exercice
→ documenter les limites
→ seulement ensuite intégrer l’outil
```

Labfy doit assister l’enquêteur, pas remplacer sa compréhension.

