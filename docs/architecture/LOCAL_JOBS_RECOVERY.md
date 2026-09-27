# Jobs locaux persistants et reprise — J5

Statut : `CURRENT` pour le parcours local SPECIMEN décrit ici. Ce document ne
déclare ni autonomie réseau, ni worker distribué, ni release.

## Décision d’architecture

`Enquete.sqlite` V20 reste l’unique vérité métier. Le stockage opérationnel
`.labfy/runtime/jobs.sqlite`, schéma `JOB_STORE_SCHEMA_V3`, conserve seulement
les demandes, tentatives, transitions, contrôles et références de résultats.
Il est lié à l’UUID de l’enquête et refuse une version ou enquête inconnue. Les
stockages historiques V1 et V2 sont migrés vers V3. Il ne reçoit jamais les
migrations métier V21 : la tranche financière demeure indépendante.

Les deux bases ne partagent pas de transaction. Une publication suit : intention
durable avec identités stables ; claim atomique avec UUID de propriétaire et de
tentative ; exécution hors transaction ; publication par le service J3/J4 ; puis
acquittement terminal dans le JobStore.

Après un arrêt entre publication et acquittement, le superviseur suivant acquiert
d’abord `.labfy/runtime/worker.lock`, réouvre le même espace et appelle le service
avec les mêmes UUID. Le replay contrôle source et artefact typé. Un résultat
cohérent devient `COMPLETED / recovered_after_publish` sans nouvelle extraction ;
un résultat ambigu devient `RECOVERY_REQUIRED`. Une ancienne génération ne peut
plus acquitter un job, car les écritures vivantes exigent son token.

## États, bornes et contrôles

La machine expose `QUEUED`, `RUNNING`, `RETRY_WAIT`, `COMPLETED`, `FAILED`,
`CANCELLED`, `RECOVERY_REQUIRED` et `BLOCKED`. La file est bornée à 128 demandes,
chaque demande à trois tentatives par défaut, et les diagnostics à 2 Kio par
contrat. La concurrence totale est un worker par enquête. `pause` bloque les
nouveaux claims ; `stop` empêche un nouveau départ ; `cancel` termine une demande
en attente ou marque une demande en cours pour réconciliation. Une dépendance
locale doit exister et réussir avant le claim ; l’auto-cycle est refusé.

Les délais d’outil sont mesurés par l’horloge monotone du runner. Les dates
persistées ne prouvent jamais seules la mort d’un propriétaire : le verrou local
est la preuve employée par ce lot.

## Reproduction

```sh
make -j8 tools/local-jobs
workspace=$(mktemp -d /tmp/labfy-j5-demo-XXXXXX)
chmod 700 "$workspace"
tools/local-jobs demo --workspace "$workspace"
tools/local-jobs status --workspace "$workspace"
tools/local-jobs export --workspace "$workspace"
```

`demo` crée seulement des sources EML/PNG synthétiques, les soumet, exécute
l’analyseur EML natif et ExifTool installé, puis provoque deux sorties brutales
contrôlées après publication et avant acquittement. Les processus suivants
rouvrent les mêmes fichiers. Relancer `run` reste idempotent : deux extractions.

Les commandes sont `init-specimen`, `enqueue`, `run`, `resume`, `pause`, `stop`,
`cancel --workspace DIR JOB_ID`, `status`, `export` et `demo`. Le point de faute
interne est compilé sous `LOCAL_JOBS_ENABLE_CRASH_HOOK`; aucune mutation HTTP ne
l’expose.

C écrit atomiquement `jobs-snapshot.json` au contrat
`labfy.local_jobs.snapshot.v1`. Python valide et sert ce fichier par
`GET /api/v1/jobs` sans ouvrir SQLite. L’interface le relit toutes les 500 ms et
rappelle que les contrôles passent par le lanceur local :

```sh
python3 prototypes/web-graph/server.py \
  --core-snapshot "$workspace/core-snapshot.json" \
  --jobs-snapshot "$workspace/jobs-snapshot.json"
```

Les tests démontrent le crash de processus et la non-duplication métier, pas une
durabilité absolue face à une coupure électrique. Le runner ferme les descripteurs
et borne processus, sorties et délais, mais n’est pas une sandbox hostile. Aucune
enquête réelle, autonomie réseau, migration financière, installation globale ou
publication distante ne fait partie de J5.
