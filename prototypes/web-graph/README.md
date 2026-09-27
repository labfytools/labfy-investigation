# Prototype Web Graph J2 et connexion cœur J3

> Prototype expérimental en lecture seule. Toutes les données sont des fixtures
> `SPECIMEN` ; aucune base d'enquête n'est ouverte et aucune action réseau OSINT
> n'est exécutée.

## Lancer

Prérequis d'exécution : Python 3.11+ et un navigateur moderne. Le serveur
n'utilise que la bibliothèque standard Python ; `jsonschema` sert aux tests de
contrat et n'est pas requis pour afficher le prototype.

```bash
cd prototypes/web-graph
python3 server.py --port 8765
```

Ouvrir `http://127.0.0.1:8765/`, puis arrêter avec `Ctrl+C`. Le serveur écoute
uniquement sur loopback et ne sert que `public/` et les endpoints de fixtures.

### Mode snapshot du cœur C

Depuis la racine du dépôt, compiler puis lancer :

```bash
make -j8 core-graph-demo
cd prototypes/web-graph
python3 run_core_demo.py --port 8765
```

Cette commande crée un répertoire temporaire privé, génère une base SQLite V20
neuve via `Database` et les DAO C, ferme les writers, exporte un snapshot v2 via
une connexion réellement read-only, puis sert ce seul snapshot. L'écran affiche
« Snapshot du cœur C » et n'ouvre aucun flux SSE. Un échec C est affiché comme
tel, sans repli vers `fixtures.py`.

Le serveur Python ne lit jamais SQLite, n'accepte aucun chemin de base par HTTP
et ne sert aucun fichier de preuve. Les coordonnées du mode cœur sont calculées
par le pont Web comme état de présentation. Le contrat et le mapping sont dans
[`CORE_GRAPH_READONLY.md`](../../docs/architecture/CORE_GRAPH_READONLY.md).

### Mode analyse EML C persistée

Depuis la racine du dépôt :

```bash
make -j8 eml-graph-demo
cd prototypes/web-graph
python3 run_eml_demo.py --port 8765
```

Le générateur crée une base V20 synthétique, importe une preuve EML par
`EvidenceImporter`, exécute réellement `eml_analyzer`, publie deux extractions
avec leurs dérivés et observations proposées, vérifie un replay idempotent,
ferme le writer puis exporte en lecture seule un snapshot v3. Python ne fabrique
aucun objet du graphe. `--variant alternate` utilise un second message fictif
et permet de vérifier que les valeurs affichées proviennent bien de l'analyse.
Le contrat complet est décrit dans
[`EML_ANALYSIS_TO_GRAPH.md`](../../docs/architecture/EML_ANALYSIS_TO_GRAPH.md).

## Comportements démontrés

Le scénario `demo` contient 14 nœuds et 12 arêtes, dont deux relations distinctes
entre `person-a` et `username` et deux sources de provenance de `artifact`.

- Les relations métier, support, contradiction et provenance ont un libellé,
  un motif/couleur et, lorsqu'elles sont dirigées, une flèche. Les relations
  parallèles suivent des courbes déterministes distinctes, y compris après drag.
- Rendu, liste, compteurs, clavier et focus utilisent une projection commune.
  Les filtres vides restent navigables sans exception et invalident proprement
  les détails/actions devenus invisibles.
- Sélection, mode focus, historique, viewport, filtres et positions épinglées
  sont séparés. `Vue globale` conserve les positions ; `Réinitialiser la
  disposition` est la seule commande qui les efface.
- Les capabilities sont un instantané serveur par `object_ref`. Une personne,
  un domaine et une preuve reçoivent des actions différentes ; RDAP n'apparaît
  que sur le domaine et reste désactivé comme simulation sans contact réseau.
- La provenance parcourt toutes les origines avec limites de profondeur et de
  branches. Chaque source peut être ouverte séparément ; cycles et références
  manquantes sont signalés sans boucle ni chaîne inventée.
- Le flux SSE applique deux patches réels (révisions 1 → 2 → 3), reconnecte avec
  les identifiants du scénario, puis annonce une fin normale. Doublons et anciens
  événements sont ignorés ; trou ou curseur inconnu déclenchent le snapshot
  courant ; JSON/contrat inattendu produisent un diagnostic explicite.

Les jeux `100`, `1000` et `5000` gardent des graines fixes et exposent
respectivement deux fois plus d'arêtes que de nœuds. Ils servent aux contrats/API,
pas à promettre la fluidité du renderer SVG ni à choisir le moteur de J6.

## Tester

Les tests Node de logique ne demandent aucune dépendance npm. Les tests Python de
schéma demandent `jsonschema`. L'automatisation navigateur utilise
`puppeteer-core` 24.28.0 (Apache-2.0), verrouillé dans `package-lock.json`, avec
un Firefox local et un profil temporaire dédié ; elle ne télécharge pas de
navigateur et n'ouvre jamais un profil personnel.

```bash
cd prototypes/web-graph
npm ci --ignore-scripts --no-audit --no-fund
npm test
python3 -m unittest discover -s tests -p 'test_*.py' -v
npm run test:browser
```

Si Firefox n'est pas `/usr/bin/firefox`, fournir explicitement son chemin :

```bash
FIREFOX_PATH=/chemin/vers/firefox npm run test:browser
```

Le dernier scénario de cette matrice valide le [poste Web graph-first](../../docs/ui/WEB_WORKBENCH.md)
aux formats 1440×900, 1366×768, 700×900 et au zoom 200 %. Ses captures locales
inspectables sont `/tmp/labfy-workbench-{selected,context,planner,provenance,report,empty,narrow}.png`.
Le scénario J9 clique séparément les cinq liens de rapport, attend la fin des
téléchargements Firefox dans un répertoire privé, puis vérifie leurs tailles,
SHA-256, HTML hors ligne, PDF et manifeste sans lire le dossier export serveur.

### Relevé de validation locale du 25 septembre 2026

Environnement observé : Firefox 156.0.1, Node 26.10.0, npm 12.1.0,
Python 3.14.7 et `jsonschema` 4.26.0.

| Validation | Résultat observé |
|---|---|
| `npm test` | 9/9 tests Node réussis |
| tests Python | 6/6 tests réussis, dont schémas, sécurité HTTP, tailles et reprise SSE |
| `npm run test:browser` | PASS Firefox sans exception JavaScript |
| Rendu large | sélection/focus, trois voisins, deux liens parallèles et fin SSE visibles |
| Rendu étroit | une colonne, projection vide, commandes et diagnostic lisibles |
| État erreur | JSON SSE invalide refusé et affiché sans faux statut synchronisé |

Le scénario navigateur exerce chargement, sélection de nœud et d'arête, drag en
plusieurs mouvements avec relâchement extérieur, `pointercancel`, distinction
clic/drag, zoom, pan, focus, retour, projection vide + clavier, repli/dépli,
capabilities par objet, provenance multiple, deux reconnexions SSE, conservation
du contexte, doublon, trou et rattrapage snapshot. Les captures sont des preuves
visuelles complémentaires ; les assertions d'interaction font foi.

### Relevé du polish visuel Web V1 du 27 septembre 2026

La coque Web actuelle est organisée autour de **Agent | Graphe | Activité** et
d’un tiroir transversal. Le graphe reste la surface dominante. Les panneaux
latéraux et le tiroir sont repliables ; sur desktop, leurs dimensions sont
redimensionnables et conservées comme préférences locales par origine. Cette
préférence ne contient ni données d’enquête, ni token, ni état métier.

L’Agent affiche un flux SPECIMEN typé, une demande d’autorisation explicitant
cible, provider, données sortantes, action, exposition, budget et raison. Il
indique clairement qu’aucun modèle local n’est connecté : cette surface prépare
un raccordement futur, elle ne simule aucun appel IA. Activité est un terminal
structuré filtrable ; son mode expert ne révèle que des paramètres SPECIMEN non
secrets et une provenance bornée.

test_visual_polish_browser.mjs, inclus dans npm run test:browser, exerce le
resize et sa restauration, les panneaux repliés, le prompt Ctrl+Entrée, le
filtre/inspecteur d’activité, le drag/pin, le mode graphe prioritaire, les
tailles 1440×900, 1280×800, 1024×768 et 700×900, ainsi qu’un diagnostic
d’erreur de contrat. Il produit des captures inspectables sous
/tmp/labfy-visual-polish-*.png, toutes sur workspace et serveur loopback
temporaires SPECIMEN.

### Relevé du lot J3 local du 26 septembre 2026

| Validation | Résultat observé |
|---|---|
| `tests/test_core_graph` | 5/5 : read-only, schéma, projection, stabilité, erreurs |
| `npm test` | 9/9 tests Node J2 réussis |
| tests Python | 8/8, dont snapshot v2 réellement généré par le binaire C |
| `npm run test:browser` | PASS J2/SSE puis PASS cœur C dans Firefox isolé |
| Rendus cœur | large, étroit et erreur d'export réellement inspectés |

Le second scénario navigateur vérifie 9 nœuds et 8 arêtes issus de SQLite :
personnes/identifiants, preuves, observation promue et non promue, exécution
historique, deux origines de provenance, lacune explicite, capabilities locales,
HTML inerte, recherche, focus/retour, filtre vide, drag et positions épinglées.

### Relevé du lot J3 EML local du 26 septembre 2026

| Validation | Résultat observé |
|---|---|
| `tests/test_eml_analysis_persistence` | 6/6 scénarios : publication, replay/conflit, rollback, chemins/sources/limites, analyse vide/annulation, projection |
| tests Python | 9/9, dont validation JSON Schema du snapshot EML v3 produit en C |
| `npm test` | 9/9 tests Node réussis |
| `npm run test:browser` | PASS J2, cœur v2 et EML v3 dans Firefox isolé |
| Rendus EML | large, étroit et erreur sans repli réellement inspectés |

Le scénario EML vérifie deux extractions, deux dérivés et les observations
réellement calculées depuis le message. Il contrôle notamment
`Elodie@Atelier.test` → `elodie@atelier.test`, les états `proposed` et
`unpromoted`, la navigation jusqu'à la preuve originale, le contenu HTML
inerte, ainsi qu'une variante indépendante contenant `maelle@variant.test`.

### Mode Toolkit local J4

```bash
make -j8 local-toolkit-demo
cd prototypes/web-graph
python3 run_local_toolkit_demo.py --port 8765
```

Le lanceur génère une enquête V20 SPECIMEN, analyse un EML via l'adapter natif,
crée les métadonnées fictives d'une image avant import, exécute l'ExifTool réel
via le runner borné, publie les deux parcours puis sert uniquement le snapshot
C relu en read-only. Les actions documentaires sont visibles mais désactivées
dans ce client, avec la raison « Exécution par le lanceur local ».

## Limites

Le serveur Python et les contrats restent des instruments de prototype. Le mode
cœur consomme un fichier d'échange généré mais n'accède pas lui-même à SQLite.
Il n'y a ni authentification utilisateur, mutation, OSINT, backend de production
ou garantie exactly-once. Host/Origin, CSP, méthodes et racine servie sont testés ;
TLS, CSRF de mutation, DNS rebinding complet, multi-utilisateur, packaging et
durcissement de production restent hors de cette consolidation J2.

### Clôture Workbench sur réseau alimenté

`test_workbench_populated_browser.mjs` exécute les six analyses proposées
depuis les commandes visibles du Web. Sur la fixture du 26 septembre 2026, le
cœur exporte réellement 62 nœuds et 100 arêtes. Ces nombres sont vérifiés mais
ne sont jamais codés dans le renderer.

Le scénario contrôle formes métier, arêtes dirigées, Infrastructure → Réseau,
provenance cliquable et retour, actualisation à UUID constants, réduction du
tiroir, `Maj+F10`/`Échap` et invalidation de l’aperçu. Les captures produites
sont `/tmp/labfy-workbench-populated-{network,navigation,provenance,drawer,menu,report,narrow}.png`.

### Espace vide et import local

`make web-workspace-local WORKSPACE=/tmp/labfy-local-workspace` n’appelle aucun
générateur SPECIMEN. Le formulaire authentifié crée l’enquête vide via le cœur
C. Le dialogue **Ajouter des preuves** utilise un véritable sélecteur multiple,
prépare EML/PNG/JPEG, permet le retrait, puis confirme chaque publication.
Le serveur fixe l’identité du lot, réserve atomiquement places et octets, borne
la lecture silencieuse/lente et interdit de rouvrir un upload préparé ou importé.
La reprise revalide intention, reçu, record V20 et original publié.
`test_local_workspace_import_browser.mjs`, inclus dans `npm run test:browser`,
part de zéro et poursuit jusqu’au planner, au graphe et au rapport PDF.

### Revue et aperçu d’une preuve locale

Le bridge Web demande au cœur C l’aperçu d’une preuve EML, PNG ou JPEG après
contrôle de son intégrité et des limites de rendu. Il retourne une représentation
inerte bornée ; ni l’original, ni un chemin d’original, ni un répertoire de
preuves ne sont servis par HTTP. L’identifiant de cache affiché décrit cet
aperçu de réponse et ne correspond pas à un cache disque persistant.

Les observations sont relues puis revues transactionnellement. Une décision de
revue, une correction humaine, une création/rattachement explicite d’indicateur
ou un retrait ont un UUID d’opération, une révision attendue, un auteur et un
motif. La confirmation concerne l’observation et ne constitue jamais une
attribution d’identité. Seuls e-mail, domaine et IP sont promouvables dans ce
lot ; le retrait préserve l’observation, l’entité partagée et les rattachements
indépendants.

Après import ou revue, les projections graphe, corrélation et planner sont
republiées depuis le persistant ; cette actualisation n’exécute pas une analyse
et n’engage aucun budget. La cible ciblée est :

```bash
make web-workspace-review-check
```

Elle compile séquentiellement `tools/local-jobs` et `tools/local-jobs-test`,
puis exécute l'intégralité de `test_local_workspace_import.py` sur des
workspaces temporaires `SPECIMEN`, avant le parcours navigateur local. La phase
Python couvre notamment le durcissement P01–P06 (cycle de vie, rejeu,
réservations et framing HTTP) ; un échec de l'une ou l'autre phase fait échouer
la cible. Elle ne constitue pas une certification de production ni une
validation de sanitizers.
