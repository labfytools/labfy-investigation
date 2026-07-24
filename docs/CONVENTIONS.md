# Conventions de développement

> **Version :** 2.0  
> **Dernière mise à jour :** 2026-07-24  
> **Projet :** Labfy Investigation

---

## 1. Objectif

Ces conventions définissent les règles communes du dépôt.

Elles visent à garantir :

- un code lisible et maintenable ;
- une séparation claire des responsabilités ;
- la traçabilité des données d'enquête ;
- la stabilité des formats persistés ;
- des tests reproductibles ;
- une documentation qui distingue l'état réel du projet de son historique.

Lorsqu'une convention contredit le code courant, le problème doit être résolu :
soit le code est corrigé, soit la convention est mise à jour. La divergence ne
doit pas devenir permanente.

---

## 2. Hiérarchie des sources

Pour déterminer l'état réel du projet, utiliser cet ordre de confiance :

1. code présent sur la branche `main` ;
2. tests automatisés ;
3. scripts SQL et migrations ;
4. commits ;
5. tickets Forgejo fermés ;
6. tickets Forgejo ouverts ;
7. documentation courante ;
8. documents historiques et anciennes roadmaps.

Un ticket ouvert décrit un chantier, pas une fonctionnalité terminée.

Un audit nommé `SCHEMA_AUDIT_V1.md` ou `SCHEMA_AUDIT_V9.md` décrit uniquement
la version indiquée. La référence courante est :

```text
docs/database/SCHEMA_AUDIT_CURRENT.md
```

---

## 3. Langue et terminologie

### 3.1 Code et identifiants techniques

Le code C, les noms de fichiers, les fonctions, les structures, les constantes
et les codes persistés utilisent un anglais technique cohérent.

Exemples :

```text
evidence_record
entity_dao
relation_service
bank_proposal
controlled_vocab
eml_pipeline_task
created_at
verification_status
```

Les identifiants persistés doivent être stables et indépendants des libellés
affichés.

Exemple :

```text
code persistant : proposed
libellé français : Proposé
```

Un libellé peut être traduit ou reformulé sans modifier le code persistant.

### 3.2 Interface et documentation

L'interface utilisateur et la documentation destinée aux utilisateurs sont
rédigées en français.

Les termes juridiques ou métier restent en français dans les textes lorsque
cela améliore la précision.

### 3.3 Abréviations

Les abréviations sont limitées aux formes techniques reconnues :

```text
UUID
SHA-256
MIME
UTC
SQL
DAO
OCR
EML
IBAN
BIC
```

Les noms de variables raccourcis sans nécessité sont interdits.

Préférer :

```c
relative_path
verification_status
evidence_record
created_at
```

Éviter :

```c
path
stat
rec
crt
```

---

## 4. Organisation et dépendances entre couches

Le projet suit une architecture en couches :

```text
Interface GTK4
      ↓
Application et contrôleurs
      ↓
Services métier et tâches
      ↓
DAO et adaptateurs
      ├── SQLite
      ├── système de fichiers
      ├── outils externes
      └── futures API
      ↓
Modèles métier
```

Règles obligatoires :

- le cœur métier ne dépend pas de GTK ;
- les modèles ne connaissent ni GTK ni SQLite ;
- les vues et widgets n'exécutent aucune requête SQL ;
- les vues et widgets ne manipulent pas directement les preuves sur disque ;
- les migrations, connexions, statements et transactions appartiennent à la
  couche Database ;
- les requêtes métier appartiennent aux DAO ;
- les services orchestrent les opérations impliquant plusieurs DAO ou
  adaptateurs ;
- les opérations longues sont exécutées dans des tâches asynchrones ;
- le graphe est une projection de SQLite, jamais une seconde source de vérité.

Chaque module possède une responsabilité clairement identifiable.

---

## 5. Code C

### 5.1 Standard

Le projet utilise exclusivement :

```text
C17
```

Le Makefile principal compile avec :

```text
-std=c17 -Wall -Wextra -Werror
```

Plusieurs cibles de tests ajoutent également `-Wpedantic`.

Aucun avertissement ne doit être ignoré sans justification documentée.

### 5.2 Nommage

- fichiers : `snake_case.c` et `snake_case.h` ;
- fonctions : `snake_case` ;
- variables : noms explicites ;
- constantes : `UPPER_SNAKE_CASE` ;
- types publics : `PascalCase` lorsque cela correspond aux conventions GLib ;
- fonctions publiques préfixées par leur module.

Exemples :

```c
database_open()
evidence_dao_insert()
relation_service_create()
eml_pipeline_task_new()
```

### 5.3 Structures

Les structures publiques sont opaques lorsque cela protège les invariants du
module.

Exemple :

```c
typedef struct EvidenceRecord EvidenceRecord;
```

Un champ ne doit être public que lorsque l'accès direct est réellement prévu
par l'API.

### 5.4 Mémoire

Toute allocation possède un propriétaire clair.

Chaque API doit permettre de déterminer :

- qui alloue ;
- qui libère ;
- si une chaîne est empruntée ou copiée ;
- si une structure peut survivre au module qui l'a créée ;
- si un callback transfère la propriété.

Les conventions GLib sont utilisées de manière cohérente :

```text
_new()       retourne généralement une nouvelle référence
_ref()       ajoute une référence
_unref()     retire une référence
_free()      libère un objet non référencé
_dup_*()     retourne une copie possédée
_peek_*()    retourne une valeur empruntée
```

### 5.5 Erreurs

Les erreurs attendues sont remontées avec `GError` ou l'abstraction d'erreur du
module concerné.

Une fonction ne doit pas :

- masquer une erreur importante ;
- transformer silencieusement un échec en succès partiel ;
- afficher directement une boîte de dialogue depuis le cœur métier ;
- journaliser des données sensibles sans nécessité.

Les erreurs techniques sont journalisées. L'application décide de leur
présentation graphique.

---

## 6. SQLite

### 6.1 Version courante

La version courante du schéma est :

```text
V10
```

Les scripts versionnés sont conservés dans :

```text
database/schema_v1.sql
...
database/schema_v10.sql
```

Le complément idempotent du schéma courant est :

```text
database/schema_current.sql
```

### 6.2 Identifiants

Les objets métier utilisent généralement :

```sql
id TEXT PRIMARY KEY
```

avec un UUID généré par l'application.

Les tables de référence peuvent utiliser un identifiant entier lorsque cela
correspond à leur rôle.

La convention doit être vérifiée table par table : aucune généralisation ne
doit remplacer la lecture du schéma réel.

### 6.3 Dates

Les dates persistées utilisent l'UTC.

Format attendu lorsque la table impose une date complète :

```text
YYYY-MM-DDTHH:MM:SSZ
```

La conversion vers l'heure locale est réservée à l'affichage.

### 6.4 Requêtes

Toute valeur variable utilise une requête préparée :

```c
sqlite3_prepare_v2()
sqlite3_bind_*()
sqlite3_step()
```

La concaténation de données utilisateur dans une requête SQL est interdite.

Les requêtes SQL statiques peuvent être exécutées avec `sqlite3_exec()` lorsque
cela reste adapté.

### 6.5 Transactions

Toute opération portant sur plusieurs écritures liées doit être atomique.

Exemples :

- création d'une enquête ;
- import d'une preuve ;
- reclassement d'une preuve ;
- création d'une relation avec ses preuves ;
- intégration de propositions OSINT ;
- migration de schéma ;
- validation finale d'un pivot EML.

En cas d'échec, les écritures partielles sont annulées et les modifications du
système de fichiers sont restaurées ou nettoyées.

### 6.6 Migrations

Toute évolution persistante doit :

1. définir une nouvelle version ;
2. ajouter un script `schema_vN.sql` ;
3. ajouter ou adapter la fonction d'installation ;
4. raccorder la migration depuis la version précédente ;
5. mettre à jour la version courante ;
6. tester une base neuve ;
7. tester une ancienne base migrée ;
8. exécuter `PRAGMA integrity_check` ;
9. exécuter `PRAGMA foreign_key_check` ;
10. mettre à jour l'audit courant et la documentation.

Une ancienne migration publiée ne doit pas être réécrite pour modifier son
sens historique.

### 6.7 Accès par couche

- `src/database` : infrastructure SQLite, schéma, statements, transactions,
  erreurs et migrations ;
- `src/dao` : requêtes métier ;
- `src/core` : orchestration métier ;
- `src/views` et `src/widgets` : aucun SQL direct.

---

## 7. Fichiers et preuves

### 7.1 Chemins

Les chemins persistés dans une enquête sont relatifs à sa racine.

Autorisé :

```text
01_Preuves_Originales/Documents/facture.pdf
```

Interdit :

```text
/home/utilisateur/Documents/facture.pdf
```

Une entrée externe peut être absolue pendant l'import, mais le chemin enregistré
dans l'enquête doit respecter le modèle prévu par le schéma.

### 7.2 Immutabilité des preuves originales

Une preuve originale n'est jamais modifiée.

Toute opération produisant :

- une annotation ;
- une conversion ;
- une extraction ;
- un OCR ;
- une analyse ;
- une version expurgée ;

crée un nouvel objet ou un nouveau fichier dérivé.

### 7.3 Valeurs brutes et valeurs interprétées

Les données suivantes restent distinctes :

```text
valeur brute
valeur normalisée
valeur dérivée
correction utilisateur
statut de vérification
confiance
provenance
```

Une valeur brute ne doit jamais être réécrite pour correspondre à une
interprétation ultérieure.

Un résultat OCR ou OSINT est une proposition à vérifier, pas un fait confirmé.

### 7.4 Suppression

La suppression logique est privilégiée pour les objets de traçabilité, mais la
règle exacte dépend de chaque table et de chaque DAO.

Une suppression physique n'est autorisée que lorsque :

- le schéma la permet explicitement ;
- le service métier l'encadre ;
- les conséquences sur les clés étrangères et les fichiers sont testées ;
- la traçabilité requise est préservée.

---

## 8. Outils externes et réseau

Les outils externes sont optionnels sauf décision explicite contraire.

Ils sont lancés avec `GSubprocess` et une liste d'arguments séparés.

Interdit :

```c
system(dynamic_command);
```

Interdit également :

- construire une commande shell par concaténation ;
- ouvrir automatiquement une pièce jointe ;
- charger automatiquement une ressource distante d'un e-mail HTML ;
- installer automatiquement un outil ;
- utiliser un secret découvert ;
- contourner une authentification ;
- effectuer une action intrusive non autorisée.

Chaque exécution doit pouvoir conserver :

- l'outil ;
- sa version ;
- les arguments ;
- la date UTC ;
- la cible ;
- le code de retour ;
- `stdout` et `stderr` lorsque nécessaire ;
- les empreintes des sorties persistées ;
- les objets créés ou réutilisés.

---

## 9. Asynchronisme

Toute opération susceptible de bloquer l'interface doit être exécutée hors du
thread GTK principal.

Exemples :

- copie ou hachage de fichiers ;
- import massif ;
- chargement du graphe ;
- interrogation réseau ;
- lancement d'un outil ;
- OCR ;
- extraction de métadonnées ;
- analyse EML ;
- génération de rapport.

Une tâche longue doit prévoir, lorsque cela est possible :

- un état ;
- une progression ;
- une annulation ;
- un résultat ;
- une erreur ;
- une date de début ;
- une date de fin.

Les écritures SQLite concurrentes doivent être évitées ou sérialisées par
l'architecture prévue.

---

## 10. Tests

Toute fonctionnalité importante possède des tests adaptés.

Les tests couvrent selon le cas :

- arguments invalides ;
- cas nominal ;
- échecs ;
- annulation ;
- rollback ;
- responsabilités mémoire ;
- limites de taille ;
- entrées malformées ;
- anciennes versions de base ;
- intégrité et clés étrangères ;
- régressions.

Les tests utilisent uniquement des données synthétiques.

Il est interdit de placer dans le dépôt ou de fournir à un agent de
développement :

- une base `Enquete.sqlite` réelle ;
- des captures réelles ;
- des e-mails réels ;
- des RIB réels ;
- des pièces jointes réelles ;
- toute donnée personnelle issue d'une enquête.

Validation recommandée :

```sh
make clean
make -j8
make -j8 test
git diff --check
```

Une exécution séquentielle est utilisée seulement si la parallélisation
provoque un échec réel et identifié :

```sh
make
make test
```

---

## 11. Git et tickets

Forgejo est la source de vérité du suivi.

À partir des tickets postérieurs à l'historique numéroté manuellement, le titre
ne contient pas de numéro ajouté à la main : Forgejo attribue le numéro.

Un ticket peut nécessiter plusieurs commits.

Chaque commit doit :

- porter un changement cohérent ;
- compiler ;
- conserver la branche stable ;
- inclure ou adapter les tests nécessaires ;
- ne contenir aucune donnée réelle d'enquête.

Les messages de commit sont rédigés en anglais.

Exemple :

```text
feat(database): add bank account entities
```

Aucun commit n'est effectué tant que la fonction concernée ne marche pas et que
la validation prévue n'a pas été réalisée.

---

## 12. Documentation

Toute décision importante est documentée.

Une modification doit mettre à jour les documents concernés dans le même
chantier, notamment pour :

- l'architecture ;
- les conventions ;
- les dépendances ;
- le schéma SQLite ;
- les migrations ;
- les formats persistés ;
- les procédures de compilation et de test ;
- les limitations légales ou techniques.

La documentation doit indiquer clairement l'un des statuts suivants lorsque
cela évite une ambiguïté :

```text
IMPLÉMENTÉ
PARTIEL
PRÉVU
HISTORIQUE
```

La priorité reste la qualité, la traçabilité et la compréhension durable du
projet.
