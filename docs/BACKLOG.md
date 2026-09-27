# Backlog préparé — après Foundation v0.1.0

> **Statut :** propositions locales, non publiées dans Forgejo
> **Règle :** aucun numéro de ticket n'est attribué ici

Ce backlog décline les jalons de [ROADMAP.md](ROADMAP.md). Il ne lance aucune
tranche et ne remplace pas la validation de ses prérequis.

## Stabiliser la baseline financière et les contrats existants

- **Problème :** le worktree contient une V21 indépendante et l'état publié peut
  différer du schéma local.
- **Fichiers/modules :** Database, migrations, DAO financiers et tests associés.
- **Prérequis :** Foundation relue ; branche et delta V21 identifiés.
- **Contrat :** aucune perte de données, version choisie depuis l'état réel.
- **Acceptation/tests :** bases synthétiques neuves et anciennes, rollback,
  `integrity_check`, `foreign_key_check`, suite complète.
- **Documentation :** architecture DB et audit courant.
- **Retour arrière :** migration transactionnelle non publiée ; aucun mélange
  avec le commit documentaire F0.

## LABFY_WEB_GRAPH_ARCHITECTURE

> **Preuve locale J2 :** le prototype `prototypes/web-graph/` valide une
> frontière snapshot/SSE en lecture seule. La sélection de production reste à
> confirmer par benchmarks et modèle de menace ; ce ticket n'est pas clos.

- **Problème :** stack frontend, moteur graphique, frontière API, événements et
  packaging sont encore `UNDECIDED`.
- **Fichiers/modules :** documents d'architecture et prototype isolé sur
  fixtures synthétiques ; aucun service métier de production au départ.
- **Prérequis :** baseline stabilisée et contrats métier inventoriés.
- **Contrat :** comparer peu de candidats depuis leurs sources primaires ;
  backend autoritaire ; local-first ; contrats versionnés ; aucune dépendance
  distante implicite.
- **Acceptation/tests :** matrice licences/dépendances/accessibilité/sécurité,
  prototype lecture seule, mesures graphe, clavier, Host/Origin/CSRF, reconnexion
  et rattrapage événementiel.
- **Documentation :** ADR de sélection, modèle de menace, contrats API et notes
  de packaging.
- **Retour arrière :** prototype sans migration, supprimable sans toucher au
  cœur ; aucun framework promu avant la porte de décision.

## Formaliser services et provenance de bout en bout

- **Problème :** la provenance existe par domaines mais n'est pas uniforme.
- **Fichiers/modules :** services extraits progressivement, exécutions OSINT,
  observations, artefacts, transformations et revue humaine.
- **Prérequis :** frontière core/API retenue.
- **Contrat :** brut immuable, normalized distinct, historique conservé,
  publication transactionnelle et ownership explicite.
- **Acceptation/tests :** navigation complète jusqu'à la source, contradictions,
  valeurs partielles, rollback et anciennes bases synthétiques.
- **Documentation :** architecture et schéma concernés.
- **Retour arrière :** migrations additives et code ancien conservé tant que le
  nouveau parcours n'est pas validé.

## Unifier Toolkit et runner borné

- **Problème :** les parcours d'outils sont spécialisés et le contrat de
  capability commun n'est pas livré.
- **Fichiers/modules :** registre, adapters, runner, Resource Governor et fakes.
- **Prérequis :** provenance formalisée.
- **Contrat :** adapters sans UI/SQLite, argv séparé, limites, brut conservé,
  schéma machine versionné.
- **Acceptation/tests :** outils manquant/suspendu/bavard/malformé, descendants,
  FD, mémoire, timeout, annulation et publication idempotente.
- **Documentation :** Toolkit, dépendances et contrats d'ownership.
- **Retour arrière :** feature gates et maintien des parcours existants jusqu'à
  équivalence validée.

## Persister jobs, budgets et policies

- **Problème :** les tâches `CURRENT` ne fournissent pas encore le moteur
  persistant cible ni le scope uniforme.
- **Fichiers/modules :** Task Runtime, Project DB, policy et Resource Governor.
- **Prérequis :** runner commun et provenance.
- **Contrat :** opération/tentative/publication séparées, lease/heartbeat,
  retry seulement si autorisé, budget multidimensionnel.
- **Acceptation/tests :** crash/restart, doublons, cancellation, concurrence,
  refus `UNKNOWN`/`OUT_OF_SCOPE` et effets non idempotents.
- **Documentation :** états, reprises, runbooks et limites.
- **Retour arrière :** migration additive ; ancien parcours manuel disponible
  tant que la reprise persistante n'est pas validée.

## Livrer un vertical Web Graph synthétique

- **Problème :** valider ensemble graphe, provenance, jobs et actions sans
  attendre toutes les projections.
- **Fichiers/modules :** frontend choisi, API locale et adaptateurs de lecture.
- **Prérequis :** architecture, services, runner et policies validés.
- **Contrat :** identités stables, backend autoritaire, vue liste accessible,
  contenus non fiables isolés.
- **Acceptation/tests :** parcours synthétique de bout en bout, erreurs,
  reconnexion, CSP, clavier, contrastes et mesures.
- **Documentation :** guide développeur et limites du vertical.
- **Retour arrière :** GTK reste fonctionnel ; aucun retrait avant couverture.

## Ajouter pivots, corrélation et revue

- **Problème :** les rapprochements doivent être explicables sans fusion
  implicite.
- **Fichiers/modules :** classification, Pivot Engine, règles et hypothèses.
- **Prérequis :** vertical et policies.
- **Contrat :** règles déterministes/versionnées, cycles bornés, contradictions
  et hypothèses rejetées conservées.
- **Acceptation/tests :** homonymes, IP partagée, avatar similaire, doublons,
  budgets et rejouabilité.
- **Documentation :** catalogue des règles et faux positifs.
- **Retour arrière :** résultats supersédés, jamais détruits.

## Ajouter planner et assistance passive

- **Problème :** proposer la prochaine recherche sans agent opaque.
- **Fichiers/modules :** planner, scoring qualitatif, jobs et UI d'explication.
- **Prérequis :** pivots/corrélation et budgets.
- **Contrat :** ordre déterministe, raisons obligatoires, arrêt visible.
- **Acceptation/tests :** reproductibilité, changement de policy, limites,
  reprise et résultat partiel.
- **Documentation :** règles de recommandation et conditions d'autonomie.
- **Retour arrière :** retour immédiat au mode `MANUAL` sans perte de résultats.

## Étendre projections et rapports

**État : premier lot local livré.** Preuves, Chronologie, Infrastructure locale
et rapports minimisés hors ligne sont CURRENT. Geo, l'unification fonctionnelle
de Finance V21 et les extensions suivantes restent au backlog.

- **Problème :** exploiter le même réseau dans Finance, Timeline,
  Infrastructure, Geo et Evidence.
- **Fichiers/modules :** projections, rapports, exports et expurgation.
- **Prérequis :** modèle commun et vertical graphe.
- **Contrat :** aucune vérité parallèle, précision et provenance conservées,
  original immuable.
- **Acceptation/tests :** flux et dates synthétiques, rapports déterministes,
  manifestes, hashes, accessibilité et grands jeux.
- **Documentation :** format d'export, minimisation et avertissements.
- **Retour arrière :** exports dérivés supprimables sans modifier les sources.

L’espace local vide et l’import navigateur EML/PNG/JPEG sont `CURRENT` dans le
worktree. Restent `TARGET` : anciennes dispositions GTK, formats additionnels,
durcissement distant/TLS et gestion multi-utilisateur.
