# Provisionnement contrôlé d'outils V1

**État : CURRENT local sur données `SPECIMEN`, sous réserve des deux portes humaines.**

Le modèle peut rechercher un paquet Debian dans les métadonnées APT d'une image
Podman rootless et proposer une demande liée au workspace, à la mission et au
turn. Cette recherche ne lance pas le gestionnaire de paquets de l'hôte. La
source admise est le dépôt Debian configuré dans l'image de base épinglée ; une
URL, un script téléchargé, un dépôt tiers ou un installateur libre sont refusés.

La porte A est une décision humaine HTTP distincte du texte du modèle. Après
approbation, Labfy construit une nouvelle génération immuable hors de l'hôte,
puis vérifie dans la quarantaine les commandes `--version`, `--help`, l'état de
santé et les documents bornés. Un échec garde la génération inactive. Le modèle
peut lire les documents comme données non fiables et proposer un adapter
déclaratif et une capability, sans produire l'approbation.

La porte B active l'intégration après contrôle humain. La capability apparaît
dans le catalogue backend et ses applications de menu sont dérivées des types
d'objets vérifiés du graphe. Son exécution repasse par l'identité du workspace,
le scope de mission, la Policy et les budgets. L'UI ne fabrique ni outil ni
permission : elle présente le manifeste du backend et demande l'exécution via
`/api/v1/capabilities/execute`. Les deux décisions sont persistées avec leurs
identifiants et clés d'idempotence ; un rejeu divergent est refusé.

Le smoke réel `manual_tool_provisioning_smoke.py` utilise un workspace
`SPECIMEN`, une image Debian rootless et `jq` pour lire un nom d'utilisateur
synthétique. Il prouve la proposition de Qwen, les deux portes, la construction,
`capability.execute` et le final `COMPLETED`. Aucun accès réseau n'est accordé
au CLI provisionné pendant son exécution.

Voir [générations](TOOLBOX_GENERATIONS.md), [manifestes](CAPABILITY_MANIFESTS.md)
et [protocole Agent](AGENT_TOOL_PROTOCOL.md).
