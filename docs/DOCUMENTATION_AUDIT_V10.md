# Audit de la documentation — état V10

> Projet : Labfy Investigation  
> Date : 2026-07-24  
> Branche examinée : `main`  
> Commit de référence : `613d2096bc5eeb2c1c4f60ae701292e19a2abe66`  
> Schéma SQLite courant : V10  
> Tickets ouverts au moment de l’audit : `#107` et `#42`

## Conclusion générale

La documentation doit être remise à niveau.

Le `README.md` a déjà reçu plusieurs ajouts récents et reste globalement utile.
En revanche, les documents de référence situés dans `docs/` décrivent encore
en grande partie l’architecture du premier socle ou de la V1 SQLite.

Le problème principal n’est pas l’absence de documentation, mais le mélange
entre :

- l’architecture historique ;
- l’architecture cible ;
- les fonctionnalités déjà implémentées ;
- les tickets encore ouverts ;
- le schéma SQLite réellement courant.

Ce mélange a déjà provoqué une mauvaise interprétation par un agent local, qui
a présenté la V1 et le ticket #023 comme l’état actuel du dépôt.

## Ordre de confiance à documenter

Toute analyse du projet doit respecter cet ordre :

1. code présent sur `main` ;
2. tests automatisés ;
3. migrations et scripts SQL ;
4. commits ;
5. tickets Forgejo fermés ;
6. tickets Forgejo ouverts ;
7. documentation générale ;
8. documents historiques et anciennes roadmaps.

Un ticket ouvert décrit un chantier, pas une fonctionnalité terminée.

Un audit versionné décrit la version qu’il nomme, pas automatiquement la
version courante.

---

# Matrice des documents

| Document | État | Priorité | Action |
|---|---|---:|---|
| `README.md` | Partiellement à jour | Haute | Corriger l’état courant et les commandes |
| `CHANGELOG.md` | Incomplet | Haute | Documenter V10 et le pivot EML préparatoire |
| `docs/ARCHITECTURE.md` | Obsolète | Critique | Réécrire l’architecture actuelle |
| `docs/DEVELOPMENT.md` | Partiellement faux | Critique | Aligner sur Makefile et couches réelles |
| `docs/CONVENTIONS.md` | Obsolète par endroits | Haute | Aligner langue, SQL, suppression et Git |
| `docs/ROADMAP.md` | Très obsolète | Critique | Remplacer par une roadmap vivante |
| `docs/BACKLOG.md` | Vide | Haute | Supprimer ou rediriger vers Forgejo |
| `docs/DEPENDENCE.md` | Globalement utile | Moyenne | Vérifier les outils réellement invoqués |
| `docs/database/DATABASE_ARCHITECTURE.md` | Marqué V1 | Critique | Transformer en architecture V10 |
| `docs/database/SCHEMA_AUDIT_V1.md` | Historique non signalé | Critique | Déplacer et ajouter un avertissement |
| `docs/database/SCHEMA_AUDIT_CURRENT.md` | À ajouter | Critique | Référence courante V10 |
| `docs/database/audits/SCHEMA_AUDIT_V10.md` | À ajouter | Haute | Photographie versionnée de V10 |

---

# 1. README.md

## Éléments corrects

Le README décrit correctement :

- le dépôt Forgejo comme source principale ;
- le caractère non opérationnel du logiciel ;
- le cadre légal et éthique ;
- l’autonomie d’une enquête ;
- l’immutabilité des preuves originales ;
- SQLite comme source de vérité ;
- l’exécution asynchrone ;
- l’interdiction de construire des commandes shell dynamiques ;
- une grande partie des fonctionnalités ajoutées jusqu’à V9/V10.

## Mises à jour nécessaires

### Ajouter explicitement le schéma courant

Ajouter dans l’état du projet :

```text
Version du schéma SQLite : V10
```

avec un lien vers :

```text
docs/database/SCHEMA_AUDIT_CURRENT.md
```

### Mettre à jour les fonctionnalités présentes

Ajouter dans la liste du socle actuel :

- pipeline EML asynchrone préparatoire ;
- extraction MIME sécurisée ;
- assainissement des noms de pièces jointes ;
- propositions bancaires IBAN/BIC ;
- vocabulaire contrôlé ;
- table `bank_account_entities` ;
- types canoniques de relations liés au pivot e-mail ;
- tests `test_controlled_vocab`, `test_bank_proposal` et
  `test_eml_pipeline_task`.

Préciser que le ticket `#107` reste ouvert et que le flux complet de révision
et d’intégration n’est pas encore terminé.

### Corriger le prochain chantier

Le texte indiquant que le prochain chantier porte sur l’initialisation
asynchrone du registre d’outils est obsolète.

Le chantier actif est désormais :

```text
#107 — Pivot e-mail forensique
```

Le ticket `#42` reste l’inventaire permanent des outils OSINT.

### Corriger les commandes de validation

Utiliser :

```sh
make clean
make -j8
make -j8 test
git diff --check
```

Prévoir un retour séquentiel uniquement si l’exécution parallèle échoue pour
une raison réelle.

---

# 2. CHANGELOG.md

## Problème

Le changelog ne reflète pas encore correctement l’ampleur du commit V10.

## Ajouts à documenter

Dans une section `Unreleased` ou `0.1.0-dev`, ajouter au minimum :

- schéma SQLite V10 ;
- table `bank_account_entities` ;
- statuts de vérification et provenances contrôlés ;
- nouveaux types système de relations ;
- pipeline EML asynchrone ;
- extraction MIME sécurisée ;
- détection et proposition IBAN/BIC ;
- vocabulaire contrôlé ;
- premiers objets d’intégration EML ;
- tests associés ;
- migration V9 vers V10 ;
- mise à jour de `schema_current.sql`.

## Règle recommandée

Le changelog doit décrire les capacités livrées, pas seulement les premiers
modules historiques de la fenêtre GTK.

---

# 3. docs/ARCHITECTURE.md

## Problèmes observés

Le document date du 14 juillet et décrit essentiellement le premier socle.

Il ne représente pas correctement :

- `src/dao` et `include/dao` ;
- les services métier ;
- les tâches asynchrones ;
- le gestionnaire de tâches ;
- le registre et le catalogue d’outils ;
- la provenance OSINT ;
- le graphe d’enquête ;
- les types canoniques de relations ;
- les extractions ;
- les comptes sociaux ;
- les personnes et leurs rôles ;
- le pipeline EML ;
- les propositions bancaires ;
- le vocabulaire contrôlé.

La phrase suivante est désormais incorrecte :

```text
Aucune requête SQL ne doit apparaître ailleurs.
```

Le dépôt possède une couche DAO contenant les requêtes métier.

## Architecture à documenter

```text
Interface GTK4
    ↓
Application et contrôleurs
    ↓
Services métier / tâches
    ↓
DAO et adaptateurs
    ├── SQLite
    ├── système de fichiers
    ├── outils externes
    └── futures API
    ↓
Modèles métier
```

## Règle SQL correcte

- l’infrastructure SQLite, les migrations et les transactions appartiennent à
  `src/database` ;
- les requêtes métier appartiennent aux DAO ;
- les widgets et vues n’accèdent jamais directement à SQLite ;
- les modèles ne connaissent pas SQLite.

## Organisation actuelle à ajouter

```text
include/core/
include/dao/
include/database/
include/models/
include/views/
include/widgets/

src/core/
src/dao/
src/database/
src/models/
src/views/
src/widgets/
```

## Statuts à utiliser

Pour éviter les ambiguïtés, chaque grande capacité doit être marquée :

```text
IMPLÉMENTÉ
PARTIEL
PRÉVU
HISTORIQUE
```

---

# 4. docs/DEVELOPMENT.md

## Incohérences avec le Makefile

### Cible `make docs`

Le document mentionne :

```sh
make docs
```

mais le Makefile courant ne définit pas de cible `docs`.

Action :

- supprimer cette commande ;
- ou créer réellement une cible Doxygen avant de la documenter.

### Options de compilation

Le document affirme que tout le projet utilise :

```text
-Wpedantic
```

Le Makefile principal utilise globalement :

```text
-std=c17 -Wall -Wextra -Werror
```

`-Wpedantic` est ajouté à plusieurs tests, mais pas aux `CFLAGS` globaux.

Il faut choisir une seule vérité :

- ajouter `-Wpedantic` globalement au Makefile ;
- ou corriger le document.

### Versions minimales

Le document annonce :

- GTK 4.10+ ;
- SQLite 3.45+ ;
- GCC 15+.

Ces versions ne sont pas imposées par le Makefile via `pkg-config`.

Pour la future cible Ubuntu, annoncer GCC 15+ risque d’exclure inutilement les
postes institutionnels.

Action :

- documenter les versions réellement minimales requises par les API utilisées ;
- distinguer l’environnement de développement Arch de la cible minimale
  Ubuntu.

### Architecture MVC

Le terme MVC est désormais trop réducteur.

Utiliser plutôt :

```text
architecture en couches
```

avec services, DAO, tâches et adaptateurs.

### Accès SQLite

Remplacer :

```text
Toutes les opérations sur SQLite passent par la couche DAO.
```

par :

```text
Les migrations, connexions, statements et transactions appartiennent à la
couche Database. Les requêtes métier passent par les DAO. Les vues, widgets et
modèles n’accèdent jamais directement à SQLite.
```

### Commandes

Mettre les commandes recommandées à jour :

```sh
make clean
make -j8
make -j8 test
git diff --check
```

---

# 5. docs/CONVENTIONS.md

## Langue du code

Le document affirme que le domaine métier est écrit en français, avec des
exemples tels que `preuve.c` et `entite.c`.

Le code actuel utilise principalement des noms techniques anglais :

```text
evidence_record
entity_dao
relation_service
bank_proposal
controlled_vocab
eml_pipeline_task
```

## Convention proposée

- code C, noms de fichiers, fonctions, structures et codes persistés : anglais
  technique cohérent ;
- libellés de l’interface et documentation utilisateur : français ;
- termes juridiques et métier : français dans les textes ;
- codes persistés : anglais stable, sans dépendre du libellé affiché.

Exemple :

```text
code persistant : proposed
libellé français : Proposé
```

## Suppression

Le document affirme que les objets importants ne sont jamais supprimés
immédiatement.

Cette règle doit être qualifiée table par table :

- suppression logique par défaut pour les objets de traçabilité ;
- suppression physique autorisée uniquement lorsqu’un DAO et les contraintes
  métier la prévoient explicitement ;
- aucune affirmation générale non vérifiée.

## Git

Remplacer la règle trop stricte :

```text
Un ticket terminé correspond à un commit.
```

par :

```text
Chaque commit porte un changement cohérent et vérifiable. Un ticket peut
nécessiter plusieurs commits, mais chaque commit doit compiler, être testé et
laisser la branche stable.
```

## Schéma

Ajouter :

- toute migration possède un script versionné ;
- une base existante est migrée transactionnellement ;
- la version courante est documentée dans
  `SCHEMA_AUDIT_CURRENT.md` ;
- les audits anciens sont historiques.

---

# 6. docs/ROADMAP.md

## Problème critique

Le document conserve plusieurs générations de roadmap dans le même fichier.

Il présente encore comme non réalisés :

- création d’une enquête ;
- validation ;
- initialisation SQLite ;
- `InvestigationProject` ;
- preuves ;
- entités ;
- relations ;
- tâches asynchrones ;
- plusieurs fonctions déjà présentes.

Il commence également une seconde roadmap au ticket `#031.1`.

Cette structure est la principale source de confusion documentaire du dépôt.

## Action recommandée

Remplacer complètement le fichier par une roadmap courte et vivante.

Structure proposée :

```markdown
# Roadmap

## Source de vérité
Les tickets Forgejo sont la source de vérité du suivi.

## État courant
- Schéma SQLite : V10
- Développement actif
- Logiciel non prêt pour la production

## Chantiers actifs
- #107 — Pivot e-mail forensique

## Inventaires permanents
- #42 — Arsenal OSINT

## Prochains axes
- terminer le flux de révision EML ;
- persister les propositions confirmées ;
- améliorer les tests de migration V9 → V10 ;
- mettre à jour le packaging Ubuntu ;
- poursuivre le graphe et les rapports.

## Historique
Consulter le changelog et les tickets fermés.
```

Déplacer l’ancienne roadmap vers :

```text
docs/archive/ROADMAP_LEGACY.md
```

ou la supprimer si Forgejo conserve déjà tout l’historique utile.

---

# 7. docs/BACKLOG.md

## État

Le fichier est vide.

## Action

Deux choix valables :

### Choix recommandé

Supprimer le fichier et utiliser uniquement Forgejo.

### Alternative

Conserver un simple pointeur :

```markdown
# Backlog

Le backlog actif est suivi dans Forgejo :

https://git.labfytools.com/fy59/labfy-investigation/issues

Ce fichier ne décrit pas l’état courant du projet.
```

Ne pas dupliquer les tickets dans un backlog Markdown.

---

# 8. docs/DEPENDENCE.md

## Éléments corrects

Le document couvre déjà :

- Tesseract ;
- ExifTool ;
- dig ;
- host ;
- whois ;
- curl ;
- OpenSSL ;
- qpdf ;
- John/pdf2john ;
- Sherlock ;
- Maigret ;
- Holehe.

## Vérifications nécessaires

Comparer les exécutables documentés avec :

- `tool_catalog.c` ;
- `tool_registry.c` ;
- `eml_pipeline_task.c` ;
- `rib_ocr.c` ;
- `exiftool_metadata.c` ;
- `pdf_password_recovery.c`.

Documenter pour chaque dépendance :

- obligatoire ou optionnelle ;
- fonctionnalité concernée ;
- commande de détection ;
- commande de version ;
- comportement lorsque l’outil est absent ;
- paquet Arch ;
- paquet Ubuntu ;
- compatibilité avec dépôts institutionnels restreints.

## Ajout recommandé

Ajouter une matrice :

| Outil | Fonction | Obligatoire | Dégradation si absent |
|---|---|---:|---|
| Tesseract | OCR | Non | Analyse partielle |
| ExifTool | Métadonnées | Non | Métadonnées externes absentes |
| dig | DNS | Non | Action DNS indisponible |
| John/pdf2john | PDF protégé | Non | Pas d’audit de mot de passe |

Ne jamais présenter un outil optionnel comme condition de démarrage.

---

# 9. docs/database/DATABASE_ARCHITECTURE.md

## Problème critique

L’en-tête indique encore :

```text
Statut : Stable (V1)
Schéma : V1
```

alors que la branche contient un schéma V10.

Le document a reçu un ajout V10 en fin de fichier, mais son titre, son statut et
la majorité de son inventaire restent centrés sur V1.

## Action recommandée

Transformer le document en architecture courante V10.

Nouvel en-tête :

```markdown
# Architecture de la base de données

> Statut : architecture courante
> Version du schéma : V10
> Source de vérité détaillée : SCHEMA_AUDIT_CURRENT.md
```

## Éléments à ajouter à l’inventaire

- `osint_executions` ;
- liaisons de provenance OSINT ;
- `comptes_sociaux` ;
- `person_roles` ;
- `extractions` ;
- tables de disposition et viewport du graphe ;
- `relation_types` ;
- `bank_account_entities`.

## Accès SQL

Remplacer la centralisation dans un unique module `database` par la séparation
réelle :

```text
Database : connexion, statements, transactions, schémas et migrations
DAO      : requêtes métier
Services : orchestration transactionnelle
GTK      : aucune requête SQL
```

## Migrations

Ajouter un résumé V1 → V10 et renvoyer vers l’audit courant pour les détails.

---

# 10. docs/database/SCHEMA_AUDIT_V1.md

## Problème

Le document est facilement interprété comme l’état actuel.

## Action

Déplacer vers :

```text
docs/database/audits/SCHEMA_AUDIT_V1.md
```

Ajouter en première ligne :

```markdown
> [!WARNING]
> Document historique. Cet audit décrit exclusivement la V1.
> Il ne représente pas le schéma courant.
> Consulter ../SCHEMA_AUDIT_CURRENT.md.
```

---

# 11. Nouveaux documents de base

Organisation recommandée :

```text
docs/database/
├── DATABASE_ARCHITECTURE.md
├── SCHEMA_AUDIT_CURRENT.md
└── audits/
    ├── SCHEMA_AUDIT_V1.md
    ├── SCHEMA_AUDIT_V9.md
    └── SCHEMA_AUDIT_V10.md
```

Règle :

- `SCHEMA_AUDIT_CURRENT.md` est mis à jour à chaque migration ;
- l’audit versionné reste immuable après validation ;
- le ticket de migration n’est pas terminé sans mise à jour documentaire.

---

# 12. Hygiène du dépôt hors documentation

Deux éléments présents à la racine doivent être contrôlés :

```text
meline59760.txt
watch_20260723-211101
```

Ils ne ressemblent pas à des fichiers source ou de documentation standards.

Avant de les conserver publiquement :

1. vérifier qu’ils utilisent exclusivement des données synthétiques ;
2. vérifier qu’ils ne contiennent aucune donnée d’enquête réelle ;
3. les déplacer vers une fixture clairement nommée s’ils sont utiles aux tests ;
4. les supprimer du dépôt sinon ;
5. ajouter les motifs nécessaires dans `.gitignore`.

Ne jamais versionner :

- `Enquete.sqlite` réelle ;
- captures ;
- e-mails réels ;
- RIB réels ;
- pièces jointes réelles ;
- exports bruts issus d’une enquête ;
- données personnelles non synthétiques.

---

# Ordre de mise à jour recommandé

## Commit 1 — Références SQLite

- ajouter `SCHEMA_AUDIT_CURRENT.md` ;
- ajouter l’audit V10 ;
- archiver l’audit V1 ;
- mettre à jour `DATABASE_ARCHITECTURE.md`.

## Commit 2 — Roadmap et suivi

- remplacer `ROADMAP.md` ;
- supprimer ou rediriger `BACKLOG.md` ;
- corriger la section suivi du README.

## Commit 3 — Architecture et développement

- mettre à jour `ARCHITECTURE.md` ;
- mettre à jour `DEVELOPMENT.md` ;
- mettre à jour `CONVENTIONS.md`.

## Commit 4 — Dépendances et présentation

- vérifier `DEPENDENCE.md` ;
- compléter `CHANGELOG.md` ;
- finaliser `README.md`.

## Commit 5 — Hygiène du dépôt

- auditer les fichiers isolés à la racine ;
- supprimer ou déplacer les éléments non conformes ;
- ajuster `.gitignore`.

---

# Critères de validation documentaire

La mise à jour est terminée lorsque :

- aucun document courant ne présente V1 comme schéma actuel ;
- V10 est identifiée comme version courante ;
- les audits historiques sont explicitement marqués historiques ;
- la roadmap ne duplique plus les tickets fermés ;
- `#107` est présenté comme chantier actif ;
- `#42` est présenté comme inventaire permanent ;
- les commandes documentées existent réellement dans le Makefile ;
- `make docs` n’est plus documenté sans cible correspondante ;
- l’architecture distingue Database, DAO, services et GTK ;
- le nommage documenté correspond au code actuel ;
- les dépendances optionnelles sont clairement identifiées ;
- les données d’enquête réelles sont explicitement interdites dans le dépôt ;
- tous les liens Markdown sont valides ;
- `git diff --check` passe.
