# Protocole d’outils agent V1

Statut : **CURRENT local de laboratoire** pour le poste Web local. Le modèle
Qwen opérationnel, le sandbox et l'egress Tor passif sont validés sur `SPECIMEN`.
Ce protocole ne livre ni autonomie générale ni outil intrusif.

## Frontière

`WorkspaceServer` possède un `AgentGateway` Python éphémère. L'agent de
démonstration déterministe et le runtime Qwen local utilisent la même frontière.
Le gateway ne lit pas SQLite, ne lance ni shell ni processus et ne possède
aucun stockage durable. Il délègue exclusivement aux projections et commandes
métier existantes.

Le catalogue backend inclut `investigation.get_overview`,
`investigation.search`, `investigation.find_correlations`, `graph.get_node`,
`graph.get_neighbors`, `evidence.get_summary`, `evidence.read_excerpt`,
`provenance.trace`, `jobs.get`, `tool.catalog`, `tool.docs.read`, `sandbox.exec`,
`agent.propose`, `web.fetch`, `research.get_state` et `research.prepare`.
Chaque outil expose `tool_id`, versions d’outil/capability, description,
schéma d’entrée, contrat de sortie, classes d’action/risque/coût, contact réseau,
besoin d’autorisation et disponibilité réelle. L’interface ne déduit jamais ces
attributs elle-même.

Les snapshots et le flux de recherche existants restent les sources de vérité.
Le résultat mémoire du gateway est seulement une réponse de transport, jamais
une projection métier parallèle.

## HTTP, turns et sécurité

Les routes sont `GET /api/v1/agent-tools/catalog`,
`POST /api/v1/agent-tools/turns`, `POST /api/v1/agent-tools/calls`,
`POST /api/v1/agent-tools/turns/{turn_id}/resume`,
`GET /api/v1/agent-tools/results/{result_id}` et
`GET /api/v1/agent-tools/events?cursor={sequence}`.

Elles exigent le `Host` exact et la session locale ; les mutations exigent aussi
Origin, CSRF et le garde-fou de fréquence. Les résultats/événements sont isolés
par instance et workspace. Chaque call transporte `turn_id`, `call_id`,
`tool_id`, `input`, `context`, `object_refs` et `idempotency_key` ; `call_id`
et `result_id` sont deux UUID distincts. Un rejeu identique est idempotent,
une même clé avec une intention différente répond `409`.

## Résultats, événements et autorisation

Les résultats structurés utilisent `COMPLETED`, `FAILED`, `DENIED`,
`AUTHORIZATION_REQUIRED`, `UNAVAILABLE` et, pour un futur contrat annulable,
`CANCELLED`. Ils contiennent `object_refs`, `artifacts`, `observations`,
`provenance_refs` et un diagnostic borné plutôt qu’un gros contenu copié.

Le polling HTTP publie `agent.turn.started`, `agent.plan.updated`,
`agent.tool.requested`, `agent.tool.started`, `agent.tool.completed`,
`agent.authorization.required`, `agent.turn.completed` et `agent.error`.
Chaque événement porte séquence, horodatage, turn, call éventuel, références et
payload borné. L’UI rend les contenus non fiables avec `textContent` et peut
mettre en évidence des objets existants sans créer de nœud autoritatif.

L’agent déterministe livré réalise réellement :
`USER → AGENT → PLAN → TOOL_REQUEST → TOOL_RESULT → AUTHORIZATION_REQUIRED → RESULT`.
Il lit progressivement l’enquête puis appelle `research.prepare`. Cette étape
ne crée ni grant, ni campagne, ni contact réseau. Après la décision humaine
persistée et la campagne admise par le flux existant, l’UI demande la reprise du
même turn ; le gateway relit l’état puis publie le bilan. L’agent ne possède
aucune voie d’auto-autorisation.

Une instance conserve au plus 64 appels/résultats de 256 Kio et 128 événements,
avec éviction FIFO ; toute cette mémoire disparaît à l’arrêt. Il n’existe pas de
SSE opérationnel pour ce protocole.

Le [runtime modèle local V1](LOCAL_MODEL_AGENT_RUNTIME.md) s’ajoute au-dessus
du gateway sans modifier ses neuf outils ni sa policy. Il est `CURRENT local` :
le smoke `SPECIMEN` a validé un Qwen réel via llama.cpp loopback, avec lecture,
pause d’autorisation, grant humain, campagne synthétique, reprise du même turn
et bilan final. La gestion automatique de Qwen, le Resource Governor AMD/RAM/
swap et le stockage durable des conversations restent `TARGET`.

Le catalogue est un contrat fermé : il expose les champs requis, types,
bornes et `additionalProperties: false` réellement validés par le gateway.
En particulier, `research.prepare` requiert `selection_ids` (1 à 8 chaînes),
`question` (chaîne de 512 caractères au maximum) et `exclusions` (0 à 8
chaînes). Les identifiants de graphe restent namespacés (`evidence:<uuid>`) à
la frontière de recherche, qui les résout exactement contre le snapshot ; un
UUID nu est refusé explicitement. Cette règle aligne l’agent sur le flux Web
humain et ne transforme jamais une référence en droit.

## Outils provisionnés et changement de code — CURRENT local

Le catalogue optionnel ajoute `tool.provision.search`, `tool.provision.propose`,
`tool.docs.read`, `tool.integration.propose`, `capability.execute` et
`code.change.propose`. Les pauses A/B/C1/C2 du runtime attendent des décisions
humaines persistées par routes HTTP sécurisées. Le modèle ne reçoit aucun
outil d'approbation. La capability dynamique reste soumise au scope, à la
mission, à la Policy et au budget au moment de son exécution. Voir les contrats
de [provisionnement](TOOL_PROVISIONING.md), de [manifeste](CAPABILITY_MANIFESTS.md)
et de [développement isolé](QWEN_SELF_INTEGRATION.md).
