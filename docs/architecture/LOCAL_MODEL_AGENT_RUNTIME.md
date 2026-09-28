# Runtime d’agent à modèle local V1

**Statut : CURRENT local de laboratoire.** Cette tranche qualifie un runtime
avec fournisseur OpenAI-compatible de test sur loopback et workspaces
`SPECIMEN`. Elle ne prouve ni llama.cpp ni des poids Qwen réels : Qwen réel et
le Resource Governor AMD/RAM/swap/scratch restent `TARGET`.

`REAL_LOCAL_MODEL_SMOKE = NOT_RUN_UNCONFIGURED`

## Frontières

`AgentRuntime`, possédé par `WorkspaceServer`, conserve seulement l’état borné
d’un turn en mémoire. Il ne possède ni SQLite, shell, chemin de fichier ou
adapter de recherche. Chaque `tool_call` passe par `AgentGateway`, donc par
capability → policy → service. Le runtime ne persiste aucun fait, observation,
relation ou conversation longue.

Les résultats injectés au modèle portent explicitement
`trust: "UNTRUSTED_DATA"`. Ils restent des données non fiables : une instruction
rencontrée dans une preuve ne change ni scope, grant, policy ni catalogue.

## Fournisseur local

`LocalModelClient` utilise seulement la stdlib Python et `POST
/v1/chat/completions`. Seuls `http://127.0.0.1:<port>` et
`http://[::1]:<port>` canoniques sont admis. DNS, LAN, `0.0.0.0`, HTTPS,
credentials, query, fragment, redirect et proxy ambiant sont refusés ; aucun
cookie, netrc ou authentification implicite n’est utilisé. Le port utilisateur
8080 n’est jamais sondé.

La configuration est explicite : `--agent-mode local-model`, `--agent-endpoint`,
`--agent-model`, `--agent-timeout`, ou `LABFY_AGENT_MODE`,
`LABFY_AGENT_MODEL_ENDPOINT`, `LABFY_AGENT_MODEL_ID`,
`LABFY_AGENT_MODEL_TIMEOUT_SECONDS`. Sans elle, le parcours reste **Agent de
démonstration déterministe** ; un endpoint défaillant est affiché indisponible,
sans bascule silencieuse vers fake ou cloud.

## Protocole, turns et sécurité

Le modèle produit uniquement un objet JSON du contrat
`labfy.agent_model_action.v1` : un `tool_call` avec `tool_id` et `arguments`, ou
un `final` avec `text`. Texte hors JSON, Markdown, champs en surplus, clés
dupliquées, outil inconnu, mauvais type et auto-autorisation sont refusés. Une
seule réparation de format est demandée ; le second échec est
`MODEL_PROTOCOL_ERROR` sans exécuter d’outil. Le prompt ne demande jamais de
chain-of-thought.

Les routes Host/session/Origin/CSRF protégées sont `GET
/api/v1/agent-runtime/status`, `POST /turns`, `GET /turns/id`, `POST
/turns/id/resume`, `POST /turns/id/cancel` et `GET /events?cursor=n`. Le
démarrage est idempotent. Un unique worker daemon, une file, les turns,
événements, rounds, appels outil, contextes et résultats ont des plafonds.
États : `QUEUED`, `RUNNING`, `AUTHORIZATION_REQUIRED`, `COMPLETED`, `FAILED`,
`CANCELLED`, `MODEL_UNAVAILABLE`, `MODEL_PROTOCOL_ERROR`, `BUDGET_EXHAUSTED`.

`research.prepare` stoppe la boucle sans contact réseau ou appel modèle
supplémentaire. La reprise du même turn exige un grant persistant actif dans le
workspace courant et relit `research.get_state` via le gateway ; le modèle ne
crée jamais le grant. L’annulation bloque tout effet suivant ; une requête déjà
en vol peut finir mais sa réponse est ignorée. L’arrêt du serveur ferme le
runtime.

## UI et validation

L’UI affiche le mode déterministe, `Agent local · <model>` ou une indisponibilité
courte. Les cartes/activités emploient `textContent`, jamais un HTML modèle ou
preuve ; elles ne montrent ni prompt complet ni raisonnement privé.
`object_refs` ne sélectionnent que des objets existants.

Les tests Python couvrent transport loopback, proxy/redirect, limites,
protocole strict, injection, budgets, pause/reprise, annulation, idempotence et
isolation. Le scénario Firefox démarre un faux fournisseur OpenAI-compatible,
un workspace et un profil temporaires `SPECIMEN` : outil, résultat, HTML hostile
inerte, annulation et JSON invalide. Aucun modèle n’est téléchargé ou démarré.
