# Roadmap canonique — Labfy Investigation v0.1.0

> **Dernière mise à jour :** 2026-09-25
> **État :** v0.1.0 Foundation en préparation, non publiée
> **Règle :** seul J0 appartient à la tranche documentaire courante

## 1. Ordre et portes

Labfy évolue sans réécriture générale. Les contrats du cœur précèdent les
fonctions Web de production et l'autonomie :

```text
J0 FOUNDATION
  → J1 BASELINE
  → J2 WEB/GRAPH ARCHITECTURE
  → J3 SERVICES ET PROVENANCE
  → J4 TOOLKIT ET RUNNER
  → J5 JOBS ET POLICIES
  → J6 WEB GRAPH VERTICAL
  → J7 PIVOTS ET CORRÉLATION
  → J8 PLANNER ET ASSISTANCE
  → J9 PROJECTIONS ET RAPPORTS
```

Un prototype Web en lecture seule peut commencer en J2 avec des données
synthétiques. Les fonctions Web de production dépendent toutefois des contrats
backend effectivement validés. Aucun jalon n'a de date promise.

## J0 — FOUNDATION 0.1.0

**Objectif.** Figer identité, vision, décisions architecturales et ordre de
construction sans démarrer la refonte fonctionnelle.

**Dépendances.** État réel du dépôt et préservation des travaux locaux, dont la
tranche financière V21 indépendante.

> **Interface CURRENT local :** la tranche Web finale a remplacé GTK par une
> surface locale Agent | Graphe | Activité + drawer. Un gateway d’outils V1
> `CURRENT de laboratoire` relit les snapshots backend et prépare une demande
> d’autorisation recherche sans l’accorder. Un runtime OpenAI-compatible local
> `CURRENT local` le pilote sur faux loopback, sans qualifier Qwen réel ; Qwen
> réel et resource governor AMD/RAM/swap restent `TARGET`.

**Livrables.** README, bannière et source éditable, index documentaire,
architecture canonique, contrat du graphe, ADR Web/graph-first/polyglotte,
Toolkit, sécurité, design system, roadmap et backlog préparé.

**Critères de sortie.** `CURRENT`, `TARGET` et `UNDECIDED` sont séparés ; liens
et assets sont valides ; v0.1.0 est annoncée non publiée ; la bannière est
inspectée ; aucun fichier fonctionnel, schéma ou dépendance n'est ajouté.

**Validation.** Contrôle Markdown/UTF-8/liens/ancres, rendu local de la bannière,
revue du diff, empreintes des fichiers V21 protégés, build et tests synthétiques
du worktree lorsque pertinents, `git diff --check`.

**Non-objectifs.** Frontend, serveur, migration, adapter, job persistant,
capability générique, pivot nouveau, autonomie, ticket distant ou release.

## J1 — BASELINE

> **Avancement local J1/J2 :** baseline V20 et fondation V21 autonome validées ;
> la migration runtime financière reste volontairement différée.

**Objectif.** Stabiliser le travail financier en cours et inventorier les
contrats, migrations et tests réellement présents.

**Dépendances.** J0 relu ; travail V21 traité dans sa propre tranche.

**Livrables.** Baseline reproductible, matrice des contrats existants, état des
migrations et dette qualifiée.

**Critères de sortie.** Création neuve, migrations autorisées, rollback,
intégrité et conservation des données synthétiques validés ; numéro suivant de
schéma choisi à partir de l'état réel.

**Tests.** Tests ciblés des migrations et DAO, `integrity_check`,
`foreign_key_check`, suite complète et contrôles de reprise.

**Non-objectifs.** Mélanger V21 au commit documentaire, choisir la stack Web ou
réécrire les modèles.

## J2 — WEB/GRAPH ARCHITECTURE

> **Avancement local J1/J2 :** pile expérimentale choisie et prototype Web
> Graph synthétique en lecture seule livré ; les choix de production restent
> soumis aux portes et benchmarks décrits ci-dessous.

**Objectif.** Choisir la frontière core/API, la stack Web et le moteur de
graphe après comparaison, puis construire un prototype synthétique en lecture
seule.

**Dépendances.** J1 et contrats métier inventoriés.

**Livrables.** Comparaison courte de candidats, modèle de menace, contrats
versionnés pour graphe/actions/jobs/erreurs/provenance, stratégie événements,
packaging et prototype local.

**Critères de sortie.** Décisions fondées sur mesures et sources primaires ;
reconnexion/rattrapage spécifiés ; surface locale et contrôle d'accès définis ;
prototype sans donnée réelle.

**Tests.** Contrats API, Host/Origin/CSRF, DNS rebinding, erreurs, accessibilité
clavier, rendu synthétique et mesures de base.

**Non-objectifs.** Choisir sur popularité, exposer le LAN par défaut, déplacer
la logique métier vers le frontend ou livrer des mutations de production.

## J3 — SERVICES ET PROVENANCE

> **Avancement local partiel :** le lot read-only relie une fixture SQLite V20
> créée par les DAO à une projection C et au Web via le snapshot v2. Le lot EML
> exécute ensuite l'analyseur natif, publie en V20 un dérivé, une extraction et
> des observations proposées, puis les projette via le snapshot v3. La
> généralisation de la provenance aux autres capacités reste à construire ; J3
> entier n'est pas déclaré terminé.

**Objectif.** Extraire progressivement les responsabilités utiles et unifier la
provenance sans migration destructive.

**Dépendances.** Baseline J1 et frontières J2.

**Livrables.** Contrats artefact/opération/tentative/transformation/observation,
revue humaine, publication et navigation « Why do we know this? », en réutilisant
notamment exécutions OSINT et observations existantes.

**Critères de sortie.** Un résultat synthétique remonte au brut, à l'outil, à la
version, aux paramètres expurgés et aux décisions ; contradictions préservées.

**Tests.** Provenance complète, valeurs partielles, supersession, rollback,
ownership, anciennes bases et absence de double publication.

**Non-objectifs.** Nouveau stockage parallèle, suppression des historiques ou
conversion de tout objet en `entites`.

## J4 — TOOLKIT ET RUNNER

> **Avancement local :** registre réellement consommé, runner borné, adapters
> EML/ExifTool, publication V20 et démonstration Web SPECIMEN livrés. Les autres
> outils locaux restent à migrer ; ce statut ne livre ni J5 ni J3 global.

**Objectif.** Livrer registre/capabilities, fake adapters et runner commun borné,
puis adapter d'abord les capacités locales existantes.

**Dépendances.** Contrats J3.

**Livrables.** Adapters `NATIVE`, `SUBPROCESS`, `HTTP_API`, `MANUAL`, contrat
machine versionné, disponibilité/health, limites, artefacts et normalisation.

**Critères de sortie.** Processus suspendu, bavard, malformé et créant des
descendants contenu et nettoyé ; absence d'outil diagnostiquée sans crash.

**Tests.** Timeout, annulation, groupe de processus, stdout/stderr bornés,
fichiers/FD/mémoire, sorties invalides, idempotence de publication et fakes.

**Non-objectifs.** Installation ou mise à jour silencieuse, accès direct SQLite
depuis l'adapter, exécution d'outils OSINT réels dans les tests.

## J5 — JOBS ET POLICIES

**Objectif.** Persistance, reprise, budgets et scope avant toute chaîne autonome
réseau.

**Dépendances.** Provenance J3 et runner J4.

**Livrables.** États de job, tentatives, dépendances, priorité, backoff,
lease/heartbeat, annulation, Resource Governor, classes d'action et snapshots de
policy.

**Critères de sortie.** Crash entre artefact et publication sans doublon ni
succès fictif ; retry limité aux contrats idempotents ; arrêt global visible.

**Tests.** Crash/restart, leases expirés, cancellation, budgets partagés,
concurrence par service, `UNKNOWN`/`OUT_OF_SCOPE`, redirections et refus.

**Non-objectifs.** Exactly-once réseau, autorisation héritée par pivot ou
`AUTONOMOUS_AUTHORIZED`.

## J6 — WEB GRAPH VERTICAL

**Objectif.** Première tranche Web utilisable, d'abord sur données synthétiques,
avec réseau unifié, provenance et actions contextuelles reliées au backend.

**Dépendances.** Architecture J2, services J3, Toolkit J4 et policies J5.

**Livrables.** Graphe, Focus Workspace, recherche, vue liste, actions, jobs et
navigation de provenance sur un parcours vertical.

**Critères de sortie.** Identités stables, contexte conservé, disponibilité
expliquée, contrôles backend, clavier et états d'erreur accessibles.

**Tests.** Parcours synthétiques, reconnexion, révisions concurrentes, CSP,
contenus hostiles, petites largeurs, contraste et performance mesurée.

**Non-objectifs.** Retirer GTK avant couverture validée ou revendiquer des
volumes non mesurés.

## J7 — PIVOTS ET CORRÉLATION

> **Avancement local partiel :** un premier lot relit en C les observations V20
> réellement publiées, indexe e-mails/domaines/IP, distingue occurrences,
> analyses, preuves et contenus, puis présente rapprochements et connexions
> exploratoires bornées dans le poste J6. Recherches réseau, revue persistante
> des hypothèses et planner autonome restent futurs. Voir
> [le contrat J7 local](architecture/LOCAL_PIVOTS_CORRELATION.md).

**Objectif.** Classification, déduplication, cycles, rapprochements exacts,
hypothèses et premières capacités réseau sélectionnées.

**Dépendances.** Vertical J6 et politiques J5.

**Livrables.** Catalogue extensible, moteur de pivots borné, règles versionnées,
explications, contradictions et revue humaine.

**Critères de sortie.** Similarité distincte de l'identité ; faux positifs et
dépendance des sources visibles ; hypothèses rejetées conservées.

**Tests.** Cycles, limites, cache, homonymes, infrastructure partagée, résultats
contradictoires et rejouabilité des règles.

**Non-objectifs.** Fusion automatique d'identités, action intrusive ou preuve
d'absence déduite d'un résultat vide.

## J8 — PLANNER ET ASSISTANCE

> **CURRENT local, périmètre borné :** après un import ou une revue, le graphe,
> les corrélations et le planner peuvent être republiés depuis les données
> persistées, sans réanalyse ni consommation de budget. Cela ne livre pas une
> assistance autonome ; les analyses et budgets restent soumis à l'approbation
> explicite de l'utilisateur.

**Objectif.** Recommander les prochaines recherches et permettre une assistance
passive bornée et explicable.

**Dépendances.** Pivots/corrélation J7 et jobs/policies J5.

**Livrables.** Classement `HIGH`/`MEDIUM`/`LOW`, raisons, gain d'information,
mode `ASSISTED`, puis `AUTONOMOUS_PASSIVE` sous portes explicites.

**Critères de sortie.** Recommandations déterministes, budgets respectés,
résultats partiels honnêtes, pause/reprise/arrêt visibles.

**Tests.** Ordre reproductible, coût/risque, déduplication, limites, reprise et
absence d'action lorsque policy ou capability change.

**Non-objectifs.** Agent opaque, autonomie sans limite ou mode autorisé intrusif.

## J9 — PROJECTIONS ET RAPPORTS

> **CURRENT local (premier lot).** Les projections Preuves, Chronologie et
> Infrastructure locale, ainsi que la sélection et le dossier de rapport
> HTML/JSON/PDF vérifiable, sont réalisés. Finance reste la tranche V21
> indépendante ; Geo et l'élargissement complet de J9 demeurent futurs. Voir
> [le contrat du lot](architecture/EVIDENCE_TIMELINE_REPORT.md).

**Objectif.** Étendre Finance, Timeline, Infrastructure, Geo et Evidence puis
produire des rapports sélectionnés, vérifiables et expurgés.

**Dépendances.** Modèle commun J3 et graphe J6.

**Livrables.** Projections du même réseau, précision temporelle, montants et
incertitudes inspectables, exports avec manifestes et hashes.

**Critères de sortie.** Aucun silo, provenance complète, expurgation dérivée et
traçable, accessibilité et performance mesurées.

**Tests.** Fuseaux/précision partielle, flux financiers synthétiques, données
géographiques fictives, exports déterministes, minimisation et grands jeux.

**Non-objectifs.** Altérer les originaux, promettre recevabilité automatique ou
dépendre d'un cloud Labfy.

## 2. Piste séparée de validation de sécurité

Nmap, Nuclei et Greenbone/OpenVAS ne font pas partie de la première livraison
Web ni de v0.1.0. Toute étude ultérieure requiert un contrat séparé, des cibles
de laboratoire explicitement autorisées, une démonstration minimale et aucun
accès inutile aux données de tiers.

## 3. Données synthétiques obligatoires

Les validations utilisent domaines `.test`, IP de documentation, faux services
locaux, comptes homonymes, documents `SPECIMEN`, résultats partiels ou
contradictoires et transactions fictives. Aucune enquête réelle n'est une
fixture.
