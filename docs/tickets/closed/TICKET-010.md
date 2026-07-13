# Ticket #010

## Titre

Construire l'arborescence d'une enquête depuis le système de fichiers.

---

## Objectif

Créer le module `InvestigationTreeBuilder`.

Ce module doit parcourir récursivement le dossier racine d'une enquête et
construire un `InvestigationTreeModel` représentant son contenu.

---

## Responsabilités

Le module `InvestigationTreeBuilder` doit :

- recevoir le chemin racine d'une enquête ;
- vérifier que ce chemin désigne un dossier existant ;
- créer le nœud racine ;
- parcourir récursivement les dossiers ;
- créer un `InvestigationNode` pour chaque dossier ;
- créer un `InvestigationNode` pour chaque fichier ;
- assembler les relations parent/enfants ;
- retourner un `InvestigationTreeModel` complet ;
- nettoyer toutes les ressources en cas d'erreur.

---

## Hors périmètre

Ce ticket ne doit pas :

- afficher l'arborescence avec GTK ;
- modifier le système de fichiers ;
- créer ou supprimer des fichiers ;
- trier les éléments ;
- filtrer les fichiers cachés ;
- surveiller les changements du disque ;
- suivre les liens symboliques ;
- communiquer avec SQLite.

---

## Architecture

```text
Investigation
        │
        ▼
InvestigationTreeBuilder
        │
        ▼
InvestigationTreeModel
        │
        ▼
InvestigationNode
```

Le module appartient à la couche `core`.

Il peut utiliser GLib et GIO, mais ne doit jamais dépendre de GTK.

---

## Fichiers concernés

```text
include/core/investigation_tree_builder.h
src/core/investigation_tree_builder.c
```

Les tests seront placés dans :

```text
tests/test_investigation_tree_builder.c
```

---

## Interface publique attendue

```c
InvestigationTreeModel *investigation_tree_builder_build(
    const char *root_path
);
```

---

## Principe de propriété

En cas de succès, la fonction retourne un nouveau
`InvestigationTreeModel`.

Le code appelant devient propriétaire du modèle retourné et doit le libérer
avec :

```c
investigation_tree_model_free(tree_model);
```

En cas d'échec, la fonction retourne `NULL` et doit avoir libéré toutes les
ressources créées pendant la construction.

---

## Comportement attendu

À partir de :

```text
Enquete_Test/
├── 00_BaseDeDonnees/
│   └── Enquete.sqlite
├── 01_Preuves_Originales/
│   ├── Captures_Ecran/
│   └── Emails/
└── README.md
```

le module doit construire en mémoire :

```text
Enquete_Test
├── 00_BaseDeDonnees
│   └── Enquete.sqlite
├── 01_Preuves_Originales
│   ├── Captures_Ecran
│   └── Emails
└── README.md
```

---

## Gestion des liens symboliques

Les liens symboliques ne sont pas suivis.

Cette règle évite :

- les boucles récursives ;
- la sortie involontaire du dossier d'enquête ;
- l'analyse de fichiers extérieurs à l'enquête.

Leur prise en charge éventuelle fera l'objet d'un ticket distinct.

---

## Dépendances

- C17 ;
- GLib ;
- GIO.

Aucune dépendance GTK ou SQLite.

---

## Contraintes techniques

- aucune variable globale ;
- aucune modification du système de fichiers ;
- parcours récursif ;
- utilisation de `GFile` et `GFileEnumerator` ;
- libération correcte des `GObject` avec `g_object_unref()` ;
- respect de la règle « le propriétaire détruit » ;
- documentation Doxygen ;
- compilation sans warning ;
- aucune fuite mémoire.

---

## Cas à gérer

- chemin valide ;
- chemin relatif ;
- chemin `NULL` ;
- chemin vide ;
- chemin inexistant ;
- chemin désignant un fichier ;
- dossier vide ;
- plusieurs niveaux de sous-dossiers ;
- fichiers et dossiers mélangés ;
- erreur rencontrée pendant le parcours.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] Un dossier valide produit un modèle.
- [ ] Le nom du dossier racine est correct.
- [ ] Les dossiers produisent des nœuds de type `DIRECTORY`.
- [ ] Les fichiers produisent des nœuds de type `FILE`.
- [ ] Les relations parent/enfants sont correctes.
- [ ] Plusieurs niveaux de profondeur sont pris en charge.
- [ ] Un dossier vide est pris en charge.
- [ ] Un chemin invalide retourne `NULL`.
- [ ] Les liens symboliques ne sont pas suivis.
- [ ] Toutes les ressources sont libérées en cas d'erreur.
- [ ] Aucun code GTK.
- [ ] Aucun code SQLite.
- [ ] `make test` exécute le nouveau test.

---

## Tests

Le test doit créer une arborescence temporaire isolée :

```text
TestCase/
├── DirectoryA/
│   └── FileA.txt
├── DirectoryB/
└── RootFile.md
```

Il doit vérifier :

- le nom du nœud racine ;
- le nombre d'enfants de la racine ;
- la présence des deux dossiers ;
- la présence du fichier racine ;
- la présence de `FileA.txt` dans `DirectoryA` ;
- le type de chaque nœud ;
- le parent de chaque enfant ;
- la gestion d'un dossier vide ;
- les chemins invalides ;
- la destruction complète du modèle.

---

## Commit attendu

```text
feat(core): build investigation tree from filesystem
```
