# Ticket #021

## Titre

Valider une enquête existante.

---

## Objectif

Ajouter au module `InvestigationProject` la capacité de vérifier qu’un dossier
correspond bien à une enquête Labfy Investigation valide.

La validation doit contrôler la présence et le type des éléments obligatoires,
sans modifier le contenu du dossier.

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

Toute ouverture d’enquête existante doit passer par
`InvestigationProject`.

La GUI ne doit jamais décider seule si un dossier est valide.

---

## Responsabilités

### InvestigationProject

Le module doit :

- recevoir le chemin d’un dossier ;
- vérifier que le chemin existe ;
- vérifier qu’il désigne un dossier ;
- vérifier la présence de l’arborescence obligatoire ;
- vérifier que les dossiers attendus sont bien des dossiers ;
- vérifier la présence de `00_BaseDeDonnees/Enquete.sqlite` ;
- vérifier que `Enquete.sqlite` est un fichier régulier ;
- retourner un résultat clair sans modifier le dossier.

---

## Interface publique attendue

Faire évoluer :

```text
include/core/investigation_project.h
src/core/investigation_project.c
```

Ajouter :

```c
bool investigation_project_validate(
    const char *investigation_path
);
```

---

## Contrat

La fonction retourne :

```c
true
```

si le dossier est une enquête valide.

Elle retourne :

```c
false
```

si :

- le chemin est invalide ;
- le dossier n’existe pas ;
- le chemin désigne un fichier ;
- un dossier obligatoire manque ;
- un élément attendu comme dossier est en réalité un fichier ;
- `Enquete.sqlite` manque ;
- `Enquete.sqlite` n’est pas un fichier régulier.

La fonction ne doit jamais modifier le système de fichiers.

---

## Structure obligatoire

Les dossiers suivants doivent exister :

```text
00_BaseDeDonnees
01_Preuves_Originales
02_Preuves_Traitees
03_Chronologie
04_Entites
05_Rapports
06_Exports
07_Notes
08_Sources
09_Hash
```

Les sous-dossiers suivants doivent également exister :

```text
01_Preuves_Originales/Captures_Ecran
01_Preuves_Originales/Conversations
01_Preuves_Originales/Documents
01_Preuves_Originales/Emails
01_Preuves_Originales/Photos
01_Preuves_Originales/Videos

02_Preuves_Traitees/Annotations
02_Preuves_Traitees/Extractions
02_Preuves_Traitees/OCR
02_Preuves_Traitees/Redactions

04_Entites/Adresses_Email
04_Entites/Comptes_Bancaires
04_Entites/Comptes_Facebook
04_Entites/Comptes_Instagram
04_Entites/Documents_Identite
04_Entites/IBAN
04_Entites/Personnes
04_Entites/Pseudonymes
04_Entites/Telephones
04_Entites/Autres
```

Le fichier suivant doit exister :

```text
00_BaseDeDonnees/Enquete.sqlite
```

---

## Réutilisation de la structure déclarative

La validation doit réutiliser la même liste de chemins que la création.

Il ne doit pas exister deux listes indépendantes décrivant l’arborescence.

La structure de référence doit rester centralisée dans
`investigation_project.c`.

---

## Hors périmètre

Ce ticket ne doit pas :

- ouvrir SQLite ;
- lire le schéma SQL ;
- vérifier la version de la base ;
- créer les éléments manquants ;
- réparer une enquête ;
- importer automatiquement des fichiers ;
- modifier GTK ;
- ouvrir automatiquement l’enquête dans l’application.

---

## Gestion des erreurs

Dans ce ticket, la fonction retourne uniquement un booléen.

Les détails d’erreur plus précis pourront être ajoutés plus tard avec :

```c
GError
```

ou une énumération métier dédiée.

La validation ne doit produire aucun `g_warning()` pour un dossier simplement
invalide : un résultat `false` suffit.

---

## Contraintes techniques

- C17 ;
- GLib autorisée ;
- aucune dépendance GTK ;
- aucune dépendance SQLite requise ;
- aucune écriture sur le disque ;
- aucun état global ;
- documentation Doxygen ;
- compilation sans warning.

---

## Tests

Faire évoluer :

```text
tests/test_investigation_project.c
```

Le test doit vérifier :

- une enquête complète est valide ;
- `NULL` est refusé ;
- une chaîne vide est refusée ;
- un chemin inexistant est refusé ;
- un fichier simple est refusé ;
- une enquête sans `Enquete.sqlite` est refusée ;
- une enquête avec un dossier obligatoire manquant est refusée ;
- un élément attendu comme dossier mais remplacé par un fichier est refusé ;
- une enquête valide reste inchangée après validation.

---

## Critères d’acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste entièrement valide.
- [ ] Une enquête créée par `investigation_project_create()` est valide.
- [ ] Un chemin invalide est refusé.
- [ ] Un dossier incomplet est refusé.
- [ ] Un faux fichier `Enquete.sqlite` incorrect est refusé.
- [ ] Aucun élément n’est créé pendant la validation.
- [ ] Aucun élément n’est supprimé pendant la validation.
- [ ] La structure de référence n’est pas dupliquée.
- [ ] Aucune dépendance GTK.
- [ ] Aucun `Gtk-CRITICAL`.

---

## Commit attendu

```text
feat(core): validate investigation project structure
```
