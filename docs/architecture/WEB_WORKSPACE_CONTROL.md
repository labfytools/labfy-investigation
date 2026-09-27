# Poste de travail Web contrôlé — J6 local

> État : `CURRENT` pour le seul espace `SPECIMEN` produit par le lanceur J6.
> Runtime métier : V20 inchangé. JobStore : V1 inchangé. V21 reste indépendante.

## Parcours

Une commande prépare ou reprend un espace synthétique, lance l’API loopback et
son worker possédé, puis affiche l’URL et un code de session éphémère :

```sh
make -j8 tools/local-jobs
make web-workspace WORKSPACE=/tmp/labfy-j6-specimen
```

L’utilisateur ouvre l’URL, saisit le code, sélectionne l’EML ou l’image dans le
graphe et choisit « Analyser les en-têtes » ou « Examiner les métadonnées ».
L’intention est persistée avant la réponse HTTP `202`. Le worker C exécute le
service J3/J4 correspondant, puis C republie atomiquement les snapshots jobs et
métier. Le navigateur les relit sans rechargement de page. Pause, reprise, stop
et annulation sont disponibles dans le panneau Jobs.

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
`Path=/`, limité à une heure. Il n’a pas l’attribut `Secure`, puisque ce mode
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

Ce poste est une fondation locale synthétique : pas d’enquête réelle, LAN,
TLS, reverse proxy, service permanent, autonomie réseau, recherche OSINT,
worker distribué ou mutation de la V21. Les anciens modes J2–J5 restent
strictement read-only. `ThreadingHTTPServer` n’est pas présenté comme un serveur
Internet ou une frontière de sécurité de production.
