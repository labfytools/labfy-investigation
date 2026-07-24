# Roadmap

> **Dernière mise à jour :** 2026-07-24  
> **État du projet :** développement actif  
> **Schéma SQLite courant :** V10  
> **Usage opérationnel :** non prêt pour la production

---

## 1. Source de vérité

La feuille de route détaillée est suivie dans Forgejo :

```text
https://git.labfytools.com/fy59/labfy-investigation/issues
```

Ce document donne une vue stratégique courte. Il ne duplique pas la liste
complète des tickets fermés.

Pour connaître l'état d'une fonctionnalité :

1. consulter le code et les tests sur `main` ;
2. consulter les migrations SQL ;
3. consulter les tickets Forgejo ;
4. utiliser cette roadmap comme synthèse.

Un ticket ouvert décrit un chantier en cours. Il ne prouve pas que toutes ses
fonctions sont déjà disponibles.

---

## 2. Vision

Labfy Investigation doit devenir un poste de travail local d'investigation
numérique et d'OSINT capable de :

- préserver les preuves originales ;
- organiser les données d'une enquête autonome ;
- extraire et normaliser des informations ;
- créer des entités et des relations traçables ;
- représenter les données sous forme de graphe ;
- conserver la provenance des traitements ;
- exécuter des outils externes de manière contrôlée ;
- présenter les résultats à l'enquêteur avant intégration ;
- produire des rapports compréhensibles et transmissibles.

Le logiciel doit rester :

- libre ;
- local ;
- documenté ;
- testable ;
- maintenable ;
- utilisable avec des dépendances optionnelles ;
- compatible à terme avec une distribution Ubuntu institutionnelle.

---

## 3. Principes non négociables

### Une enquête est autonome

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

### SQLite est la source de vérité

Le graphe, la barre latérale, les tableaux et les futures vues chronologiques
sont des projections des mêmes données persistées.

### Les preuves originales sont immuables

Toute transformation produit un fichier ou un objet dérivé.

### Toute donnée importante possède une provenance

Le système doit pouvoir expliquer :

- ce qui a été trouvé ;
- quand ;
- à partir de quelle preuve ou cible ;
- avec quel outil et quelle version ;
- avec quels paramètres ;
- quelle sortie brute a été produite ;
- quelle interprétation a été confirmée ou rejetée.

### L'enquêteur garde le contrôle

Une sortie OCR, une donnée OSINT ou une corrélation automatique reste une
proposition tant qu'elle n'a pas été explicitement confirmée.

### L'interface ne doit pas être bloquée

Les traitements longs sont exécutés en arrière-plan et restent annulables
lorsque cela est techniquement possible.

### Les outils externes restent optionnels

L'absence d'un outil réduit les capacités disponibles, mais ne doit pas
empêcher le lancement de l'application.

---

## 4. Socle déjà implémenté

La branche `main` contient notamment :

- création, validation et ouverture d'enquêtes ;
- session d'enquête remplaçable proprement ;
- infrastructure SQLite et migrations jusqu'à V10 ;
- couche Database, DAO et services métier ;
- import de preuves avec copie contrôlée et SHA-256 ;
- vérification d'intégrité et reclassement des preuves ;
- modèles et DAO d'entités et de relations ;
- associations preuves–entités et preuves–relations ;
- chargement asynchrone du graphe ;
- déplacement des nœuds et persistance du viewport ;
- registre, catalogue et exécution sécurisée d'outils externes ;
- gestionnaire de tâches et panneau d'activité ;
- premiers pivots DNS avec propositions puis intégration ;
- conservation de la provenance des exécutions OSINT ;
- comptes sociaux structurés ;
- personnes, rôles d'enquête et identité usurpée ;
- extractions liées aux preuves et aux entités ;
- analyse EML, IBAN, métadonnées ExifTool et PDF ;
- types canoniques de relations ;
- vocabulaire contrôlé ;
- table V10 `bank_account_entities` ;
- premiers composants du pipeline EML.

Cette liste est une synthèse et non un contrat de stabilité.

---

## 5. Chantier actif

### Ticket #107 — Pivot e-mail forensique

Statut :

```text
PARTIEL
```

Les briques préparatoires existent, mais le flux complet reste à terminer.

Objectif principal :

```text
preuve EML
    ↓
vérification d'intégrité
    ↓
analyse des en-têtes et de la structure MIME
    ↓
extraction sécurisée des pièces jointes
    ↓
SHA-256, type MIME et métadonnées
    ↓
OCR et détection d'indicateurs
    ↓
normalisation sans perte de la valeur brute
    ↓
interface de révision
    ↓
confirmation explicite
    ↓
intégration transactionnelle
    ↓
rafraîchissement des vues et du graphe
```

Priorités immédiates :

1. terminer l'extraction MIME robuste ;
2. conserver les pièces jointes comme fichiers dérivés ;
3. réunir EML, OCR, IBAN et ExifTool dans un pipeline unique ;
4. terminer l'interface de révision ;
5. persister uniquement les propositions confirmées ;
6. réutiliser ou créer les entités correspondantes ;
7. créer les relations canoniques avec leur provenance ;
8. garantir un rollback complet en cas d'échec ;
9. rafraîchir la barre latérale et le graphe ;
10. compléter les tests de migration et d'intégration ;
11. mettre à jour la documentation avec l'état réellement livré.

Le ticket reste ouvert tant que son flux complet et ses critères d'acceptation
ne sont pas validés.

---

## 6. Inventaire permanent

### Ticket #42 — Arsenal OSINT

Statut :

```text
INVENTAIRE PERMANENT
```

Ce ticket sert à qualifier les outils et services potentiellement intégrables.

Un outil n'est pas ajouté en masse. Son intégration suit un besoin concret :

1. compréhension manuelle de la technique ;
2. vérification du cadre légal ;
3. vérification de la licence ;
4. vérification Arch Linux et Ubuntu ;
5. création d'un ticket dédié ;
6. développement d'un adaptateur ;
7. conservation des sorties brutes ;
8. normalisation des résultats ;
9. tests avec données synthétiques ;
10. documentation de ses limites.

Le ticket #42 reste ouvert tant que l'inventaire est utile au projet.

---

## 7. Axes suivants

L'ordre exact dépendra des tickets créés dans Forgejo.

### 7.1 Fiabilité des données

- fixture dédiée V9 vers V10 ;
- tests de rollback pour les migrations récentes ;
- `PRAGMA integrity_check` et `PRAGMA foreign_key_check` systématiques ;
- contrôle des références orphelines ;
- vérification des sorties brutes persistées ;
- journal d'audit plus complet.

### 7.2 Graphe d'enquête

- améliorer la lisibilité des grands graphes ;
- filtrer par type, date, source, statut et confiance ;
- enregistrer des vues ;
- créer et éditer des relations depuis le canvas ;
- synchroniser toutes les modifications avec SQLite ;
- préparer l'export du graphe.

### 7.3 Recherche et chronologie

- recherche transversale ;
- filtres structurés ;
- chronologie liée aux preuves, entités et relations ;
- notes, pistes et hypothèses clairement séparées des faits.

### 7.4 Rapports et transmission

- rapport Markdown structuré ;
- export PDF ;
- annexes et empreintes ;
- manifeste d'enquête ;
- export autonome ;
- expurgation et anonymisation ;
- présentation adaptée aux forces de l'ordre.

### 7.5 Distribution Ubuntu

- classifier les dépendances obligatoires et optionnelles ;
- produire un script de compilation robuste ;
- produire un paquet `.deb` ;
- fonctionner avec des dépôts institutionnels restreints ;
- documenter l'installation hors ligne ;
- tester Wayland et X11 ;
- éviter toute dépendance AUR obligatoire.

---

## 8. Critères de qualité communs

Un chantier n'est pas terminé tant que :

- le code compile ;
- les tests ciblés passent ;
- la suite complète passe ;
- les erreurs sont présentées correctement ;
- l'interface reste réactive ;
- les données brutes sont préservées ;
- la provenance est suffisante ;
- aucune donnée réelle d'enquête n'est présente dans le dépôt ;
- la documentation reflète l'état livré.

Validation recommandée :

```sh
make clean
make -j8
make -j8 test
git diff --check
```

Le retour à une compilation séquentielle n'est utilisé que si la
parallélisation provoque un échec réel.

---

## 9. Historique

L'historique détaillé du développement se trouve dans :

- les tickets fermés Forgejo ;
- les commits ;
- `CHANGELOG.md` ;
- les audits de schéma versionnés.

Les anciennes listes de tickets ne doivent pas être recopiées dans ce fichier,
car elles deviennent rapidement obsolètes.
