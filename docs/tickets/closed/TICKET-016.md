# Ticket #016

## Titre

Structurer le `Workspace` avec `GtkStack`.

---

## Objectif

Remplacer la gestion manuelle de visibilité des pages du `Workspace` par un
`GtkStack`.

Le `Workspace` doit pouvoir afficher proprement plusieurs pages internes sans
modifier son API publique actuelle.

Ce ticket prépare l'ajout futur de nouvelles vues :

- aperçu texte ;
- aperçu image ;
- aperçu PDF ;
- chronologie ;
- fiches d'entités ;
- rapports.

---

## Architecture

```text
Workspace
└── GtkStack
    ├── WelcomePage
    └── NodeInformationPage
```

Plus tard :

```text
Workspace
└── GtkStack
    ├── WelcomePage
    ├── NodeInformationPage
    ├── TextPreviewPage
    ├── ImagePreviewPage
    ├── PdfPreviewPage
    └── EntityPage
```

Le `Workspace` reste responsable de son affichage interne.

---

## Responsabilités

Le module `Workspace` doit :

- créer un `GtkStack` comme widget racine interne ;
- ajouter une page d'accueil ;
- ajouter une page d'informations sur le nœud sélectionné ;
- afficher la page d'accueil lorsqu'aucun nœud n'est sélectionné ;
- afficher la page d'informations lorsqu'un nœud est sélectionné ;
- conserver l'API publique existante.

---

## API publique conservée

Aucune nouvelle fonction publique n'est requise.

L'API reste :

```c
Workspace *workspace_new(void);

GtkWidget *workspace_get_widget(
    const Workspace *workspace
);

void workspace_set_selected_node(
    Workspace *workspace,
    const InvestigationNode *node
);

void workspace_free(
    Workspace *workspace
);
```

---

## Fichiers concernés

```text
src/widgets/workspace.c
```

Éventuellement :

```text
include/widgets/workspace.h
```

uniquement pour mettre à jour la documentation.

Aucun autre module ne doit être modifié.

---

## Structure interne attendue

```c
struct Workspace
{
    GtkWidget *root_widget;
    GtkWidget *stack;

    GtkWidget *welcome_page;
    GtkWidget *welcome_title_label;
    GtkWidget *welcome_status_label;
    GtkWidget *welcome_instruction_label;

    GtkWidget *node_page;
    GtkWidget *node_name_label;
    GtkWidget *node_type_label;
    GtkWidget *node_parent_label;
    GtkWidget *node_children_label;
};
```

Les noms exacts peuvent varier, mais doivent rester explicites.

---

## Pages attendues

### Page d'accueil

Nom interne :

```text
welcome
```

Contenu :

```text
Labfy Investigation

Aucune enquête ouverte

Sélectionnez ou créez une enquête
```

### Page d'informations

Nom interne :

```text
node-information
```

Contenu pour un dossier :

```text
00_BaseDeDonnees

Type : dossier
Parent : Template
Enfants : 2
```

Contenu pour un fichier :

```text
Enquete.sqlite

Type : fichier
Parent : 00_BaseDeDonnees
```

---

## Comportement attendu

Lorsque :

```c
workspace_set_selected_node(workspace, NULL);
```

le `GtkStack` doit afficher :

```text
welcome
```

Lorsqu'un nœud valide est transmis :

```c
workspace_set_selected_node(workspace, node);
```

le `GtkStack` doit afficher :

```text
node-information
```

---

## Hors périmètre

Ce ticket ne doit pas :

- ouvrir un fichier ;
- afficher son contenu ;
- ajouter une barre de navigation ;
- ajouter une animation personnalisée ;
- afficher plusieurs nœuds simultanément ;
- modifier le Core ;
- communiquer avec SQLite.

---

## Contraintes techniques

- GTK4 uniquement ;
- utiliser `GtkStack` ;
- aucune gestion manuelle de visibilité entre les pages ;
- aucun état global ;
- aucun changement de propriété des nœuds ;
- documentation cohérente ;
- compilation sans warning ;
- aucun `Gtk-CRITICAL`.

---

## Gestion de la propriété

Le `Workspace` possède uniquement sa structure d'encapsulation.

Les widgets sont gérés par l'arbre GTK une fois intégrés dans la fenêtre.

Le nœud reçu reste la propriété du `InvestigationTreeModel`.

Le `Workspace` ne doit jamais appeler :

```c
investigation_node_free(node);
```

---

## Critères d'acceptation

- [ ] Le projet compile sans warning.
- [ ] `make test` reste entièrement valide.
- [ ] `GtkStack` est utilisé.
- [ ] La page d'accueil apparaît sans sélection.
- [ ] La page d'informations apparaît après sélection.
- [ ] Le passage d'une page à l'autre fonctionne.
- [ ] Aucun appel manuel à `gtk_widget_set_visible()` n'est utilisé pour changer de page.
- [ ] L'API publique de `Workspace` reste stable.
- [ ] Aucun autre module n'est modifié.
- [ ] Aucun `Gtk-CRITICAL`.

---

## Tests manuels

1. Lancer l'application.
2. Ouvrir une enquête.
3. Vérifier que la page d'accueil est affichée sans sélection.
4. Sélectionner un dossier.
5. Vérifier l'affichage de la page d'informations.
6. Sélectionner un fichier.
7. Vérifier la mise à jour de la même page.
8. Changer d'enquête.
9. Vérifier le retour à la page d'accueil.
10. Lancer `make test`.

---

## Commit attendu

```text
refactor(gui): use GtkStack in workspace
```
