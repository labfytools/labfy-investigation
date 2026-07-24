# Architecture

> **Version :** 3.0  
> **Dernière mise à jour :** 2026-07-24  
> **Schéma SQLite courant :** V10  
> **Statut :** architecture courante

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
initialisation transactionnelle de SQLite V10
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

Statut :

```text
PARTIEL
```

La branche contient des briques d'analyse EML, d'extraction MIME, d'IBAN, OCR,
ExifTool, vocabulaire contrôlé, propositions bancaires et pipeline de tâche.

Le flux cible est :

```text
EML original
    ↓
en-têtes et MIME
    ↓
pièces jointes dérivées
    ↓
empreintes et métadonnées
    ↓
OCR et indicateurs
    ↓
valeurs brutes + normalisées + dérivées
    ↓
révision humaine
    ↓
intégration transactionnelle
```

Les valeurs brutes restent immuables.

Une IP de relais SMTP ne doit pas être présentée comme l'adresse personnelle
d'un suspect.

Une donnée bancaire observée ne prouve pas l'identité de l'auteur d'une fraude.

Le ticket Forgejo #107 reste la référence fonctionnelle du chantier.

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
