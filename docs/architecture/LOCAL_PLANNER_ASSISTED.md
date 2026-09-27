# Planner local et assistance contrôlée — lot J8 local

> État : `COMPLETE_LOCAL` pour l'espace synthétique J8. `Enquete.sqlite` reste V20 ;
> la V21 financière est indépendante. Aucun réseau ni J9.

## Parcours

```sh
workspace=$(mktemp -d /tmp/labfy-j8-specimen-XXXXXX)
chmod 700 "$workspace"
make -j8 tools/local-jobs
make web-workspace-j8 WORKSPACE="$workspace"
```

La commande crée seulement un corpus `SPECIMEN` si l'espace n'existe pas. Elle
ne remet jamais à zéro un espace existant. Le panneau **Prochaines actions**
affiche les recommandations issues du cœur C, leur priorité, raison, capability,
versions, empreinte, coût et contact réseau `NONE`. Un GET ne lance rien.

Après sélection, le navigateur transmet uniquement les identifiants de
recommandations, la révision, le profil et une clé d'idempotence. C revalide la
projection, les preuves, empreintes, capabilities et budgets, puis admet plan et
jobs en une transaction. Le worker existant exécute l'ensemble fini. Les
snapshots graphe, rapprochements et planner sont recalculés après publication ;
aucune recommandation nouvelle ne s'exécute sans nouvelle approbation.

## Contrats et identités

- `labfy.local_planner.snapshot.v1` est un cache recalculable ;
- `labfy.local_plan.v1` est l'intention durable ;
- `labfy.local_planner.rules.v1` classe `HIGH`, `MEDIUM`, `LOW` sans score
  d'identité ;
- la clé d'analyse couvre enquête, preuve, hash/taille, capability, adapter et
  versions ;
- même clé et même intention retrouvent plan et jobs ; une autre intention est
  un conflit ; résultat compatible ou job actif rend la recommandation
  indisponible. Les dérivés ne sont jamais candidats.

## JobStore V3 et budgets

V2 a ajouté `plans` et `plan_jobs`. V3 ajoute aux tentatives l'enveloppe de
temps réservée, la durée mesurée et l'acquittement comptable, ainsi que
l'unicité d'une analyse active. Une ouverture en écriture migre V1 ou V2 sous
`BEGIN IMMEDIATE`; un lecteur read-only ne migre rien. Jobs, tentatives,
transitions, contrôles et plans existants sont conservés.

`LOCAL_PRUDENT` borne 8 analyses, 16 tentatives réservées, 256 Mio de sources et
240 s actives. `SPECIMEN_SMALL` borne 2 analyses, 3 tentatives, 8 Mio et 30 s.
La réservation analyses/tentatives/octets est atomique. Avant chaque départ,
claim, propriétaire, tentative et enveloppe temporelle sont débités dans la
même transaction. L'horloge monotone mesure l'exécution ; seule la part
inutilisée prouvée est rendue. Après crash, l'enveloppe reste consommée. La
minuterie annule réellement EML ou ExifTool au solde autorisé. Une
réconciliation sans relance ne débite rien.

L'export C fournit `PENDING`, `ACTIVE`, `PAUSED`, `COMPLETED_SUCCESS`,
`PROCESSED_WITH_FAILURES`, `STOPPED`, `LIMIT_REACHED` ou
`RECOVERY_REQUIRED`, avec plafonds, réservations, consommations, soldes et
compteurs de jobs. Le Web affiche ces valeurs en unités réelles, sans pourcentage.

## Cohérence et limites

La projection métier utilise un snapshot SQLite. La base métier et le JobStore
ne partagent pas de transaction : la révision est donc revalidée avant admission,
puis hash/taille/capability avant création. Les limites atteintes ou résultats
incomplets ne prouvent aucune absence d'information.

Ce lot n'apporte ni autonomie, ni OSINT réseau, ni LLM, ni analyse d'enquête
réelle. Le serveur Python reste une passerelle loopback bornée ; il ne lit aucune
base et ne décide aucune recommandation.

## Validation de clôture locale

La clôture couvre le chemin C réel de migration JobStore V1/V2 vers V3, avec
rollback injecté après mutation de schéma, réouverture idempotente, lecture
seule sans migration, version future et fichier absent. Les trois interruptions
de processus sont distinguées : après admission avant claim, après
claim/débit avant publication, et après publication avant acquittement. Les
rejeux conservent identités, réservations et résultats sans double extraction.

Le service C de corrélation exerce les frontières observations, groupes,
connexions et octets JSON sur des observations `SPECIMEN` persistées. Les
projections incomplètes sont signalées, les connexions ne pendent pas et un
refus de taille ne remplace pas le dernier export valide. La révision couvre
les règles, les données et les limites effectives.

La commande canonique `npm run test:browser` inclut J8. Firefox couvre le refus
pré-admission atomique, la limite pendant l'exécution, pause/reprise,
annulation en attente, annulation RUNNING après barrière, stop/reprise, et le
redémarrage sur le même workspace sans recharge de budget ni republication.
Les hooks de faute et l'outil lent n'existent que dans `tools/local-jobs-test` ;
le bridge opérationnel et les routes HTTP ne les exposent pas.

`J8_LOCAL_PLANNER_ASSISTED = COMPLETE_LOCAL` ne signifie ni J9, ni publication,
ni validation sur une enquête réelle.
