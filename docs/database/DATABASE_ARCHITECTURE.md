# Architecture de la base de données

> **Statut :** Stable (V1)
>
> Ce document décrit l'architecture de référence de la base de données de Labfy Investigation.
>
> Toute évolution incompatible devra faire l'objet d'une nouvelle version du schéma et d'une migration documentée.

## Version

**Schéma :** V1

---

# 1. Introduction

## 1.1 Objectif

La base de données de Labfy Investigation constitue le cœur du modèle métier de
l'application.

Chaque enquête possède sa propre base SQLite, indépendante des autres
enquêtes.

Cette base centralise toutes les informations produites ou découvertes pendant
une investigation :

- les sources consultées ;
- les recherches réalisées ;
- les preuves collectées ;
- les entités identifiées ;
- les relations établies ;
- les hypothèses formulées ;
- la chronologie de l'enquête ;
- le journal d'audit de l'application ;
- les catégories et les tags.

L'objectif de cette architecture est de fournir un modèle de données robuste,
cohérent et facilement extensible, tout en restant suffisamment simple pour
être compris et maintenu sur le long terme.

Cette documentation décrit les choix d'architecture retenus pour la première
version du schéma de la base de données.

Elle constitue le document de référence pour le développement de la couche
Database de Labfy Investigation.

---

## 1.2 Philosophie

La conception de cette base de données repose sur plusieurs principes.

Le premier est la séparation des responsabilités.

Chaque table représente un concept métier unique et clairement identifié.

Par exemple :

- une preuve représente un élément collecté ;
- une entité représente un objet identifié ;
- une relation représente un lien entre deux entités ;
- une hypothèse représente un raisonnement de l'enquêteur.

Aucune table ne doit mélanger plusieurs responsabilités.

Le deuxième principe est la traçabilité.

Toute information importante doit pouvoir être reliée à son origine.

Il doit toujours être possible d'expliquer :

- d'où provient une information ;
- quelle recherche l'a produite ;
- quelles preuves la soutiennent ;
- quelles relations en découlent ;
- quelles hypothèses en résultent.

Le troisième principe est l'évolutivité.

Le schéma doit pouvoir évoluer sans remettre en cause les données existantes.

Les nouvelles fonctionnalités devront privilégier l'ajout de nouvelles tables
ou de nouvelles relations plutôt que la modification des structures déjà
publiées.

Enfin, le schéma privilégie la lisibilité plutôt que la recherche d'une
optimisation prématurée.

La compréhension du modèle par les développeurs constitue une priorité.

---

## 1.3 Une base par enquête

Chaque enquête est totalement autonome.

Lors de la création d'une nouvelle enquête, Labfy Investigation génère une
base SQLite dédiée :

```text
00_BaseDeDonnees/
└── Enquete.sqlite
```

Toutes les informations propres à cette enquête sont enregistrées dans cette
base.

Aucune information métier n'est partagée entre plusieurs enquêtes.

Cette organisation présente plusieurs avantages :

- chaque enquête est portable ;
- les sauvegardes sont simplifiées ;
- l'export d'une enquête est immédiat ;
- les risques de corruption croisée sont limités ;
- plusieurs enquêtes peuvent être ouvertes indépendamment.

La base SQLite ne contient jamais les fichiers originaux.

Les preuves (captures d'écran, photographies, vidéos, documents, archives,
etc.) restent stockées dans l'arborescence de l'enquête.

La base conserve uniquement les métadonnées nécessaires à leur exploitation.

Cette séparation permet de préserver l'intégrité des fichiers originaux tout en
offrant un accès rapide aux informations nécessaires aux traitements réalisés
par l'application.

---

# 2. Principes généraux

Cette section décrit les conventions utilisées dans toute la couche Database.

Ces règles doivent rester cohérentes dans l'ensemble du projet afin de garantir
la lisibilité du code, la stabilité du schéma et la pérennité des données.

---

## 2.1 UUID

Tous les objets métier utilisent un identifiant unique universel (UUID) comme
clé primaire.

Exemple :

```sql
id TEXT PRIMARY KEY
```

Les UUID sont générés par l'application lors de la création des objets.

Cette approche présente plusieurs avantages :

- unicité garantie entre plusieurs enquêtes ;
- simplicité des imports et exports ;
- possibilité de fusionner plusieurs bases de données ;
- indépendance vis-à-vis des identifiants internes SQLite.

Les tables de référence utilisent en revanche des identifiants entiers
statiques.

Exemples :

- `types_preuve`
- `types_entite`
- `types_source`
- `types_outil`

Ces identifiants sont contrôlés par l'application et ne sont pas destinés à
être modifiés par les utilisateurs.

---

## 2.2 Dates UTC

Toutes les dates enregistrées dans la base utilisent le temps universel (UTC).

Le format retenu est :

```text
YYYY-MM-DDTHH:MM:SSZ
```

Exemple :

```text
2026-07-15T18:42:10Z
```

L'utilisation de l'UTC évite les problèmes liés :

- aux fuseaux horaires ;
- aux changements d'heure ;
- aux déplacements géographiques ;
- aux échanges de bases entre plusieurs machines.

La conversion vers l'heure locale est réalisée uniquement lors de
l'affichage dans l'interface utilisateur.

---

## 2.3 Suppression logique

Les objets métier ne sont généralement pas supprimés physiquement.

Ils utilisent une suppression logique basée sur la colonne :

```text
status
```

Les valeurs autorisées dépendent de la table concernée mais utilisent
principalement :

```text
active
archived
deleted
```

Cette approche permet :

- de préserver l'historique de l'enquête ;
- d'éviter les suppressions accidentelles ;
- de conserver la cohérence des relations entre objets ;
- de restaurer un objet si nécessaire.

Une suppression physique ne doit intervenir que dans le cadre d'opérations
d'administration ou de maintenance spécifiques.

---

## 2.4 Intégrité référentielle

Les clés étrangères sont activées systématiquement lors de l'ouverture de la
base.

```sql
PRAGMA foreign_keys = ON;
```

Toutes les relations entre objets sont protégées par des contraintes
d'intégrité.

Selon le contexte, les suppressions utilisent :

- `CASCADE` ;
- `RESTRICT` ;
- `SET NULL`.

Le choix dépend du rôle métier de la relation et non d'une règle unique.

---

## 2.5 Transactions

Toutes les opérations critiques sont exécutées dans une transaction SQLite.

La création d'une enquête constitue une transaction unique comprenant :

- l'installation du schéma ;
- l'enregistrement des métadonnées ;
- la création de l'enquête.

En cas d'échec d'une étape, l'ensemble de la transaction est annulé.

Cette stratégie garantit qu'une enquête ne peut jamais être créée dans un
état partiellement initialisé.

---

## 2.6 Requêtes préparées

Toutes les requêtes contenant des données variables utilisent des requêtes
préparées SQLite.

Exemple :

```c
sqlite3_prepare_v2(...)
sqlite3_bind_text(...)
sqlite3_step(...)
```

La concaténation de chaînes SQL contenant des données utilisateur est
interdite.

Cette règle permet :

- d'éviter les injections SQL ;
- d'améliorer les performances ;
- de simplifier la gestion des erreurs.

Les requêtes SQL statiques peuvent être exécutées avec `sqlite3_exec()`.

---

## 2.7 Séparation des responsabilités

Le schéma suit une séparation stricte des responsabilités.

Chaque table représente un unique concept métier.

Les relations entre concepts sont modélisées à l'aide de tables de liaison
explicites plutôt que par des structures ambiguës ou des colonnes
multifonctions.

Cette approche facilite :

- la maintenance ;
- les évolutions futures ;
- les tests unitaires ;
- la compréhension du modèle.

---

## 2.8 Couche Database

L'accès à la base de données est centralisé dans le module `database`.

Aucun autre composant de l'application ne doit exécuter directement de
requêtes SQL.

Toutes les opérations passent par des fonctions dédiées de la couche
Database.

Cette règle garantit :

- une architecture modulaire ;
- une gestion uniforme des erreurs ;
- une meilleure testabilité ;
- une évolution simplifiée du schéma de la base.

---

# 3. Vue d'ensemble

Ce chapitre présente l'organisation générale de la base de données.

Le schéma de Labfy Investigation n'a pas été conçu comme un simple ensemble de
tables indépendantes.

Chaque objet métier représente une étape du processus d'investigation.

L'ensemble forme un modèle cohérent permettant de suivre le cycle complet
d'une enquête, depuis la collecte initiale jusqu'à la formulation
d'hypothèses.

---

## 3.1 Organisation générale

Le modèle est organisé autour de plusieurs domaines fonctionnels.

Chaque domaine possède une responsabilité clairement définie.

```text
Métadonnées
Référentiels
Collecte
Connaissance
Raisonnement
Traçabilité
Classification
```

Cette séparation facilite :

- la compréhension du modèle ;
- la maintenance du code ;
- les évolutions futures ;
- les tests unitaires.

---

## 3.2 Domaines fonctionnels

### Métadonnées

Les métadonnées décrivent la base de données elle-même.

Elles permettent notamment de connaître :

- la version du schéma ;
- l'application ayant créé la base ;
- la date de création ;
- l'identifiant de l'enquête.

Tables :

```text
metadata
investigation
```

---

### Référentiels

Les référentiels regroupent les listes de valeurs stables utilisées dans le
reste de la base.

Ils évitent la duplication de chaînes de caractères et garantissent une
classification cohérente.

Tables :

```text
types_preuve
types_entite
types_source
types_outil
```

---

### Collecte

La collecte représente toutes les actions réalisées pour obtenir de nouvelles
informations.

Elle comprend :

- les sources consultées ;
- les recherches effectuées ;
- les preuves collectées.

Tables :

```text
sources
recherches
preuves
```

---

### Connaissance

Les preuves permettent d'identifier des objets réels.

Ces objets deviennent des entités.

Les relations établissent ensuite les liens entre ces entités.

Tables :

```text
entites
relations
```

---

### Raisonnement

Les hypothèses représentent les conclusions provisoires formulées pendant
l'enquête.

Elles s'appuient sur :

- les preuves ;
- les recherches ;
- les relations ;
- les entités.

Table :

```text
hypotheses
```

---

### Traçabilité

Deux mécanismes distincts assurent la traçabilité.

La chronologie décrit les événements de l'enquête.

Le journal décrit les actions réalisées dans l'application.

Tables :

```text
chronologie
journal
```

---

### Classification

La classification facilite l'organisation des informations.

Les catégories structurent les objets.

Les tags permettent une annotation libre.

Tables :

```text
categories
tags
```

---

## 3.3 Flux d'investigation

Le déroulement d'une enquête peut être représenté de manière simplifiée par le
schéma suivant :

```text
Source
   │
   ▼
Recherche
   │
   ▼
Preuve
   │
   ▼
Entité
   │
   ▼
Relation
   │
   ▼
Hypothèse
```

Ce schéma représente uniquement le cheminement principal.

En pratique, une enquête est beaucoup plus dynamique.

Une recherche peut :

- produire plusieurs preuves ;
- confirmer une preuve existante ;
- découvrir plusieurs entités ;
- modifier une relation ;
- renforcer ou contredire une hypothèse.

De même :

- une preuve peut être utilisée dans plusieurs recherches ;
- une entité peut apparaître dans plusieurs preuves ;
- une relation peut être soutenue par plusieurs preuves ;
- une hypothèse peut évoluer tout au long de l'enquête.

Le modèle de données repose donc sur un réseau d'objets reliés entre eux,
plutôt que sur une chaîne linéaire.

Les nombreuses tables de liaison présentes dans le schéma permettent de
représenter cette richesse tout en conservant un modèle relationnel simple et
cohérent.

---

# 4. Tables métier

Les tables métier représentent les objets manipulés quotidiennement par
l'application.

Contrairement aux tables de référence, elles contiennent les données propres à
chaque enquête.

Chaque table possède une responsabilité unique.

Les relations entre ces objets sont assurées par des clés étrangères ou par
des tables de liaison dédiées.

---

## 4.1 metadata

### Responsabilité

La table `metadata` stocke les informations techniques concernant la base de
données.

Elle permet notamment d'identifier :

- la version du schéma ;
- la version de l'application ayant créé la base ;
- les informations nécessaires aux futures migrations.

Cette table ne contient aucune donnée liée à l'enquête elle-même.

### Identifiant

La table repose sur des clés textuelles (`key` / `value`) et non sur un UUID.

### Relations principales

Aucune.

### Cycle de vie

Les métadonnées sont créées lors de l'initialisation de la base puis modifiées
uniquement lors des migrations ou des mises à jour du schéma.

### Points d'attention

Cette table est utilisée pour déterminer la compatibilité entre la base de
données et l'application.

---

## 4.2 investigation

### Responsabilité

La table `investigation` décrit l'enquête elle-même.

Elle contient les informations générales nécessaires à son identification.

Une base de données ne contient qu'une seule enquête.

### Identifiant

UUID.

### Relations principales

L'enquête constitue la racine logique de l'ensemble des objets métier.

Les autres tables appartiennent implicitement à cette enquête.

### Cycle de vie

Créée automatiquement lors de l'initialisation de la base.

Elle est ensuite très rarement modifiée.

### Points d'attention

Aucune seconde enquête ne doit être créée dans la même base SQLite.

---

## 4.3 sources

### Responsabilité

Une source représente l'origine d'une information.

Une source peut être :

- un site web ;
- un réseau social ;
- une API ;
- un document ;
- une base publique ;
- une personne interrogée.

### Identifiant

UUID.

### Relations principales

Une source peut être utilisée par plusieurs recherches.

### Cycle de vie

Une source peut être enrichie au cours de l'enquête sans perdre son identité.

### Points d'attention

Une source ne constitue jamais une preuve.

Elle décrit uniquement l'origine de l'information.

---

## 4.4 recherches

### Responsabilité

Une recherche représente une action réalisée par l'enquêteur.

Exemples :

- requête WHOIS ;
- recherche Google ;
- interrogation d'une API ;
- analyse d'un document ;
- recherche DNS.

### Identifiant

UUID.

### Relations principales

Une recherche peut :

- utiliser une source ;
- produire plusieurs preuves ;
- identifier des entités ;
- confirmer une relation ;
- enrichir une hypothèse ;
- générer des événements de chronologie.

### Cycle de vie

Une recherche est créée lorsqu'une action d'investigation est réalisée.

Elle peut être enrichie ultérieurement.

### Points d'attention

Une recherche décrit une action, pas son résultat.

Les résultats sont représentés par d'autres objets.

---

## 4.5 preuves

### Responsabilité

Une preuve représente un élément collecté pendant l'enquête.

Exemples :

- capture d'écran ;
- photographie ;
- document ;
- vidéo ;
- archive ;
- export JSON.

### Identifiant

UUID.

### Relations principales

Une preuve peut :

- provenir de plusieurs recherches ;
- contenir plusieurs entités ;
- soutenir plusieurs relations ;
- soutenir plusieurs hypothèses.

### Cycle de vie

Les preuves originales sont conservées.

Les traitements réalisés sur une preuve créent de nouvelles informations mais
ne modifient pas le fichier original.

### Points d'attention

L'intégrité de la preuve est essentielle.

Les métadonnées permettent notamment de suivre les empreintes cryptographiques.

L'import groupé conserve ce modèle : chaque fichier sélectionné est révisé et
importé séparément. Les imports sont exécutés séquentiellement afin d'éviter
les écritures SQLite concurrentes. L'échec d'un fichier n'annule pas les
preuves déjà importées et apparaît dans le bilan final.

Le classement, la source et la description peuvent être corrigés après
l'import. Un changement de type déplace la copie interne vers le dossier
adapté dans la même opération logique que la mise à jour SQLite. L'empreinte
et la taille sont vérifiées avant et après ; un échec annule la transaction et
restaure le chemin initial. L'UUID, le nom interne et la date d'import restent
inchangés.

---

## 4.6 entites

### Responsabilité

Une entité représente un objet identifié.

Exemples :

- personne ;
- entreprise ;
- adresse IP ;
- adresse email ;
- nom de domaine ;
- numéro de téléphone.

### Identifiant

UUID.

### Relations principales

Les entités peuvent être reliées entre elles par des relations.

Elles peuvent également apparaître dans plusieurs preuves.

### Cycle de vie

Une entité peut être enrichie au fur et à mesure de l'enquête.

### Points d'attention

Une entité doit représenter un objet unique.

Les doublons doivent être évités.

---

## 4.7 relations

### Responsabilité

Une relation représente un lien entre deux entités.

Elle permet de formaliser les connaissances acquises.

### Identifiant

UUID.

### Relations principales

Une relation relie :

- une entité source ;
- une entité cible.

Elle peut être soutenue ou contredite par plusieurs preuves.

### Cycle de vie

Une relation peut évoluer avec l'arrivée de nouvelles preuves.

### Points d'attention

Une relation n'est jamais une hypothèse.

Elle décrit un lien observé.

---

## 4.8 chronologie

### Responsabilité

La chronologie raconte les événements importants de l'enquête.

Elle permet de reconstruire le déroulement des investigations.

### Identifiant

UUID.

### Relations principales

Une entrée peut être liée à plusieurs objets métier.

### Cycle de vie

Les événements peuvent être créés automatiquement ou manuellement.

### Points d'attention

La chronologie décrit l'enquête, pas le fonctionnement interne de
l'application.

---

## 4.9 journal

### Responsabilité

Le journal constitue la trace d'audit des opérations réalisées par
l'application.

### Identifiant

UUID.

### Relations principales

Les références vers les objets utilisent le couple :

- objet_type ;
- objet_id.

### Cycle de vie

Les entrées sont ajoutées chronologiquement.

### Points d'attention

Le journal est conçu selon un modèle append-only.

Les entrées existantes ne doivent pas être modifiées dans le fonctionnement
normal.

---

## 4.10 hypotheses

### Responsabilité

Une hypothèse représente un raisonnement de l'enquêteur.

Elle peut être soutenue, contredite ou confirmée.

### Identifiant

UUID.

### Relations principales

Une hypothèse peut être liée :

- aux preuves ;
- aux entités ;
- aux relations ;
- aux recherches.

### Cycle de vie

Une hypothèse évolue au cours de l'enquête.

Son niveau de confiance peut être réévalué.

### Points d'attention

Une hypothèse ne constitue pas un fait.

Elle représente une conclusion provisoire.

---

## 4.11 categories

### Responsabilité

Les catégories permettent de classer les objets métier dans un domaine
fonctionnel.

### Identifiant

UUID.

### Relations principales

Une catégorie peut être associée à plusieurs objets.

Chaque objet ne possède qu'une seule catégorie.

### Cycle de vie

Les catégories évoluent peu.

### Points d'attention

Les catégories structurent les données.

Elles ne remplacent pas les tags.

---

## 4.12 tags

### Responsabilité

Les tags permettent d'annoter librement les objets métier.

### Identifiant

UUID.

### Relations principales

Les tags peuvent être associés à plusieurs types d'objets via des tables de
liaison.

### Cycle de vie

Les utilisateurs peuvent créer ou supprimer leurs propres tags.

### Points d'attention

Les tags complètent la classification mais ne modifient pas la structure du
modèle métier.

---

# 5. Tables de référence

Les tables de référence regroupent les listes de valeurs stables utilisées
dans l'ensemble de la base de données.

Contrairement aux tables métier, leur contenu évolue peu.

Leur objectif est de normaliser les données et d'éviter la duplication de
chaînes de caractères dans plusieurs tables.

Les objets métier référencent ces tables au moyen d'identifiants entiers.

Cette approche présente plusieurs avantages :

- cohérence des valeurs utilisées ;
- réduction des risques de fautes de frappe ;
- simplification des contrôles d'intégrité ;
- meilleure lisibilité des données ;
- évolutions facilitées.

Les tables de référence ne sont pas destinées à contenir des informations
propres à une enquête.

Elles décrivent uniquement des classifications communes à toutes les
investigations.

---

## 5.1 types_preuve

Cette table décrit les différents types de preuves pouvant être enregistrés.

Exemples :

- capture d'écran ;
- photographie ;
- document ;
- archive ;
- vidéo ;
- export JSON.

Les preuves référencent cette table afin d'identifier leur nature.

---

## 5.2 types_entite

Cette table définit les différents types d'entités manipulés par
l'application.

Exemples :

- personne ;
- organisation ;
- adresse IP ;
- nom de domaine ;
- adresse email ;
- numéro de téléphone ;
- compte de réseau social.

Chaque entité appartient à un type unique.

---

## 5.3 types_source

Cette table décrit les catégories de sources utilisées pendant une enquête.

Exemples :

- site web ;
- moteur de recherche ;
- réseau social ;
- API ;
- document ;
- base publique.

Les recherches utilisent ces informations pour caractériser leurs sources.

---

## 5.4 types_outil

Cette table répertorie les outils utilisés pendant les recherches.

Exemples :

- navigateur web ;
- dig ;
- whois ;
- curl ;
- nmap ;
- outil interne de Labfy Investigation.

Cette classification permet de documenter précisément les méthodes employées
lors de la collecte d'informations.

---

## Gestion des identifiants

Contrairement aux objets métier, les tables de référence utilisent des
identifiants entiers.

Ces identifiants sont définis par l'application.

Ils sont stables et ne doivent pas être modifiés manuellement.

Leur objectif est uniquement de faciliter les références entre les tables.

Ils ne constituent pas des identifiants métier.

---

## Évolutions

L'ajout d'un nouveau type ne nécessite généralement aucune modification du
schéma de la base de données.

Il suffit d'ajouter une nouvelle ligne dans la table concernée.

En revanche, la suppression ou la modification d'un type existant doit être
réalisée avec précaution afin de préserver la cohérence des données déjà
enregistrées.

### Traçabilité OSINT structurée — schéma V3

La table `osint_executions` conserve chaque exécution terminée, y compris
lorsque la révision est annulée. Elle contient notamment :

- l'outil et sa version ;
- la cible et les arguments de l'exécution ;
- la date d'exécution ;
- les sorties standard et d'erreur sous forme de BLOB ;
- leur empreinte SHA-256.

Les tables `osint_execution_entities` et `osint_execution_relations` relient
l'exécution aux objets intégrés. Le champ `disposition` distingue les objets
créés des objets réutilisés. Ces liaisons sont ajoutées dans la même
transaction que l'intégration DNS.

Les descriptions métier restent présentes pour la lisibilité, mais les
entités DNS demeurent des résultats OSINT à vérifier et non des faits établis.

Le menu contextuel du workspace permet de consulter cet historique pour
l'entité ou la relation sélectionnée. La vue est strictement en lecture seule
et expose les métadonnées, les sorties brutes rendues en UTF-8, l'empreinte et
les objets liés avec leur disposition `created` ou `reused`.

Depuis ce détail, une vérification explicite recalcule l'empreinte SHA-256 à
partir des BLOB `stdout_raw` et `stderr_raw`, séparés par l'octet nul défini
lors de l'enregistrement. Le résultat indique si les sorties sont intactes,
altérées ou impossibles à vérifier. Ce contrôle est strictement en lecture
seule : aucune sortie ni empreinte persistée n'est corrigée automatiquement.

### Comptes sociaux structurés — schéma V4

La table `comptes_sociaux` complète une ligne de `entites` sans dupliquer le
nœud affiché dans le graphe. Elle conserve la plateforme, l'URL de profil, le
pseudonyme affiché, l'identifiant stable facultatif, la première observation,
l'état observé et les notes factuelles. La contrainte unique sur
`(plateforme, url_profil)` évite les doublons.

Une capture, une vidéo ou un courriel déjà importé peut être rattaché au
compte via `preuve_entites`. La création des deux lignes et de cette liaison
est transactionnelle : aucun nœud incomplet n'est conservé si une étape
échoue.

### Rôles d'enquête des personnes — schémas V5 et V6

La table `person_roles` associe une entité de type personne à une catégorie
d'enquête contrôlée : scammer présumé, victime, témoin, suspect, personne liée,
identité usurpée ou non catégorisée. La clé étrangère avec suppression en cascade évite les
rôles orphelins. Le code stable est persisté ; le libellé et la couleur restent
des choix de présentation.

Une personne sans ligne dans cette table est toujours interprétée comme non
catégorisée, ce qui garantit la compatibilité avec les enquêtes antérieures.

---

# 6. Tables de liaison

Le modèle de données de Labfy Investigation repose largement sur des relations
de type **plusieurs-à-plusieurs**.

Plutôt que de multiplier les colonnes ou de stocker plusieurs valeurs dans un
même champ, chaque relation complexe est représentée par une table de liaison
dédiée.

Cette approche permet :

- de conserver un schéma normalisé ;
- d'éviter les redondances ;
- de garantir l'intégrité référentielle ;
- d'ajouter des informations propres à une relation lorsque cela est
  nécessaire (par exemple une colonne `role`) ;
- de faire évoluer le modèle sans modifier les tables métier.

Toutes les tables de liaison utilisent une clé primaire composite afin
d'empêcher les doublons.

---

## 6.1 Liaisons des recherches

Les recherches constituent le point d'entrée de nombreuses informations.

Une même recherche peut produire plusieurs objets métier.

Inversement, un objet métier peut résulter de plusieurs recherches.

Les tables suivantes modélisent ces relations :

```text
recherche_preuves
recherche_entites
recherche_relations
recherche_hypotheses
```

Ces tables permettent notamment de répondre aux questions suivantes :

- quelles preuves ont été produites par cette recherche ?
- quelles recherches ont conduit à cette preuve ?
- quelles recherches ont confirmé cette relation ?
- quelle recherche est à l'origine d'une hypothèse ?

---

## 6.2 Liaisons des preuves

Les preuves constituent le socle de l'enquête.

Elles peuvent révéler plusieurs entités.

Inversement, une même entité peut apparaître dans plusieurs preuves.

Les relations suivantes assurent cette représentation :

```text
preuve_entites
relation_preuves
```

Ces tables permettent notamment de déterminer :

- quelles entités apparaissent dans une preuve ;
- quelles preuves soutiennent une relation.

---

## 6.3 Liaisons de la chronologie

La chronologie décrit les événements importants de l'enquête.

Un même événement peut être associé à plusieurs objets métier.

Les tables suivantes assurent ces liens :

```text
recherche_chronologie
preuve_chronologie
entite_chronologie
relation_chronologie
```

Cette architecture permet d'enrichir la chronologie sans modifier les tables
métier.

---

## 6.4 Liaisons des hypothèses

Les hypothèses constituent le niveau de raisonnement de l'enquête.

Elles peuvent être :

- soutenues ;
- contredites ;
- confirmées ;

par différents objets.

Les tables suivantes représentent ces relations :

```text
hypothese_preuves
hypothese_entites
hypothese_relations
```

Certaines de ces tables utilisent une colonne :

```text
role
```

Cette colonne précise le rôle joué par l'objet dans l'hypothèse.

Exemples :

```text
supports
contradicts
confirms
```

Cette approche offre une grande souplesse tout en conservant un schéma
relationnel simple.

---

## 6.5 Liaisons des tags

Les tags sont des annotations transversales.

Ils peuvent être associés à plusieurs types d'objets métier.

Les tables de liaison sont :

```text
tag_preuves
tag_recherches
tag_entites
tag_relations
tag_hypotheses
tag_chronologie
```

Chaque table assure une relation plusieurs-à-plusieurs entre les tags et les
objets concernés.

Cette organisation permet de conserver une architecture claire sans ajouter de
colonnes spécifiques dans chaque table métier.

---

## Clés primaires composites

Toutes les tables de liaison utilisent une clé primaire composite.

Exemple :

```sql
PRIMARY KEY (
    recherche_id,
    preuve_id
)
```

Cette contrainte garantit qu'une même relation ne peut être enregistrée qu'une
seule fois.

Elle évite les doublons sans nécessiter d'identifiant supplémentaire.

---

## Clés étrangères

Les tables de liaison utilisent systématiquement des clés étrangères.

Le comportement associé (`CASCADE`, `RESTRICT` ou `SET NULL`) est choisi en
fonction de la signification métier de la relation.

L'objectif est de préserver la cohérence des données tout en limitant les
suppressions accidentelles.

---

## Évolutivité

Les tables de liaison constituent l'un des principaux mécanismes d'extension du
modèle.

Lorsqu'une nouvelle relation apparaît entre deux objets métier, il est
généralement préférable d'ajouter une nouvelle table de liaison plutôt que de
modifier les tables existantes.

Cette stratégie limite les impacts sur le reste du schéma et facilite les
évolutions futures.

---

# 7. Contraintes

La qualité des données repose en grande partie sur les contraintes définies
dans le schéma SQL.

Ces contraintes permettent de détecter les incohérences le plus tôt possible,
avant même que les données ne soient utilisées par l'application.

Les contraintes SQL ne remplacent pas les validations réalisées dans le code C.

Les deux mécanismes sont complémentaires :

- SQLite garantit l'intégrité de la base de données ;
- l'application garantit la cohérence métier.

---

## 7.1 Clés primaires

Chaque objet métier possède une clé primaire.

Les objets métier utilisent un UUID :

```sql
id TEXT PRIMARY KEY
```

Les tables de référence utilisent des identifiants entiers.

Les tables de liaison utilisent des clés primaires composites.

Exemple :

```sql
PRIMARY KEY (
    preuve_id,
    entite_id
)
```

Cette approche garantit :

- l'unicité des objets ;
- l'absence de doublons dans les relations ;
- une meilleure portabilité des données.

---

## 7.2 Clés étrangères

Les relations entre objets sont protégées par des clés étrangères.

SQLite vérifie automatiquement que les objets référencés existent.

Le comportement lors de la suppression dépend du contexte métier.

Les principales stratégies utilisées sont :

### CASCADE

La suppression de l'objet parent entraîne automatiquement la suppression des
relations associées.

Cette stratégie est principalement utilisée dans les tables de liaison.

---

### RESTRICT

La suppression est refusée tant que l'objet est encore utilisé.

Cette stratégie protège les données importantes.

---

### SET NULL

La relation est conservée mais devient facultative.

Cette approche est utilisée lorsque l'information reste pertinente même si
l'objet référencé disparaît.

---

## 7.3 Contraintes CHECK

Les contraintes `CHECK` assurent la validité élémentaire des données.

Exemples :

- chaînes non vides ;
- valeurs comprises dans une plage ;
- états autorisés ;
- cohérence entre plusieurs colonnes.

Exemple :

```sql
CHECK (
    confiance BETWEEN 0 AND 100
)
```

Ou :

```sql
CHECK (
    status IN (
        'active',
        'archived',
        'deleted'
    )
)
```

Ces contraintes empêchent l'enregistrement de valeurs incohérentes.

---

## 7.4 Contraintes UNIQUE

Certaines informations doivent rester uniques.

Les contraintes `UNIQUE` permettent d'empêcher les doublons.

Exemples :

- nom d'une catégorie ;
- nom d'un tag ;
- autres identifiants définis comme uniques.

Lorsque cela est pertinent, la comparaison est réalisée sans tenir compte de
la casse (`COLLATE NOCASE`).

---

## 7.5 Validation applicative

Toutes les validations ne peuvent pas être exprimées uniquement en SQL.

Certaines règles restent du ressort de l'application.

Exemples :

- validité d'un UUID ;
- format d'une adresse email ;
- validité d'un IBAN ;
- calcul des empreintes cryptographiques ;
- contrôle de la taille des fichiers ;
- vérification du contenu d'une archive.

Le code C complète donc les protections offertes par SQLite.

---

## 7.6 Intégrité métier

Certaines règles concernent le fonctionnement même de l'enquête.

Par exemple :

- une preuve originale ne doit pas être modifiée ;
- une hypothèse ne constitue jamais un fait établi ;
- le journal est append-only ;
- une recherche décrit une action et non son résultat.

Ces règles sont appliquées par la couche métier de l'application et ne peuvent
pas être garanties uniquement par le schéma SQL.

---

## 7.7 Défense en profondeur

Labfy Investigation applique plusieurs niveaux de validation.

```text
Utilisateur
      │
      ▼
Interface GTK
      │
      ▼
Validation métier
      │
      ▼
Couche Database
      │
      ▼
SQLite
```

Chaque niveau vérifie les données avant de les transmettre au niveau suivant.

Cette approche permet :

- de détecter rapidement les erreurs ;
- de limiter les incohérences ;
- de protéger la base contre les corruptions accidentelles ;
- de simplifier le débogage.

---

# 8. Index

Les index ont pour objectif d'améliorer les performances des requêtes les plus
fréquentes sans modifier le modèle de données.

Ils permettent à SQLite de localiser rapidement les enregistrements recherchés
sans parcourir l'intégralité des tables.

Les index sont définis dès la conception du schéma afin de garantir des
performances cohérentes, même lorsque les bases de données deviennent
volumineuses.

Le choix des index repose sur les usages attendus de Labfy Investigation et
non sur une optimisation prématurée.

---

## 8.1 Objectifs

Les index sont principalement utilisés pour accélérer les opérations suivantes :

- recherche d'un objet par son identifiant ;
- navigation entre les objets liés ;
- filtrage par statut ;
- tri chronologique ;
- recherche par catégorie ;
- recherche par tag ;
- consultation de la chronologie ;
- consultation du journal.

Ils permettent également de limiter le coût des nombreuses jointures entre les
tables métier.

---

## 8.2 Politique

Les index sont créés selon plusieurs principes.

### Clés étrangères

Les colonnes utilisées comme clés étrangères sont systématiquement indexées.

Cette règle améliore les performances des jointures ainsi que les contrôles
d'intégrité réalisés par SQLite.

---

### Dates

Les colonnes utilisées pour les tris chronologiques sont indexées.

Exemples :

- `created_at`
- `updated_at`
- `event_time`

Ces index facilitent la consultation des événements récents et des historiques
d'une enquête.

---

### États

Les colonnes `status` sont indexées lorsque leur utilisation est fréquente.

Cela permet notamment de retrouver rapidement les objets :

- actifs ;
- archivés ;
- supprimés logiquement.

---

### Tables de liaison

Les tables de liaison possèdent une clé primaire composite.

Des index complémentaires peuvent être ajoutés lorsque les recherches sont
souvent réalisées dans le sens inverse de cette clé.

Exemple :

```sql
PRIMARY KEY (
    tag_id,
    preuve_id
)
```

Un index supplémentaire sur :

```sql
preuve_id
```

permet de retrouver rapidement tous les tags associés à une preuve.

---

### Équilibre

Chaque index améliore certaines requêtes mais augmente également :

- la taille de la base de données ;
- le coût des insertions ;
- le coût des mises à jour.

Les index sont donc créés uniquement lorsqu'ils répondent à un besoin
identifié.

Aucun index n'est ajouté sans justification fonctionnelle.

---

## 8.3 Évolution

La politique d'indexation pourra évoluer à mesure que l'application grandira.

Toute modification devra être basée sur :

- des mesures de performances ;
- des profils d'utilisation réels ;
- des besoins fonctionnels identifiés.

Les optimisations devront privilégier la simplicité du schéma et préserver la
compatibilité avec les versions précédentes de la base de données.

---

# 9. Versionnement

Le schéma de la base de données est versionné.

Chaque base SQLite créée par Labfy Investigation possède un numéro de version
permettant à l'application de déterminer si elle est compatible avec le
logiciel utilisé.

Le versionnement constitue un élément essentiel de la pérennité des données.

Une enquête créée aujourd'hui doit pouvoir être ouverte plusieurs années plus
tard par une version plus récente de l'application.

---

## 9.1 Version du schéma

La version du schéma est enregistrée dans la table :

```text
metadata
```

La clé :

```text
schema_version
```

identifie la version de l'architecture utilisée par la base.

Pour cette première version :

```text
schema_version = 1
```

Cette valeur constitue la référence de toute la documentation associée à la
V1.

---

## 9.2 Compatibilité

Le logiciel vérifie la version du schéma avant d'ouvrir une enquête.

Trois situations sont possibles :

### Même version

La base est compatible.

L'ouverture de l'enquête peut se poursuivre normalement.

---

### Version plus ancienne

Une migration peut être proposée si elle existe.

La migration permet d'adapter progressivement la base vers une version plus
récente du schéma.

---

### Version plus récente

Le logiciel refuse d'ouvrir la base.

Cette situation indique généralement que l'enquête a été créée avec une
version plus récente de Labfy Investigation.

Une ancienne version du logiciel ne doit jamais tenter de modifier une base
qu'elle ne comprend pas.

---

## 9.3 Migrations

Toute évolution incompatible du schéma devra être réalisée à l'aide d'une
migration.

Une migration est une opération contrôlée permettant de transformer une base
existante vers une nouvelle version du schéma.

Chaque migration devra respecter les règles suivantes :

- être transactionnelle ;
- préserver les données existantes ;
- être documentée ;
- être reproductible ;
- pouvoir détecter les erreurs.

Avant toute migration, une sauvegarde complète de la base devra être réalisée.

En cas d'échec, la base devra être restaurée dans son état initial.

---

## 9.4 Compatibilité ascendante

Une fois une version du schéma publiée, elle devient une référence.

Les évolutions futures devront privilégier :

- l'ajout de nouvelles tables ;
- l'ajout de nouvelles colonnes compatibles ;
- l'ajout de nouveaux index ;
- l'ajout de nouvelles contraintes lorsque cela reste compatible.

Les modifications destructives devront être évitées autant que possible.

---

## 9.5 Politique d'évolution

Les évolutions du schéma suivent les principes suivants :

- ne jamais casser une enquête existante sans migration ;
- documenter chaque changement ;
- conserver la cohérence du modèle métier ;
- maintenir la compatibilité avec les outils de développement et de test.

Toute modification du schéma doit être accompagnée :

- d'une mise à jour de `schema_v1.sql` (ou de la version concernée) ;
- d'une mise à jour de la documentation ;
- d'une adaptation des tests unitaires ;
- d'une revue d'architecture si le changement impacte le modèle métier.

---

## 9.6 Cycle de vie du schéma

Le cycle de vie d'une nouvelle version suit les étapes suivantes :

```text
Conception
      │
      ▼
Validation
      │
      ▼
Implémentation SQL
      │
      ▼
Tests unitaires
      │
      ▼
Documentation
      │
      ▼
Publication
      │
      ▼
Maintenance
```

Une version publiée ne doit plus être modifiée directement.

Toute évolution ultérieure devra conduire à une nouvelle version du schéma.

---

## 9.7 Philosophie

Le schéma de la base de données constitue un contrat entre l'application et les
données.

Une évolution du schéma ne doit jamais être décidée uniquement pour simplifier
le code.

Au contraire, le code de l'application doit s'adapter au schéma validé.

Cette approche garantit :

- la stabilité des données ;
- la lisibilité du projet ;
- la fiabilité des migrations ;
- la pérennité des enquêtes.

---

# 10. Architecture C

La base de données est entièrement encapsulée dans une couche logicielle
dédiée.

Les autres composants de Labfy Investigation ne manipulent jamais directement
SQLite.

Cette séparation permet d'isoler les détails d'implémentation de la base de
données du reste de l'application.

L'objectif est de garantir une architecture modulaire, testable et facilement
maintenable.

---

## 10.1 Organisation

La couche Database est répartie entre les répertoires :

```text
include/database/
src/database/
```

Chaque objet métier possède son propre module.

Cette organisation permet de limiter les dépendances entre les différents
composants.

L'ensemble de la couche Database constitue l'unique point d'accès aux données.

---

## 10.2 Modules

L'organisation cible est la suivante :

```text
database/
├── database.c
├── database_connection.c
├── schema.c
├── metadata.c
├── investigation.c
├── source.c
├── recherche.c
├── preuve.c
├── entite.c
├── relation.c
├── chronologie.c
├── journal.c
├── hypothese.c
├── categorie.c
└── tag.c
```

Chaque fichier `.c` possède son fichier d'en-tête correspondant dans :

```text
include/database/
```

Cette organisation représente l'architecture cible de la couche Database.

Tous les modules ne sont pas nécessairement implémentés dès la première
version de l'application.

---

## 10.3 Responsabilités

Chaque module possède une responsabilité clairement définie.

Par exemple :

- `preuve.c` gère les preuves ;
- `entite.c` gère les entités ;
- `relation.c` gère les relations ;
- `source.c` gère les sources ;
- `hypothese.c` gère les hypothèses.

Un module ne doit jamais gérer plusieurs concepts métier indépendants.

Les opérations transversales sont regroupées dans des modules spécifiques.

---

## 10.4 API publique

Les autres composants de l'application utilisent exclusivement les fonctions
publiques exposées par les fichiers d'en-tête.

Ils ne doivent jamais :

- ouvrir directement une base SQLite ;
- construire une requête SQL ;
- préparer une instruction SQLite ;
- manipuler les structures internes de SQLite.

Cette règle garantit une séparation claire entre la logique métier et la
persistance des données.

---

## 10.5 Gestion des erreurs

Toutes les erreurs provenant de SQLite sont traitées par la couche Database.

Les fonctions publiques retournent des valeurs adaptées à leur usage :

- `bool` pour les opérations simples ;
- pointeurs vers des objets en cas de création ou de lecture ;
- `NULL` lorsqu'une opération échoue.

Les messages d'erreur détaillés sont journalisés dans un point unique afin de
faciliter le débogage.

---

## 10.6 Gestion mémoire

La couche Database respecte les conventions mémoire du projet.

Les objets retournés par l'API possèdent un propriétaire clairement identifié.

Chaque fonction de création possède une fonction de destruction associée.

Exemple :

```c
preuve_new(...)
preuve_free(...)
```

Cette règle garantit une gestion mémoire simple et prévisible.

---

## 10.7 Transactions

Les transactions SQLite sont gérées exclusivement par la couche Database.

Une fonction appelante ne doit jamais ouvrir ou fermer directement une
transaction.

Cette centralisation garantit la cohérence des opérations complexes et limite
les risques d'états intermédiaires incohérents.

---

## 10.8 Évolutivité

L'organisation modulaire permet d'ajouter de nouvelles fonctionnalités sans
modifier les modules existants.

L'ajout d'un nouvel objet métier conduit généralement à la création :

- d'une nouvelle table ;
- d'un nouveau module `.c` ;
- d'un nouveau fichier d'en-tête ;
- de nouveaux tests unitaires.

Cette approche limite les régressions et facilite la maintenance du projet.

---

## 10.9 Tests unitaires

Chaque module Database doit disposer de ses propres tests unitaires.

Les tests vérifient notamment :

- les créations d'objets ;
- les lectures ;
- les mises à jour ;
- les suppressions logiques ;
- les contraintes d'intégrité ;
- les cas d'erreur.

Aucune fonctionnalité de la couche Database ne doit être considérée comme
terminée sans tests associés.

---

## 10.10 Principe directeur

La couche Database constitue la seule interface entre l'application et les
données persistantes.

Les autres composants manipulent uniquement des objets métier et des fonctions
publiques.

Cette séparation garantit :

- une architecture claire ;
- une meilleure testabilité ;
- une maintenance simplifiée ;
- une évolution indépendante de la base de données et de l'interface
  utilisateur.

---

# 11. Diagramme général

Le schéma relationnel complet de Labfy Investigation comporte un nombre
important de tables et de relations.

Afin de faciliter sa compréhension, ce document présente une vue simplifiée du
modèle métier.

L'objectif de ce diagramme n'est pas de représenter chaque clé étrangère mais
de montrer la circulation de l'information au sein d'une enquête.

---

## 11.1 Vue métier

```text
                            Investigation
                                  │
      ┌───────────────────────────┼───────────────────────────┐
      │                           │                           │
      ▼                           ▼                           ▼
  Métadonnées                 Collecte                  Classification
      │                           │                           │
      │                     ┌─────┴─────┐               ┌─────┴─────┐
      │                     ▼           ▼               ▼           ▼
      │                 Sources    Recherches     Catégories      Tags
      │                                  │
      │                                  ▼
      │                              Preuves
      │                                  │
      │                         ┌────────┴────────┐
      │                         ▼                 ▼
      │                     Entités          Chronologie
      │                         │
      │                         ▼
      │                     Relations
      │                         │
      │                         ▼
      │                    Hypothèses
      │
      ▼
    Journal
```

---

## 11.2 Cycle d'une investigation

Le déroulement d'une enquête suit généralement les étapes suivantes.

```text
Source
    │
    ▼
Recherche
    │
    ▼
Preuve
    │
    ▼
Entité
    │
    ▼
Relation
    │
    ▼
Hypothèse
```

Ce cycle représente uniquement le cheminement principal.

En pratique, une enquête est itérative.

Une nouvelle recherche peut :

- confirmer une preuve existante ;
- découvrir une nouvelle entité ;
- modifier une relation ;
- renforcer ou affaiblir une hypothèse.

Le modèle de données a été conçu pour permettre ces allers-retours sans perdre
la traçabilité des informations.

---

## 11.3 Traçabilité

Chaque information importante peut être reliée à son origine.

```text
Source
    │
    ▼
Recherche
    │
    ▼
Preuve
    │
    ▼
Entité
    │
    ▼
Relation
    │
    ▼
Hypothèse
```

En parallèle :

```text
Recherche
      │
      ├──────────────► Chronologie
      │
      └──────────────► Journal
```

La chronologie décrit les événements de l'enquête.

Le journal enregistre les actions réalisées dans le logiciel.

Ces deux mécanismes sont volontairement séparés.

---

## 11.4 Philosophie du modèle

Le modèle de Labfy Investigation repose sur plusieurs principes.

Les objets métier représentent les concepts manipulés par les enquêteurs.

Les relations entre ces objets sont explicites et documentées.

Les fichiers originaux restent inchangés.

Toutes les informations produites pendant une enquête demeurent traçables.

Le modèle privilégie :

- la clarté ;
- la cohérence ;
- l'intégrité des données ;
- l'évolutivité.

L'objectif est de permettre à l'application de reproduire fidèlement le
raisonnement suivi pendant une investigation, plutôt que de simplement stocker
des fichiers ou des notes.

---

## 11.5 Disposition générique du graphe

La disposition visuelle du graphe est un état de présentation et non une
donnée métier. Elle est stockée dans `graph_layout_positions`, séparément des
tables `entites` et `relations`.

Chaque ligne associe un UUID de nœud à des coordonnées logiques et à une date
UTC de mise à jour. Un nœud peut représenter une entité ou une relation. SQLite
ne proposant pas de clé étrangère polymorphe, l'intégrité de ce stockage est
assurée par deux triggers qui suppriment la position correspondante lors de la
suppression physique d'une entité ou d'une relation.

La table historique `graph_node_positions` ne référençait que `entites(id)`.
À l'ouverture d'une enquête, ses lignes sont copiées de manière idempotente
vers `graph_layout_positions`, puis retirées de la table historique afin
qu'une réinitialisation volontaire de la disposition ne puisse pas restaurer
des coordonnées obsolètes.

Cette stratégie préserve les positions existantes tout en permettant aux
nœuds de relation de suivre exactement le même cycle de chargement,
d'enregistrement et de réinitialisation que les nœuds d'entité.

---

# 12. Conclusion

La base de données de Labfy Investigation constitue bien davantage qu'un simple
espace de stockage.

Elle représente le modèle métier de l'application.

Chaque décision d'architecture a été prise afin de répondre à trois objectifs
principaux :

- préserver l'intégrité des données ;
- garantir leur traçabilité ;
- permettre l'évolution du logiciel sur le long terme.

Le schéma V1 repose sur une séparation claire des responsabilités.

Chaque table représente un concept métier unique.

Les relations entre ces concepts sont explicites et documentées.

Les contraintes SQL assurent la cohérence des données tandis que la couche
Database applique les règles métier qui ne peuvent être exprimées dans le
schéma relationnel.

Cette architecture permet de représenter fidèlement le déroulement d'une
investigation numérique :

```text
Source
    │
    ▼
Recherche
    │
    ▼
Preuve
    │
    ▼
Entité
    │
    ▼
Relation
    │
    ▼
Hypothèse
```

À chaque étape, les informations restent reliées à leur origine et peuvent
être replacées dans leur contexte grâce à la chronologie et au journal
d'audit.

Le choix d'utiliser une base SQLite autonome pour chaque enquête garantit la
portabilité, facilite les sauvegardes et simplifie les échanges entre
enquêteurs.

Le schéma V1 constitue désormais la référence de développement de Labfy
Investigation.

Toute évolution future devra respecter les principes définis dans cette
documentation :

- documenter les besoins avant toute modification ;
- préserver la compatibilité des données ;
- privilégier l'ajout de nouveaux objets plutôt que la modification des objets
  existants ;
- accompagner toute évolution d'une migration, d'une documentation et de tests
  adaptés.

Le schéma SQL, la documentation et les tests unitaires forment un ensemble
indissociable.

Aucun de ces éléments ne doit évoluer indépendamment des autres.

L'objectif final n'est pas uniquement de stocker des données, mais de fournir
une base solide permettant de développer un logiciel d'investigation fiable,
maintenable et pérenne.

---

## Statut

À la publication de ce document :

- le schéma V1 est considéré comme stable ;
- il constitue la référence officielle de la couche Database ;
- toute évolution incompatible devra faire l'objet d'une nouvelle version du
  schéma et d'une migration documentée.

---

## Références

Documents associés :

- `database/schema_v1.sql`
- `docs/database/SCHEMA_AUDIT_V1.md`
- `docs/database/DATABASE_ARCHITECTURE.md`
- `docs/CONVENTIONS.md`

Ces documents constituent ensemble la documentation de référence de
l'architecture de la base de données de Labfy Investigation.
