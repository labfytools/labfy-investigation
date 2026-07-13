# Ticket #011

## Titre

Connecter le modèle d'arborescence à la `Sidebar`.

---

## Objectif

Relier l'enquête sélectionnée au panneau latéral.

Après la sélection d'un dossier, l'application doit :

1. créer l'objet `Investigation` ;
2. construire son `InvestigationTreeModel` ;
3. transmettre le modèle à la `Sidebar` ;
4. afficher temporairement le nom du nœud racine dans le panneau latéral.

Ce ticket valide la communication entre le Core et l'interface graphique.

---

## Architecture

```text
FolderDialog
      │
      ▼
Application
      │
      ├── Investigation
      │
      └── InvestigationTreeModel
                    │
                    ▼
                 Sidebar
```

Le module `Application` reste responsable de la coordination.

La `Sidebar` ne parcourt jamais directement le système de fichiers.

---

## Responsabilités

### Application

Le module `Application` doit :

- recevoir le dossier sélectionné ;
- créer l'objet `Investigation` ;
- construire le modèle avec `InvestigationTreeBuilder` ;
- conserver le modèle pendant toute la durée de l'enquête ;
- transmettre le modèle à `MainWindow` ;
- libérer l'ancien modèle avant d'en ouvrir un nouveau ;
- libérer le modèle à la fermeture de l'application.

### MainWindow

Le module `MainWindow` doit :

- recevoir un modèle d'arborescence ;
- le transmettre au composant `Sidebar`.

### Sidebar

Le module `Sidebar` doit :

- recevoir un `InvestigationTreeModel` en lecture seule ;
- lire le nœud racine ;
- afficher temporairement son nom dans le titre du panneau ;
- ne jamais détruire le modèle reçu.

---

## Principe de propriété

`Application` est propriétaire de :

```text
Investigation
InvestigationTreeModel
MainWindow
```

La `Sidebar` reçoit uniquement une référence non propriétaire vers le modèle.

Elle ne doit jamais appeler :

```c
investigation_tree_model_free(tree_model);
```

Le modèle est libéré uniquement par `Application`.

---

## Fichiers concernés

```text
src/core/application.c

include/views/main_window.h
src/views/main_window.c

include/widgets/sidebar.h
src/widgets/sidebar.c
```

Aucun nouveau module n'est nécessaire.

---

## Interfaces publiques à ajouter

### MainWindow

```c
void main_window_set_tree_model(
    MainWindow *main_window,
    const InvestigationTreeModel *tree_model
);
```

### Sidebar

```c
void sidebar_set_tree_model(
    Sidebar *sidebar,
    const InvestigationTreeModel *tree_model
);
```

---

## Comportement attendu

Avant l'ouverture d'une enquête, la `Sidebar` affiche :

```text
Dossier d'enquête
```

Après la sélection du dossier :

```text
Template
```

ou le nom réel du dossier racine sélectionné.

À ce stade, les enfants ne sont pas encore affichés.

---

## Hors périmètre

Ce ticket ne doit pas :

- afficher les enfants du nœud racine ;
- utiliser `GtkTreeListModel` ;
- créer un explorateur de fichiers complet ;
- permettre de sélectionner un nœud ;
- ouvrir un fichier ;
- rafraîchir automatiquement le modèle ;
- modifier le système de fichiers ;
- communiquer avec SQLite.

---

## Gestion des erreurs

Si la construction du modèle échoue :

- l'application ne doit pas planter ;
- l'ancien modèle doit rester valide jusqu'à son remplacement explicite ;
- un message d'erreur doit être affiché dans le terminal ;
- la `Sidebar` ne doit recevoir aucun pointeur invalide.

Si une nouvelle enquête est ouverte avec succès :

1. créer la nouvelle enquête ;
2. construire le nouveau modèle ;
3. seulement ensuite libérer l'ancienne enquête et l'ancien modèle ;
4. installer les nouveaux objets.

Cette séquence évite de perdre l'enquête actuellement ouverte en cas d'échec.

---

## Contraintes techniques

- C17 ;
- aucun état global ;
- aucune lecture du système de fichiers dans `Sidebar` ;
- aucun transfert de propriété vers `Sidebar` ;
- documentation Doxygen ;
- compilation sans warning ;
- absence de `Gtk-CRITICAL` ;
- respect des conventions de nommage.

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste valide.
- [ ] La sélection d'un dossier construit un modèle.
- [ ] `Application` conserve le modèle.
- [ ] `MainWindow` transmet le modèle à `Sidebar`.
- [ ] La `Sidebar` affiche le nom du nœud racine.
- [ ] L'ouverture successive de deux dossiers fonctionne.
- [ ] L'ancien modèle est correctement libéré.
- [ ] Une erreur de construction ne provoque pas de crash.
- [ ] La fermeture de l'application libère le modèle.
- [ ] Aucun code de parcours du disque n'apparaît dans `Sidebar`.
- [ ] Aucun enfant n'est encore affiché.

---

## Tests manuels

1. Lancer l'application.
2. Sélectionner le dossier `Template`.
3. Vérifier que la `Sidebar` affiche `Template`.
4. Fermer l'application.
5. Vérifier l'absence de warning critique.
6. Ouvrir successivement deux dossiers différents.
7. Vérifier que le titre de la `Sidebar` est mis à jour.
8. Vérifier que `make test` reste entièrement valide.

---

## Commit attendu

```text
feat(gui): connect investigation model to sidebar
```
