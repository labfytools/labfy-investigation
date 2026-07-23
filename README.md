# Labfy Investigation

> [!IMPORTANT]
> **Forgejo est le dépôt principal du projet.**
>
> Le code peut également être publié sur GitHub comme miroir public, mais le suivi du développement, les tickets, les décisions techniques et la feuille de route se trouvent sur :
>
> **https://git.labfytools.com/fy59/labfy-investigation**
>
> Tickets :
>
> **https://git.labfytools.com/fy59/labfy-investigation/issues**
>
> Les tickets et pull requests ouverts uniquement sur GitHub risquent de ne pas être suivis.

Labfy Investigation est un poste de travail libre d’investigation numérique et d’OSINT, développé en **C17** avec **GTK4**.

Le projet vise à fournir un environnement local, modulaire et traçable pour organiser une enquête, préserver les preuves originales, analyser des données, corréler des entités et produire des rapports exploitables.

> **État du projet : développement actif**
>
> Le logiciel n’est pas encore prêt pour un usage opérationnel en production. Les formats internes, l’interface et les mécanismes d’intégration peuvent encore évoluer.

---

## Objectifs

Labfy Investigation doit permettre de :

- créer et ouvrir une enquête autonome ;
- conserver les preuves originales sans les modifier ;
- organiser les fichiers, entités, relations et événements ;
- stocker les données structurées dans SQLite ;
- afficher l’arborescence complète d’une enquête ;
- exécuter des traitements longs en arrière-plan ;
- intégrer progressivement des outils OSINT externes ;
- conserver les sorties brutes, les versions et les paramètres d’exécution ;
- distinguer les faits observés, les résultats d’outils, les corrélations et les hypothèses ;
- produire des rapports compréhensibles et traçables.

Le logiciel est pensé pour des usages légaux par des particuliers, journalistes, analystes OSINT, experts judiciaires et forces de l’ordre.

---

## Cadre légal et éthique

Labfy Investigation est conçu pour travailler avec :

- des sources publiquement accessibles ;
- des données fournies légalement par une victime ou un enquêteur ;
- des API utilisées conformément à leurs autorisations ;
- des recherches passives ou explicitement autorisées ;
- des copies locales dont la provenance peut être documentée.

Le projet n’a pas vocation à fournir ou automatiser :

- l’intrusion dans un système ;
- le contournement d’une authentification ;
- l’exploitation de vulnérabilités ;
- le brute force ou le credential stuffing ;
- le phishing ou l’usurpation ;
- l’utilisation de secrets découverts ;
- l’accès à des données privées sans autorisation ;
- la modification ou la suppression de données distantes.

Un résultat produit par un outil OSINT constitue une **piste à vérifier**, pas une preuve d’identité à lui seul.

---

## Principes fondamentaux

### Une enquête est autonome

Chaque enquête est stockée dans un dossier transportable :

```text
MonEnquete/
├── 00_BaseDeDonnees/
│   └── Enquete.sqlite
├── 01_Preuves_Originales/
├── 02_Preuves_Traitees/
├── 03_Chronologie/
├── 04_Entites/
└── 05_Rapports/
```

Une enquête peut être copiée, déplacée, sauvegardée, archivée ou transmise avec ses données.

### Les preuves originales sont immuables

Toute annotation, conversion, extraction ou analyse doit produire un nouveau fichier. Une preuve originale ne doit jamais être modifiée.

### SQLite est la source de vérité

Les tableaux, graphes, chronologies et résultats de recherche sont des vues différentes des mêmes données persistées.

### Les résultats bruts et normalisés sont séparés

Chaque traitement doit conserver :

- l’outil utilisé ;
- sa version ;
- les arguments ;
- la date et l’heure UTC ;
- la source interrogée ;
- la sortie brute ;
- l’empreinte des fichiers produits ;
- les données normalisées utilisées par l’application.

### L’interface ne doit jamais être bloquée

Les opérations longues doivent s’exécuter en arrière-plan et rester annulables.

### Aucun shell construit dynamiquement

Les outils externes sont lancés avec `GSubprocess` et des arguments séparés. Les commandes concaténées puis transmises à un shell sont interdites.

---

## Fonctionnalités déjà présentes

Le socle actuel comprend notamment :

- création et ouverture d’enquêtes ;
- validation de l’arborescence ;
- sessions d’enquête remplaçables proprement ;
- base SQLite versionnée ;
- transactions et remontée structurée des erreurs ;
- DAO et modèles d’enquête ;
- arborescence des fichiers ;
- fenêtre principale GTK4 ;
- barre latérale et espace de travail ;
- navigation et recherche locale des entités dans la barre latérale ;
- navigation et recherche locale des relations dans la barre latérale ;
- affichage graphique des erreurs ;
- tâches asynchrones annulables ;
- gestionnaire de tâches ;
- panneau d’activité GTK ;
- import groupé de preuves avec confirmation globale, révision individuelle,
  copie contrôlée, empreinte SHA-256 et bilan détaillé ;
- correction du type, de la source et de la description d'une preuve avec
  contrôle d'intégrité et déplacement cohérent de sa copie interne ;
- registre d’outils externes ;
- exécution sécurisée par `GSubprocess` ;
- exécution d’outils en tâche de fond ;
- catalogue initial d’outils ;
- détection de présence et de version ;
- menu OSINT contextuel préparé pour les entités et relations du graphe ;
- contexte de sélection OSINT indépendant de GTK et validé par des tests ;
- catalogue déterministe des actions compatibles avec la sélection OSINT ;
- disponibilité des actions OSINT synchronisée avec le registre d’outils ;
- résolution DNS asynchrone avec `dig` depuis une entité domaine ;
- affichage sélectionnable des sorties standard et d’erreur, sans persistance ;
- révision des réponses DNS sous forme de propositions avant intégration ;
- sélection explicite et intégration transactionnelle des propositions DNS
  compatibles, avec normalisation et détection des doublons ;
- création transactionnelle des relations DNS `resolves_to`, `aliases_to` et
  `uses_name_server` depuis l'entité interrogée ;
- provenance OSINT SQLite V3 conservant les arguments, sorties brutes,
  empreinte SHA-256 et liaisons vers les entités et relations intégrées ;
- comptes sociaux structurés en SQLite V4 (TikTok, Instagram, Facebook, X,
  Telegram ou autre), avec URL, pseudonyme, identifiant stable facultatif,
  première observation, état, notes et rattachement à une preuve ;
- pictogrammes vectoriels des plateformes sociales dans les nœuds du graphe,
  conservant leur lisibilité pendant le zoom ;
- création de personnes observées avec statut d'identification, confiance,
  notes factuelles et rattachement facultatif à une preuve ;
- catégories d'enquête des personnes en SQLite V5, modifiables depuis leur
  fiche et représentées par une couleur et un libellé dans le graphe ;
- relations représentées par des flèches directes à libellé cliquable, sans
  ajouter de faux nœud visuel entre les entités ;
- historique OSINT contextuel en lecture seule avec détail des exécutions,
  sorties standard et d'erreur, et objets créés ou réutilisés ;
- vérification manuelle de l'intégrité des sorties OSINT enregistrées, sans
  réécriture de l'empreinte ou des données contrôlées.
- glisser-déposer des extractions texte depuis l'arborescence vers le graphe,
  avec confirmation explicite, création d'entité ou rattachement à une entité
  existante, sans déplacement du fichier produit ;

Les outils actuellement présents dans le catalogue initial sont :

```text
dig
host
whois
curl
openssl
```

Ils restent optionnels : l’absence d’un outil ne doit pas empêcher Labfy Investigation de démarrer.

---

## Architecture

Le projet sépare strictement les responsabilités :

```text
Interface GTK4
      ↓
Application
      ↓
Services métier
      ↓
Adaptateurs
├── SQLite
├── système de fichiers
├── outils CLI
└── futures API
```

Règles principales :

- le cœur métier ne dépend pas de GTK ;
- les widgets ne manipulent ni SQLite ni les preuves ;
- les modèles ne connaissent ni GTK ni SQLite ;
- les erreurs remontent jusqu’à l’application ;
- chaque allocation possède une responsabilité de libération claire ;
- les tests du cœur ne doivent pas nécessiter le lancement de GTK.

Organisation actuelle :

```text
database/          Ressources et éléments liés à la base
docs/              Architecture, conventions et feuille de route
include/core/      Interfaces du cœur
include/dao/       Interfaces d’accès aux données
include/database/  Infrastructure SQLite
include/models/    Modèles métier
include/views/     Fenêtres GTK
include/widgets/   Widgets réutilisables
resources/         Ressources de l’application
src/core/          Implémentation du cœur
src/dao/           Accès aux données
src/database/      Implémentation SQLite
src/models/        Modèles métier
src/views/         Vues GTK
src/widgets/       Widgets GTK
tests/             Tests unitaires
```

La documentation détaillée se trouve dans :

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/CONVENTIONS.md`](docs/CONVENTIONS.md)
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)
- [`docs/ROADMAP.md`](docs/ROADMAP.md)
- [`docs/database/`](docs/database/)

---

## Environnements ciblés

### Ubuntu

Ubuntu est la cible principale de distribution, notamment pour un futur déploiement auprès des forces de l’ordre.

Dépendances de compilation :

```bash
sudo apt update
sudo apt install     build-essential     pkg-config     libgtk-4-dev     libglib2.0-dev     libsqlite3-dev
```

La disponibilité réelle des paquets devra être vérifiée sur les postes utilisant des dépôts institutionnels restreints.

### Arch Linux

Arch Linux est l’environnement principal de développement et de validation.

```bash
sudo pacman -S --needed     base-devel     pkgconf     gtk4     glib2     sqlite
```

Les paquets AUR ne devront jamais devenir une dépendance obligatoire du futur paquet Ubuntu.

---

## Compilation

Depuis la racine du dépôt :

```bash
make
```

Le binaire produit est :

```text
./labfy-investigation
```

Lancer l’application :

```bash
make run
```

Nettoyer les fichiers générés :

```bash
make clean
```

Le projet est compilé en C17 avec les avertissements traités comme des erreurs.

---

## Tests

Lancer tous les tests :

```bash
make test
```

Vérifications recommandées avant chaque commit :

```bash
make clean
make
make test
git diff --check
```

Les nouveaux modules doivent être accompagnés de tests couvrant :

- les arguments invalides ;
- le fonctionnement nominal ;
- les erreurs ;
- l’annulation lorsque nécessaire ;
- les responsabilités mémoire ;
- les régressions possibles.

---

## Développement

Conventions essentielles :

- C17 uniquement ;
- fichiers et fonctions en `snake_case` ;
- fonctions préfixées par leur module ;
- noms de variables explicites ;
- aucune logique métier dans les widgets ;
- aucun commit tant que la fonctionnalité ne compile pas et ne fonctionne pas ;
- compilation sans avertissement ;
- tests valides avant intégration.

Exemples de préfixes :

```text
database_*
investigation_*
task_manager_*
tool_registry_*
tool_process_*
tool_catalog_*
```

---

## Outils OSINT externes

Le ticket historique **#42** reste ouvert comme inventaire évolutif des outils OSINT potentiels.

Les outils ne sont pas intégrés en masse. Lorsqu’un besoin concret apparaît :

1. un ticket Forgejo dédié est créé ;
2. l’outil est audité techniquement et juridiquement ;
3. sa compatibilité Ubuntu et Arch est vérifiée ;
4. son adaptateur est développé ;
5. ses sorties brutes et normalisées sont testées ;
6. son état est mis à jour dans l’inventaire.

Aucun outil absent n’est installé automatiquement par l’application.

---

## Suivi du projet

Les tickets sont désormais suivis directement dans Forgejo :

```text
https://git.labfytools.com/fy59/labfy-investigation/issues
```

Les tickets historiques jusqu’au numéro 40 conservent leur numérotation. Les nouveaux tickets utilisent uniquement le numéro attribué automatiquement par Forgejo.

Le prochain chantier porte sur l’initialisation asynchrone du registre et des versions d’outils au démarrage.

---

## État du packaging

Le packaging n’est pas encore finalisé.

Les cibles prévues sont :

- paquet `.deb` pour Ubuntu ;
- dossier source accompagné d’un script de compilation ;
- procédure de développement et de test pour Arch Linux.

Le futur installateur Ubuntu devra fonctionner autant que possible sans dépendre de dépôts non standards.

---

## Contribution

Avant toute modification :

1. consulter les tickets ouverts ;
2. lire l’architecture et les conventions ;
3. limiter chaque changement à un objectif cohérent ;
4. ajouter ou adapter les tests ;
5. vérifier la compilation complète ;
6. documenter toute dérogation architecturale.

---

## Licence

Labfy Investigation est distribué sous licence MIT.

Voir le fichier `LICENSE` pour les conditions complètes.
