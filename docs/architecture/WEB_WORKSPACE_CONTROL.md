# Poste de travail Web contrôlé — J6 local

> État : `CURRENT` pour le poste Web local contrôlé et sa bibliothèque
> explicitement choisie. Runtime métier : V20 inchangé. JobStore : V3 inchangé.
> V21 reste indépendante.

## Parcours

La commande principale lance l’API loopback sur une bibliothèque locale
explicitement choisie. Elle affiche l’URL et un code de session éphémère :

```sh
make web PORT=8081 LIBRARY=/chemin/vers/la-bibliotheque-locale
```

L'instance écoute sur `127.0.0.1:8081` dans cet exemple. Après authentification,
la bibliothèque permet de créer une enquête locale par le bridge C ou d'ouvrir
une enquête enregistrée. Une seule enquête peut être active par instance ; une
autre ouverture exige le redémarrage du poste local. L'utilisateur sélectionne
ensuite l’EML ou l’image dans le graphe et choisit « Analyser les en-têtes » ou
« Examiner les métadonnées ». L’intention est persistée avant la réponse HTTP
`202`. Le worker C exécute le service J3/J4 correspondant, puis C republie
atomiquement les snapshots jobs et métier. Le navigateur les relit sans
rechargement de page. Pause, reprise, stop et annulation sont disponibles dans
le panneau Jobs.

Le lanceur gère une seule instance authentifiée. Ses sous-commandes sont :

```sh
python3 prototypes/web-graph/web_app.py start --library /chemin/vers/la-bibliotheque-locale --port 8081
python3 prototypes/web-graph/web_app.py status
python3 prototypes/web-graph/web_app.py stop
python3 prototypes/web-graph/web_app.py code
```

`start` refuse une seconde instance active. `status`, `stop` et `code` valident
l'identité du processus et l'état de santé loopback avant d'agir ou de révéler
le code éphémère. Ne pas consigner ce code dans des scripts, URLs, journaux ou
documents. Le mode `--workspace` reste le parcours ciblé existant ; il ne crée
pas une bibliothèque implicite.

## Bibliothèque, état XDG et limites

La bibliothèque est privée (`0700`), refuse les liens symboliques et conserve
un registre atomique borné à 256 enquêtes. Chaque enquête est un enfant direct
identifié par UUID ; le bridge C existant est le seul créateur de son runtime
V20 et de son JobStore V3. La création est idempotente pour une même clé et le
même titre ; réemployer la clé avec un autre titre est refusé. L'ouverture exige
la génération courante du registre afin de ne pas utiliser une projection
périmée.

L'instance place ses verrous, identité et code d'amorçage dans
`$XDG_RUNTIME_DIR/labfy-investigation-web/`, ou dans un repli privé par UID
sous `/tmp` si `XDG_RUNTIME_DIR` n'est pas défini. Sa configuration durable est
dans `$XDG_STATE_HOME/labfy-investigation-web/` (ou le repli XDG usuel). Ces
fichiers ne sont ni des données métier, ni une bibliothèque : la destination
des enquêtes reste l'argument explicite `LIBRARY`.

La session a une durée maximale de `3600` secondes (une heure) à compter de chaque connexion
réussie, indépendamment de l'heure de démarrage du serveur. Après expiration,
le code de l'instance permet une nouvelle authentification : les secrets de
session et CSRF sont renouvelés et l'ancien cookie reste invalide. Un code
incorrect ou une origine refusée ne renouvelle jamais cette durée.
Le premier refus `session_required` reçu par le navigateur arrête ses flux et
pollings, invalide les secrets seulement en mémoire et présente une reconnexion
explicite. Il ne redirige pas en boucle et ne rejoue ni une mutation refusée ni
une admission dont l'issue est inconnue. Les brouillons autorisés restent dans
la session du navigateur, tandis que les jobs déjà admis conservent leurs
budgets et leur cycle de vie propres.
Les limites HTTP et opérationnelles
restent opposables : corps JSON de commande de 4 Kio, huit fichiers sélectionnés
pour 16 Mio, 4 Mio par réception, 64 Mio de staging, deux réceptions actives et
deux générations de rapport simultanées. Une limite, une annulation ou une
erreur d'export est un état explicite et ne prouve jamais l'absence de résultat.

## Décision HTTP et bridge

Deux solutions ont été comparées : GNU libmicrohttpd aurait ajouté une
dépendance C et son cycle d’événements à ce prototype ; `http.server` est livré
avec Python mais sa documentation officielle précise qu’il ne convient pas à
la production. J6 retient donc une passerelle Python dédiée et bornée, fondée
sur `ThreadingHTTPServer`, uniquement en loopback et explicitement non destinée
à la production. Elle ajoute toutes les validations nécessaires au périmètre
local au lieu de promouvoir le serveur read-only historique.

Références :

- <https://docs.python.org/3/library/http.server.html>
- <https://www.gnu.org/software/libmicrohttpd/manual/libmicrohttpd.html>
- <https://cheatsheetseries.owasp.org/cheatsheets/Cross-Site_Request_Forgery_Prevention_Cheat_Sheet.html>
- <https://www.sqlite.org/isolation.html>

Python n’ouvre ni `Enquete.sqlite` ni `jobs.sqlite`, ne choisit aucun exécutable
et ne lance jamais ExifTool directement. Il appelle `tools/local-jobs` avec une
liste blanche d’opérations et des argv construits par le serveur. Le stdout JSON
machine utilise `labfy.local_jobs.command.v1`; les diagnostics sont sur stderr.
Les analyses longues s’exécutent dans un subprocess hors de la requête HTTP.

Le service C recharge la preuve et son enquête, vérifie son type, son intégrité,
l’applicabilité et la disponibilité du registre, fixe adapter et paramètres,
puis écrit dans le JobStore. Seuls `labfy.capability.eml_headers.v1` et
`labfy.capability.exif_metadata.v1` sont admis. Aucun chemin, SQL, URL, argv,
exécutable, environnement ou paramètre d’outil n’est accepté depuis HTTP.

## Session et sécurité locale

Le serveur écoute exclusivement sur l’autorité annoncée
`127.0.0.1:<port>`. Le code d’amorçage et les secrets de session proviennent de
`secrets`, ne sont pas placés dans une URL et ne sont pas journalisés par les
routes. Après amorçage, le cookie est host-only, `HttpOnly`, `SameSite=Strict`,
`Path=/`, avec `Max-Age=3600` secondes (une heure). Il n’a pas l’attribut `Secure`, puisque ce mode
testé utilise HTTP loopback et ne revendique pas TLS.

Lectures opérationnelles et mutations exigent la session. Les mutations exigent
en plus l’`Origin` exact et `X-Labfy-CSRF`. Elles utilisent exclusivement
`POST application/json`, des ensembles de champs exacts et des corps limités à
4 Kio. Host étranger, Origin absent/étranger/null, CSRF absent, type simple,
JSON malformé, champ inconnu, route GET de mutation et capability inconnue sont
refusés. CSP, `nosniff`, `no-store`, `frame-ancestors none`, `X-Frame-Options`
et rendu DOM par `textContent` restent actifs. Il n’existe ni CORS, ni route de
fichier arbitraire, ni fetch distant, ni exposition LAN.

## Idempotence, contrôle et reprise

Le navigateur conserve l’UUID d’intention en `sessionStorage` uniquement tant
que l’admission n’est pas connue. Ce n’est pas un secret. Le cœur dérive de
façon déterministe les UUID de requête et de dérivé ; la contrainte JobStore
rend l’admission transactionnellement idempotente. Même clé et même intention
retrouvent le job ; une autre preuve ou capability produit un conflit. Une
réanalyse volontaire après succès reçoit une nouvelle clé et ne remplace aucun
résultat.

La pause empêche les nouveaux claims. `resume` lève explicitement pause et stop.
Le stop empêche les départs et termine le groupe worker possédé. L’annulation
persistée d’une attente produit `CANCELLED`. Pour un worker actif, le serveur
arrête uniquement son groupe de session puis relance la réconciliation. Au
redémarrage, le cœur inspecte d’abord l’extraction : publication typée présente
→ acquittement sans outil ; absence + annulation → `CANCELLED` ; absence sans
ambiguïté → nouvelle tentative bornée ; incohérence → `RECOVERY_REQUIRED`.
Une simple vérification de replay ne lance donc plus une analyse cachée dans
l’ancienne tentative.

L’EML synchrone reste coopératif aux points définis par son service. Le worker
intercepte l’arrêt et demande au runner de terminer puis récolter la session
isolée d’ExifTool avant de libérer son verrou ; worker et outil ne partagent donc
pas un groupe de processus. J6 ne prétend toujours
pas fournir une sandbox hostile ou une garantie de coupure électrique.

## Contrats de lecture et cohérence

- graphe : `labfy.web_graph.snapshot.v3` ;
- jobs : `labfy.local_jobs.snapshot.v1` ;
- session : `labfy.workspace.session.v1` ;
- contrôle : `labfy.workspace.control.v1` ;
- erreur : `labfy.workspace.error.v1`.

Le polling est borné et non chevauchant : 500 ms pour les jobs, 700 ms pour le
graphe. Chaque publication métier réussie déclenche un export du graphe même si
un autre job échoue. Un export indisponible laisse la dernière vue valide avec
un diagnostic ; il ne change jamais l’état métier du job. Sélection, focus,
zoom, filtres, repli et positions épinglées restent dans l’état du navigateur.

## Limites

Ce poste est une fondation locale contrôlée : les validations automatisées
emploient exclusivement des fixtures `SPECIMEN`. Il n'autorise ni enquête
réelle, ni LAN, TLS, reverse proxy, service permanent, autonomie réseau,
recherche OSINT, worker distribué ou mutation de la V21. Les anciens modes
J2–J5 restent strictement read-only. `ThreadingHTTPServer` n’est pas présenté
comme un serveur Internet ou une frontière de sécurité de production.

## Validation Web

La cible `make web-workspace-web-check` construit d'abord les bridges C requis,
puis exécute les unités Node, la découverte Python et les parcours navigateur
séquentiels. Le runner navigateur classe chaque scénario avec l’un des statuts
`MISSING`, `LAUNCH_ERROR`, `EXIT_CODE`, `SIGNALED`, `TIMEOUT`, `INTERRUPTED`
ou `SUCCESS`. Sur Linux, il crée et possède le groupe et la session de
processus de chaque scénario ; son nettoyage est limité à ce groupe/session et
envoie `SIGTERM`, attend une grâce bornée, puis escalade avec `SIGKILL` si
nécessaire. Il ne recherche ni ne termine des processus utilisateur.
Les régressions emploient un Firefox, des profils, ports, XDG et bibliothèques
`SPECIMEN` isolés, notamment pour la connexion tardive, l'expiration et la
reconnexion sans redémarrer le serveur. Elles ne constituent pas à elles seules
une certification de sécurité, de production, de navigateur ou de données non
synthétiques.
