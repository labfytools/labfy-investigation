# Ticket #019

## Titre

Afficher une icône selon le type de nœud dans l'arborescence.

---

## Objectif

Améliorer la lisibilité de `InvestigationTreeView` en affichant une icône
devant chaque dossier ou fichier.

Les icônes doivent être déterminées uniquement dans la couche graphique.

---

## Architecture

```text
InvestigationNode
        │
        ▼
InvestigationTreeView
        │
        ▼
GtkImage + GtkLabel
```

Le Core fournit :

- le nom ;
- le chemin ;
- le type.

La vue choisit l'icône à afficher.

---

## Responsabilités

### InvestigationTreeView

Le module doit :

- afficher une icône devant chaque nœud ;
- utiliser une icône de dossier pour les répertoires ;
- utiliser une icône générique pour les fichiers inconnus ;
- choisir certaines icônes spécialisées selon l'extension ;
- conserver le nom du nœud à côté de l'icône ;
- rester compatible avec `GtkTreeExpander`.

---

## Fichiers concernés

```text
src/widgets/investigation_tree_view.c
```

Éventuellement :

```text
include/widgets/investigation_tree_view.h
```

uniquement si la documentation doit être précisée.

Aucune modification du Core.

---

## Extensions reconnues

Première version minimale :

```text
.sqlite .db
.jpg .jpeg .png .webp
.pdf
.txt .md .log
.csv
.zip .tar .gz .xz
.mp4 .mkv .avi .mov
```

Les extensions inconnues utilisent une icône de fichier générique.

La comparaison doit être insensible à la casse.

---

## Icônes GTK attendues

Utiliser les noms d'icônes du thème système, par exemple :

```text
folder-symbolic
text-x-generic-symbolic
application-pdf-symbolic
image-x-generic-symbolic
video-x-generic-symbolic
package-x-generic-symbolic
x-office-spreadsheet-symbolic
folder-database-symbolic
```

Si une icône spécialisée n'est pas disponible dans le thème, GTK doit
retomber proprement sur une icône générique.

---

## Structure visuelle d'une ligne

```text
GtkTreeExpander
└── GtkBox horizontal
    ├── GtkImage
    └── GtkLabel
```

Le label continue d'afficher le nom du nœud.

---

## Fonctions privées suggérées

```c
static const char *investigation_tree_view_get_icon_name(
    const InvestigationNode *node
);
```

Cette fonction doit :

- retourner `folder-symbolic` pour un dossier ;
- inspecter l'extension pour un fichier ;
- retourner une icône générique si aucune extension ne correspond.

---

## Hors périmètre

Ce ticket ne doit pas :

- charger des miniatures ;
- analyser le contenu réel des fichiers ;
- lire les types MIME ;
- modifier le Core ;
- ouvrir les fichiers ;
- colorer les lignes ;
- ajouter des badges ;
- ajouter un menu contextuel.

---

## Contraintes techniques

- GTK4 uniquement ;
- utiliser `GtkImage` ;
- utiliser les icônes du thème système ;
- aucun fichier d'icône embarqué ;
- aucun état global ;
- aucune lecture directe du système de fichiers ;
- compilation sans warning ;
- aucun `Gtk-CRITICAL`.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste valide.
- [ ] Les dossiers affichent une icône de dossier.
- [ ] Les fichiers génériques affichent une icône de fichier.
- [ ] Les PDF ont une icône dédiée.
- [ ] Les images ont une icône dédiée.
- [ ] Les vidéos ont une icône dédiée.
- [ ] Les archives ont une icône dédiée.
- [ ] Les bases SQLite ont une icône dédiée.
- [ ] Le nom du nœud reste visible.
- [ ] Le développement et le repli fonctionnent toujours.
- [ ] La sélection fonctionne toujours.
- [ ] Aucun code Core n'est modifié.
- [ ] Aucun `Gtk-CRITICAL`.

---

## Tests manuels

1. Ouvrir une enquête contenant plusieurs types de fichiers.
2. Vérifier l'icône des dossiers.
3. Vérifier une image.
4. Vérifier un PDF.
5. Vérifier une base SQLite.
6. Vérifier une archive.
7. Vérifier un fichier inconnu.
8. Développer et replier plusieurs branches.
9. Sélectionner plusieurs nœuds.
10. Lancer `make test`.

---

## Commit attendu

```text
feat(gui): add icons to investigation tree
```
