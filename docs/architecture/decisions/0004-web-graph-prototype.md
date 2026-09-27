# ADR-0004 — Pile du prototype Web Graph J2

- **Statut :** acceptée pour le prototype, révisable avant J6
- **Date :** 2026-09-25
- **Portée :** démonstration synthétique en lecture seule

## Comparaison courte

Sources primaires consultées le 2026-09-25.

| Axe | Options évaluées | Décision et raison |
|---|---|---|
| Frontend | JavaScript natif ; Preact ; Lit | JavaScript natif : aucune chaîne ni dépendance d'exécution pour cette expérience courte. `puppeteer-core` est une dépendance de test locale et verrouillée, pas une dépendance frontend. Preact/Lit restent candidats si les composants J6 justifient leur coût. |
| Graphe | SVG natif ; Cytoscape.js 3.34.x ; Sigma.js 3 / Graphology, avec Sigma 4 encore alpha | SVG natif pour éprouver contrats, clavier et interactions sur le petit scénario. Cytoscape.js (MIT, rendu/analyse intégrés) est le candidat généraliste principal de J6 ; Sigma/Graphology (MIT, WebGL, séparation renderer/structure) mérite le benchmark grands graphes. |
| Serveur/API | Python stdlib de fixtures ; serveur C/GLib ; serveur JavaScript | Python stdlib uniquement pour le prototype : zéro paquet, isolation nette. La cible de production reste une API locale étroite devant les services C ; la bibliothèque HTTP sera choisie après prototype et modèle de menace. |
| Événements | SSE ; WebSocket ; polling | SSE : flux serveur→client natif, identifiants et reconnexion, suffisant ici. WebSocket est inutile sans bidirectionnel ; polling ajoute latence/requêtes. |
| Construction | fichiers natifs ; Vite/npm ; intégration Make | fichiers natifs isolés, sans toucher au build GTK. Une construction frontend verrouillée sera décidée avant J6. |

Références : documentation officielle
[Cytoscape.js](https://js.cytoscape.org/),
[Sigma.js](https://www.sigmajs.org/docs/),
[Graphology](https://graphology.github.io/),
[SSE/WHATWG](https://html.spec.whatwg.org/multipage/server-sent-events.html) et
[`http.server`](https://docs.python.org/3/library/http.server.html).

## Frontière retenue

Le frontend consomme des snapshots et événements versionnés. Le backend valide
identités, accès, policy, capacités et mutations ; le frontend conserve seulement
l'état de présentation (viewport, filtres, groupes, positions épinglées). Le
prototype ne lit pas SQLite et ne recrée aucune règle métier.

Les contrats expérimentaux sont `snapshot-v1.schema.json` et
`event-v1.schema.json`. Le snapshot sépare catalogue de capabilities et
applicabilité par `node_id` + `object_ref` ; le frontend ne déduit pas une action
du type visuel. Le scénario d'événements porte `base_revision`, `revision` et
des patches de nœud bornés. Un trou de révision ou un curseur inconnu impose le
snapshot serveur courant ; doublon/ancien est ignoré et la fin normale ferme le
client. Les identifiants viennent du scénario serveur, mais aucune garantie
exactly-once réseau n'est annoncée.

Le frontend conserve séparément sélection, focus, historique, filtres, viewport,
groupes repliés et positions épinglées. Une projection unique alimente rendu,
liste, compteurs, clavier et opérations de focus. Les arêtes gardent leur identité,
leur nature, leur direction et un décalage déterministe pour les liens parallèles.
La provenance effectue un parcours multi-origines borné qui signale cycle et
source manquante au lieu de choisir arbitrairement le premier antécédent.

## Validation de consolidation

La consolidation locale du 25 septembre 2026 a exécuté les tests de contrats
Python, la logique Node et un scénario Puppeteer dans Firefox isolé. Celui-ci a
piloté drag multi-mouvements, relâchement extérieur, annulation de pointeur,
pan/zoom, clavier sur projection vide, focus/retour, repli, actions par objet,
provenance multiple et reconnexions SSE, avec conservation du contexte et sans
exception JavaScript. Des rendus large, étroit/vide et erreur ont été inspectés.
Le relevé reproductible et les versions observées sont dans le README du
prototype ; cette validation ne qualifie ni un moteur 5 000 nœuds ni un backend
de production.

## Migration GTK vers Web

GTK reste fonctionnel. J3 doit extraire des services/projections typés ; J4/J5
ajoutent capabilities, runner et jobs ; J6 remplace progressivement des parcours
seulement après équivalence testée. Aucune vue ne lit directement SQLite.

## Sécurité et révision

Le serveur expérimental n'est pas le serveur de production. Il lie
`127.0.0.1`, sert une racine fixe, refuse mutations et Host/Origin inattendus,
et applique CSP. Authentification locale, TLS éventuel, CSRF de mutations, DNS
rebinding complet et packaging restent des portes J2/J6. Revoir cette ADR après
benchmarks renderer 1 000/5 000 nœuds et choix du serveur local de production ;
la validation clavier du petit scénario est désormais démontrée.
