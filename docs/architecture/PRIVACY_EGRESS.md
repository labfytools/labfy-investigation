# Egress Internet confidentialité V1

**État : `CURRENT local` pour le contact passif via Tor rootless.** Le smoke
Tor live et le smoke Qwen opérationnel unifié ont réussi sur `SPECIMEN` avec
`TOR_EGRESS_VERIFIED`, `direct_fallback: false` et nettoyage du conteneur possédé.

Le profil produit est `PRIVACY_TOR`, jamais « anonymous guaranteed ». Tor réduit
l'exposition de l'adresse IP source mais ne garantit ni anonymat, ni innocuité du
contenu distant. Les pages, snippets, redirections et résultats restent
`UNTRUSTED_DATA` et ne créent ni fait validé, ni observation, ni autorisation.

## Runtime possédé

`PrivacyEgressSupervisor.start_owned_tor()` démarre un `PodmanTorRuntime` et en
possède le cycle de vie. Les états sont `STOPPED`, `STARTING`, `BOOTSTRAPPING`,
`READY` et `FAILED`. `READY` n'est atteint qu'après :

1. preuve que Podman fonctionne en mode rootless ;
2. démarrage d'un conteneur portant un nom aléatoire et un label de propriété ;
3. message Tor `Bootstrapped 100% (done)` ;
4. découverte d'un port SOCKS éphémère publié exactement sur `127.0.0.1` ;
5. requête bornée vers `https://check.torproject.org/api/ip` par SOCKS5h et
   réponse `IsTor: true`.

L'adresse éventuellement contenue dans cette réponse de contrôle n'est ni
retournée, ni journalisée, ni placée dans la provenance. Une erreur à l'une de
ces étapes produit `FAILED`; aucun transport direct n'est alors créé.

Le nettoyage inspecte le label avant `podman rm --force`. Un conteneur absent ou
portant un autre label n'est jamais supprimé. Le runtime ne recherche et ne
nettoie aucun conteneur par préfixe global.

## Image et confinement

L'image locale `localhost/labfy-privacy-tor:1` est construite depuis
`docker.io/library/debian:12.12-slim`, base officielle versionnée, par
`prototypes/web-graph/privacy-egress/Containerfile`. Elle installe la version
Debian explicitement fixée de Tor et s'exécute sous `debian-tor`, avec UID/GID
non-root déterministes `10001:10001`.

Le lancement n'utilise ni réseau hôte, ni mode privilégié, ni montage hôte. Il
applique un filesystem racine en lecture seule, `cap-drop ALL`,
`no-new-privileges`, des limites mémoire/PID et seulement deux `tmpfs` privés
bornés pour `/run/tor` et `/var/lib/tor`. Le SOCKS interne écoute dans le réseau
du conteneur ; Podman publie `127.0.0.1::9050`, donc choisit un port hôte
éphémère. La configuration Tor active `SafeSocks`, `TestSocks`,
`ClientRejectInternalAddresses` et `ClientDNSRejectInternalAddresses`.

## Transport et refus

`CurlSocksTransport` utilise exclusivement `socks5h://127.0.0.1:<port>` : la
résolution DNS a lieu via Tor et ne fuit pas vers le résolveur de l'hôte. Il
limite les protocoles à HTTP(S), désactive les redirections automatiques de
curl, borne connexion, durée, en-têtes et corps, et ne lance jamais de shell.
Chaque redirection est revalidée par `PrivacyEgressSupervisor` avant une nouvelle
requête.

Seuls `GET` et `HEAD` sont permis. Sont refusés : userinfo, fragments, schémas
non HTTP, localhost, loopback, RFC1918, link-local, multicast, réservé, non
spécifié et noms metadata connus. Tor refuse en plus les résolutions distantes
vers les adresses internes. Une réponse trop grande est supprimée et rend
`RESPONSE_LIMIT`. Tor absent, arrêté ou non validé rend
`PRIVACY_EGRESS_UNAVAILABLE` ou un échec de transport avec
`direct_fallback: false`.

`InternetResearch.search()` refuse également d'appeler un provider lorsque
l'egress `PRIVACY_TOR` est indisponible. Le provider reçoit le superviseur
Privacy comme dépendance obligatoire ; son contrat interdit tout transport
réseau parallèle.

La provenance `labfy.privacy_egress.v1` contient mode, raison et chaîne de
redirections bornée. Pour SOCKS5h, le pair est noté `privacy-proxy`; aucune
adresse de sortie Tor n'est exposée.

## Validation

Les tests unitaires utilisent seulement des fakes `SPECIMEN` et couvrent la
propriété, la commande Podman, l'indisponibilité sans fallback, SSRF,
redirections et dépassement de taille :

```bash
python3 -m unittest prototypes/web-graph/tests/test_privacy_egress.py \
  prototypes/web-graph/tests/test_privacy_tor_runtime.py \
  prototypes/web-graph/tests/test_internet_research.py
```

Le smoke réel construit l'image, contacte uniquement les endpoints bénins
ci-dessus et ferme dans `finally` seulement le conteneur possédé :

```bash
python3 prototypes/web-graph/tests/manual_privacy_tor_smoke.py
```

`--skip-build` réutilise explicitement l'image locale. Un échec Podman, build,
réseau ou bootstrap reste un échec ; il ne doit jamais être transformé en PASS.
