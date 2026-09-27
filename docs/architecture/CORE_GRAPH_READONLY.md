# Projection cœur → Web en lecture seule — lot J3

> **État :** `CURRENT` local, expérimental, uniquement sur fixture synthétique
> **Runtime lu :** schéma V20
> **Contrat transport :** `labfy.web_graph.snapshot.v2`

Ce lot relie pour la première fois le prototype Web aux données réellement
persistées et relues par le cœur C :

```text
core_graph_demo
  → database_initialize + DAO de production
  → Enquete.sqlite V20 synthétique
  → database_open_read_only
  → CoreGraphProjectionService
  → snapshot JSON v2 atomique
  → serveur Python statique
  → prototype Web
```

Le JSON n'est ni une nouvelle base métier, ni une source de vérité. Le serveur
Python ne lit pas SQLite, ne reçoit aucun chemin de base par HTTP et ne complète
pas les décisions du cœur.

## API, ownership et cohérence

- `database_open_read_only(path, error)` ouvre un fichier existant avec
  `SQLITE_OPEN_READONLY`, active `query_only`, refuse toute version autre que
  V20 et n'appelle ni migration ni installation de schéma. L'appelant ferme la
  connexion avec `database_close()`.
- `database_transaction_begin_read_only()` démarre une transaction différée :
  la première lecture fixe le snapshot SQLite sans demander de verrou
  d'écriture. Le service valide ou annule cette transaction avant de retourner.
- L'ouverture ne change ni `journal_mode` ni `synchronous` et n'active pas
  `immutable`. Le writer de fixture utilise donc le journal de rollback du
  runtime ; son éventuel fichier `Enquete.sqlite-journal` est transitoire et
  supprimé à la fermeture. Le lecteur fermé ne laisse ni `-journal`, ni `-wal`,
  ni `-shm`, propriété vérifiée par le test C ciblé.
- `core_graph_projection_service_collect()` emprunte la connexion et retourne
  un `CoreGraphSnapshot` possédé par l'appelant. Une erreur DAO ou une limite
  atteinte invalide toute la collecte ; aucun graphe vide n'est présenté comme
  complet.
- Le snapshot possède ses chaînes, nœuds, arêtes et capabilities. Les vues de
  getters sont empruntées jusqu'à `core_graph_snapshot_free()`.
- `core_graph_snapshot_write_atomic()` sérialise entièrement en mémoire, vérifie
  UTF-8 et volume, écrit un staging privé voisin puis renomme. Aucun fichier
  final tronqué n'est publié.

## Mapping actuel

| Objet demandé | Stockage V20 | API de lecture | Provenance exposée | Limite honnête |
|---|---|---|---|---|
| Entité/personne/identifiant | `entites` | `InvestigationGraphLoader` / `EntityDao` | état et identité persistante | provenance uniforme absente |
| Relation métier | `relations` | `InvestigationGraphLoader` / `RelationDao` | identité, direction, sémantique | justifications non projetées dans ce lot |
| Preuve | `preuves` | `EvidenceDao` | source/description si présentes | aucun fichier servi par HTTP |
| Rattachement preuve-entité | `preuve_entites` | `EvidenceEntityDao` | lien `evidence_attachment` | rattachement ≠ preuve d'identité |
| Observation | `evidence_entity_observations` | variante typée `list_observations()` | brut, normalisé, corrigé, revue, origine et promotion | transformation parfois non identifiée |
| Exécution OSINT historique | `osint_executions` | `OsintExecutionDao` typé | outil/version, dates, état, extrait brut borné et liens | pas d'artefact/transformation séparé en V20 |
| Claims/hypothèses/finance V21 | non raccordés au runtime supporté | aucune dans ce lot | aucune | aucune ligne ou chaîne fictive |

Les références sont qualifiées par `investigation_id`, `object_kind` et
`object_id`. Les identifiants de graphe préfixent la catégorie, par exemple
`entity:<uuid>` et `evidence:<uuid>` : deux tables peuvent donc partager une
valeur d'identifiant sans collision. Les arêtes composites sans UUID de table
utilisent une identité dérivée déterministe à partir des clés persistées ; leur
sémantique indique explicitement cette projection.

## Contrat snapshot v2

La v1 J2 reste stricte et inchangée. La v2 ajoute :

- `origin: "core"`, `schema_version: 20`, `transport_revision: 1` ;
- `complete` et les limites ayant gouverné l'export ;
- `object_ref` avec contexte d'enquête ;
- détails structurés d'observation et de provenance ;
- sémantique et disposition des arêtes ;
- absence de coordonnées métier.

Le pont Web calcule une disposition déterministe en mémoire. Les déplacements
et positions épinglées restent de l'état de vue et ne sont jamais réécrits dans
SQLite. Les limites actuelles sont 500 nœuds, 1 000 arêtes, 1 MiB de JSON et
512 octets UTF-8 pour l'extrait stdout autorisé. Un dépassement échoue au lieu
de publier un résultat partiel. La navigation de provenance du prototype reste
bornée à 12 niveaux et 20 branches.

Les paramètres/argv, chemins absolus, stderr et fichiers de preuve ne sont pas
exportés. La fixture atteste explicitement son caractère synthétique au
sérialiseur ; celui-ci ne le déduit jamais du nom du dossier.

## Démonstration

Après compilation :

```bash
make -j8 core-graph-demo
cd prototypes/web-graph
python3 run_core_demo.py --port 8765
```

Le lanceur crée un répertoire temporaire privé, invoque l'exécutable fixé avec
des argv séparés, borne son exécution à 30 secondes, charge uniquement
`core-snapshot.json` accompagné de son manifeste, puis écoute sur
`127.0.0.1`. Un échec d'export affiche un état d'erreur et ne retombe jamais sur
les fixtures Python J2.

Ce lot ne clôt pas J3 : provenance uniforme en écriture, publication
transactionnelle généralisée, backend Web de production, Toolkit, jobs,
policies, claims/hypothèses et migration financière restent distincts.

L'extension ciblée [analyse EML → graphe](EML_ANALYSIS_TO_GRAPH.md) réutilise
ce lecteur, ajoute le snapshot v3 pour les extractions persistées et ne modifie
ni la v2 ni la fixture de ce lot.
