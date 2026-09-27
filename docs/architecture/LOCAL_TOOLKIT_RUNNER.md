# Toolkit local et runner borné — lot J4

> **État :** `CURRENT` local, synthétique, non publié\\
> **Runtime persistant :** V20 inchangé\\
> **Contrats de sortie :** `labfy.local_tool_result.v1`,
> `labfy.exiftool.metadata.derivative.v1`

## Parcours livré

Le registre statique expose deux capacités réellement consommées :
`labfy.capability.eml_headers.v1` (`NATIVE`) et
`labfy.capability.exif_metadata.v1` (`SUBPROCESS`). Le lanceur unique exécute
les deux sur des preuves SPECIMEN, publie en V20, ferme le writer, relit SQLite
en lecture seule et exporte le snapshot v3 :

```bash
make -j8 local-toolkit-demo
cd prototypes/web-graph
python3 run_local_toolkit_demo.py --port 8765
```

L'image reçoit ses métadonnées fictives avant import. L'adapter de production
n'accepte qu'un fichier contrôlé et les options fixes `-config "" -j -G1 -n --`.
Python ne fabrique ni résultat, ni disponibilité, ni métadonnée de secours.

## Registre et disponibilité

Chaque descripteur porte identités/version de capacité, adapter et outil,
intention française, types acceptés/produits, nature, classe `PASSIVE`, contact
`NONE`, profil de limites et version du contrat. Les états sont `available`,
`missing`, `incompatible`, `unverified` et `detection_error` avec raison.

La sonde ExifTool passe elle-même par le runner (3 s, 16 Kio par flux) et
désactive la configuration utilisateur. Le lookup n'analyse aucun fichier. La
projection reçoit un registre déjà sondé et ne lance jamais de processus. Le
snapshot distingue applicabilité et déclenchement : le prototype HTTP reste
read-only et affiche « Exécution par le lanceur local » sur un bouton désactivé.

## Runner POSIX borné

`local_tool_runner_run()` exige un exécutable absolu résolu, un argv séparé et
un répertoire privé. L'enfant crée une session/groupe propre, ferme stdin,
reçoit seulement `PATH=/usr/bin:/bin`, `LANG/LC_ALL=C.UTF-8` et
`HOME=/nonexistent`, puis applique `RLIMIT_CPU`, `RLIMIT_AS`, `RLIMIT_FSIZE` et
`RLIMIT_NOFILE`. Aucun shell, profil utilisateur, config personnelle ou fd
inutile ne survit à `execve`.

Le profil normal est 30 s, grâce 250 ms, CPU 30 s, espace d'adressage 256 Mio,
fichier 4 Mio, 64 fd, stdout 1 Mio et stderr 256 Kio. Le délai monotone couvre
processus et vidange des pipes. Timeout, annulation ou plafond envoient
`SIGTERM`, puis `SIGKILL` au seul groupe créé. Le processus directement possédé
est récupéré. Un descendant conservant le pipe est donc borné. Un programme
qui s'échappe volontairement de la session n'est pas confiné par ce mécanisme :
J4 ne prétend pas fournir une sandbox ni une limite mémoire agrégée par cgroup.

Les états distinguent sortie normale, non-zéro, signal, timeout, annulation,
plafond, absence et erreur I/O. stdout/stderr restent des `GBytes`; chaque flux
porte taille observée, taille conservée, complétude et SHA-256 du préfixe
réellement conservé. Un JSON incomplet n'est jamais parsé.

Le résultat appartient à l'appelant jusqu'à
`local_tool_runner_result_free()`. Après une erreur d'argument ou système,
aucun résultat partiel ne s'échappe, même avec `GError ** == NULL`.

## Adapter et persistance ExifTool

JSON-GLib parse strictement une racine tableau contenant exactement un objet,
avec au plus 2 048 tags et profondeur 16. Valeurs texte, nombres, booléens,
`null` et tableaux sont préservés ; les tags inconnus restent inspectables.
Une métadonnée n'atteste ni authenticité ni identité.

Le service recharge l'`EvidenceRecord`, refuse chemins absolus, `..` et liens
symboliques, vérifie taille/SHA-256, analyse une copie privée puis revérifie
l'original. L'artefact privé conserve provenance, paramètres expurgés, version,
stdout base64, hash/tailles/complétude et métadonnées typées. Le snapshot
n'expose aucun chemin absolu ni stderr complet ; il projette source → extraction
→ dérivé et un aperçu borné de champs sûrs avec la référence stable du dérivé.

Le `request_id` est la clé idempotente : un replay compatible ne relance pas
ExifTool. Une divergence est un conflit ; extraction sans dérivé cohérent est
`RECOVERY_REQUIRED`. Staging et transaction sont compensés en supprimant
seulement le fichier créé par la tentative. Aucun DDL, `osint_execution` fictif
ou nouvelle migration n'est introduit.

## Ajouter un adapter

1. Ajouter un descripteur stable au registre et un profil mesuré.
2. Implémenter l'adapter sans GTK/SQLite et sans arguments libres du Web.
3. Versionner et borner le format brut/normalisé ; conserver les octets.
4. Réutiliser le service de preuve contrôlée et les DAO V20, sans détournement.
5. Projeter seulement des données persistées, sans exécution dans le lecteur.
6. Ajouter fakes déterministes, outil réel optionnel et tests d'échec/replay.

## Limites et suite

Les anciens parcours documentaires n'utilisent pas tous ce runner et ne sont
pas déclarés protégés par J4. La provenance J3 reste à généraliser. J5 devra
ajouter jobs persistants, leases/reprise, Resource Governor, budgets partagés,
policy par tentative et arrêt global. J4 ne livre ni serveur de production,
secrets, autonomie réseau, contrôle d'un outil hostile ou reprise après crash.

`J4_LOCAL_TOOLKIT_RUNNER = COMPLETE_LOCAL`
