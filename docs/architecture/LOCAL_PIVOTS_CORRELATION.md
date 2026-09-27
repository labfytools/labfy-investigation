# Pivots locaux et rapprochements explicables — lot J7 local

> État : `CURRENT` pour le corpus `SPECIMEN` J7 et les types e-mail, domaine et IP.\\
> Runtime métier : V20 inchangé. JobStore : V1 inchangé. V21 reste indépendante.

## Reproduction synthétique

```sh
workspace=$(mktemp -d /tmp/labfy-j7-specimen-XXXXXX)
chmod 700 "$workspace"
make -j8 tools/local-jobs
make web-workspace-j7 WORKSPACE="$workspace"
```

Cette commande ne réinitialise jamais un espace existant. Le corpus créé par C
contient quatre EML, dont une copie de contenu identique, et une image. Il
démarre sans observation. Les boutons Web soumettent les analyses EML/ExifTool
existantes ; les observations sont publiées en SQLite V20 avant d’être relues
par `LocalCorrelationService`. Python ne lit aucune base et JavaScript ne
décide aucune égalité.

## Contrat du cœur

`labfy.local_correlation.snapshot.v1` est un cache recalculable, jamais une
preuve ni une décision humaine. Il porte l’UUID d’enquête, une révision dérivée
des observations persistées, `complete`, les bornes, les observations, groupes
et connexions exploratoires. Sa publication atomique utilise un temporaire
unique dans le même répertoire.

Les capacités de chaque observation éligible sont calculées par C : voir les
occurrences, voir les preuves, et explorer localement le domaine d’une adresse
compatible. Chaque groupe expose règle/version, brut/normalisé, état de revue,
extraction, preuve, hash de contenu, membres et quatre compteurs distincts :
occurrences, analyses, preuves et contenus. Son identité dépend des membres et
de la révision d’entrée.

Les connexions sont non dirigées, exploratoires et limitées aux chemins de
profondeur deux `groupe → preuve persistée → groupe`, avec au plus 1 000
connexions. Elles ne créent aucune relation métier. Une limite atteinte rend le
résultat incomplet ; elle ne signifie jamais « aucun lien ».

## Normalisation et faux positifs

Règle `labfy.local_identifier_normalization.v1` :

- e-mail : partie locale conservée byte pour byte ; seul le domaine ASCII est
  mis en minuscules, sans retirer point ni suffixe `+` ;
- domaine : forme ASCII supportée, point terminal retiré, minuscules ; aucun
  domaine enregistrable n’est deviné ;
- IP : parsing numérique GLib et forme canonique, sans DNS.

Les formes invalides et observations `rejected`/`invalid` restent visibles avec
leur exclusion. Un e-mail, domaine ou IP commun ne confirme ni personne, ni
origine réseau, ni source indépendante. Deux fichiers de même hash restent deux
preuves distinctes mais un contenu distinct unique.

## Processus, API et cohérence

Le worker conserve son verrou jusqu’à la fin des descendants gérés. SIGTERM
demande au runner une annulation coopérative ; le runner termine puis récolte la
session isolée de l’outil avant réconciliation. Annuler un job en attente ne tue
plus le worker actif. Stop bloque les nouveaux claims ; pause laisse finir le
job courant. Les exports concurrents utilisent des temporaires uniques.

Le serveur sert du JSON validé. Les mutations exigent session, Origin exact et
CSRF, avec champs textuels exacts et bornés. Aucune route n’accepte SQL, chemin,
argv, programme ou recherche réseau.

## Limites

Pas de recherche réseau, DNS, fusion, attribution automatique, score de
personne, revue persistante d’hypothèse, planner autonome ni performance de gros
graphe revendiquée. Les snapshots v1/v2/v3 restent inchangés.

`J7_LOCAL_PIVOTS_CORRELATION = COMPLETE_LOCAL`
