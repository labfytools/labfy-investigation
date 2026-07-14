# Ticket #020

## Titre

Créer une nouvelle enquête.

---

## Objectif

Ajouter la capacité de créer automatiquement une nouvelle enquête dans un dossier choisi par l'utilisateur.

La création doit produire une arborescence standard et préparer l'emplacement de la future base SQLite.

Ce ticket ne crée pas encore le schéma SQL complet.

---

## Architecture

```text
Application
    │
    ▼
InvestigationProject
    │
    ▼
FileSystem
```

Le module `InvestigationProject` devient le point d'entrée métier pour la création d'une enquête.

La GUI ne doit jamais créer directement les dossiers.

---

## Responsabilités

### InvestigationProject

Le module doit :

- recevoir le chemin du dossier parent ;
- recevoir le nom de la nouvelle enquête ;
- créer le dossier racine de l'enquête ;
- créer l'arborescence standard ;
- créer le dossier `00_BaseDeDonnees` ;
- créer un fichier SQLite vide nommé `Enquete.sqlite` ;
- nettoyer ce qui a été créé en cas d'échec partiel ;
- retourner le chemin complet de l'enquête créée.

---

## Arborescence à créer

```text
NomEnquete/
├── 00_BaseDeDonnees/
│   └── Enquete.sqlite
├── 01_Preuves_Originales/
│   ├── Captures_Ecran/
│   ├── Conversations/
│   ├── Documents/
│   ├── Emails/
│   ├── Photos/
│   └── Videos/
├── 02_Preuves_Traitees/
│   ├── Annotations/
│   ├── Extractions/
│   ├── OCR/
│   └── Redactions/
├── 03_Chronologie/
├── 04_Entites/
│   ├── Adresses_Email/
│   ├── Comptes_Bancaires/
│   ├── Comptes_Facebook/
│   ├── Comptes_Instagram/
│   ├── Documents_Identite/
│   ├── IBAN/
│   ├── Personnes/
│   ├── Pseudonymes/
│   ├── Telephones/
│   └── Autres/
├── 05_Rapports/
├── 06_Exports/
├── 07_Notes/
├── 08_Sources/
└── 09_Hash/
```

Les noms de dossiers ne doivent pas contenir d'accents.

---

## Interface publique attendue

Créer :

```text
include/core/investigation_project.h
src/core/investigation_project.c
```

Avec :

```c
char *investigation_project_create(
    const char *parent_directory,
    const char *investigation_name
);
```

---

## Contrat de propriété

En cas de succès, la fonction retourne une nouvelle chaîne allouée contenant le chemin complet du dossier d'enquête.

Le code appelant devient propriétaire de cette chaîne et doit la libérer avec :

```c
g_free(investigation_path);
```

En cas d'échec, la fonction retourne :

```c
NULL
```

---

## Règles de validation

La fonction doit refuser :

- `parent_directory == NULL` ;
- un chemin parent vide ;
- `investigation_name == NULL` ;
- un nom vide ;
- un dossier parent inexistant ;
- un chemin parent qui n'est pas un dossier ;
- un dossier d'enquête qui existe déjà ;
- un nom contenant `/`.

---

## Gestion des erreurs

Si une étape échoue après la création partielle :

- tous les fichiers créés par la fonction doivent être supprimés ;
- tous les dossiers créés par la fonction doivent être supprimés ;
- aucun dossier partiel ne doit rester sur le disque ;
- la fonction retourne `NULL`.

La fonction ne doit jamais supprimer un dossier qui existait avant son appel.

---

## Création de `Enquete.sqlite`

Pour ce ticket, le fichier doit seulement être créé.

Le schéma SQLite sera initialisé dans le ticket #022.

Le fichier attendu est :

```text
00_BaseDeDonnees/Enquete.sqlite
```

---

## Hors périmètre

Ce ticket ne doit pas :

- créer les tables SQLite ;
- générer un UUID ;
- créer les métadonnées ;
- importer une enquête existante ;
- modifier l'interface GTK ;
- ouvrir automatiquement l'enquête créée ;
- créer un rapport ;
- copier des preuves.

---

## Dépendances

- C17 ;
- GLib ;
- GIO autorisé.

Aucune dépendance GTK ou SQLite requise dans ce ticket.

---

## Contraintes techniques

- structure modulaire ;
- aucun état global ;
- aucune logique de création dans `Application` ;
- aucune logique de création dans les widgets ;
- documentation Doxygen ;
- compilation sans warning ;
- nettoyage complet en cas d'erreur ;
- noms de fonctions préfixés par `investigation_project_`.

---

## Tests

Créer :

```text
tests/test_investigation_project.c
```

Le test doit :

- créer un dossier temporaire ;
- créer une enquête valide ;
- vérifier chaque dossier attendu ;
- vérifier la présence de `Enquete.sqlite` ;
- vérifier que la fonction retourne le bon chemin ;
- vérifier le refus des paramètres `NULL` ;
- vérifier le refus des chaînes vides ;
- vérifier le refus d'un nom contenant `/` ;
- vérifier le refus si le dossier existe déjà ;
- supprimer complètement l'enquête de test ;
- vérifier qu'aucun résidu ne reste.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste valide.
- [ ] Une enquête complète peut être créée.
- [ ] Tous les dossiers attendus existent.
- [ ] `Enquete.sqlite` existe.
- [ ] Aucun dossier partiel ne reste en cas d'échec.
- [ ] Un dossier existant n'est jamais écrasé.
- [ ] Le chemin retourné est correct.
- [ ] Aucune dépendance GTK.
- [ ] Aucun code SQLite métier.
- [ ] Le nouveau test est intégré à `make test`.

---

## Commit attendu

```text
feat(core): create investigation project structure
```
