# Analyse EML persistée vers le graphe Web — lot J3

> **État :** `CURRENT` local, synthétique, non publié
> **Runtime d'écriture et de lecture :** V20 inchangé
> **Contrat transport :** `labfy.web_graph.snapshot.v3`

Ce lot exécute réellement l'analyseur natif `eml_analyzer` sur une preuve EML
importée par les API de production. Les observations ne sont ni préinsérées ni
reconstruites par Python ou JavaScript : le service C les calcule, les publie
dans SQLite avec leur extraction, ferme la connexion d'écriture, puis la
projection C les relit en lecture seule pour produire le snapshot Web.

```text
EML SPECIMEN
  → EvidenceImporter → preuve originale contrôlée
  → EmlAnalysisPersistenceService::prepare
       chemin relatif + anti-symlink + taille + SHA-256
       eml_analyzer_analyze_file (en-têtes uniquement)
       dérivé JSON versionné en mémoire
  → staging privé + rename du dérivé
  → transaction SQLite unique
       preuve dérivée + extraction + observations proposed
  → fermeture / réouverture V20 read-only
  → CoreGraphProjectionService → snapshot v3
  → serveur statique loopback → graphe Web
```

## Contrat du service

La demande fournit trois UUID distincts et une date UTC : preuve source,
identité de requête/extraction et identité de preuve dérivée. Le service
emprunte une `Database` ouverte et copie la racine d'enquête ; l'appelant reste
propriétaire des deux. `prepare()` possède son résultat jusqu'à
`eml_analysis_prepared_free()` et ne modifie ni SQLite ni le disque.
`publish()` retourne un résultat possédé, libéré avec
`eml_analysis_publication_result_free()`.

Avant l'analyse comme avant un replay idempotent, la copie contrôlée est un
fichier régulier sous la racine, sans composant symbolique. Sa taille et son
SHA-256 doivent être identiques au `EvidenceRecord`. Les limites locales sont
4 Mio pour la source, 256 observations et 1 Mio pour le dérivé. Elles bornent
le travail de cette capacité sans imposer une limite scientifique globale à
l'enquête.

Le dérivé `labfy.eml_analysis.derivative.v1` conserve :

- UUID et date de demande ;
- UUID, taille et SHA-256 de l'original ;
- `labfy.eml_analyzer`, version `1` ;
- portée `headers-only` et limites appliquées ;
- pour chaque observation : type, brut, normalisé, rôle, en-tête,
  occurrence et nature de provenance.

Le dérivé est préparé hors transaction, écrit dans un staging privé puis
renommé. La preuve dérivée, l'extraction et toutes les observations sont ensuite
insérées dans une transaction SQLite unique. Un échec annule les lignes et
supprime uniquement le fichier final créé par cette tentative. Les originaux
ne sont jamais modifiés.

## Idempotence et réanalyse

L'UUID de demande est la clé idempotente. Un replay après réouverture vérifie à
nouveau l'original, le dérivé, son hash, son contrat, l'outil, les paramètres,
la date et le lot exact d'observations persistées. Il retourne le même résultat
sans seconde publication. Une identité réutilisée avec une source, un dérivé,
une date ou des observations incompatibles produit un conflit explicite. Un
dérivé orphelin ou un lot incomplet demande une récupération explicite.

Une réanalyse est une nouvelle demande, avec un nouvel UUID d'extraction et un
nouvel UUID de dérivé. Elle peut donc produire un second lot proposé sans
écraser le premier. Le scénario de démonstration exerce les deux cas : replay
de la première demande, puis réanalyse distincte.

## Persistance V20 et projection v3

Aucune migration ni DDL n'est ajouté. Le lot réutilise :

| Objet | Persistance V20 | Relation |
|---|---|---|
| original EML | `preuves` | entrée de l'analyse |
| dérivé structuré | `preuves` | `extractions.evidence_id` |
| tentative publiée | `extractions` | `source_kind=evidence`, `source_id=<original>` |
| observation calculée | `evidence_entity_observations` | `evidence_id=<original>`, `extraction_id=<demande>` |

Les observations restent `proposed`, non promues et sans entité créée. Le lot
ne fabrique pas d'`osint_execution` : le runtime V20 ne permet pas d'y exprimer
une sélection de type preuve, et falsifier cette exécution détruirait la
provenance.

Le snapshot v2 demeure inchangé pour le scénario J3 read-only historique. La
v3 ajoute seulement `object_kind: extraction`, les nœuds d'extraction et les
arêtes de provenance `analysis_input`, `analysis_derivative` et
`analysis_observation`. L'original, le dérivé et chaque observation conservent
leurs identités SQLite. Le navigateur reçoit ce snapshot ; il ne lit jamais la
base ou le dérivé et n'invente aucune observation.

## Démonstration et validation

```bash
make -j8 eml-graph-demo
cd prototypes/web-graph
python3 run_eml_demo.py --port 8765
```

Le lanceur crée une racine privée temporaire, exécute un binaire fixé avec des
arguments séparés et un délai de 45 secondes, puis ne sert que le snapshot
accompagné du manifeste EML attendu. `--variant alternate` produit un autre EML
synthétique afin de prouver que l'affichage suit le contenu réellement analysé.
Un échec C est affiché ; aucun repli vers les fixtures J2 n'est autorisé.

Les tests C couvrent analyse, valeurs brutes/normalisées, provenance,
fermeture/réouverture, replay, conflit, altération, source absente, limite,
analyse vide, annulation, liens symboliques, rollback et intégrité SQLite. Les
tests Firefox couvrent snapshot v3, deux
extractions, deux dérivés, observations proposées, navigation de provenance,
recherche, focus/retour, drag, vue étroite, erreur sans repli et contenu HTML
inerte. Toutes les données utilisent `.test`, les plages IP de documentation et
des noms `SPECIMEN`.

Ce lot ne livre ni analyse automatique d'une enquête ouverte, ni promotion
d'entité, ni mutation Web, ni serveur de production, ni action réseau OSINT.

`J3_EML_ANALYSIS_TO_GRAPH = COMPLETE_LOCAL`
