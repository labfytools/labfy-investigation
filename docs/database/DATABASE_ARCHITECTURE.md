# Architecture de la base de données

> **Statut :** architecture courante  
> **Version du schéma :** V10  
> **Dernière mise à jour :** 2026-07-24  
> **Source de vérité détaillée :** `SCHEMA_AUDIT_CURRENT.md`

---

## 1. Objet

Chaque enquête Labfy Investigation possède une base SQLite autonome :

```text
00_BaseDeDonnees/
└── Enquete.sqlite
```

La base contient les données structurées et les références nécessaires à
l'enquête.

Les fichiers originaux et dérivés restent dans l'arborescence de l'enquête. Ils
ne sont pas stockés comme blobs dans SQLite.

Cette architecture vise à garantir :

- portabilité ;
- intégrité ;
- traçabilité ;
- migrations contrôlées ;
- compréhension durable du modèle ;
- séparation des données métier et de l'état de présentation.

Pour l'inventaire détaillé des tables, contraintes et constats d'audit,
consulter :

```text
docs/database/SCHEMA_AUDIT_CURRENT.md
```

---

## 2. Sources de vérité

L'état réel du schéma est déterminé par :

1. les constantes de version dans le code ;
2. `database/schema_v1.sql` à `database/schema_v10.sql` ;
3. `database/schema_current.sql` ;
4. les fonctions d'installation et de migration ;
5. `tests/test_database.c` et les tests DAO ;
6. l'audit courant.

Les anciens audits sont historiques.

Un document V1 ne décrit pas le schéma V10.

---

## 3. Principes

### 3.1 Une base par enquête

Une base contient une seule enquête.

Les données métier de plusieurs enquêtes ne sont pas mélangées.

### 3.2 Fichiers hors de SQLite

SQLite conserve notamment :

- chemins relatifs ;
- noms ;
- tailles ;
- empreintes ;
- métadonnées ;
- provenance ;
- relations.

Les fichiers restent sur disque.

### 3.3 UUID

Les objets métier utilisent généralement :

```sql
id TEXT PRIMARY KEY
```

contenant un UUID généré par l'application.

Les tables de référence peuvent utiliser une clé entière.

Cette règle est vérifiée par le schéma réel et non appliquée aveuglément.

### 3.4 UTC

Les dates persistées sont en UTC.

Format de référence lorsqu'une date complète est exigée :

```text
YYYY-MM-DDTHH:MM:SSZ
```

### 3.5 Clés étrangères

Chaque connexion active :

```sql
PRAGMA foreign_keys = ON;
```

Les actions `CASCADE`, `RESTRICT` et `SET NULL` sont choisies selon la
sémantique de chaque relation.

### 3.6 Requêtes préparées

Toute valeur variable utilise un statement préparé et des paramètres liés.

La concaténation de données utilisateur dans le SQL est interdite.

### 3.7 Transactions

Une opération critique multi-étapes est atomique.

Les migrations, imports, reclassements et intégrations de propositions doivent
prévoir un rollback complet.

### 3.8 Valeur brute et valeur interprétée

Le modèle distingue lorsque nécessaire :

- valeur brute ;
- valeur normalisée ;
- valeur dérivée ;
- correction utilisateur ;
- statut de vérification ;
- confiance ;
- provenance.

La valeur brute n'est jamais modifiée pour refléter une correction ultérieure.

---

## 4. Architecture d'accès

```text
Services métier
      ↓
DAO
      ↓
Infrastructure Database
      ↓
SQLite
```

### 4.1 Infrastructure Database

`src/database` gère :

- ouverture et fermeture ;
- activation des pragmas ;
- version du schéma ;
- installation ;
- migrations ;
- statements ;
- transactions ;
- erreurs SQLite.

### 4.2 DAO

`src/dao` gère les requêtes métier :

- insertion ;
- lecture ;
- mise à jour autorisée ;
- recherche ;
- transformation ligne ↔ modèle.

### 4.3 Services

Les services définissent les workflows et frontières transactionnelles qui
impliquent plusieurs DAO ou le système de fichiers.

### 4.4 Interface

Les vues et widgets n'exécutent aucune requête SQL.

---

## 5. Versionnement

La version courante est stockée dans :

```text
metadata.schema_version
```

L'application connaît également une constante de version courante.

Une base plus récente que l'application doit être refusée.

Une base plus ancienne est migrée étape par étape jusqu'à la version courante.

Les scripts sont conservés :

```text
database/schema_v1.sql
database/schema_v2.sql
database/schema_v3.sql
database/schema_v4.sql
database/schema_v5.sql
database/schema_v6.sql
database/schema_v7.sql
database/schema_v8.sql
database/schema_v9.sql
database/schema_v10.sql
```

`database/schema_current.sql` contient des extensions ou réparations
idempotentes nécessaires au schéma courant.

Ce fichier ne remplace pas les migrations versionnées.

---

## 6. Création d'une base neuve

Une base neuve est initialisée dans une transaction.

Le flux général est :

```text
ouverture SQLite
    ↓
PRAGMA foreign_keys = ON
    ↓
BEGIN
    ↓
installation V1 à V10
    ↓
application du schéma courant idempotent
    ↓
métadonnées et enquête
    ↓
COMMIT
```

Un échec provoque un rollback.

Une base neuve doit aboutir directement à :

```text
schema_version = 10
```

---

## 7. Chaîne de migrations

Résumé fonctionnel :

| Version | Évolution principale |
|---|---|
| V1 | socle métier initial de l'enquête |
| V2 | persistance enrichie des preuves |
| V3 | provenance structurée des exécutions OSINT |
| V4 | comptes sociaux |
| V5 | rôles d'enquête des personnes et présentation associée |
| V6 | extensions liées aux personnes et identités observées |
| V7 | extractions liées aux preuves ou entités |
| V8 | persistance de l'état du graphe et du viewport |
| V9 | types canoniques de relations |
| V10 | entités bancaires et types de relations du pivot e-mail |

Chaque migration possède une fonction dédiée.

Le numéro de version est mis à jour uniquement après l'installation réussie de
la nouvelle version.

---

## 8. Domaines du schéma

Le schéma couvre plusieurs domaines.

### 8.1 Identité de l'enquête

- métadonnées techniques ;
- identité de l'enquête ;
- version du schéma.

### 8.2 Référentiels

- types de preuves ;
- types d'entités ;
- types d'outils et sources selon le schéma ;
- types canoniques de relations ;
- vocabulaires contrôlés gérés par le code et les contraintes.

### 8.3 Preuves

- enregistrement des preuves ;
- classification ;
- chemins relatifs ;
- empreintes ;
- taille ;
- source ;
- intégrité ;
- associations avec d'autres objets.

La preuve originale reste sur disque.

### 8.4 Entités

- entités génériques ;
- comptes sociaux ;
- personnes et rôles ;
- extensions spécialisées ;
- comptes bancaires V10.

### 8.5 Relations

Une relation relie une source et une cible.

Les types de relations sont centralisés pour éviter les variantes textuelles
incohérentes.

Les preuves peuvent soutenir ou documenter une relation selon les tables de
liaison prévues.

### 8.6 OSINT et provenance

Le schéma conserve selon les fonctionnalités :

- exécution ;
- outil et version ;
- cible ;
- arguments ;
- dates ;
- code de retour ;
- sorties brutes ;
- empreintes ;
- liens vers les objets créés ou réutilisés.

### 8.7 Extractions

Une extraction est reliée à une preuve ou à une entité source.

Elle conserve l'outil, la date et son origine logique.

Les fichiers ou textes produits doivent rester traçables.

### 8.8 Graphe

Les positions et le viewport sont des données de présentation.

Ils restent séparés des entités et relations métier.

Une clé étrangère polymorphe n'étant pas disponible dans SQLite, le nettoyage
de certaines positions génériques est assuré par des triggers.

---

## 9. V10 — Entités bancaires

La V10 ajoute :

```text
bank_account_entities
```

Cette table conserve :

- `id` ;
- `iban` ;
- `bic` ;
- `holder_name` ;
- `bank_name` ;
- `bank_address` ;
- `country_code` ;
- `bank_code` ;
- `branch_code` ;
- `account_number` ;
- `rib_key` ;
- `verification_status` ;
- `provenance_kind` ;
- `evidence_id` ;
- `extraction_id` ;
- `created_at` ;
- `updated_at`.

### 9.1 Statuts contrôlés

```text
proposed
confirmed
rejected
conflicted
invalid
```

### 9.2 Provenances contrôlées

```text
observed
ocr
header
metadata
derived
manual
```

### 9.3 Références

```text
evidence_id   → preuves(id)     ON DELETE SET NULL
extraction_id → extractions(id) ON DELETE SET NULL
```

La disparition d'une source ne supprime pas automatiquement la donnée bancaire
structurée.

### 9.4 Index

La V10 crée des index sur :

- l'IBAN ;
- la preuve source.

### 9.5 Interprétation

Un nom observé comme titulaire ne prouve pas que cette personne est l'auteur
d'une fraude.

Un IBAN détecté par OCR reste une proposition tant que sa validation et sa
confirmation n'ont pas été établies.

Une correction OCR ne doit jamais remplacer silencieusement la valeur brute.

---

## 10. Types de relations V10

La V10 ajoute les codes système suivants :

```text
sent_from
sent_to
reply_to
has_attachment
relayed_by
uses_domain
held_at
named_as_holder_of
supports
```

Les codes sont stables.

Les libellés français peuvent évoluer sans migration des codes.

La formulation d'une relation doit rester factuelle.

Exemples :

- `relayed_by` décrit un relais observé dans la chaîne SMTP ;
- `named_as_holder_of` décrit un nom présenté comme titulaire ;
- aucun de ces liens ne constitue automatiquement une attribution criminelle.

---

## 11. Schéma courant idempotent

`database/schema_current.sql` complète les structures nécessaires à
l'ouverture, notamment autour :

- des extractions ;
- des positions du graphe ;
- du viewport ;
- des types de relations ;
- des triggers de nettoyage.

Les instructions utilisent `IF NOT EXISTS` ou des opérations idempotentes
lorsque cela est nécessaire.

Ce mécanisme sert à maintenir la compatibilité, mais ne doit pas devenir une
migration cachée non versionnée.

Toute évolution métier persistante significative doit recevoir une nouvelle
version de schéma.

---

## 12. Intégrité et suppression

### 12.1 Suppression logique

La suppression logique est privilégiée lorsque le modèle prévoit un champ
d'état et que la traçabilité l'exige.

### 12.2 Suppression physique

Elle reste possible pour certaines tables selon leurs contraintes.

La règle doit être définie table par table.

### 12.3 Vérifications

Les tests de migration doivent exécuter :

```sql
PRAGMA integrity_check;
PRAGMA foreign_key_check;
```

Une migration n'est pas considérée sûre uniquement parce que son script ne
retourne pas d'erreur.

---

## 13. Tests attendus

La couche Database doit couvrir :

- création d'une base neuve V10 ;
- lecture de la version ;
- refus d'une version future ;
- migration d'une ancienne base ;
- conservation des données ;
- rollback provoqué ;
- contraintes ;
- clés étrangères ;
- statements ;
- transactions imbriquées ou interdites selon l'API ;
- réouverture idempotente ;
- DAO principaux ;
- V9 vers V10 avec données synthétiques ;
- table `bank_account_entities` ;
- unicité et réutilisation des types de relations.

Matrice minimale recommandée :

| Entrée | Résultat |
|---|---|
| base neuve | V10 valide |
| V1 | migration complète vers V10 |
| V9 | migration directe vers V10 |
| V10 | réouverture sans modification destructive |
| version > V10 | refus clair |
| migration forcée en échec | rollback complet |

---

## 14. Procédure de modification

Pour une future V11 :

1. auditer le schéma V10 ;
2. définir les invariants ;
3. ajouter `schema_v11.sql` ;
4. ajouter `schema_install_v11()` ;
5. ajouter `database_migrate_v10_to_v11()` ;
6. raccorder la boucle de migration ;
7. mettre à jour la version courante ;
8. adapter la création d'une base neuve ;
9. adapter `schema_current.sql` seulement si nécessaire ;
10. ajouter une fixture V10 ;
11. tester le rollback ;
12. exécuter les deux pragmas d'intégrité ;
13. mettre à jour ce document ;
14. mettre à jour `SCHEMA_AUDIT_CURRENT.md` ;
15. conserver une copie versionnée `SCHEMA_AUDIT_V11.md`.

---

## 15. Limites et points de vigilance

- V10 fournit le socle bancaire, mais ne termine pas à elle seule le ticket
  complet du pivot EML ;
- les valeurs OCR ne doivent pas être confirmées automatiquement ;
- la provenance doit rester suffisante pour revenir à la source ;
- le schéma courant idempotent ne doit pas masquer l'absence d'une migration ;
- les DAO doivent rester la seule couche de requêtes métier ;
- les widgets ne doivent jamais accéder directement à SQLite ;
- les fixtures utilisent exclusivement des données synthétiques ;
- aucune base réelle d'enquête ne doit être ajoutée au dépôt.

---

## 16. Références

```text
database/schema_v1.sql
database/schema_v2.sql
database/schema_v3.sql
database/schema_v4.sql
database/schema_v5.sql
database/schema_v6.sql
database/schema_v7.sql
database/schema_v8.sql
database/schema_v9.sql
database/schema_v10.sql
database/schema_current.sql

src/database/database.c
src/database/schema.c
src/database/statement.c
src/database/transaction.c
src/database/error.c

src/dao/
tests/test_database.c
docs/database/SCHEMA_AUDIT_CURRENT.md
```
