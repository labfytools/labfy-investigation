# Ticket #032 — Factoriser le chargement et l’installation d’une enquête

## Contexte

Après le ticket #031.1, l’application sait :

- créer une enquête ;
- ouvrir une enquête existante ;
- remplacer la session active ;
- fermer proprement l’application.

Cependant, les flux « Nouvelle enquête » et « Ouvrir une enquête » dupliquent encore une partie importante de la logique :

```text
investigation_session_open()
→ récupération du projet
→ récupération du chemin racine
→ investigation_tree_builder_build()
→ application_install_session()
→ nettoyage en cas d’échec
```

Cette duplication augmentera avec les futures fonctions :

- enquêtes récentes ;
- ouverture depuis la ligne de commande ;
- restauration de session ;
- ouverture depuis un rapport ou une archive.

## Objectif

Créer dans `src/core/application.c` une fonction interne unique qui ouvre et installe une enquête à partir de son dossier racine.

Contrat attendu :

```c
static gboolean application_open_and_install_investigation(
    Application *application,
    const char *root_path,
    GError **error
);
```

Cette fonction doit garantir :

```text
succès :
    Application devient propriétaire de la nouvelle session
    Application devient propriétaire du nouvel arbre
    ancienne session libérée
    ancien arbre libéré
    fenêtre mise à jour

échec :
    ancienne session conservée
    ancien arbre conservé
    nouvelle session libérée
    nouvel arbre libéré
    erreur transmise à l’appelant
```

---

# Travail à réaliser

## 1. Ajouter un domaine d’erreur privé

Dans `src/core/application.c`, ajouter un domaine d’erreur uniquement utilisé par le contrôleur.

Exemple :

```c
typedef enum
{
    APPLICATION_OPEN_ERROR_INVALID_ARGUMENT,
    APPLICATION_OPEN_ERROR_INVALID_PROJECT,
    APPLICATION_OPEN_ERROR_TREE_BUILD,
    APPLICATION_OPEN_ERROR_INSTALL
} ApplicationOpenError;
```

Ajouter :

```c
#define APPLICATION_OPEN_ERROR \
    application_open_error_quark()
```

Puis :

```c
static GQuark application_open_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-application-open-error"
    );
}
```

Le domaine reste privé à `application.c`.

---

## 2. Créer la fonction factorisée

Ajouter :

```c
static gboolean application_open_and_install_investigation(
    Application *application,
    const char *root_path,
    GError **error
);
```

La fonction doit :

1. valider `application` ;
2. valider `application->main_window` ;
3. valider `root_path` ;
4. ouvrir une nouvelle `InvestigationSession` ;
5. récupérer son `InvestigationProject` ;
6. récupérer le chemin racine canonique ;
7. construire un nouvel `InvestigationTreeModel` ;
8. appeler `application_install_session()` ;
9. transférer la propriété uniquement en cas de succès.

### Règle de propriété

Avant `application_install_session()` :

```text
la fonction possède new_session
la fonction possède new_tree_model
```

Après succès :

```text
Application possède new_session
Application possède new_tree_model
```

Après échec :

```text
la fonction doit libérer les objets qu’elle possède encore
```

### Validation de `GError`

La fonction doit respecter la convention GLib :

```c
g_return_val_if_fail(
    error == NULL || *error == NULL,
    FALSE
);
```

L’utilisation de `g_return_val_if_fail()` est acceptable ici pour vérifier le contrat du développeur.

Les erreurs utilisateur doivent être produites avec :

```c
g_set_error()
g_set_error_literal()
g_propagate_prefixed_error()
```

---

## 3. Propager l’erreur de `InvestigationSession`

Si :

```c
investigation_session_open()
```

échoue, la fonction doit conserver l’erreur métier d’origine et lui ajouter du contexte.

Exemple conceptuel :

```text
Impossible d’ouvrir l’enquête : la base SQLite est absente
```

Ne pas remplacer l’erreur précise par un simple :

```text
Erreur inconnue
```

lorsqu’un `GError` est disponible.

---

## 4. Refactoriser l’ouverture d’une enquête existante

`application_on_folder_selected()` ne doit plus contenir directement :

```c
investigation_session_open()
investigation_tree_builder_build()
application_install_session()
```

Elle doit seulement :

1. accepter l’annulation ;
2. appeler `application_open_and_install_investigation()` ;
3. journaliser l’erreur ;
4. libérer le `GError`.

Structure attendue :

```c
static void application_on_folder_selected(
    const char *folder_path,
    gpointer user_data
)
{
    Application *application = user_data;
    GError *error = NULL;

    if (application == NULL ||
        folder_path == NULL)
    {
        return;
    }

    if (!application_open_and_install_investigation(
            application,
            folder_path,
            &error
        ))
    {
        g_warning(
            "Impossible d'ouvrir l'enquête '%s' : %s",
            folder_path,
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        g_clear_error(&error);
    }
}
```

L’annulation ne doit pas produire de warning.

---

## 5. Refactoriser la création d’une enquête

`application_on_create_investigation()` doit conserver uniquement :

1. la création physique du projet ;
2. l’appel à la fonction factorisée ;
3. le message spécifique indiquant que le projet existe sur le disque si son ouverture échoue.

Le flux devient :

```text
investigation_project_create()
→ application_open_and_install_investigation()
```

La fonction ne doit plus reconstruire elle-même :

```text
session
projet
chemin canonique
arbre
installation
```

En cas d’échec après création, le dossier créé reste volontairement sur le disque.

Le message doit être explicite :

```text
L’enquête a été créée dans « ... », mais son ouverture a échoué : ...
```

---

## 6. Ne pas modifier `application_install_session()`

Cette fonction conserve sa responsabilité actuelle :

- valider les objets prêts à installer ;
- libérer l’ancien état ;
- transférer la propriété ;
- mettre à jour `MainWindow`.

Le nouveau helper prépare les objets.

`application_install_session()` réalise le remplacement final.

---

# Pseudo-code de la fonction factorisée

```c
static gboolean application_open_and_install_investigation(
    Application *application,
    const char *root_path,
    GError **error
)
{
    InvestigationSession *new_session = NULL;
    InvestigationTreeModel *new_tree_model = NULL;
    const InvestigationProject *project = NULL;
    const char *canonical_root_path = NULL;
    GError *session_error = NULL;

    valider les arguments;

    new_session = investigation_session_open(
        root_path,
        &session_error
    );

    si échec :
        propager session_error avec contexte;
        return FALSE;

    project = investigation_session_get_project(
        new_session
    );

    valider project;

    canonical_root_path =
        investigation_project_get_root_path(project);

    valider canonical_root_path;

    new_tree_model =
        investigation_tree_builder_build(
            canonical_root_path
        );

    si échec :
        produire GError;
        fermer new_session;
        return FALSE;

    si application_install_session() échoue :
        produire GError;
        libérer new_tree_model;
        fermer new_session;
        return FALSE;

    return TRUE;
}
```

---

# Tests manuels

## Création valide

1. lancer l’application ;
2. créer une enquête ;
3. vérifier le titre ;
4. vérifier la barre d’état ;
5. vérifier l’arborescence.

## Ouverture valide

1. fermer et relancer ;
2. ouvrir l’enquête créée ;
3. vérifier le même résultat.

## Remplacement valide

1. ouvrir une enquête A ;
2. ouvrir une enquête B ;
3. vérifier que B remplace A.

## Échec d’ouverture

1. ouvrir une enquête valide A ;
2. tenter d’ouvrir un dossier invalide ;
3. vérifier que A reste active ;
4. vérifier que l’erreur est journalisée.

## Échec après création

Provoquer si possible un échec d’ouverture après création.

Vérifier :

- le dossier créé reste présent ;
- l’ancienne session reste active ;
- aucun double `free` ;
- aucun crash.

## Annulation

Annuler le sélecteur de dossier.

Vérifier :

- aucun warning ;
- aucun changement d’état ;
- aucun crash.

---

# Critères d’acceptation

- [ ] Une seule fonction ouvre une session et construit son arbre.
- [ ] La création utilise cette fonction.
- [ ] L’ouverture utilise cette fonction.
- [ ] `application_install_session()` n’est pas dupliquée.
- [ ] L’ancienne session reste active en cas d’échec.
- [ ] L’ancien arbre reste actif en cas d’échec.
- [ ] Les nouveaux objets sont libérés en cas d’échec.
- [ ] Les erreurs de `InvestigationSession` sont propagées.
- [ ] L’annulation ne produit pas d’erreur.
- [ ] Aucun code SQLite direct n’est ajouté.
- [ ] Aucun comportement GTK visible n’est cassé.
- [ ] `make` réussit.
- [ ] `make test` réussit.
- [ ] `git diff --check` ne retourne aucune erreur.

---

# Audit attendu

La logique d’ouverture ne doit apparaître qu’une seule fois :

```bash
rg -n \
    'investigation_session_open|investigation_tree_builder_build' \
    src/core/application.c
```

Résultat attendu :

```text
une occurrence de investigation_session_open
une occurrence de investigation_tree_builder_build
```

La création et l’ouverture doivent appeler le helper :

```bash
rg -n \
    'application_open_and_install_investigation' \
    src/core/application.c
```

Vérifier l’absence de SQLite direct :

```bash
rg -n \
    'sqlite3_|#include <sqlite3.h>' \
    src/core/application.c
```

Résultat attendu :

```text
aucune sortie
```

Vérifier le format :

```bash
git diff --check
```

---

# Fichiers concernés

```text
src/core/application.c
```

Aucune modification publique n’est normalement nécessaire dans :

```text
include/core/application.h
```

---

# Commit attendu

```bash
make clean
make
make test
git diff --check
git status --short
```

```bash
git add src/core/application.c
```

```bash
git diff --cached --stat
git diff --cached
```

```bash
git commit -m "refactor(core): centralize investigation loading"
git push
```

---

# Résultat attendu

Après ce ticket, tous les futurs points d’entrée utiliseront le même flux :

```text
Création
Ouverture manuelle
Enquêtes récentes
Ligne de commande
Restauration de session
        ↓
application_open_and_install_investigation()
```

Le ticket #033 pourra ensuite afficher graphiquement les `GError` déjà correctement produits par ce flux.
