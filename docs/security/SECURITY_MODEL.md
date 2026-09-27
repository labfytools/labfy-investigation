# Sécurité, scope et contenus non fiables

> **Statut :** exigences `TARGET` à satisfaire avant exposition Web ou
> autonomie ; J0 ne livre ni serveur ni capacité intrusive

## 1. Local-first n'est pas une frontière de sécurité suffisante

La future surface Web écoute en loopback par défaut ; l'exposition LAN est
désactivée. Le serveur devra néanmoins appliquer authentification locale,
contrôle `Host` et `Origin`, protection CSRF et défense contre le DNS rebinding.
`127.0.0.1` seul ne protège ni des requêtes intersites ni d'un navigateur
compromis.

Le backend valide URL, redirections et résolutions, prévient SSRF et interdit
l'accès arbitraire aux fichiers de la machine. Les preuves ne sont jamais
exposées comme un répertoire HTTP.

## 2. Contenus non fiables

Noms de fichiers, HTML, SVG, PDF, images, archives et sorties d'outils sont
hostiles par défaut. Les previews appliquent échappement, isolation, CSP et
absence de scripts ou ressources distantes implicites. Imports, décompression,
dimensions, temps et mémoire sont bornés ; path traversal et liens symboliques
hors racine sont refusés.

Les artefacts téléchargés exigent contrôle d'accès par investigation et noms
sûrs. Aucun CDN, tracker, analytique, police distante, télémétrie ou service
d'IA externe n'est obligatoire.

## 3. Classes d'action

```text
PASSIVE | PUBLIC_ACTIVE | AUTHORIZED_INTRUSIVE | PROHIBITED
```

Le contact réseau est orthogonal : `NONE`, `THIRD_PARTY` ou `TARGET`. Les
effets sont déclarés séparément :

```text
READ_ONLY | STATE_CHANGING | DESTRUCTIVE | PERSISTENCE |
CREDENTIAL_ACCESS | DATA_EXTRACTION
```

Une information publique, une vulnérabilité détectée ou un soupçon ne crée pas
d'autorisation. `UNKNOWN` et `OUT_OF_SCOPE` restent visibles mais interdisent
toute action intrusive. Refus et raisons sont conservés.

Une future autorisation comporte référence, cibles et actions exactes, période,
opérateur et exclusions. Chaque tentative revérifie la policy. Sous-domaines,
IP résolues, redirections et hébergements mutualisés n'héritent jamais du scope.

v0.1.0 n'implémente ni exploitation automatique, ni persistance distante, ni
destruction, ni récupération d'identifiants, ni extraction de données privées.

## 4. Budgets et arrêt

Toute chaîne déclare profondeur, pivots, requêtes, durée, octets, coût et risque
maximaux. La concurrence est limitée par outil et service. L'arrêt global est
visible et l'annulation se propage aux descendants. Une limite, un timeout ou
un résultat vide n'est jamais une preuve d'absence.

`AUTONOMOUS_AUTHORIZED` demeure une tranche séparée avec policy, journal,
budget, arrêt immédiat et validation humaine des actions sensibles.

## 5. Secrets

Secret Service/libsecret reste une piste à évaluer ; aucune dépendance n'est
ajoutée en J0 et le cas sans session graphique reste `UNDECIDED`. SQLite métier
ne stocke que des références opaques, jamais les valeurs des clés API.

L'injection est contrôlée, les permissions minimales et l'expurgation
structurelle. Secrets absents des arguments affichés, logs, sorties, rapports,
exports, captures et Git. Une donnée sensible contenue dans une preuve n'est
pas un secret d'exécution : l'original reste immuable et une copie expurgée
distincte conserve la provenance de sa transformation.

## 6. Tiers et minimisation

Avant une requête réseau, l'interface indique quel tiers recevra quel type
d'identifiant ainsi que frais, quotas ou conditions connus. Les exports sont
sélectionnés et minimisés ; les investigations restent isolées. Aucun contrat
ne promet automatiquement conformité réglementaire ou recevabilité judiciaire.

## 7. Portes de validation futures

Avant production Web : modèle de menace, tests Host/Origin/CSRF/DNS rebinding,
SSRF/redirections, contrôles d'accès, path traversal/symlinks, archives et
fichiers volumineux, CSP/previews, reprise après crash, limites de ressources et
absence de ressource distante implicite doivent être validés sur données
synthétiques.
