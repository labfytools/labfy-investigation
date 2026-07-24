# Guide de développement

> **Version :** 2.0  
> **Dernière mise à jour :** 2026-07-24  
> **Projet :** Labfy Investigation

---

## 1. Objet

Ce document décrit l'environnement et le workflow de développement.

Les règles d'architecture se trouvent dans :

```text
docs/ARCHITECTURE.md
docs/CONVENTIONS.md
docs/database/DATABASE_ARCHITECTURE.md
```

Forgejo est la source de vérité des tickets :

```text
https://git.labfytools.com/fy59/labfy-investigation/issues
```

---

## 2. Environnements

### 2.1 Environnement principal

Le développement courant est réalisé sous Arch Linux.

### 2.2 Cible de distribution

Ubuntu est la cible principale de distribution, notamment pour un futur
déploiement dans des environnements institutionnels.

Le code ne doit pas dépendre d'un paquet AUR.

Les dépendances optionnelles doivent se désactiver proprement lorsqu'elles sont
absentes.

### 2.3 Interface graphique

Le projet utilise GTK4 et doit rester compatible avec Wayland et X11 lorsque
GTK le permet.

Aucune hypothèse ne doit dépendre de la présence d'une barre de titre fournie
par le gestionnaire de fenêtres.

---

## 3. Dépendances de compilation

Le Makefile utilise `pkg-config` pour GTK4 et SQLite.

### Arch Linux

```sh
sudo pacman -S --needed \
    base-devel \
    pkgconf \
    gtk4 \
    glib2 \
    sqlite
```

### Ubuntu

```sh
sudo apt update
sudo apt install \
    build-essential \
    pkg-config \
    libgtk-4-dev \
    libglib2.0-dev \
    libsqlite3-dev
```

Les versions minimales exactes doivent correspondre aux API réellement utilisées
dans le code. Elles ne doivent pas être inventées dans la documentation si le
système de build ne les impose pas.

Les outils OSINT, OCR, métadonnées ou PDF sont documentés séparément et restent
optionnels sauf décision explicite.

---

## 4. Récupération du dépôt

```sh
git clone https://git.labfytools.com/fy59/labfy-investigation.git
cd labfy-investigation
```

Avant de commencer :

```sh
git status
git pull --ff-only
```

Ne pas écraser un travail local non validé.

---

## 5. Compilation

### 5.1 Compilation recommandée

```sh
make -j8
```

Le projet utilise globalement :

```text
-std=c17
-Wall
-Wextra
-Werror
```

Plusieurs cibles de tests ajoutent `-Wpedantic`.

### 5.2 Compilation séquentielle

Utiliser une compilation séquentielle seulement lorsque l'exécution parallèle
provoque un échec réel et identifié :

```sh
make
```

Une erreur de code ne doit pas être masquée en supprimant simplement `-j8`.

### 5.3 Nettoyage

```sh
make clean
```

### 5.4 Lancement

```sh
make run
```

ou :

```sh
./labfy-investigation
```

### 5.5 Documentation générée

Le Makefile courant ne fournit pas de cible `make docs`.

Ne pas documenter ou utiliser cette commande tant qu'une cible réelle n'a pas
été ajoutée et testée.

---

## 6. Tests

### 6.1 Suite complète

```sh
make -j8 test
```

En cas de problème réellement causé par la parallélisation :

```sh
make test
```

### 6.2 Test ciblé

Les tests sont des exécutables dans `tests/`.

Exemple :

```sh
make tests/test_database
./tests/test_database
```

Autre exemple :

```sh
make tests/test_eml_pipeline_task
./tests/test_eml_pipeline_task
```

Le nom exact d'une cible doit être vérifié dans le Makefile.

### 6.3 Validation avant intégration

```sh
make clean
make -j8
make -j8 test
git diff --check
```

Vérifier également :

```sh
git status --short
```

Aucun fichier de preuve réel, base réelle ou artefact temporaire ne doit
apparaître.

---

## 7. Workflow d'un ticket

### 7.1 Avant de coder

1. lire le ticket Forgejo ;
2. vérifier l'état de la branche `main` ;
3. identifier les modules concernés ;
4. lire les tests existants ;
5. vérifier les migrations si SQLite est concerné ;
6. distinguer clairement ce qui est déjà implémenté de ce qui est seulement
   demandé ;
7. préparer uniquement des données synthétiques.

### 7.2 Pendant le développement

1. limiter le changement à un objectif cohérent ;
2. compiler régulièrement ;
3. ajouter les tests au fur et à mesure ;
4. conserver une API claire ;
5. respecter les propriétaires mémoire ;
6. éviter toute logique métier dans GTK ;
7. ne pas créer une seconde architecture concurrente ;
8. ne pas toucher aux données réelles d'enquête.

### 7.3 Avant le commit

1. lancer la validation complète ;
2. relire le diff ;
3. vérifier les erreurs et chemins de rollback ;
4. vérifier la documentation ;
5. vérifier l'absence de données sensibles ;
6. demander la validation manuelle prévue par le workflow.

Aucun commit n'est effectué tant que la fonction ne marche pas.

---

## 8. Organisation des modules

```text
include/core/       interfaces du cœur et des services
include/dao/        interfaces des DAO
include/database/   interfaces SQLite
include/models/     interfaces des modèles
include/views/      interfaces des vues GTK
include/widgets/    interfaces des widgets

src/core/           services, tâches et orchestration
src/dao/            accès métier à SQLite
src/database/       connexion, schéma, statements, transactions
src/models/         modèles métier
src/views/          fenêtres et dialogues GTK
src/widgets/        widgets réutilisables
```

### Ajouter un modèle

Un modèle doit généralement fournir :

- un header dans `include/models/` ;
- une implémentation dans `src/models/` ;
- des constructeurs et destructeurs clairs ;
- des validations ;
- un test dans `tests/`.

Il ne dépend pas de GTK ni de SQLite.

### Ajouter un DAO

Un DAO doit généralement fournir :

- une interface dans `include/dao/` ;
- une implémentation dans `src/dao/` ;
- des statements préparés ;
- une transformation explicite ligne ↔ modèle ;
- des tests sur une base temporaire ;
- des erreurs structurées.

Il ne contient pas de logique GTK ni d'exécution d'outil.

### Ajouter un service

Un service est approprié lorsqu'une opération combine :

- plusieurs DAO ;
- une transaction ;
- le système de fichiers ;
- une validation métier ;
- un résultat composé.

Le service définit la frontière transactionnelle.

### Ajouter une tâche

Une tâche est appropriée lorsque l'opération peut bloquer l'interface.

Elle doit définir :

- ses entrées copiées ou référencées clairement ;
- son annulation ;
- son résultat ;
- son erreur ;
- son nettoyage ;
- la remise du résultat au thread principal.

### Ajouter une vue ou un widget

Une vue GTK :

- collecte l'intention de l'utilisateur ;
- appelle un contrôleur ou un service ;
- présente le résultat ;
- ne manipule pas directement SQLite ;
- ne lance pas de processus ;
- ne déplace pas directement les preuves.

---

## 9. Développement SQLite

### 9.1 Avant toute modification

Lire :

```text
docs/database/DATABASE_ARCHITECTURE.md
docs/database/SCHEMA_AUDIT_CURRENT.md
database/schema_current.sql
database/schema_v10.sql
src/database/database.c
src/database/schema.c
tests/test_database.c
```

### 9.2 Nouvelle version de schéma

Pour créer V11, par exemple :

1. ajouter `database/schema_v11.sql` ;
2. déclarer et implémenter `schema_install_v11()` ;
3. ajouter `database_migrate_v10_to_v11()` ;
4. raccorder la migration dans la boucle vers la version courante ;
5. mettre à jour les constantes de version ;
6. installer V11 lors de la création d'une base neuve ;
7. adapter `schema_current.sql` si nécessaire ;
8. ajouter une fixture V10 vers V11 ;
9. tester une base neuve ;
10. tester le rollback ;
11. vérifier l'intégrité et les clés étrangères ;
12. mettre à jour l'audit courant ;
13. créer l'audit versionné de V11.

Ne pas réécrire une ancienne migration publiée pour changer sa signification.

### 9.3 Vérifications SQLite

Sur une base synthétique :

```sql
PRAGMA integrity_check;
PRAGMA foreign_key_check;
SELECT value
FROM metadata
WHERE key = 'schema_version';
```

### 9.4 Transactions et fichiers

Lorsqu'une opération combine SQLite et le système de fichiers :

- préparer les fichiers temporaires ;
- vérifier les empreintes ;
- ouvrir la transaction au moment approprié ;
- ne valider qu'après toutes les étapes critiques ;
- nettoyer ou restaurer les fichiers en cas d'échec ;
- tester explicitement le rollback.

---

## 10. Outils externes

Avant d'intégrer un outil :

1. vérifier sa licence ;
2. vérifier son usage légal ;
3. vérifier sa disponibilité Arch et Ubuntu ;
4. documenter son caractère obligatoire ou optionnel ;
5. ajouter sa détection au registre si nécessaire ;
6. utiliser `GSubprocess` ;
7. transmettre les arguments séparément ;
8. définir un délai maximal ;
9. gérer l'annulation ;
10. limiter la taille des sorties ;
11. conserver la provenance ;
12. tester son absence.

Interdit :

```c
system(command);
```

Interdit également de construire une chaîne puis de l'exécuter via un shell.

Une dépendance absente doit produire un statut clair, pas un plantage.

---

## 11. GTK4

### 11.1 Thread principal

Seul le thread GTK principal modifie les widgets.

Les résultats d'une tâche sont transférés vers ce thread par les mécanismes
GLib adaptés.

### 11.2 Dialogues

Un dialogue :

- valide ses entrées ;
- ne conserve pas de pointeur vers une session détruite ;
- permet l'annulation ;
- ne réalise pas une opération longue directement ;
- présente un résumé avant une écriture importante.

### 11.3 Messages utilisateur

Les détails techniques restent dans les journaux.

Le message graphique indique :

- ce qui a échoué ;
- les conséquences ;
- ce qui a été conservé ou annulé ;
- l'action possible.

---

## 12. Mémoire et diagnostic

Compiler avec les avertissements activés est obligatoire.

Pour une erreur mémoire, utiliser les outils disponibles localement sans
modifier durablement les options du dépôt.

Exemple avec Valgrind lorsque l'application et l'environnement le permettent :

```sh
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    ./tests/test_cible
```

Les faux positifs provenant de bibliothèques externes doivent être distingués
des allocations du projet.

Une tâche annulée, une erreur SQLite et un échec de processus doivent tous
libérer leurs ressources.

---

## 13. Données de test et sécurité

Le dépôt et les agents de développement utilisent uniquement des fixtures
synthétiques.

Ne jamais fournir à un agent local ou versionner :

```text
Enquete.sqlite réelle
captures réelles
conversations réelles
e-mails réels
RIB ou IBAN réels
pièces jointes réelles
documents d'identité réels
sorties OSINT contenant des données personnelles réelles
```

Les tests doivent utiliser :

- domaines réservés comme `example.com` ;
- adresses IP de documentation ;
- noms fictifs ;
- IBAN de test explicitement synthétiques ;
- fichiers générés pendant le test ;
- bases SQLite temporaires.

Avant chaque commit :

```sh
git diff --check
git status --short
```

Inspecter tout nouveau fichier inhabituel à la racine du dépôt.

---

## 14. Git

Les messages de commit sont en anglais.

Format recommandé :

```text
type(scope): description
```

Exemples :

```text
feat(email): add MIME attachment extraction
fix(database): rollback failed V10 migration
test(relations): cover canonical type reuse
docs(architecture): align documentation with V10
```

Un ticket peut être découpé en plusieurs commits cohérents.

Ne pas ajouter manuellement un numéro dans le titre d'un nouveau ticket :
Forgejo le fournit.

Ne pas effectuer de commit ou de push avant la validation manuelle prévue par
le workflow du projet.

---

## 15. Revue finale

Avant de considérer une modification terminée :

- [ ] le code respecte C17 ;
- [ ] les dépendances entre couches sont correctes ;
- [ ] la propriété mémoire est claire ;
- [ ] les erreurs sont remontées ;
- [ ] les opérations longues sont asynchrones ;
- [ ] les requêtes utilisent des paramètres liés ;
- [ ] les transactions possèdent un rollback testé ;
- [ ] les preuves originales restent intactes ;
- [ ] les valeurs brutes restent intactes ;
- [ ] les tests utilisent des données synthétiques ;
- [ ] `make -j8` passe ;
- [ ] `make -j8 test` passe ;
- [ ] `git diff --check` passe ;
- [ ] la documentation est à jour.
