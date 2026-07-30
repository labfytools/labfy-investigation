# Architecture

`EvidencePreviewWidget` est l’adaptateur GTK partagé de `EvidencePreview` pour
`CreatePersonDialog`, l’import, la révision OCR et la fiche directe. Il
encapsule les états, le rendu EML/PDF/image/texte/vidéo, l’annulation, la
génération, la garde de session et l’arrêt du média. Les écrans lui
transmettent uniquement une source interne ou de staging accompagnée du
SHA-256 attendu.

## Aperçu contrôlé multi-format

`EvidencePreview` vérifie l’empreinte du fichier interne ou de staging,
détecte son contenu et produit un résultat borné sans GTK. `BackgroundTask`
travaille hors du thread principal ; le widget crée ensuite textures, buffers
et médias sur le contexte GTK, rejette les générations périmées et arrête tout
média au changement ou à la fermeture. L’aperçu ne persiste rien.

Pour les images et PDF, la barre compacte fournit zoom avant, zoom arrière et
retour à l’ajustement. Une image agrandie reste défilable horizontalement et
verticalement. Les PDF multipages disposent des actions page précédente et
suivante et d’un compteur `Page X / N` ; le changement de page conserve le
zoom. `OcrProvenanceOverlay` applique le même zoom, le même défilement et la
même page afin que la provenance reste alignée.

Le mode aperçu de `EmlMimeExtractor` réutilise le parcours récursif et les
décodages RFC 2047/2231 sans écrire les pièces jointes. Il préfère
`text/plain`, transforme un éventuel HTML en texte inerte et retourne
l’inventaire possédé. Le contrôleur vidéo indépendant de GTK orchestre
pause, retour au début, détachement et libération via des actions injectées.

> **Version :** 3.2
> **Dernière mise à jour :** 2026-07-30
> **Schéma SQLite courant :** V17

## Personnes contextuelles — SQLite V14

`person_role_assignments` sépare les rôles contextuels des champs de l’entité.
Une personne peut porter plusieurs rôles avec preuve facultative, provenance,
confiance et notes. Les anciens codes sont copiés littéralement depuis
`person_roles` avec la provenance `legacy_manual`. Le service crée l’entité,
les rôles et le rattachement manuel dans une transaction unique.

Le cœur `EvidencePreview` vérifie l’intégrité avant décodage PNG/JPEG, applique
des bornes et ne renvoie qu’un rendu mémoire réduit. Une `BackgroundTask`
travaille hors du thread GTK ; la texture est créée sur le contexte principal.
Le dialogue annule l’ancienne tâche et rejette les générations obsolètes.
L’interface conserve une génération de session et les chemins stables du
projet et de sa base.

## OCR contrôlé d’identité — SQLite V15 à V17

`IdentityOcrWorkflow` est l’unique orchestration du moteur OCR, réutilisée par
`CreatePersonDialog`, `EvidenceIdentityOcrDialog`, l’import normal et
Workspace. La V15 sépare l’exécution OCR, l’observation du document et les
observations de champs. La V16 ajoute notamment l’origine `manual_entry` pour
un champ visible mais omis par l’OCR. La V17 ajoute une transcription corrigée
humaine distincte du texte OCR brut.

Le moteur reçoit uniquement une copie contrôlée ou une page PDF explicitement
choisie, conserve texte brut, TSV, paramètres, langues, version, SHA-256,
confiance et coordonnées, puis laisse la révision à l’utilisateur. Le texte
brut n’est jamais remplacé. Une correction de valeur garde
`manual_override`; une saisie sans valeur OCR garde `manual_entry`. Les notes
restent factuelles et ne reconstruisent jamais une partie absente d’un
document tronqué.

Les widgets n’accèdent pas à SQLite. Le coordinateur persiste sur l’UUID
définitif de la preuve dans une transaction unique, avec rollback,
compensation des fichiers et garde de session juste avant commit. L’import
multiple reste sans OCR groupé : chaque preuve est analysée ensuite,
individuellement et sans réimport ni doublon.

La provenance visuelle est rendue par `OcrProvenanceOverlay`. La conversion
des coordonnées source vers la zone affichée est indépendante de GTK et tient
compte du ratio, de la réduction, de l’agrandissement et des marges. Le
rectangle est transitoire : aucune annotation n’est écrite dans la preuve.
Les langues proposées proviennent exclusivement de `tesseract --list-langs`.
Les DAO structurés exposent des lectures possédées pour les runs, observations
de documents et de champs, notes et artefacts. La fiche de preuve recharge ces
données depuis SQLite, sélectionne explicitement un run dans un modèle
`GtkDropDown` stable et affiche texte brut, transcription corrigée, personne
liée, SHA-256 et provenance graphique. Réviser conserve l’UUID du run et ne
relance pas Tesseract ; relancer crée un nouveau run sans écraser
l’historique.
> **Statut :** architecture courante

## Politique des dialogues GTK métiers

`labfy_dialog_prepare()` et `labfy_dialog_present()` centralisent la séquence
des dialogues complexes : `transient_for` vers la vraie fenêtre parente,
modalité appropriée, géométrie commune, puis `gtk_window_present()`. Aucune
coordonnée absolue n’est utilisée sous GTK4/Wayland.

La cible initiale est proche de 1200 × 800, avec un minimum utile de
800 × 600 lorsque la zone de travail le permet. Un `GtkPaned` place
initialement le formulaire défilable à gauche sur environ deux tiers et
l’aperçu à droite sur un tiers ; sa position n’est appliquée qu’après la
première allocation réelle puis reste entièrement modifiable. Les actions
restent fixes en bas. Cette politique exclut les alertes simples, les popups
`GtkDropDown` et les sélecteurs de fichiers natifs.

---

## 1. Objectif

Ce document décrit l'architecture logicielle actuelle de Labfy Investigation.

Il définit :

- les responsabilités des couches ;
- la direction autorisée des dépendances ;
- le cycle de vie d'une enquête ;
- la place de SQLite et du système de fichiers ;
- le fonctionnement des tâches asynchrones ;
- l'intégration des outils externes ;
- la projection graphique des données ;
- les règles de sécurité et de traçabilité.

La documentation détaillée du schéma SQLite se trouve dans :

```text
docs/database/DATABASE_ARCHITECTURE.md
docs/database/SCHEMA_AUDIT_CURRENT.md
```

---

## 2. Vision

Labfy Investigation est un poste de travail local d'investigation numérique et
d'OSINT développé en C17 avec GTK4.

Une enquête doit pouvoir être :

- créée ;
- ouverte ;
- copiée ;
- sauvegardée ;
- archivée ;
- transmise ;

avec son dossier et sa base SQLite.

Le logiciel n'est pas encore prêt pour un usage opérationnel en production.
Les formats et interfaces peuvent évoluer tant que le projet reste en
développement actif.

---

## 3. Sources de vérité

Pour déterminer l'état réel d'une fonction :

1. code de `main` ;
2. tests ;
3. migrations SQL ;
4. commits ;
5. tickets Forgejo ;
6. documentation.

Les documents historiques ne décrivent pas l'état courant.

SQLite est la source de vérité des données structurées d'une enquête.

Le système de fichiers est la source de vérité des fichiers originaux et
dérivés référencés par la base.

Le graphe, la barre latérale et les autres vues ne possèdent pas leur propre
copie métier indépendante.

---

## 4. Principes fondamentaux

### 4.1 Une enquête est autonome

```text
MonEnquete/
├── 00_BaseDeDonnees/
│   └── Enquete.sqlite
├── 01_Preuves_Originales/
├── 02_Preuves_Traitees/
├── 03_Chronologie/
├── 04_Entites/
└── 05_Rapports/
```

Aucune donnée métier d'une enquête ne doit être stockée dans une base globale.

Les préférences générales de l'application, lorsqu'elles seront ajoutées,
resteront séparées des données d'enquête.

### 4.2 Les preuves originales sont immuables

L'application ne modifie jamais un fichier original importé.

Toute annotation, conversion, extraction, analyse ou expurgation produit un
objet dérivé.

### 4.3 Le cœur ne dépend pas de GTK

Les modèles, DAO et services métier doivent pouvoir être testés sans lancer
l'interface graphique.

### 4.4 Les widgets ne contiennent pas la logique métier

Un widget :

- affiche un état ;
- collecte une intention utilisateur ;
- déclenche une action de l'application ;
- présente le résultat.

Il ne :

- construit pas de requête SQL ;
- ne déplace pas directement une preuve ;
- ne lance pas une commande shell ;
- ne décide pas seul de la validation métier d'une donnée.

### 4.5 Les traitements longs sont asynchrones

Le thread GTK principal ne réalise pas les opérations longues.

### 4.6 Les résultats automatiques restent révisables

Une donnée OCR, OSINT ou dérivée ne devient pas un fait confirmé sans action
explicite lorsque le domaine l'exige.

---

## 5. Architecture en couches

```text
┌──────────────────────────────────────────────┐
│ Interface GTK4                              │
│ views / widgets                             │
└──────────────────────┬───────────────────────┘
                       │ intentions et affichage
┌──────────────────────▼───────────────────────┐
│ Application et contrôleurs                  │
│ cycle de vie, session, navigation, messages │
└──────────────────────┬───────────────────────┘
                       │ orchestration
┌──────────────────────▼───────────────────────┐
│ Services métier et tâches                   │
│ import, relations, OSINT, EML, graphe       │
└───────────────┬───────────────────┬──────────┘
                │                   │
┌───────────────▼────────────┐ ┌────▼─────────────────┐
│ DAO                        │ │ Adaptateurs          │
│ requêtes métier SQLite     │ │ fichiers / CLI / API│
└───────────────┬────────────┘ └────┬─────────────────┘
                │                   │
┌───────────────▼───────────────────▼──────────┐
│ Infrastructure                              │
│ SQLite / transactions / système de fichiers │
└──────────────────────────────────────────────┘
```

Les modèles métier circulent entre ces couches sans dépendre de GTK ni de
SQLite.

---

## 6. Organisation du dépôt

```text
database/           scripts SQL versionnés et schéma courant
docs/               architecture, conventions et procédures
include/core/       interfaces des services et du cœur
include/dao/        interfaces d'accès métier aux données
include/database/   interfaces de l'infrastructure SQLite
include/models/     modèles métier
include/views/      fenêtres et dialogues GTK
include/widgets/    widgets réutilisables
resources/          ressources de l'application
src/core/           services, tâches et orchestration métier
src/dao/            requêtes métier SQLite
src/database/       connexion, schéma, statements, transactions, erreurs
src/models/         implémentation des modèles
src/views/          fenêtres et dialogues GTK
src/widgets/        composants GTK réutilisables
tests/              tests unitaires et d'intégration ciblée
```

---

## 7. Responsabilités des modules

### 7.1 Point d'entrée et application

`src/main.c` démarre l'application.

La couche application :

- gère le cycle de vie GTK ;
- crée la fenêtre principale ;
- ouvre ou crée une enquête ;
- remplace la session active uniquement après succès ;
- coordonne les messages utilisateur ;
- relie les services métier aux vues ;
- ferme proprement les ressources.

Une erreur d'ouverture ne doit pas détruire une session valide déjà active.

### 7.2 Core et services métier

`src/core` contient notamment :

- représentation et validation d'une enquête ;
- session et projet d'enquête ;
- construction de l'arborescence ;
- tâches d'arrière-plan ;
- gestionnaire de tâches ;
- registre et catalogue d'outils ;
- exécution de processus ;
- hachage et copie de fichiers ;
- import et intégrité des preuves ;
- services d'entités et de relations ;
- chargement du graphe ;
- actions OSINT ;
- analyse EML, IBAN, OCR, métadonnées et PDF ;
- vocabulaire contrôlé ;
- pipeline EML.

Un service métier peut coordonner plusieurs DAO, une transaction et une
opération de fichier.

### 7.3 Modèles

`src/models` représente les données manipulées par l'application.

Les modèles :

- ne lancent pas de requête SQL ;
- ne dépendent pas de GTK ;
- valident leurs invariants lorsque cela leur appartient ;
- exposent une API claire de création, lecture et destruction.

### 7.4 Infrastructure Database

`src/database` gère :

- ouverture et fermeture de SQLite ;
- activation des clés étrangères ;
- lecture de la version ;
- installation du schéma ;
- migrations ;
- statements préparés ;
- transactions ;
- traduction structurée des erreurs.

Cette couche ne contient pas l'orchestration complète d'une fonctionnalité
utilisateur.

### 7.5 DAO

`src/dao` contient les requêtes métier.

Un DAO :

- transforme des lignes SQLite en modèles ;
- utilise des statements préparés ;
- ne dépend pas de GTK ;
- ne lance pas d'outil externe ;
- ne décide pas seul d'un workflow multi-étapes ;
- expose des erreurs exploitables par le service appelant.

### 7.6 Vues et widgets

`src/views` contient les fenêtres et dialogues complets.

`src/widgets` contient les composants réutilisables, notamment :

- barre latérale ;
- arborescence ;
- panneau de tâches ;
- listes ;
- espace de travail ;
- graphe.

Les vues et widgets ne deviennent pas propriétaires des données persistées.

---

## 8. Cycle de vie d'une enquête

### 8.1 Création

```text
sélection du dossier
    ↓
validation du chemin
    ↓
création de l'arborescence
    ↓
initialisation transactionnelle de SQLite V17
    ↓
création de l'identité de l'enquête
    ↓
ouverture d'une session
    ↓
construction des vues
```

Un échec laisse l'application dans un état cohérent et nettoie les artefacts
partiels prévus par le service.

### 8.2 Ouverture

```text
sélection du dossier
    ↓
validation de l'arborescence
    ↓
ouverture de SQLite
    ↓
lecture de schema_version
    ↓
migration éventuelle
    ↓
chargement de l'enquête
    ↓
construction de l'arborescence et du graphe
    ↓
remplacement atomique de la session active
```

Une base plus récente que l'application doit être refusée.

### 8.3 Fermeture

La fermeture libère :

- les tâches ;
- les références de modèles ;
- la connexion SQLite ;
- la session ;
- les widgets dépendants ;
- les ressources externes.

---

## 9. Gestion des preuves

### 9.1 Import

Le flux d'import vise à garantir l'intégrité :

```text
validation de la source
    ↓
SHA-256 source
    ↓
copie vers une destination contrôlée
    ↓
SHA-256 destination
    ↓
écriture SQLite
    ↓
validation ou nettoyage
```

L'import groupé traite les fichiers de manière contrôlée et produit un bilan
détaillé.

### 9.2 Reclassement

Un changement de type peut impliquer :

- vérification préalable de l'intégrité ;
- déplacement cohérent du fichier interne ;
- mise à jour SQLite ;
- rollback et restauration en cas d'échec.

L'UUID, l'empreinte et l'historique ne doivent pas être recréés silencieusement.

### 9.3 Dérivés

Les extractions, OCR, pièces jointes EML et versions traitées restent reliés à
leur preuve source.

---

## 10. Tâches asynchrones

### 10.1 Création d’une personne avec preuves

`PersonEvidenceSelection` possède la collection ordonnée des preuves retenues
et distingue les preuves existantes des copies de staging. Il ne dépend ni de
GTK ni de SQLite. `EvidenceStaging` refuse les fichiers spéciaux et liens
symboliques, calcule l’empreinte de la source et de sa copie, détecte le MIME
et nettoie les temporaires lors d’un retrait ou d’une annulation.

`PersonCreationCoordinator` valide toutes les empreintes avant écriture, crée
la personne, importe les nouvelles preuves et rattache toute la collection
dans une transaction SQLite. Les copies définitives réalisées avant un échec
sont supprimées après rollback. `PersonCreationTask` exécute cette orchestration
hors du thread GTK. Un changement de session annule les tâches et interdit le
rafraîchissement d’une autre enquête.

Les aperçus utilisent exclusivement le fichier interne d’une preuve existante
ou sa copie de staging. L’OCR d’identité n’est lancé que sur demande explicite
et sur un format compatible contrôlé.

Les tâches longues utilisent l'infrastructure de tâche d'arrière-plan et le
gestionnaire de tâches.

Une tâche expose selon ses besoins :

```text
pending
running
completed
failed
cancelled
```

Elle doit :

- éviter l'accès direct à GTK depuis un thread de travail ;
- transférer le résultat vers le thread principal ;
- conserver une erreur structurée ;
- libérer ses ressources même après annulation ;
- ne pas laisser une transaction ouverte ;
- ne pas écrire simultanément dans SQLite sans coordination.

---

## 11. Outils externes et OSINT

### 11.1 Registre et catalogue

Le registre détecte la présence et la version des outils.

Le catalogue décrit les capacités et les actions compatibles avec une
sélection.

Une dépendance absente désactive l'action concernée sans empêcher le démarrage.

### 11.2 Exécution

Les outils sont lancés avec `GSubprocess`.

Les arguments sont transmis séparément.

Aucune commande shell dynamique n'est construite.

Les exécutions persistées peuvent conserver :

- outil et version ;
- cible et paramètres ;
- dates ;
- code de retour ;
- sorties brutes ;
- empreintes ;
- statut d'analyse ;
- objets intégrés.

### 11.3 Intégration

Une action OSINT suit le modèle :

```text
sélection
    ↓
validation de l'action
    ↓
tâche asynchrone
    ↓
sortie brute
    ↓
propositions normalisées
    ↓
révision
    ↓
intégration transactionnelle
    ↓
rafraîchissement des vues
```

---

## 12. Graphe d'enquête

Le graphe est une projection de SQLite.

Il représente notamment :

- entités ;
- relations ;
- preuves ou extractions lorsque le modèle le prévoit ;
- positions et viewport persistés séparément des données métier.

Les coordonnées et le zoom sont un état de présentation.

Une modification du graphe qui change le métier doit passer par un service et
être persistée dans SQLite avant d'être considérée comme acquise.

Le graphe ne doit jamais inventer une relation seulement parce que deux nœuds
sont proches visuellement.

---

## 13. Pivot EML

### 13.1 Parcours

```text
preuve EML
    ↓ contrôle d'intégrité SHA-256
tâche asynchrone
    ↓
analyse des en-têtes
    ↓
extraction MIME récursive vers un staging
    ↓
outils documentaires optionnels
    ↓
propositions temporaires
    ↓ confirmation explicite
observations persistantes dans la fiche
    ↓ promotion facultative
entités canoniques du graphe
```

La preuve originale n'est jamais modifiée. Le staging et ses pièces jointes
extraites sont supprimés après rejet, annulation, erreur ou intégration.
L'absence d'ExifTool, Tesseract ou Poppler conserve un résultat partiel :
l'analyse des en-têtes et MIME reste utilisable.

### 13.2 Responsabilités

- `EmlAnalyzer` lit les en-têtes, conserve leurs occurrences et qualifie les
  adresses, domaines et IP.
- `EmlMimeExtractor` parcourt les parties imbriquées, y compris
  `message/rfc822`, décode Base64, quoted-printable, RFC 2047 et RFC 2231,
  assainit les noms et applique les limites de profondeur, nombre et taille.
- `EmlPipelineTask` orchestre en arrière-plan l'analyse, le staging, les outils
  documentaires et les propositions bancaires.
- `DocumentToolRunner` lance les programmes avec `GSubprocess`, sans shell,
  draine simultanément `stdout` et `stderr`, borne les sorties et propage
  l'annulation.
- les modules ExifTool, OCR et PDF structurent les résultats sans modifier la
  source. Le PDF privilégie le texte natif puis utilise l'OCR page par page.
- `BankProposal` conserve les valeurs bancaires détectées, leur normalisation,
  leur validation et une éventuelle correction OCR distincte.
- `EmlAnalysisDialog` présente les résultats et collecte séparément la
  conservation et la promotion.
- `EmlIntegration`, les DAO et `EvidenceObservation` assurent l'écriture
  transactionnelle, la déduplication, la promotion et le retrait.
- `Application`, `MainWindow` et `Workspace` raccordent la tâche au contexte
  GTK principal, rafraîchissent le graphe et rechargent la fiche depuis SQLite.

### 13.3 Proposition, observation et entité

Une **proposition** est un résultat temporaire. Elle peut être rejetée,
invalidée, corrigée, conservée ou accompagnée d'une demande de promotion.

Une **observation** est une information confirmée liée à une preuve. La table
`evidence_entity_observations` conserve son UUID, son type, ses valeurs brute,
normalisée et corrigée éventuelle, son rôle, l'en-tête et son occurrence, sa
provenance, son statut, ses dates et une association facultative à une entité.
Une observation peut donc exister durablement sans nœud de graphe.

Une **entité** est un objet canonique réutilisable de `entites`. Elle n'est
créée ou réutilisée que si « Promouvoir en entité » est explicitement coché.
`preuve_entites` fournit alors le rattachement nécessaire à la projection du
graphe. Une même entité peut servir plusieurs rôles, observations, preuves ou
relations.

### 13.4 MIME et outils documentaires

L'extracteur accepte un EML de 50 Mio au maximum. Il limite une partie décodée
à 8 Mio, le total décodé à 32 Mio, le message à 128 parties, la profondeur à
12 niveaux et un nom produit à 240 octets. Les chemins MIME sont conservés,
les fichiers inline et `Content-ID` sont inventoriés, les traversées de chemin
sont neutralisées et les écritures passent par un temporaire renommé après
succès. Cette prise en charge volontairement bornée ne prétend pas couvrir
l'intégralité des RFC MIME.

Une analyse documentaire accepte au maximum 50 Mio, 8 Mio de `stdout`,
256 Kio de `stderr`, 100 pages PDF et 128 documents par pipeline. Un PDF
chiffré n'est pas contourné. `pdfinfo` inspecte le document, `pdftotext`
fournit en priorité le texte natif et `pdftoppm` rend les pages nécessitant un
OCR. L'ordre des pages, les résultats partiels et la méthode utilisée sont
conservés ; les images temporaires sont nettoyées.

Tesseract reçoit `fra+eng` dans le pipeline. Son texte brut n'est pas corrigé
silencieusement : une correction OCR proposée reste distincte. Les arguments,
la version et l'état de l'exécution documentent la provenance.

ExifTool est appelé en sortie JSON avec les groupes de tags. Les champs connus
sont normalisés et les tags inconnus sont conservés avec leur groupe, leur nom
et leur valeur brute. La version de l'outil est attachée à l'exécution. Les
coordonnées GPS sont marquées sensibles et ne créent jamais automatiquement
une entité.

Les propositions bancaires peuvent contenir IBAN brut et normalisé,
validation MOD-97, BIC, banque, titulaire déclaré, adresse, éléments de RIB et
correction OCR distincte. Une donnée invalide n'est pas intégrable. Un
titulaire déclaré dans un document n'établit ni identité certaine ni
attribution pénale : la donnée reste une proposition puis une observation
tant que l'enquêteur ne choisit pas de la promouvoir.

### 13.5 Migrations V11, V12 et V13

V11 crée le premier modèle `evidence_entity_observations`, où chaque
observation est obligatoirement liée à une entité.

V12 donne un UUID propre à l'observation, rend `entity_id` nullable, ajoute les
valeurs corrigées, l'extraction, les avertissements, les dates d'observation,
d'intégration et de promotion ainsi que `promotion_kind`. Les lignes V11 sont
reprises avec `promotion_kind = 'legacy'`. L'index sémantique assure la
déduplication, y compris lorsque `extraction_id` est nul.

V13 ajoute `preuve_entite_sources`. Chaque rattachement matérialisé possède
une justification `manual`, `legacy_manual` ou `eml_observation`. La migration
protège les lignes historiques par `legacy_manual` et reprend les promotions
V12 identifiables.

### 13.6 Promotion et retrait

La conservation seule écrit l'observation et l'affiche dans la fiche, sans
créer `entites` ni `preuve_entites`. La promotion explicite crée ou réutilise
une entité, l'associe à l'observation et ajoute le rattachement nécessaire.
L'opération est transactionnelle et idempotente.

« Retirer du graphe » efface uniquement la source `eml_observation` portant
l'UUID de l'observation et conserve
l'observation. Le nœud n'est supprimé que si les références connues
(observations, preuves, relations, tags, recherches, chronologie, hypothèses,
OSINT, comptes sociaux et rôles de personne) sont absentes.

### 13.7 Qualification et provenance

Les rôles couvrent `From`, `Sender`, `Reply-To`, `Return-Path`, `To`, `Cc`,
`Bcc`, les relais `Received` et le domaine de `Message-ID`. Une même adresse
peut conserver plusieurs rôles sans multiplier l'entité canonique.
`192.0.2.10` et `198.51.100.20` sont des IP, jamais des domaines ;
`MIME-Version: 1.0` ne produit pas de domaine intégrable.

### 13.8 Asynchronisme et sécurité

`BackgroundTask`, `TaskManager` et `GCancellable` portent l'état, la
progression, l'annulation et la remise du résultat au contexte principal.
Le worker ne manipule aucun widget. Avant de présenter le résultat,
`Application` vérifie que la session attendue est toujours active ; un
changement d'enquête rend le résultat caduc et déclenche le nettoyage.

Tous les arguments externes sont séparés, sans shell. `stdout` et `stderr`
sont drainés en parallèle pour éviter un interblocage. Les processus sont
forcés à terminer lors d'une annulation, puis les répertoires temporaires sont
supprimés.

### 13.9 Limites connues

- la promotion est disponible dans le dialogue d'analyse, pas directement
  depuis la fiche ;
- la fiche affiche la valeur canonique et les codes persistés, mais pas encore
  la valeur brute distincte ni l'UUID de l'entité associée ;
- le runner borne les sorties et gère l'annulation, mais ne possède pas de
  délai maximal autonome ;
- les pièces jointes analysées dans le staging ne sont pas persistées comme
  dérivés confirmés par ce parcours ;
- les rattachements antérieurs à V13 restent volontairement protégés par
  `legacy_manual`, faute de provenance historique plus précise ;
- la couverture visuelle du dialogue et de la fiche reste manuelle.

---

## 14. Erreurs et journalisation

Les couches basses produisent des erreurs structurées.

Les services ajoutent le contexte métier.

L'application décide :

- du journal technique ;
- du message utilisateur ;
- du maintien ou du remplacement de la session ;
- de la possibilité de réessayer.

Les messages ne doivent pas divulguer inutilement :

- chemins absolus sensibles ;
- données personnelles ;
- contenu complet d'une preuve ;
- secrets ou jetons.

---

## 15. Sécurité

Règles obligatoires :

- requêtes préparées pour les données variables ;
- clés étrangères activées ;
- chemins contrôlés ;
- neutralisation des traversées `../` ;
- aucune ouverture automatique de pièce jointe ;
- aucun chargement automatique de ressource distante d'un e-mail ;
- limites de taille et de profondeur pour les formats complexes ;
- aucune commande shell dynamique ;
- aucune installation automatique d'outil ;
- aucune donnée réelle d'enquête dans les tests ou le dépôt ;
- aucune action intrusive non autorisée.

---

## 16. Tests

Le cœur, les modèles, DAO, migrations et tâches possèdent des tests ciblés.

Les tests GTK restent limités aux composants qui nécessitent réellement GTK.

Les scénarios critiques incluent :

- base neuve ;
- migration ;
- rollback ;
- import interrompu ;
- annulation ;
- erreur d'outil ;
- donnée malformée ;
- doublon ;
- intégrité ;
- clés étrangères ;
- conservation de la valeur brute ;
- absence d'une dépendance optionnelle.

Les parcours OCR ferment puis rouvrent leurs bases SQLite temporaires afin de
vérifier la persistance du texte brut, de la transcription corrigée, des
champs et de l’historique multi-run. Les tests GTK réels passent par
`MainWindow`, `Workspace` et les contrôles de production, avec des fixtures
`SPECIMEN`, `G_DEBUG=fatal-criticals` et un timeout. Ils couvrent aussi
l’aperçu PNG/JPEG/PDF multipage, la provenance, la géométrie, l’annulation,
les résultats tardifs, les erreurs de transaction et les cycles de vie.

Validation :

```sh
make clean
make -j8
make -j8 test
git diff --check
```

---

## 17. Règles d'évolution

Une nouvelle fonctionnalité doit :

1. respecter la direction des dépendances ;
2. réutiliser les services existants avant d'en créer un concurrent ;
3. définir clairement la propriété mémoire ;
4. ajouter les tests nécessaires ;
5. documenter les formats persistés ;
6. utiliser des fixtures synthétiques ;
7. préserver les données brutes ;
8. maintenir la branche compilable ;
9. mettre à jour l'architecture lorsque ses responsabilités changent.
