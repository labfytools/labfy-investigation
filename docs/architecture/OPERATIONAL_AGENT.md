# Agent opérationnel local — fondations V1

**État : CURRENT local, validé exclusivement sur `SPECIMEN`.** Cette
tranche étend le runtime Qwen local existant avec un overview borné, une mission
humaine bornée, des propositions candidates et des fondations d'outils. Elle ne
publie ni une autonomie générale, ni une mémoire agent persistante, ni une
autorisation implicite.

## Contexte et corrélations

Après l'ouverture explicite d'un workspace valide, le runtime reçoit uniquement
un contexte `labfy.investigation_context.v1` : identité opaque, titre, révision,
compteurs, types, sélection et activité récente bornée. Il ne reçoit ni SQLite,
ni chemins, ni corpus de preuves. Les références sont réévaluées dans le
workspace actif.

Les corrélations sont des candidats déterministes d'égalité normalisée (email,
domaine, téléphone, username, URL, IP, IBAN/BIC, hash, identifiant structuré,
provenance ou observation). Elles gardent les sources et `confidence_kind`; elles
ne créent ni relation, ni identité confirmée, ni écriture automatique.

## Mission et propositions

Une mission requiert un démarrage humain explicite, des références de scope et
pivots appartenant à l'espace actif, et des plafonds conservateurs. V1 admet
seulement `LOCAL_READ_ONLY` et `PASSIVE_PUBLIC`; `PUBLIC_ACTIVE`,
`AUTHORIZED_INTRUSIVE` et `PROHIBITED` sont refusés. Chaque tentative repasse
par Policy, scope et budget.

Une proposition est `CANDIDATE`, avec motifs et références. Une décision
`APPROVED` garde explicitement `policy_grant_created: false`: ce n'est ni un
fait ni un grant ni l'exécution d'une capability.

## Modèle local

`LocalModelSupervisor` est opt-in. Il lit une configuration XDG JSON explicite
non versionnée, démarre seulement son propre `llama-server` sur
`127.0.0.1:<port éphémère>`, et ne signale prêt qu'après `GET /health = 200`.
Il ne recherche, ne tue ni ne réutilise un processus tiers. L'arrêt gracieux,
puis ciblé après délai, ne concerne que son PID/groupe de processus possédé.

Le chemin de modèle reste hors du dépôt. Sans configuration, binaire ou
healthcheck valides, l'agent est indisponible sans bascule cloud ou fake.

Le service local utilise `--agent-mode local-model --agent-autostart
--agent-timeout 90`. Le superviseur démarre son propre serveur modèle sur un
port loopback éphémère ; le navigateur appelle seulement Labfy. Le champ
« Objectif utilisateur » existant lance le turn Qwen et ses appels d'outils
passent par `AgentRuntime` et les capabilities. L'interface distingue Qwen prêt,
occupé et indisponible. Le choix manuel d'outil reste dans « Mode expert/debug ».

## Limites actuelles

La fondation Sandbox/Tool Registry et l'egress privacy sont décrites dans leurs
documents dédiés. Le smoke réel
`prototypes/web-graph/tests/manual_operational_agent_smoke.py` a
atteint `COMPLETED` avec 8 appels d'outils Qwen dans trois turns liés par la
même mission et les références renvoyées par la recherche : overview,
search, corrélation, registre, documentation, sandbox offline, proposition
candidate et `web.fetch`. Le contact `PASSIVE_PUBLIC` passe par l'egress
rootless `PRIVACY_TOR` vérifié ; un contact est comptabilisé dans la mission,
sans repli direct. La proposition ne crée aucun grant. Les sondes actives
restent soumises à la Policy et n'ont pas été exercées live ; l'intrusif est
`LAB-ONLY TARGET`. Le [provisionnement contrôlé](TOOL_PROVISIONING.md) et la
[self integration](QWEN_SELF_INTEGRATION.md) sont `CURRENT local` sur
`SPECIMEN` avec portes humaines distinctes. Resource Governor et Persistent
Agent Memory restent `TARGET`.
