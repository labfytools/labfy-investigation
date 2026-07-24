# Audit du schéma SQLite V1

Version : 1.0

Date : 2026-07-15

---

# Objectif

Ce document analyse le schéma historique de Labfy Investigation afin de
définir le schéma métier officiel V1.

Chaque table historique reçoit l'une des décisions suivantes :

- `CONSERVER`
- `MODIFIER`
- `RENOMMER`
- `FUSIONNER`
- `SUPPRIMER`

Le schéma historique sert de fondation. Les concepts utiles sont conservés,
mais les noms, contraintes et relations peuvent être corrigés.

---

# Conventions générales

## Identifiants métier

Tous les objets métier utilisent un UUID stocké sous forme de texte :

```sql
id TEXT PRIMARY KEY
```

L'UUID est généré par l'application avec :

```c
g_uuid_string_random()
```

Les tables de référence utilisent des identifiants entiers.

---

## Dates

Les dates techniques sont stockées en UTC au format ISO 8601 :

```text
YYYY-MM-DDTHH:MM:SSZ
```

---

## Chemins

Tous les chemins enregistrés dans la base sont relatifs à la racine de
l'enquête.

Exemple valide :

```text
01_Preuves_Originales/Documents/facture.pdf
```

Exemple interdit :

```text
/home/utilisateur/Enquetes/Test/01_Preuves_Originales/Documents/facture.pdf
```

---

## Nommage SQL

Les noms utilisent :

- des minuscules ;
- le `snake_case` ;
- aucun accent ;
- aucun espace.

---

## Suppression logique

Les objets importants ne sont pas supprimés immédiatement de la base.

Un statut métier permet de conserver leur historique et leurs relations.

Une purge physique éventuelle devra être une opération explicite et
journalisée.

---

# Audit des tables

## Table historique `preuves`

### Décision

```text
MODIFIER
```

### Nom final

```text
preuves
```

### Justification

Le concept est central et doit être conservé.

La table historique doit cependant être renforcée afin de garantir :

- l'identification unique des preuves ;
- la portabilité des chemins ;
- l'intégrité des fichiers ;
- la traçabilité de l'import ;
- la suppression logique ;
- la cohérence des types et statuts.

---

## Définition métier

Une preuve représente un élément collecté dans le cadre d'une enquête.

Elle peut correspondre notamment à :

- une capture d'écran ;
- une photographie ;
- une vidéo ;
- un document ;
- un email exporté ;
- une archive ;
- un fichier audio ;
- un journal technique ;
- un résultat produit par un outil.

Le contenu d'une preuve originale ne doit jamais être modifié.

Les annotations, conversions, extractions et autres transformations produisent
de nouveaux fichiers dans `02_Preuves_Traitees`.

---

## Identifiant

La colonne :

```text
id
```

contient directement l'UUID de la preuve.

Il n'est pas nécessaire d'avoir deux colonnes distinctes `id` et `uuid`.

Exemple :

```text
550e8400-e29b-41d4-a716-446655440000
```

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID de la preuve |
| `name` | `TEXT` | oui | Nom visible du fichier ou de l'élément |
| `relative_path` | `TEXT` | oui | Chemin relatif dans l'enquête |
| `type_id` | `INTEGER` | oui | Référence vers `types_preuve` |
| `source_id` | `TEXT` | non | Référence vers la source principale |
| `size_bytes` | `INTEGER` | non | Taille du fichier en octets |
| `sha256` | `TEXT` | non | Empreinte SHA-256 |
| `mime_type` | `TEXT` | non | Type MIME détecté |
| `description` | `TEXT` | non | Description synthétique |
| `comment` | `TEXT` | non | Notes détaillées |
| `file_created_at` | `TEXT` | non | Date de création connue du fichier |
| `imported_at` | `TEXT` | oui | Date d'ajout dans l'enquête |
| `updated_at` | `TEXT` | oui | Dernière modification des métadonnées |
| `status` | `TEXT` | oui | État logique de la preuve |
| `locked` | `INTEGER` | oui | Verrouillage logique, `0` ou `1` |

---

## Statuts autorisés

Première version :

```text
active
archived
deleted
```

Une preuve marquée :

```text
deleted
```

reste présente dans la base afin de préserver :

- le journal ;
- les relations ;
- les références dans les rapports ;
- la traçabilité.

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
UNIQUE (relative_path)
```

```sql
FOREIGN KEY (type_id)
    REFERENCES types_preuve(id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT
```

```sql
FOREIGN KEY (source_id)
    REFERENCES sources(id)
    ON UPDATE CASCADE
    ON DELETE SET NULL
```

```sql
CHECK (length(trim(name)) > 0)
```

```sql
CHECK (length(trim(relative_path)) > 0)
```

```sql
CHECK (size_bytes IS NULL OR size_bytes >= 0)
```

```sql
CHECK (
    sha256 IS NULL
    OR (
        length(sha256) = 64
        AND sha256 = lower(sha256)
    )
)
```

```sql
CHECK (status IN ('active', 'archived', 'deleted'))
```

```sql
CHECK (locked IN (0, 1))
```

---

## Index prévus

La contrainte `UNIQUE` crée déjà un index sur :

```text
relative_path
```

Ajouter également :

```sql
CREATE INDEX idx_preuves_type_id
ON preuves(type_id);
```

```sql
CREATE INDEX idx_preuves_source_id
ON preuves(source_id);
```

```sql
CREATE INDEX idx_preuves_status
ON preuves(status);
```

```sql
CREATE INDEX idx_preuves_sha256
ON preuves(sha256);
```

L'index sur `sha256` permettra notamment de détecter rapidement les fichiers
déjà présents dans l'enquête.

---

## Relation avec les sources

Une preuve peut posséder une source principale via :

```text
source_id
```

Ce champ ne remplace pas nécessairement toutes les relations possibles.

Une future table de liaison pourra être ajoutée si une preuve doit être reliée
à plusieurs sources :

```text
preuve_sources
```

Pour le schéma V1, une source principale est suffisante tant que le besoin
multi-source n'est pas confirmé.

---

## Relation avec les tags

La relation entre les preuves et les tags sera stockée dans :

```text
preuve_tags
```

La table historique :

```text
preuves_tag
```

sera donc renommée et consolidée.

---

## Structure C cible

La structure C envisagée est :

```c
typedef struct InvestigationEvidence InvestigationEvidence;
```

Sa représentation privée contiendra conceptuellement :

```c
struct InvestigationEvidence
{
    char *id;
    char *name;
    char *relative_path;

    int type_id;
    char *source_id;

    guint64 size_bytes;

    char *sha256;
    char *mime_type;
    char *description;
    char *comment;

    char *file_created_at;
    char *imported_at;
    char *updated_at;

    char *status;
    gboolean locked;
};
```

Cette structure n'est pas implémentée dans le ticket #023.

Elle sert uniquement à vérifier que le schéma SQL correspond bien au futur
modèle C.

---

## Modifications par rapport au schéma historique

- conservation du concept `preuves` ;
- adoption d'un UUID comme clé primaire ;
- utilisation exclusive d'un chemin relatif ;
- ajout de contraintes `NOT NULL` ;
- ajout de la taille en octets ;
- ajout du hash SHA-256 ;
- ajout du type MIME ;
- séparation entre date du fichier et date d'import ;
- ajout d'une date de mise à jour ;
- ajout d'un statut de suppression logique ;
- ajout du verrouillage ;
- ajout d'une relation optionnelle vers `sources` ;
- ajout d'index métier ;
- renommage futur de `preuves_tag` en `preuve_tags`.

---

## Décision finale

```text
CONSERVER LE CONCEPT
MODIFIER LE SCHÉMA
CONSERVER LE NOM `preuves`
```
---

## Table historique `sources`

### Décision

```text
MODIFIER
```
---

### Nom final

```text
sources
```
---

### Justification

Le concept doit être conservé.

Une source représente l'origine d'une information ou d'une preuve. Elle ne
représente pas la preuve elle-même.

Exemples :

- site web ;
- réseau social ;
- témoin ;
- téléphone ;
- disque ;
- document ;
- email ;
- outil externe.

La table historique doit être renforcée avec :

- un UUID ;
- un type obligatoire ;
- une référence technique générique ;
- des dates techniques ;
- des contraintes explicites.

---

### Colonnes finales proposées

 Colonnes finales proposées
 
|Colonne|Type|Obligatoire|Description|
|-|-|-|-|
|id|TEXT|oui|UUID de la source|
|type_id|INTEGER|oui|Référence vers types_source|
|nom|TEXT|oui|Nom visible de la source|
|reference|TEXT|non|URL, identifiant, numéro de série, Message-ID ou autre référence technique|
|description|TEXT|non|Description libre|
|created_at|TEXT|oui|Date de création dans l'enquête|
|updated_at|TEXT|oui|Dernière modification des métadonnées|

---

### Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
FOREIGN KEY (type_id)
    REFERENCES types_source(id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT
```

```sql
CHECK (length(trim(nom)) > 0)
```

```sql
CHECK (
    reference IS NULL
    OR length(trim(reference)) > 0
)
```
---

### Relation avec les preuves

Le modèle prévu est :

```
source
    ↓
recherche
    ↓
preuve
```

Une recherche documentera :

- la source interrogée ;
- l'outil utilisé ;
- la requête ;
- les résultats ;
- les preuves produites.

---

### Index prévus

```sql
CREATE INDEX idx_sources_type_id
ON sources(type_id);
```

```sql
CREATE INDEX idx_sources_reference
ON sources(reference);
```
---

### Structure C cible

```C
typedef struct Source Source;
```

```C
struct Source
{
    char *id;

    int type_id;

    char *nom;
    char *reference;
    char *description;

    char *created_at;
    char *updated_at;
};
```
Cette structure n'est pas implémentée dans le ticket #023.

---

### Modifications par rapport au schéma historique

- conservation du concept sources ;
- UUID comme clé primaire ;
- type obligatoire ;
- remplacement des champs trop spécialisés par reference ;
- ajout des dates techniques ;
- ajout des contraintes ;
- ajout des index ;
- absence de relation directe avec preuves.

---

### Décision finale

```text
CONSERVER LE CONCEPT
MODIFIER LE SCHÉMA
CONSERVER LE NOM `sources`
```

## Table historique `recherche`

### Décision

```text
RENOMMER ET MODIFIER
```

### Nom final

```text
recherches
```

### Justification

Le concept doit être conservé, mais la table est renommée au pluriel afin de
respecter les conventions SQL du projet.

Une recherche représente une opération documentée réalisée au cours d'une
enquête.

Elle peut correspondre à :

- une requête sur un moteur de recherche ;
- une consultation de profil social ;
- une requête DNS ou WHOIS ;
- une analyse de fichier ;
- une extraction de métadonnées ;
- une recherche dans une base publique ;
- une procédure manuelle ;
- l'exécution d'un script ou d'un outil OSINT.

La recherche conserve le contexte de l'opération afin que son résultat puisse
être compris et reproduit.

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID de la recherche |
| `source_id` | `TEXT` | non | Source principalement interrogée |
| `type_outil_id` | `INTEGER` | oui | Catégorie de l'outil utilisé |
| `outil_nom` | `TEXT` | oui | Nom précis de l'outil ou de la procédure |
| `requete` | `TEXT` | non | Requête, commande ou opération exécutée |
| `resultat` | `TEXT` | non | Résultat synthétique |
| `observations` | `TEXT` | non | Notes et interprétations |
| `started_at` | `TEXT` | oui | Début de l'opération en UTC |
| `completed_at` | `TEXT` | non | Fin de l'opération en UTC |
| `created_at` | `TEXT` | oui | Création de l'enregistrement |
| `updated_at` | `TEXT` | oui | Dernière modification |
| `status` | `TEXT` | oui | État logique de la recherche |

---

## Statuts autorisés

```text
planned
running
completed
failed
cancelled
archived
```

Une recherche échouée reste enregistrée.

Un échec peut être important pour comprendre :

- les méthodes déjà essayées ;
- les limitations rencontrées ;
- les changements d'une cible ;
- les erreurs techniques ;
- les étapes non reproductibles.

---

## Relation avec les sources

La colonne :

```text
source_id
```

est facultative.

Certaines recherches interrogent une source identifiable :

```text
profil Instagram
site web
export téléphonique
document
```

D'autres opérations n'en possèdent pas directement :

```text
calcul de hash
OCR local
analyse de métadonnées
script interne
```

La suppression logique ou physique d'une source ne doit pas supprimer
l'historique des recherches.

La relation utilisera donc :

```sql
ON DELETE SET NULL
```

---

## Relation avec les outils

`types_outil` représente une catégorie technique :

```text
dns_tool
whois_tool
web_browser
ocr_tool
custom_script
```

La colonne :

```text
outil_nom
```

contient le nom concret utilisé :

```text
dig
Firefox
ExifTool
Tesseract
labfy-whois
```

Cela évite de créer immédiatement une table `outils` alors que le besoin
d'un catalogue complet d'outils n'est pas encore établi.

---

## Relation avec les preuves

Une recherche peut :

- utiliser une preuve comme donnée d'entrée ;
- produire une preuve comme résultat.

Une preuve peut également être liée à plusieurs recherches.

La relation est donc plusieurs-à-plusieurs et sera stockée dans :

```text
recherche_preuves
```

Cette table comportera un rôle :

```text
input
output
```

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
FOREIGN KEY (source_id)
    REFERENCES sources(id)
    ON UPDATE CASCADE
    ON DELETE SET NULL
```

```sql
FOREIGN KEY (type_outil_id)
    REFERENCES types_outil(id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT
```

```sql
CHECK (length(trim(outil_nom)) > 0)
```

```sql
CHECK (
    requete IS NULL
    OR length(trim(requete)) > 0
)
```

```sql
CHECK (
    completed_at IS NULL
    OR completed_at >= started_at
)
```

```sql
CHECK (
    status IN (
        'planned',
        'running',
        'completed',
        'failed',
        'cancelled',
        'archived'
    )
)
```

---

## Index prévus

```sql
CREATE INDEX idx_recherches_source_id
ON recherches(source_id);
```

```sql
CREATE INDEX idx_recherches_type_outil_id
ON recherches(type_outil_id);
```

```sql
CREATE INDEX idx_recherches_status
ON recherches(status);
```

```sql
CREATE INDEX idx_recherches_started_at
ON recherches(started_at);
```

---

## Structure C cible

```c
typedef struct Recherche Recherche;
```

Représentation privée envisagée :

```c
struct Recherche
{
    char *id;
    char *source_id;

    int type_outil_id;
    char *outil_nom;

    char *requete;
    char *resultat;
    char *observations;

    char *started_at;
    char *completed_at;
    char *created_at;
    char *updated_at;

    char *status;
};
```

Cette structure ne sera pas implémentée dans le ticket #023.

---

## Modifications par rapport au schéma historique

- renommage de `recherche` en `recherches` ;
- adoption d'un UUID comme clé primaire ;
- ajout d'une source optionnelle ;
- distinction entre catégorie et nom concret de l'outil ;
- ajout du contexte de la requête ;
- ajout des résultats et observations ;
- ajout des dates de début et de fin ;
- ajout d'un statut ;
- création d'une relation plusieurs-à-plusieurs avec les preuves.

---

## Décision finale

```text
CONSERVER LE CONCEPT
RENOMMER EN `recherches`
MODIFIER LE SCHÉMA
```

## Table historique `entites`

### Décision

```text
MODIFIER
```

### Nom final

```text
entites
```

### Justification

Le concept est central et doit être conservé.

Une entité représente une information identifiable découverte au cours de
l'enquête.

Elle peut être :

- extraite d'une preuve ;
- produite par une recherche ;
- ajoutée manuellement ;
- enrichie progressivement ;
- reliée à d'autres entités.

La table historique doit être renforcée avec :

- un UUID ;
- un type obligatoire ;
- une valeur canonique ;
- une valeur d'affichage ;
- un statut ;
- des dates techniques ;
- des contraintes explicites.

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID de l'entité |
| `type_id` | `INTEGER` | oui | Référence vers `types_entite` |
| `valeur` | `TEXT` | oui | Valeur canonique utilisée pour les comparaisons |
| `label` | `TEXT` | non | Valeur lisible ou nom d'affichage |
| `description` | `TEXT` | non | Description libre |
| `confiance` | `INTEGER` | oui | Niveau de confiance de 0 à 100 |
| `created_at` | `TEXT` | oui | Date de création dans l'enquête |
| `updated_at` | `TEXT` | oui | Dernière modification |
| `status` | `TEXT` | oui | État logique de l'entité |

---

## Différence entre `valeur` et `label`

La colonne :

```text
valeur
```

contient la forme canonique utilisée pour les recherches et les doublons.

Exemples :

```text
contact@example.com
+33612345678
arnaque_123
example.com
192.0.2.10
```

La colonne :

```text
label
```

contient une forme d'affichage facultative.

Exemples :

```text
Adresse principale
Téléphone du vendeur
Compte Instagram suspect
Site de la société
```

---

## Niveau de confiance

La colonne :

```text
confiance
```

utilise une valeur entière comprise entre :

```text
0 et 100
```

Exemples :

```text
100 = vérifié
80  = fortement probable
50  = à confirmer
20  = faible
0   = inconnu
```

La valeur par défaut est :

```text
50
```

---

## Statuts autorisés

```text
active
archived
deleted
```

Une entité supprimée logiquement reste dans la base afin de préserver :

- les recherches ;
- les relations ;
- les rapports ;
- la chronologie ;
- le journal.

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
FOREIGN KEY (type_id)
    REFERENCES types_entite(id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT
```

```sql
CHECK (length(trim(valeur)) > 0)
```

```sql
CHECK (
    label IS NULL
    OR length(trim(label)) > 0
)
```

```sql
CHECK (confiance BETWEEN 0 AND 100)
```

```sql
CHECK (
    status IN (
        'active',
        'archived',
        'deleted'
    )
)
```

---

## Unicité

Une même valeur peut exister pour plusieurs types différents.

Exemple :

```text
valeur = "example.com"
```

pourrait être interprétée comme :

- nom de domaine ;
- site web.

L'unicité doit donc porter sur :

```sql
UNIQUE (type_id, valeur)
```

---

## Index prévus

```sql
CREATE INDEX idx_entites_type_id
ON entites(type_id);
```

```sql
CREATE INDEX idx_entites_valeur
ON entites(valeur);
```

```sql
CREATE INDEX idx_entites_status
ON entites(status);
```

```sql
CREATE INDEX idx_entites_confiance
ON entites(confiance);
```

---

## Relation avec les recherches

Une recherche peut découvrir ou enrichir plusieurs entités.

Une entité peut être liée à plusieurs recherches.

La relation sera stockée dans :

```text
recherche_entites
```

avec un rôle :

```text
discovered
enriched
validated
contradicted
```

---

## Relation avec les preuves

Une entité peut être extraite d'une ou plusieurs preuves.

Une preuve peut contenir plusieurs entités.

Cette relation sera stockée dans :

```text
preuve_entites
```

---

## Structure C cible

```c
typedef struct Entite Entite;
```

Représentation privée envisagée :

```c
struct Entite
{
    char *id;

    int type_id;

    char *valeur;
    char *label;
    char *description;

    int confiance;

    char *created_at;
    char *updated_at;

    char *status;
};
```

Cette structure n'est pas implémentée dans le ticket #023.

---

## Modifications par rapport au schéma historique

- conservation du concept `entites` ;
- UUID comme clé primaire ;
- type obligatoire ;
- séparation entre valeur canonique et label d'affichage ;
- ajout d'un niveau de confiance ;
- ajout d'un statut logique ;
- ajout des dates techniques ;
- ajout d'une contrainte d'unicité ;
- ajout de relations avec les recherches et les preuves.

---

## Décision finale

```text
CONSERVER LE CONCEPT
MODIFIER LE SCHÉMA
CONSERVER LE NOM `entites`
```

## Table historique `associations`

### Décision

```text
RENOMMER ET MODIFIER
```

### Nom final

```text
relations
```

### Justification

Le concept de liaison entre objets métier doit être conservé, mais la table
historique `associations` est trop générique.

Une relation représente un lien documenté entre deux entités.

Exemples :

- une personne utilise une adresse email ;
- un pseudonyme appartient probablement à une personne ;
- un compte social utilise un numéro de téléphone ;
- un IBAN est associé à une adresse email ;
- un domaine pointe vers une adresse IP ;
- une organisation possède un site web.

Une relation n'est pas un simple raccord technique. Elle constitue une
information d'enquête à part entière.

Elle doit donc posséder :

- un UUID ;
- une entité source ;
- une entité cible ;
- un type de relation ;
- un niveau de confiance ;
- une justification ;
- des dates ;
- un statut.

---

## Nom et orientation

Une relation est orientée :

```text
entite_source
    │
    ▼
entite_cible
```

Exemple :

```text
Personne
    │ utilise
    ▼
Adresse email
```

L'orientation doit être choisie de manière cohérente lors de la création.

La relation inverse ne doit pas être créée automatiquement dans la base.

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID de la relation |
| `entite_source_id` | `TEXT` | oui | Entité à l'origine de la relation |
| `entite_cible_id` | `TEXT` | oui | Entité ciblée par la relation |
| `type_relation` | `TEXT` | oui | Code stable décrivant la relation |
| `label` | `TEXT` | non | Libellé d'affichage facultatif |
| `justification` | `TEXT` | non | Explication du raisonnement |
| `confiance` | `INTEGER` | oui | Niveau de confiance de 0 à 100 |
| `created_at` | `TEXT` | oui | Date de création |
| `updated_at` | `TEXT` | oui | Dernière modification |
| `status` | `TEXT` | oui | État logique de la relation |

---

## Type de relation

La colonne :

```text
type_relation
```

contient un code technique stable.

Exemples :

```text
uses
owns
belongs_to
linked_to
communicates_with
resolves_to
registered_with
paid_to
same_as
possibly_same_as
```

Le libellé humain pourra être traduit plus tard.

Pour la V1, aucune table de référence `types_relation` n'est imposée.

Le type reste une chaîne contrôlée par l'application.

Une table dédiée pourra être introduite si le catalogue devient important.

---

## Niveau de confiance

La colonne :

```text
confiance
```

est comprise entre :

```text
0 et 100
```

Exemples :

```text
100 = relation vérifiée
80  = fortement probable
50  = hypothèse raisonnable
20  = faible indice
0   = inconnu
```

---

## Statuts autorisés

```text
active
archived
deleted
disputed
```

Le statut :

```text
disputed
```

indique que la relation est contestée ou qu'un élément contradictoire a été
découvert.

La relation reste conservée pour préserver l'historique du raisonnement.

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
FOREIGN KEY (entite_source_id)
    REFERENCES entites(id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT
```

```sql
FOREIGN KEY (entite_cible_id)
    REFERENCES entites(id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT
```

```sql
CHECK (entite_source_id <> entite_cible_id)
```

```sql
CHECK (length(trim(type_relation)) > 0)
```

```sql
CHECK (
    label IS NULL
    OR length(trim(label)) > 0
)
```

```sql
CHECK (confiance BETWEEN 0 AND 100)
```

```sql
CHECK (
    status IN (
        'active',
        'archived',
        'deleted',
        'disputed'
    )
)
```

---

## Unicité

Un même couple d'entités peut posséder plusieurs types de relations.

Exemple :

```text
Personne A
    owns
Compte X

Personne A
    uses
Compte X
```

L'unicité doit donc porter sur :

```sql
UNIQUE (
    entite_source_id,
    entite_cible_id,
    type_relation
)
```

Cela empêche un doublon exact sans interdire plusieurs relations distinctes.

---

## Relation avec les preuves

Une relation peut être soutenue par plusieurs preuves.

Une preuve peut soutenir plusieurs relations.

La liaison sera stockée dans :

```text
relation_preuves
```

---

## Relation avec les recherches

Une relation peut être découverte, confirmée ou contredite par plusieurs
recherches.

La liaison sera stockée dans :

```text
recherche_relations
```

avec un rôle :

```text
discovered
confirmed
contradicted
```

---

## Index prévus

```sql
CREATE INDEX idx_relations_entite_source_id
ON relations(entite_source_id);
```

```sql
CREATE INDEX idx_relations_entite_cible_id
ON relations(entite_cible_id);
```

```sql
CREATE INDEX idx_relations_type_relation
ON relations(type_relation);
```

```sql
CREATE INDEX idx_relations_status
ON relations(status);
```

```sql
CREATE INDEX idx_relations_confiance
ON relations(confiance);
```

---

## Structure C cible

```c
typedef struct Relation Relation;
```

Représentation privée envisagée :

```c
struct Relation
{
    char *id;

    char *entite_source_id;
    char *entite_cible_id;

    char *type_relation;
    char *label;
    char *justification;

    int confiance;

    char *created_at;
    char *updated_at;

    char *status;
};
```

Cette structure n'est pas implémentée dans le ticket #023.

---

## Modifications par rapport au schéma historique

- remplacement de `associations` par `relations` ;
- UUID comme clé primaire ;
- relation orientée entre deux entités ;
- ajout d'un type de relation ;
- ajout d'un niveau de confiance ;
- ajout d'une justification ;
- ajout d'un statut contestable ;
- ajout des dates techniques ;
- ajout de liaisons avec les preuves et les recherches ;
- ajout de contraintes et d'index.

---

## Décision finale

```text
CONSERVER LE CONCEPT
RENOMMER `associations` EN `relations`
MODIFIER PROFONDÉMENT LE SCHÉMA
```

## Table historique `chronologie`

### Décision

```text
MODIFIER
```

### Nom final

```text
chronologie
```

### Justification

Le concept doit être conservé.

Une entrée de chronologie représente un événement important de l'enquête.

Elle peut être :

- saisie manuellement ;
- produite par une recherche ;
- associée à une preuve ;
- liée à une entité ;
- liée à une relation ;
- générée automatiquement par l'application.

La chronologie ne doit pas être confondue avec le journal technique.

La chronologie raconte les événements utiles à la compréhension de l'enquête.

Le journal enregistre les actions réalisées dans le logiciel.

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID de l'événement |
| `event_time` | `TEXT` | oui | Date et heure de l'événement décrit |
| `titre` | `TEXT` | oui | Titre court |
| `description` | `TEXT` | non | Description détaillée |
| `origine` | `TEXT` | oui | Origine manuelle ou automatique |
| `created_at` | `TEXT` | oui | Date de création de l'enregistrement |
| `updated_at` | `TEXT` | oui | Dernière modification |
| `status` | `TEXT` | oui | État logique |

---

## Origines autorisées

```text
manual
automatic
imported
```

`manual` :

- saisie directe par l'enquêteur.

`automatic` :

- génération par Labfy Investigation.

`imported` :

- événement importé depuis une source externe.

---

## Statuts autorisés

```text
active
archived
deleted
```

Une entrée supprimée logiquement reste conservée afin de préserver :

- les rapports ;
- les références ;
- les liens avec les preuves ;
- l'historique de l'enquête.

---

## Relation avec les objets métier

Une entrée de chronologie peut être liée à plusieurs objets métier.

Les relations sont stockées dans des tables spécialisées :

```text
recherche_chronologie
preuve_chronologie
entite_chronologie
relation_chronologie
```

Cette solution est préférée à un couple polymorphe :

```text
objet_type
objet_id
```

car elle permet de conserver de vraies clés étrangères SQLite.

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
CHECK (length(trim(titre)) > 0)
```

```sql
CHECK (
    description IS NULL
    OR length(trim(description)) > 0
)
```

```sql
CHECK (
    origine IN (
        'manual',
        'automatic',
        'imported'
    )
)
```

```sql
CHECK (
    status IN (
        'active',
        'archived',
        'deleted'
    )
)
```

---

## Index prévus

```sql
CREATE INDEX idx_chronologie_event_time
ON chronologie(event_time);
```

```sql
CREATE INDEX idx_chronologie_origine
ON chronologie(origine);
```

```sql
CREATE INDEX idx_chronologie_status
ON chronologie(status);
```

---

## Structure C cible

```c
typedef struct EvenementChronologie EvenementChronologie;
```

Représentation privée envisagée :

```c
struct EvenementChronologie
{
    char *id;

    char *event_time;

    char *titre;
    char *description;

    char *origine;

    char *created_at;
    char *updated_at;

    char *status;
};
```

Cette structure n'est pas implémentée dans le ticket #023.

---

## Modifications par rapport au schéma historique

- conservation du concept `chronologie` ;
- UUID comme clé primaire ;
- séparation entre date de l'événement et dates techniques ;
- ajout d'un titre obligatoire ;
- ajout d'une origine ;
- ajout d'un statut logique ;
- ajout de relations explicites vers les objets métier ;
- ajout de contraintes et d'index.

---

## Décision finale

```text
CONSERVER LE CONCEPT
MODIFIER LE SCHÉMA
CONSERVER LE NOM `chronologie`
```

## Table historique `journal`

### Décision

```text
MODIFIER
```

### Nom final

```text
journal
```

### Justification

Le concept doit être conservé.

Le journal constitue la trace d'audit des actions significatives réalisées
dans Labfy Investigation.

Il ne décrit pas directement les événements de l'enquête.

Il décrit les opérations effectuées dans le logiciel.

Exemples :

- création d'une enquête ;
- import d'une preuve ;
- modification d'une entité ;
- création d'une relation ;
- changement du niveau de confiance ;
- archivage d'un objet ;
- génération d'un rapport ;
- calcul ou vérification d'un hash.

Le journal doit permettre de répondre aux questions suivantes :

- quelle action a été effectuée ;
- quand elle a été effectuée ;
- sur quel objet ;
- avec quel résultat ;
- avec quels détails complémentaires.

---

## Différence avec `chronologie`

La table :

```text
chronologie
```

raconte les événements utiles à la compréhension de l'enquête.

La table :

```text
journal
```

raconte les actions effectuées dans l'application.

Exemple :

```text
Chronologie :
Le compte Instagram suspect a été découvert à 14 h 32.

Journal :
Une entité de type compte Instagram a été créée à 14 h 34.
```

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID de l'entrée du journal |
| `event_time` | `TEXT` | oui | Date et heure de l'action |
| `action` | `TEXT` | oui | Code technique de l'action |
| `objet_type` | `TEXT` | non | Type de l'objet concerné |
| `objet_id` | `TEXT` | non | UUID de l'objet concerné |
| `resultat` | `TEXT` | oui | Résultat de l'action |
| `details` | `TEXT` | non | Informations complémentaires |
| `acteur` | `TEXT` | non | Utilisateur, processus ou composant ayant réalisé l'action |
| `created_at` | `TEXT` | oui | Date de création de l'enregistrement |

---

## Actions prévues

Les actions utilisent un code technique stable.

Exemples :

```text
create
update
archive
restore
delete
import
export
open
close
hash_compute
hash_verify
report_generate
relation_create
relation_dispute
```

La liste n'est pas limitée par une contrainte SQL afin de permettre
l'évolution du logiciel sans migration du schéma.

L'application reste responsable de l'utilisation de codes cohérents.

---

## Types d'objets possibles

Exemples :

```text
investigation
preuve
source
recherche
entite
relation
chronologie
hypothese
rapport
```

Une action peut ne concerner aucun objet précis.

Exemple :

```text
application_start
application_stop
```

Dans ce cas :

```text
objet_type = NULL
objet_id   = NULL
```

---

## Cohérence entre `objet_type` et `objet_id`

Les deux colonnes doivent être :

- toutes les deux nulles ;
- ou toutes les deux renseignées.

Contrainte prévue :

```sql
CHECK (
    (
        objet_type IS NULL
        AND objet_id IS NULL
    )
    OR
    (
        objet_type IS NOT NULL
        AND objet_id IS NOT NULL
    )
)
```

Le journal utilise volontairement une relation polymorphe.

Aucune clé étrangère ne peut garantir automatiquement l'existence de
l'objet référencé.

L'application doit vérifier la cohérence lors de l'écriture.

---

## Résultats autorisés

```text
success
failure
partial
cancelled
```

Une opération échouée doit rester enregistrée.

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
CHECK (length(trim(action)) > 0)
```

```sql
CHECK (
    objet_type IS NULL
    OR length(trim(objet_type)) > 0
)
```

```sql
CHECK (
    objet_id IS NULL
    OR length(trim(objet_id)) > 0
)
```

```sql
CHECK (
    (
        objet_type IS NULL
        AND objet_id IS NULL
    )
    OR
    (
        objet_type IS NOT NULL
        AND objet_id IS NOT NULL
    )
)
```

```sql
CHECK (
    resultat IN (
        'success',
        'failure',
        'partial',
        'cancelled'
    )
)
```

```sql
CHECK (
    details IS NULL
    OR length(trim(details)) > 0
)
```

```sql
CHECK (
    acteur IS NULL
    OR length(trim(acteur)) > 0
)
```

---

## Index prévus

```sql
CREATE INDEX idx_journal_event_time
ON journal(event_time);
```

```sql
CREATE INDEX idx_journal_action
ON journal(action);
```

```sql
CREATE INDEX idx_journal_objet
ON journal(objet_type, objet_id);
```

```sql
CREATE INDEX idx_journal_resultat
ON journal(resultat);
```

---

## Structure C cible

```c
typedef struct EntreeJournal EntreeJournal;
```

Représentation privée envisagée :

```c
struct EntreeJournal
{
    char *id;

    char *event_time;
    char *action;

    char *objet_type;
    char *objet_id;

    char *resultat;
    char *details;
    char *acteur;

    char *created_at;
};
```

Cette structure n'est pas implémentée dans le ticket #023.

---

## Suppression

Les entrées du journal ne doivent pas être modifiées ni supprimées par les
fonctions métier ordinaires.

Une éventuelle purge devra être :

- explicite ;
- réservée à une opération administrative ;
- documentée ;
- elle-même journalisée lorsque cela est possible.

---

## Modifications par rapport au schéma historique

- conservation du concept `journal` ;
- UUID comme clé primaire ;
- séparation entre action, objet, résultat et détails ;
- ajout de l'acteur ;
- ajout de contraintes de cohérence ;
- ajout d'index ;
- affirmation du caractère append-only du journal.

---

## Décision finale

```text
CONSERVER LE CONCEPT
MODIFIER LE SCHÉMA
CONSERVER LE NOM `journal`
```

## Table historique `hypotheses`

### Décision

```text
MODIFIER
```

### Nom final

```text
hypotheses
```

### Justification

Le concept doit être conservé.

Une hypothèse représente une proposition de travail formulée pendant
l'enquête.

Elle ne doit pas être confondue avec :

- une relation ;
- une preuve ;
- une note libre ;
- une conclusion définitive.

Une hypothèse peut évoluer à mesure que de nouveaux éléments apparaissent.

Elle doit pouvoir être :

- proposée ;
- étudiée ;
- soutenue ;
- contredite ;
- confirmée ;
- rejetée ;
- archivée.

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID de l'hypothèse |
| `titre` | `TEXT` | oui | Titre court |
| `description` | `TEXT` | oui | Description détaillée |
| `confiance` | `INTEGER` | oui | Niveau de confiance de 0 à 100 |
| `evaluation` | `TEXT` | non | Justification du niveau de confiance |
| `created_at` | `TEXT` | oui | Date de création |
| `updated_at` | `TEXT` | oui | Dernière modification |
| `status` | `TEXT` | oui | État logique de l'hypothèse |

---

## Niveau de confiance

La colonne :

```text
confiance
```

est comprise entre :

```text
0 et 100
```

Elle représente l'évaluation actuelle de l'enquêteur.

Exemples :

```text
0   = aucune confiance
25  = faible
50  = plausible
75  = fortement probable
100 = confirmé
```

La confiance ne doit pas être calculée automatiquement dans la V1.

L'application pourra proposer des aides à l'évaluation plus tard, mais
l'enquêteur reste responsable de la valeur enregistrée.

---

## Évaluation

La colonne :

```text
evaluation
```

explique pourquoi le niveau de confiance a été choisi.

Exemple :

```text
Deux preuves indépendantes,
un numéro de téléphone commun
et une relation confirmée.
```

Cette colonne est facultative, mais fortement recommandée lorsque la confiance
est modifiée.

---

## Statuts autorisés

```text
proposed
under_review
supported
contradicted
confirmed
rejected
archived
```

Signification :

```text
proposed
```

Hypothèse nouvellement formulée.

```text
under_review
```

Hypothèse en cours d'étude.

```text
supported
```

Plusieurs éléments la soutiennent, sans confirmation définitive.

```text
contradicted
```

Des éléments la contredisent.

```text
confirmed
```

Hypothèse considérée comme confirmée.

```text
rejected
```

Hypothèse écartée.

```text
archived
```

Hypothèse conservée uniquement pour l'historique.

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
CHECK (length(trim(titre)) > 0)
```

```sql
CHECK (length(trim(description)) > 0)
```

```sql
CHECK (confiance BETWEEN 0 AND 100)
```

```sql
CHECK (
    evaluation IS NULL
    OR length(trim(evaluation)) > 0
)
```

```sql
CHECK (
    status IN (
        'proposed',
        'under_review',
        'supported',
        'contradicted',
        'confirmed',
        'rejected',
        'archived'
    )
)
```

---

## Relation avec les preuves

Une hypothèse peut être :

- soutenue ;
- contredite ;
- confirmée ;

par plusieurs preuves.

Une preuve peut intervenir dans plusieurs hypothèses.

La relation est stockée dans :

```text
hypothese_preuves
```

avec un rôle :

```text
supports
contradicts
confirms
```

---

## Relation avec les entités

Une hypothèse peut concerner plusieurs entités.

La liaison est stockée dans :

```text
hypothese_entites
```

---

## Relation avec les relations

Une hypothèse peut s'appuyer sur des relations existantes ou les remettre en
cause.

La liaison est stockée dans :

```text
hypothese_relations
```

avec un rôle :

```text
supports
contradicts
```

---

## Relation avec les recherches

Une recherche peut :

- produire une hypothèse ;
- l'enrichir ;
- la confirmer ;
- la contredire.

La liaison est stockée dans :

```text
recherche_hypotheses
```

avec un rôle :

```text
created
enriched
confirmed
contradicted
```

---

## Index prévus

```sql
CREATE INDEX idx_hypotheses_status
ON hypotheses(status);
```

```sql
CREATE INDEX idx_hypotheses_confiance
ON hypotheses(confiance);
```

```sql
CREATE INDEX idx_hypotheses_updated_at
ON hypotheses(updated_at);
```

---

## Structure C cible

```c
typedef struct Hypothese Hypothese;
```

Représentation privée envisagée :

```c
struct Hypothese
{
    char *id;

    char *titre;
    char *description;

    int confiance;
    char *evaluation;

    char *created_at;
    char *updated_at;

    char *status;
};
```

Cette structure n'est pas implémentée dans le ticket #023.

---

## Modifications par rapport au schéma historique

- conservation du concept `hypotheses` ;
- UUID comme clé primaire ;
- ajout d'un titre obligatoire ;
- ajout d'une description obligatoire ;
- ajout d'un niveau de confiance ;
- ajout d'une évaluation explicative ;
- ajout d'un statut détaillé ;
- ajout des dates techniques ;
- ajout de relations vers les preuves, entités, relations et recherches ;
- ajout de contraintes et d'index.

---

## Décision finale

```text
CONSERVER LE CONCEPT
MODIFIER LE SCHÉMA
CONSERVER LE NOM `hypotheses`
```

## Nouvelle table `categories`

### Décision

```text
AJOUTER
```

### Nom final

```text
categories
```

### Justification

Le schéma historique ne distinguait pas clairement la classification
structurante des annotations libres.

Une catégorie sert à classer un objet métier dans un domaine principal.

Exemples :

- OSINT ;
- finance ;
- réseaux sociaux ;
- téléphonie ;
- documents ;
- infrastructure.

Une catégorie est stable, peu nombreuse et généralement administrée.

Elle ne doit pas être confondue avec un tag, qui constitue une annotation
libre et potentiellement multiple.

---

## Objets catégorisables dans la V1

Les catégories peuvent être attribuées à :

```text
preuves
recherches
hypotheses
```

Chaque objet possède au maximum une catégorie.

Les entités disposent déjà de `type_id`.

Les relations disposent déjà de `type_relation`.

Aucune catégorie supplémentaire n'est donc prévue pour elles dans la V1.

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID de la catégorie |
| `nom` | `TEXT` | oui | Nom visible |
| `description` | `TEXT` | non | Description libre |
| `icone` | `TEXT` | non | Nom d'icône du thème GTK |
| `couleur` | `TEXT` | non | Couleur hexadécimale |
| `system` | `INTEGER` | oui | Catégorie fournie par l'application |
| `created_at` | `TEXT` | oui | Date de création |
| `updated_at` | `TEXT` | oui | Dernière modification |
| `status` | `TEXT` | oui | État logique |

---

## Colonne `system`

```text
system = 1
```

indique une catégorie livrée avec Labfy Investigation.

```text
system = 0
```

indique une catégorie créée par l'utilisateur.

L'application pourra empêcher la suppression ordinaire d'une catégorie
système.

---

## Couleur

La couleur facultative utilise le format :

```text
#RRGGBB
```

Exemple :

```text
#FF0000
```

Le schéma vérifie uniquement la longueur et le préfixe.

La validation complète des caractères hexadécimaux sera aussi effectuée dans
le code C.

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
UNIQUE (nom COLLATE NOCASE)
```

```sql
CHECK (length(trim(nom)) > 0)
```

```sql
CHECK (
    description IS NULL
    OR length(trim(description)) > 0
)
```

```sql
CHECK (
    icone IS NULL
    OR length(trim(icone)) > 0
)
```

```sql
CHECK (
    couleur IS NULL
    OR (
        length(couleur) = 7
        AND substr(couleur, 1, 1) = '#'
    )
)
```

```sql
CHECK (system IN (0, 1))
```

```sql
CHECK (
    status IN (
        'active',
        'archived',
        'deleted'
    )
)
```

---

## Structure C cible

```c
typedef struct Categorie Categorie;
```

Représentation privée envisagée :

```c
struct Categorie
{
    char *id;

    char *nom;
    char *description;
    char *icone;
    char *couleur;

    gboolean system;

    char *created_at;
    char *updated_at;

    char *status;
};
```

---

## Décision finale

```text
AJOUTER LA TABLE `categories`
```

## Table historique `tags`

### Décision

```text
MODIFIER
```

### Nom final

```text
tags
```

### Justification

Le concept doit être conservé.

Un tag est une annotation libre et transversale.

Contrairement à une catégorie :

- plusieurs tags peuvent être appliqués au même objet ;
- les utilisateurs peuvent en créer librement ;
- ils ne structurent pas le modèle métier principal.

Exemples :

- urgent ;
- à vérifier ;
- escroquerie ;
- Telegram ;
- fraude ;
- prioritaire.

---

## Objets pouvant recevoir des tags

Dans la V1 :

```text
preuves
recherches
entites
relations
hypotheses
chronologie
```

---

## Colonnes finales proposées

| Colonne | Type | Obligatoire | Description |
|---|---:|:---:|---|
| `id` | `TEXT` | oui | UUID du tag |
| `nom` | `TEXT` | oui | Nom visible |
| `description` | `TEXT` | non | Description libre |
| `couleur` | `TEXT` | non | Couleur hexadécimale |
| `system` | `INTEGER` | oui | Tag fourni par l'application |
| `created_at` | `TEXT` | oui | Date de création |
| `updated_at` | `TEXT` | oui | Dernière modification |
| `status` | `TEXT` | oui | État logique |

---

## Contraintes prévues

```sql
PRIMARY KEY (id)
```

```sql
UNIQUE (nom COLLATE NOCASE)
```

```sql
CHECK (length(trim(nom)) > 0)
```

```sql
CHECK (
    description IS NULL
    OR length(trim(description)) > 0
)
```

```sql
CHECK (
    couleur IS NULL
    OR (
        length(couleur) = 7
        AND substr(couleur, 1, 1) = '#'
    )
)
```

```sql
CHECK (system IN (0, 1))
```

```sql
CHECK (
    status IN (
        'active',
        'archived',
        'deleted'
    )
)
```

---

## Structure C cible

```c
typedef struct Tag Tag;
```

Représentation privée envisagée :

```c
struct Tag
{
    char *id;

    char *nom;
    char *description;
    char *couleur;

    gboolean system;

    char *created_at;
    char *updated_at;

    char *status;
};
```

---

## Modifications par rapport au schéma historique

- conservation du concept ;
- UUID comme clé primaire ;
- unicité du nom sans tenir compte de la casse ;
- ajout d'une description ;
- ajout d'une couleur ;
- distinction entre tags système et utilisateur ;
- ajout des dates techniques ;
- ajout de la suppression logique ;
- ajout de tables de liaison explicites.

---

## Décision finale

```text
CONSERVER LE CONCEPT
MODIFIER LE SCHÉMA
CONSERVER LE NOM `tags`
```

