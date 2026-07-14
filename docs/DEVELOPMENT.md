# Développement

Version : 1.0

Dernière mise à jour : 2026-07-06

Auteur : fy59

# Sommaire

1. Objet
2. Objectif
3. Technologies
4. Outils
5. Commandes
6. Revue de code
7. Philosophie
8. Architecture
9. Règles de codage
10. Documentation
11. Organisation des fichiers
12. Gestion mémoire
13. Base de données
14. Git
15. Développement
16. Tests
17. Gestion des erreurs
18. Convention de nommage
   - Fichiers
   - Fonctions
   - Variables
   - Types
   - Constantes
   - Énumérations
19. Principes de conception
   - Simplicité
   - Lisibilité
   - Responsabilité unique
   - Zéro surprise
   - Style de code
   - Le compilateur est notre premier relecteur
   - Documentation
   - Boy Scout Rule
   - Robustesse avant optimisation
   - Une fonctionnalité = un commit
20. Décisions techniques
21. Branche principale
22. Dépendances
---

## Objet

Ce document définit les règles de développement du projet **Labfy Investigation**.

L'objectif est de garantir un code :

- lisible ;
- maintenable ;
- documenté ;
- portable ;
- simple à faire évoluer.

Ces règles s'appliquent à l'ensemble du projet.

---

# Objectif

L'objectif du projet est de produire un logiciel libre, robuste et documenté destiné à faciliter la gestion d'enquêtes OSINT, tout en constituant un support d'apprentissage du langage C, de GTK4, de SQLite et des bonnes pratiques de développement logiciel.



---

# Technologies

Le projet repose sur les technologies suivantes :

| Technologie | Version |
|-------------|----------|
| Langage | C17 |
| Interface graphique | GTK 4.10+ |
| Base de données | SQLite 3.45+ |
| Compilateur | GCC 15+ |
| Build | Make |
| Documentation | Doxygen |
| Gestion de version | Git |

---

# Outils

Les outils suivants sont utilisés pendant le développement :

- gcc
- clang
- clang-format
- clang-tidy
- cppcheck
- valgrind
- doxygen
- graphviz
- make
- Git

---

# Commandes

Compilation :

make

Exécution :

make run

Nettoyage :

make clean

Documentation :

make docs

Tests :

make test

---

# Revue de code

Avant chaque commit important, le code doit être vérifié selon les critères suivants :

- respecte les conventions de nommage ;
- compile sans warning ;
- est documenté ;
- respecte l'architecture MVC ;
- ne duplique pas de code ;
- gère correctement les erreurs ;
- libère correctement les ressources.

---

# Philosophie

Labfy Investigation est développé comme un logiciel professionnel.

Les priorités sont les suivantes :

1. Simplicité.
2. Lisibilité.
3. Robustesse.
4. Documentation.
5. Évolutivité.

Un code plus simple est toujours préféré à un code plus complexe.

---

# Architecture

Le projet suit une architecture de type MVC.

```
Vue (GTK)
        │
        ▼
Contrôleur
        │
        ▼
DAO
        │
        ▼
SQLite
```

Les responsabilités sont clairement séparées.

Une couche ne doit jamais accéder directement à une couche qui ne lui appartient pas.

---

# Règles de codage

Le projet est développé en **C17**.

Les options de compilation sont :

- `-std=c17`
- `-Wall`
- `-Wextra`
- `-Wpedantic`
- `-Werror`

Aucun warning n'est accepté.

Le projet doit compiler sans erreur ni avertissement.

---

# Documentation

Chaque fichier possède un en-tête.

Chaque fonction publique est documentée avec Doxygen.

Les commentaires expliquent :

- pourquoi un choix a été fait ;
- les contraintes techniques ;
- les hypothèses.

Les commentaires ne doivent jamais simplement répéter le code.

---

# Organisation des fichiers

Chaque fichier possède une responsabilité unique.

Une fonction ne doit réaliser qu'une seule tâche.

Lorsque cela devient nécessaire, le code est découpé en plusieurs modules.

---

# Gestion mémoire

Toute allocation mémoire possède une fonction de libération correspondante.

Les fuites mémoire sont considérées comme des bugs.

Les vérifications sont réalisées régulièrement avec Valgrind.

---

# Base de données

Toutes les opérations sur SQLite passent par la couche DAO.

Le reste de l'application ne manipule jamais directement SQLite.

---

# Git

Le dépôt Git contient uniquement :

- le code source ;
- la documentation ;
- les modèles ;
- les scripts.

Les enquêtes réelles ne sont jamais versionnées.

Chaque commit :

- compile ;
- fonctionne ;
- correspond à une seule fonctionnalité.

Les messages de commit suivent la convention :

```
type(scope): description
```

Exemples :

```
feat(gui): create main window

feat(database): add evidence dao

fix(core): close sqlite connection

docs: update architecture
```

---

# Développement

Avant toute nouvelle fonctionnalité :

1. Définir le besoin.
2. Concevoir l'architecture.
3. Développer.
4. Tester.
5. Documenter.
6. Commit.

Aucune fonctionnalité n'est considérée comme terminée tant que ces six étapes ne sont pas réalisées.

---

# Tests

Chaque fonctionnalité doit être testée avant son intégration.

Lorsque cela est possible :

- tests unitaires ;
- tests fonctionnels ;
- vérification sous Valgrind ;
- compilation sans warning.

Un correctif est toujours accompagné d'un test permettant de vérifier que le problème est résolu.

---

# Gestion des erreurs

Aucune erreur ne doit être ignorée.

Les valeurs de retour des fonctions sont systématiquement vérifiées.

Les messages d'erreur doivent être explicites et permettre d'identifier rapidement l'origine du problème.

Les ressources ouvertes doivent toujours être libérées, même en cas d'erreur.

---

# Convention de nommage

Afin de garantir la cohérence du projet, une convention de nommage stricte est appliquée.

Toute dérogation à cette convention doit être justifiée.

---

## Fichiers

Les noms de fichiers sont écrits en **snake_case**.

Exemples :

```text
database.c
database.h

preuve.c
preuve.h

main_window.c
main_window.h

types_entite.c
types_entite.h
```

Les noms doivent être explicites et refléter la responsabilité du module.

---

## Fonctions

Les fonctions sont toujours préfixées par le nom du module auquel elles appartiennent.

Exemples :

```c
db_open();
db_close();

preuve_new();
preuve_free();

main_window_create();
main_window_destroy();
```

Les fonctions génériques telles que :

```c
create();
init();
run();
```

sont interdites, car elles deviennent rapidement ambiguës lorsque le projet grandit.

---

## Variables

Les variables utilisent également la convention **snake_case**.

Exemples :

```c
preuve_id

type_id

main_window

database

source_id

date_collecte
```

Les noms doivent décrire clairement le contenu de la variable.

Les noms suivants sont à proscrire :

```c
x

tmp

toto

test
```

à l'exception des variables locales très courtes utilisées dans une boucle ou un contexte limité :

```c
for (size_t i = 0; i < count; ++i)
```

---

## Types

Les structures représentent des objets métiers et utilisent le **PascalCase**.

Exemples :

```c
typedef struct
{
    ...
} Preuve;

typedef struct
{
    ...
} Entite;

typedef struct
{
    ...
} Personne;
```

---

## Constantes

Les constantes et macros sont écrites en majuscules avec des underscores.

Exemples :

```c
MAX_PATH_LENGTH

SHA256_LENGTH

DEFAULT_WINDOW_WIDTH
```

---

## Énumérations

Les énumérations utilisent un préfixe correspondant au type.

Exemple :

```c
typedef enum
{
    PREUVE_CAPTURE,
    PREUVE_EMAIL,
    PREUVE_VIDEO
} PreuveType;
```

---

## Objectif

Le nom d'un fichier, d'une fonction ou d'une variable doit permettre de comprendre immédiatement son rôle, sans avoir à consulter son implémentation.

Le code doit être explicite avant d'être concis.

---

# Principes de conception

Les principes suivants guident le développement de l'ensemble du projet.

Ils doivent être respectés avant toute considération d'optimisation.

---

## Simplicité

La solution la plus simple est privilégiée.

Un code plus court n'est pas forcément un meilleur code.

La lisibilité prime toujours.

---

## Lisibilité

Le code doit pouvoir être compris plusieurs mois après son écriture.

Les noms des fichiers, fonctions, variables et structures doivent être explicites.

---

## Responsabilité unique

Chaque module possède une responsabilité unique.

Chaque fonction réalise une seule tâche.

Si une fonction devient difficile à expliquer, elle doit probablement être découpée.

---

## Zéro surprise

Le comportement d'une fonction doit être prévisible.

Le nom d'une fonction doit permettre de comprendre ce qu'elle réalise sans avoir à lire son implémentation.

Exemple :

```c
preuve_save();
```

est préférable à :

```c
save();
```

---

# Style de code

- Indentation : 4 espaces.
- Largeur maximale : 100 colonnes.
- Accolades sur une nouvelle ligne (style Allman).
- Une déclaration par ligne.
- Une instruction par ligne.

---

## Le compilateur est notre premier relecteur

Les warnings sont considérés comme des erreurs.

Le projet compile toujours avec :

- `-Wall`
- `-Wextra`
- `-Wpedantic`
- `-Werror`

---

## Documentation

Le code explique **comment** fonctionne une fonctionnalité.

Les commentaires expliquent **pourquoi** elle existe.

Les commentaires ne doivent jamais simplement répéter le code.

---

## Boy Scout Rule

À chaque modification d'un fichier, celui-ci doit être laissé dans un état au moins aussi propre qu'avant la modification.

Cela peut être :

- améliorer un nom de variable ;
- corriger un commentaire ;
- supprimer du code mort ;
- simplifier une fonction.

---

## Robustesse avant optimisation

Les optimisations ne sont réalisées que lorsqu'un besoin est identifié et mesuré.

La robustesse et la lisibilité sont prioritaires.

---

## Une fonctionnalité = un commit

Chaque commit correspond à une seule fonctionnalité.

Chaque commit :

- compile ;
- est testé ;
- est documenté.

Les messages de commit suivent la convention :

```
type(scope): description
```

Exemples :

```
feat(gui): create main window
feat(database): add evidence dao
fix(core): close sqlite connection
docs: update development guide
```

---

# Décisions techniques

Toute décision technique importante doit être documentée.

Le projet privilégie les choix simples, documentés et facilement maintenables.

Lorsque plusieurs solutions existent, la préférence est donnée à celle qui facilite la compréhension du code par un nouveau développeur.

---

## Gestion de la mémoire

Le projet applique une règle unique concernant la propriété des ressources.

> Le propriétaire crée.
> Le propriétaire détruit.

Lorsqu'une ressource est transmise à un objet qui en devient propriétaire, le code appelant ne doit plus la libérer.

Chaque module est responsable uniquement des ressources qu'il possède.

Cette règle s'applique à toutes les structures du projet.

---

## Bibliothèques autorisées

Le projet privilégie les bibliothèques éprouvées plutôt que des réimplémentations.

### Couche Core

Autorisé :

- Langage C17
- GLib
- SQLite

Interdit :

- GTK

Les structures de données fournies par GLib (GPtrArray, GHashTable, GList, etc.) doivent être privilégiées lorsqu'elles répondent au besoin du projet.

---

# Branche principale

La branche `main` est toujours stable.

Le projet doit toujours :

- compiler ;
- démarrer ;
- être documenté.

---

# Dépendances

Les nouvelles dépendances doivent être justifiées.

Avant d'ajouter une bibliothèque externe, il convient de vérifier :

- si la bibliothèque standard suffit ;
- si GTK ou GLib proposent déjà la fonctionnalité ;
- si la nouvelle dépendance apporte un réel bénéfice.

Labfy Investigation est un projet GTK4 natif.
Aucun nouveau code ne doit utiliser une API dépréciée ou héritée de GTK3.
Les nouveaux développements utilisent exclusivement les composants modernes de GTK4.

---

# Licence

Le projet est distribué sous la licence MIT.

Toute nouvelle contribution est considérée comme publiée sous cette même licence.

Le texte complet de la licence est disponible dans le fichier `LICENSE` situé à la racine du projet.

---

# Historique

## Version 1.0

- Création du guide de développement.
- Définition des conventions de codage.
- Définition de l'architecture.
- Définition des règles Git.
