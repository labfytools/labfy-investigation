# Architecture de Labfy Investigation

Le parcours local `CURRENT` de jobs persistants et de reprise est détaillé dans
[LOCAL_JOBS_RECOVERY.md](architecture/LOCAL_JOBS_RECOVERY.md). Son JobStore
opérationnel séparé ne remplace pas la base métier V20 et ne modifie pas V21.

Le contrôle navigateur local sur espace exclusivement synthétique est décrit
dans [WEB_WORKSPACE_CONTROL.md](architecture/WEB_WORKSPACE_CONTROL.md). Cette
surface mutationnelle J6 est distincte des démonstrations read-only J2–J5.

> **Statut :** direction canonique v0.1.0
> **Dernière mise à jour :** 2026-09-25
> **Portée :** architecture actuelle et cible Labfy V2

## 1. Objet et niveaux de maturité

Labfy Investigation est un environnement local-first d'investigation numérique
centré sur un graphe interactif. Chaque élément pertinent doit pouvoir devenir
un pivot vers une capacité applicable, sans perdre sa provenance.

Ce document distingue explicitement :

- **CURRENT** : comportement présent dans le code et les migrations ;
- **TARGET** : contrat architectural v0.1.0 à implémenter progressivement ;
- **UNDECIDED** : décision réservée à une tranche architecturale ultérieure.

Une description `TARGET` ne prouve pas qu'une fonctionnalité est disponible.
Pour l'état courant, le code, les tests et les migrations priment.

Les contrats spécialisés complètent ce document sans le remplacer :

- [graphe d'investigation](architecture/INVESTIGATION_GRAPH.md) ;
- [Toolkit et adapters](osint/TOOLKIT.md) ;
- [sécurité, scope et secrets](security/SECURITY_MODEL.md) ;
- [design system](ui/DESIGN_SYSTEM.md) ;
- [décisions architecturales](architecture/decisions/0001-web-local-first.md).

## 2. Principes non négociables

### 2.1 The graph is the investigation

Le graphe devient la représentation principale de l'enquête. Il ne constitue
ni une base indépendante ni une copie appauvrie de SQLite : il projette le
réseau logique des objets métier et de leurs liens.

Les objets projetables incluent notamment :

```text
PERSON        ORGANIZATION    COMPANY
EMAIL         PHONE           USERNAME       ALIAS
ACCOUNT       SOCIAL_PROFILE
DOMAIN        HOSTNAME        IP_ADDRESS     URL        CERTIFICATE
ADDRESS       LOCATION
BANK_ACCOUNT  IBAN            BIC            TRANSACTION
CRYPTO_WALLET CRYPTO_TRANSACTION
DOCUMENT      IMAGE           VIDEO          FILE
DEVICE        EVENT
EVIDENCE      SOURCE          OBSERVATION    CLAIM      HYPOTHESIS
```

Cette projection unifiée ne transforme pas tous ces objets en lignes de
`entites`. Le modèle conserve des catégories distinctes :

- entités et identifiants pivotables ;
- artefacts et preuves ;
- sources et exécutions ;
- observations et claims ;
- événements et transactions ;
- hypothèses et décisions humaines.

### 2.2 Une enquête, plusieurs projections

```text
                    INVESTIGATION GRAPH
                           │
          ┌────────────────┼────────────────┐
          │                │                │
       Identity       Infrastructure      Finance
          │                │                │
       Timeline            Geo           Evidence
```

Identity, Infrastructure, Finance, Timeline, Geo et Evidence sont des vues du
même réseau logique. Elles partagent identités, relations, provenance et règles
d'accès. Elles ne possèdent pas leur propre vérité métier.

### 2.3 Local-first

Par défaut :

- SQLite reste local ;
- les artefacts et preuves restent locaux ;
- le graphe et les services principaux s'exécutent localement ;
- aucune dépendance à un cloud Labfy n'est requise ;
- un accès réseau existe seulement lorsqu'une capacité le demande.

Une interface Web locale ne modifie pas ce contrat. La surface HTTP future doit
écouter localement par défaut, appliquer une authentification adaptée à sa
surface réelle et ne jamais exposer silencieusement une enquête sur le réseau.

### 2.4 Preuves originales immuables

Une preuve originale importée n'est jamais modifiée. Toute conversion,
annotation, extraction, transcription, analyse ou expurgation produit un objet
dérivé, lié à son entrée et accompagné d'une empreinte.

### 2.5 Provenance avant automatisation

Une automatisation ne peut être activée avant de pouvoir répondre à :

```text
WHAT?              WHERE FROM?        WHEN?
HOW?               WHICH TOOL?        WHICH VERSION?
WHICH QUERY?       WHICH PARAMETERS?  WHAT RAW RESULT?
WHAT NORMALIZED RESULT?               WHAT TRANSFORMATION?
WHAT HASH?         WHAT CONFIDENCE?    WHO CONFIRMED IT?
```

### 2.6 Une hypothèse n'est pas un fait

Une similarité, une corrélation ou un identifiant commun produit un signal ou
une hypothèse. Il ne devient pas silencieusement une identité confirmée.

## 3. Architecture actuelle — CURRENT

L'application existante est principalement écrite en C17 avec GTK4, GLib/GIO
et SQLite.

```text
GTK views/widgets
       │ intentions et affichage
Application / services / tasks
       │ orchestration
Models / DAO / Database
       │
     SQLite

Evidence files ── controlled services ── derived artifacts
External tools ── GSubprocess ────────── raw/normalized results
```

Contrats actuels à préserver :

- aucun accès SQLite direct depuis les widgets ;
- aucun GTK dans les modèles, DAO ou services métier ;
- transactions pour les opérations composées ;
- migrations compatibles avec les anciennes bases ;
- ownership explicite des allocations ;
- outils lancés avec des arguments séparés, jamais via un shell construit ;
- opérations longues hors du thread principal ;
- données de test exclusivement synthétiques.

Le socle comprend déjà :

- enquêtes autonomes et base versionnée ;
- preuves, extractions, entités et relations ;
- graphe interactif GTK et positions persistantes ;
- tâches asynchrones en mémoire ;
- registre initial d'outils ;
- premiers parcours DNS et documentaires ;
- provenance partielle des exécutions OSINT ;
- observations brutes, normalisées et corrigées dans certains domaines ;
- décisions humaines append-only pour les parcours d'identité.
- projection J3 expérimentale indépendante de GTK, lisant une base V20 en
  read-only effectif et publiant un snapshot v2 pour le prototype Web ; cette
  capacité est documentée dans
  [CORE_GRAPH_READONLY.md](architecture/CORE_GRAPH_READONLY.md).
- service J3 ciblé d'analyse EML native : original contrôlé, dérivé versionné,
  extraction et observations `proposed` sont publiés atomiquement en V20, puis
  projetés en snapshot v3 sans reconstruction dans le pont Web ; voir
  [EML_ANALYSIS_TO_GRAPH.md](architecture/EML_ANALYSIS_TO_GRAPH.md).
- poste Web local contrôlé : une instance loopback possède une bibliothèque
  explicitement choisie et ne rend active qu'une enquête à la fois. Le bridge
  C reste le créateur des enquêtes, le lecteur des projections et l'unique
  accès aux stockages métier ; voir
  [WEB_WORKSPACE_CONTROL.md](architecture/WEB_WORKSPACE_CONTROL.md).

Ces composants doivent être réutilisés ou migrés. La direction V2 n'autorise
pas une réécriture générale du cœur fonctionnel.

## 4. Architecture cible — TARGET

```text
┌───────────────────────────────────────────────┐
│                 WEB FRONTEND                  │
│                                               │
│       INVESTIGATION GRAPH — MAIN UI           │
│                                               │
│ Dashboard / Graph / Timeline / Evidence       │
│ Finance / Infrastructure / Geo / Reports      │
└──────────────────────┬────────────────────────┘
                       │
                HTTP API / EVENTS
                       │
┌──────────────────────▼────────────────────────┐
│                  LABFY CORE                   │
│                                               │
│ Evidence / Provenance                         │
│ Entity & Observation Model                    │
│ Adapter Registry                              │
│ Job Engine                                    │
│ Pivot Engine                                  │
│ Correlation Engine                            │
│ Investigation Planner                         │
│ Scope / Policy                                │
└──────────────────────┬────────────────────────┘
                       │
                     SQLite
```

La future interface principale est Web. GTK reste l'interface fonctionnelle
temporaire et ne sera pas supprimé avant qu'un remplacement validé couvre les
parcours nécessaires.

Restent volontairement `UNDECIDED` :

- framework frontend ;
- JavaScript ou TypeScript exact ;
- serveur HTTP local ;
- format détaillé de l'API ;
- Server-Sent Events, WebSocket, polling ou combinaison ;
- stratégie de packaging du frontend.

Le choix JavaScript natif + SVG + serveur Python stdlib + SSE est `CURRENT`
uniquement pour le prototype J2. Le mode J3 cœur réutilise cette surface sans
SSE et sans accès SQLite côté Python. Ces choix expérimentaux ne décident pas
le serveur, le renderer ou le packaging de production.

Le moteur graphique et la bibliothèque HTTP restent également `UNDECIDED`.

Aucune dépendance Node, npm ou frontend n'est introduite par la tranche
fondation.

## 5. Socle polyglotte

C17 reste privilégié pour :

- modèles et services métier existants ;
- SQLite, migrations et transactions ;
- provenance et intégrité des données ;
- orchestration principale ;
- API locale si l'étude dédiée le confirme ;
- chemins sensibles aux performances et à la mémoire.

Labfy V2 peut aussi utiliser :

```text
Python                  adapters ou outils matures
JavaScript/TypeScript   frontend Web
SQL                     persistance et requêtes
controlled shell        scripts maintenus, sans entrée concaténée
external CLI tools      capacités spécialisées isolées
HTTP APIs               services publics ou autorisés
```

Un composant externe mature est préférable à une réimplémentation médiocre si
sa licence, sa maintenance, ses limites, son isolation, sa normalisation et sa
provenance sont maîtrisées.

## 6. Modèle de connaissance

### 6.1 Objets métier

Une entité possède une identité durable et peut servir de pivot. Un artefact est
un fichier ou résultat matériel. Une observation décrit ce qu'une source a
fourni. Un claim exprime une affirmation structurée. Une hypothèse relie des
signaux qui nécessitent une validation. Un événement ou une transaction porte
une sémantique temporelle propre.

Les nouveaux types simples doivent être ajoutés par un registre descriptif et
des canonicalizers, sans multiplier les branchements UI. Une table spécialisée
est réservée aux domaines qui possèdent de vraies contraintes structurelles.

### 6.2 Nature, revue et hypothèses

Trois dimensions restent orthogonales :

```text
information_nature:
  OBSERVED | DECLARED | DERIVED | CORRELATED | INFERRED

review_state:
  UNREVIEWED | CONFIRMED | CONTRADICTED | REJECTED

hypothesis_state:
  PROPOSED | UNDER_REVIEW | SUPPORTED |
  CONTRADICTED | CONFIRMED | REJECTED
```

`HYPOTHESIS` est un objet, pas une nature d'observation.

### 6.3 Confiance explicable

La direction canonique est :

```text
UNKNOWN | LOW | MODERATE | HIGH
```

Chaque niveau doit être accompagné de raisons, facteurs contradictoires,
nombre de sources indépendantes et origine humaine ou automatique. Les scores
historiques peuvent être conservés pour compatibilité, sans être réinterprétés
silencieusement.

## 7. Graphe et espace de travail

### 7.1 Compression sémantique

Le graphe cible doit rester utilisable avec des milliers d'objets :

- semantic clustering ;
- collapse/expand ;
- niveaux de détail ;
- filtrage par type, temps, relation, source, état et confiance ;
- isolation d'un chemin ;
- focus mode ;
- agrégats visuels réversibles.

Une agrégation masque ou résume. Elle ne supprime ni ne fusionne les objets
métier sous-jacents.

### 7.2 Focus Workspace

La sélection d'un objet doit pouvoir :

- recentrer le graphe ;
- isoler le voisinage pertinent ;
- présenter relations et chemins ;
- afficher observations, preuves et sources ;
- afficher timeline et hypothèses ;
- demander les actions et pivots applicables ;
- revenir immédiatement au graphe global.

Le novice voit des actions métier, par exemple « Analyser le domaine ». L'expert
peut descendre jusqu'aux versions, paramètres, sorties et transformations.

## 8. Toolkit Registry et adapters

Le frontend consomme des capabilities et ne connaît pas les commandes internes
des outils.

Une déclaration de capacité contient conceptuellement :

```text
tool_id                 adapter_id
capability_id           accepted_object_types
produced_object_types   action_class
effects                 requirements
network_required        credentials_required
cost                    risk
availability            health
tool_version            adapter_version
```

Familles d'adapters prévues :

- native adapter ;
- subprocess adapter ;
- HTTP API adapter ;
- manual adapter.

Un adapter ne dépend ni de GTK ni de SQLite. Il exécute ou collecte, conserve
un résultat brut, puis produit des observations normalisées. Les services du
cœur contrôlent persistance, jobs, provenance, ressources et scope.

Le premier groupe de candidats comprend ExifTool, Tesseract, qpdf, RDAP,
Certificate Transparency, Wayback, Subfinder, theHarvester et Maigret. Leur
présence dans la roadmap ne signifie pas qu'ils sont actuellement intégrés.

## 9. Provenance

La navigation « Why do we know this? » suit :

```text
ENTITY / CLAIM
      ↑
OBSERVATION
      ↑
TRANSFORMATION
      ↑
NORMALIZED RESULT
      ↑
EXECUTION ATTEMPT
      ↑
RAW ARTIFACT / SOURCE
      ↑
REQUEST + TOOL + ADAPTER + SCOPE
```

La cible conserve :

- opération logique et tentatives ;
- outil, adapter et versions ;
- requête, paramètres expurgés et cible ;
- début, fin, statut et erreur ;
- stdout, stderr et réponses brutes bornées ;
- artefacts, tailles, rôles, types MIME et SHA-256 ;
- observations normalisées ;
- transformations et règles versionnées ;
- décisions humaines ;
- snapshot du scope et de la politique.

Les logs texte ne remplacent pas ce modèle.

## 10. Job Engine et isolation

Le moteur persistant cible connaît :

```text
QUEUED | RUNNING | WAITING | RATE_LIMITED |
COMPLETED | PARTIAL | FAILED | CANCELLED
```

Il gère dépendances, priorité, progression, timeout, retry, backoff,
cancellation, concurrence, idempotence et récupération après crash.

L'exécution d'un outil externe doit prévoir :

- argv séparé, sans shell concaténé ;
- environnement minimal ;
- répertoire de travail isolé ;
- capture en streaming avec plafonds ;
- timeout et cancellation ;
- groupe de processus et arrêt des descendants ;
- limites CPU, mémoire, fichiers et descripteurs ;
- artefacts atomiques et empreintes ;
- publication idempotente.

Le statut `PARTIAL`, l'aperçu tronqué et l'artefact complet réellement conservé
restent distincts. Une empreinte ne décrit jamais implicitement des octets qui
n'ont pas été conservés. Une opération logique, sa tentative physique et la
publication de ses résultats portent des identités séparées ; une clé
d'idempotence ne garantit pas une exécution réseau exactement une fois.

## 11. Pivots, budgets et planner

### 11.1 Pivots contextuels

```text
OBJECT
  ↓
CAPABILITY DISCOVERY
  ↓
PIVOT CANDIDATE
  ↓
JOB / ADAPTER
  ↓
OBSERVATIONS
  ↓
NEW OBJECTS / NEW PIVOTS
```

Le moteur contrôle déduplication, cache, profondeur, cycles, dépendances,
priorité, coût, risque et rate limits.

### 11.2 Pivot budget

Le budget est multidimensionnel :

```text
max_depth
max_pivots
max_network_requests
max_wall_time
max_bytes
max_external_cost
max_risk
```

Une action `INVESTIGATE` ne signifie jamais « tout exécuter sans limite ».

### 11.3 Investigation Planner

Le planner répond :

```text
Given everything currently known,
what should we investigate next?
```

Sa première version doit être déterministe et explicable. Elle considère les
capacités, exécutions passées, dépendances, coût, risque, fiabilité, budget et
gain d'information attendu. Chaque classement `HIGH`, `MEDIUM` ou `LOW` est
accompagné de raisons. Une IA autonome opaque n'est pas un objectif v0.1.0.

## 12. Autonomie, actions et scope

L'autonomie progresse strictement dans cet ordre :

```text
MANUAL
ASSISTED
AUTONOMOUS_PASSIVE
AUTONOMOUS_AUTHORIZED
```

La première autonomie réelle visée est `AUTONOMOUS_PASSIVE`, après provenance,
jobs persistants, adapters, pivots, budgets, politiques et scope.

Les actions sont classées :

```text
PASSIVE | PUBLIC_ACTIVE | AUTHORIZED_INTRUSIVE | PROHIBITED
```

Leurs effets sont déclarés séparément :

```text
READ_ONLY | STATE_CHANGING | DESTRUCTIVE | PERSISTENCE |
CREDENTIAL_ACCESS | DATA_EXTRACTION
```

Chaque tentative doit recevoir une décision `IN_SCOPE`, `OUT_OF_SCOPE` ou
`UNKNOWN`. Une cible découverte, un sous-domaine, une IP résolue ou une
infrastructure partagée n'hérite jamais automatiquement d'une autorisation
intrusive.

Le contact réseau `NONE`, `THIRD_PARTY` ou `TARGET` est déclaré séparément de
la classe d'action. Une future autorisation référence cibles, actions, période,
opérateur et exclusions ; l'interface ne crée jamais à elle seule le droit
d'agir.

Les capacités intrusives ne font pas partie de v0.1.0. Leur développement
exigerait une tranche et une autorisation explicites compatibles avec le cadre
légal et le contrat produit.

## 13. Corrélation et similarité

Les règles de corrélation doivent être déterministes, documentées, testables,
versionnées et rejouables. Chaque résultat conserve ses inputs, normalisations,
règle, version et explication.

Les égalités exactes et similarités restent séparées :

- SHA-256 identique : contenu binaire identique ;
- image perceptuellement proche : signal de similarité ;
- même username : réutilisation possible ;
- même IP : infrastructure partagée possible ;
- proximité temporelle : signal contextuel.

Aucune de ces observations ne confirme à elle seule une personne ou un
opérateur commun.

## 14. UX et design system

Le design cible utilise Catppuccin Mocha et Lavender comme accent principal.
Les couleurs sont des tokens sémantiques centralisés ; elles ne doivent pas être
dupliquées arbitrairement dans les composants.

L'UX novice expose des actions métier. L'UX expert rend accessibles :

```text
tool / version
adapter / version
query / parameters
execution / stdout / stderr
raw artifact / normalized observations
transformation / correlation rule / provenance
```

Le mode simplifié masque des détails ; il ne les supprime jamais.

## 15. Ordre de construction

L'ordre canonique est défini par [ROADMAP.md](ROADMAP.md). En résumé :

1. J0, fondation documentaire et contrat v0.1.0 ;
2. J1, baseline et stabilisation du travail financier indépendant ;
3. J2, choix Web/graphe/API et prototype synthétique en lecture seule ;
4. J3 à J5, services, provenance, runner, jobs et policies ;
5. J6, vertical Web Graph relié au backend ;
6. J7 et J8, pivots, corrélation, planner et autonomie passive ;
7. J9, projections spécialisées et rapports.

Le premier lot J9 CURRENT est défini dans
[EVIDENCE_TIMELINE_REPORT.md](architecture/EVIDENCE_TIMELINE_REPORT.md) : trois
projections du graphe commun et un dossier de rapport minimisé, atomique et
vérifiable, sans évolution de V20, V3 ou V21.

J0 s'arrête à la documentation. Aucun choix de stack ou prototype J2 n'est
réalisé par cette fondation.

## 16. Incrément local J4

Le lot [Toolkit local et runner borné](architecture/LOCAL_TOOLKIT_RUNNER.md) est
`CURRENT` dans le worktree : registre statique réellement consommé, capacité
EML native, adapter ExifTool subprocess, runner POSIX borné, publication V20 et
projection Web read-only. Il ne transforme pas le prototype en serveur de
production et ne livre pas les jobs/policies J5. Les autres runners historiques
restent une dette explicitement non couverte.
Le parcours `CURRENT` de création d’espace vide et d’import navigateur est
défini dans [`LOCAL_WORKSPACE_IMPORT.md`](architecture/LOCAL_WORKSPACE_IMPORT.md).
Il conserve V20 et le JobStore V3, et sépare création, réception, publication,
analyse approuvée et rapport.

## 17. Espace local, revue et projections — CURRENT expérimental

Le parcours local est limité à une instance loopback et à des validations sur
fixtures `SPECIMEN`. Son lanceur peut ouvrir une bibliothèque locale
explicitement choisie, en créer les enquêtes par le bridge C et n'en active
qu'une seule dans une instance. Il ne rouvre pas implicitement l'enquête d'une
session précédente. Cette surface ne change pas son statut : GTK reste le
parcours fonctionnel actuel et le serveur, son packaging ainsi que le choix du
renderer de production restent `UNDECIDED`.

Le runtime métier demeure V20 et le JobStore opérationnel demeure V3 ; le
poste Web ne migre ni ne modifie la tranche financière V21. L'état d'instance
et le secret d'amorçage sont séparés dans le runtime privé XDG (avec un repli
privé propre à l'UID lorsque `XDG_RUNTIME_DIR` est absent) ; la configuration
durable est placée dans l'état XDG privé. La bibliothèque est une destination
locale fournie explicitement au lancement : elle n'est pas déduite d'une
dernière enquête ouverte.

Après la publication d'une preuve V20, le bridge Web demande au cœur C un
aperçu EML, PNG ou JPEG. L'original, son chemin et son répertoire ne sont jamais
servis par HTTP. Le cœur vérifie l'intégrité, les bornes et le type effectif,
puis retourne seulement une représentation inerte et bornée. L'identité de
cache retournée décrit cette représentation ; il n'existe pas de cache disque
persistant d'aperçu. Les réponses sont `no-store` et le navigateur ne peut pas
choisir un UUID autre que celui de la preuve demandée par l'interface.

La revue d'observation est une transaction V20 dont chaque intention porte un
UUID d'opération et une révision attendue. Le journal V20 consigne l'opération
mais n'est pas signé : il fournit de la traçabilité locale, pas une
certification. Confirmer une observation signifie uniquement que sa revue est
confirmée ; cela ne confirme jamais une identité. La correction humaine reste
une valeur distincte des valeurs brute et normalisée. Un même UUID avec la même
intention est rejoué sans doublon ; une intention différente est refusée.

La promotion demeure volontaire. Elle peut créer ou rattacher explicitement un
indicateur e-mail, domaine ou IP ; le rattachement exige l'UUID d'une entité
compatible. Le retrait conserve l'observation et les liens indépendants, et ne
supprime pas aveuglément une entité partagée. Les données brutes, UUID
d'observation et UUID d'extraction ne sont pas réécrits par ces opérations.

Le rejeu idempotent d'une extraction EML existante n'accepte que la revue, la
correction, la promotion ou le retrait humains liés à une entrée du journal
`observation.review.v1`. Il ne relance pas l'analyse et ne consomme aucun
budget. Les champs originels de l'extraction et ses artefacts restent soumis
aux contrôles d'immuabilité et ne sont pas modifiables par ce rejeu.

La republication du graphe, des corrélations et du planner relit les données
persistées. Elle est réessayable après import ou revue, sans réanalyse, sans
nouveau claim et sans consommation de budget. Les rapprochements restent des
signaux exploratoires ; ils ne prouvent ni identité ni relation métier.

Le durcissement P01–P06 couvre le cycle de vie des imports, les réservations,
le rejeu, les états terminaux et le framing HTTP du corps. Il ne transforme pas
le protocole local en service distant ni en garantie de production.
