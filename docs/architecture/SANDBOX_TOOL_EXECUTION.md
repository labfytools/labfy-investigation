# Exécution d'outils sandboxée V1

**État : CURRENT local pour la frontière et ses tests `SPECIMEN`; disponibilité
d'exécution dépendante de `bwrap` et de la Policy.**

`ToolRegistry` porte une identité stable explicite, distincte du nom du binaire,
ses contrats d'entrée/sortie, sa classe de risque, disponibilité documentaire,
disponibilité détectée et admission Policy. La détection utilise uniquement
`argv` bornés (`--version`, puis `--help`); elle n'installe rien.

`sandbox.exec` n'accepte pas un shell ni une chaîne de commande. Les arguments
sont une liste structurée, l'exécutable est choisi dans le registre et les
artefacts sont fournis via un identifiant opaque, matérialisés en lecture seule
sous `/input`. Les sorties déclarées restent sous `/output` avec une provenance
bornée. NUL, chemins hôte, `~`, traversées, outil inconnu et sortie non déclarée
sont refusés.

La [toolbox Podman rootless](TOOLBOX_GENERATIONS.md) est une frontière
supplémentaire pour les outils provisionnés après Gate A/B. Elle ne remplace
pas le contrat `sandbox.exec` des outils déjà enregistrés : son adapter
déclaratif, son image immuable et l'exécution offline sont distincts.

L'exécution exige Bubblewrap rootless avec réseau isolé, environnement nettoyé,
répertoires système en lecture seule, `/proc` isolé, `/dev` minimal, timeout et
limite combinée de sortie. L'absence ou l'échec de `bwrap` retourne
`UNAVAILABLE`; il n'existe jamais de fallback en subprocess hôte libre.

Les documents locaux sont mis en cache XDG privé et borné comme
`UNTRUSTED_DATA`. Les lire, y compris pour un outil dual-use, ne le rend pas
exécutable et ne transforme aucune instruction documentée en permission.
