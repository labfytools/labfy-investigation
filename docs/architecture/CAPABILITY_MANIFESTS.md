# Adapters et manifestes de capability dynamiques V1

**État : CURRENT local sur `SPECIMEN`.**

Une proposition d'intégration fixe `tool_id`, binaire admis, version de
contrat, schéma d'entrée/sortie et tokens argv déclaratifs. Le validateur
rejette les champs inconnus, les chemins hôte, les arguments libres et les
types hors schéma. Une proposition ne devient exécutable qu'après Gate B.

Le manifeste de capability décrit son identifiant, la classe de risque, les
types d'objets compatibles, la disponibilité et les applications contextuelles.
`WorkspaceServer` associe ces applications uniquement aux nœuds effectivement
présents dans le snapshot du workspace actif. Le menu Web utilise les données
du manifeste ; aucun JS spécifique à `jq` n'est nécessaire. L'identifiant et
les paramètres renvoyés par l'UI sont revérifiés côté backend avant
`capability.execute`.

L'exemple `jq.username.v1` est une donnée d'intégration backend bornée pour
la démonstration `SPECIMEN`. Il extrait une valeur de pseudo synthétique en
offline. Il ne constitue ni une autorisation de chercher une personne réelle
ni un mécanisme permettant au modèle de choisir librement argv ou réseau.

Une capability dynamique ne modifie pas le comportement d'une capability
intégrée. Lorsque le changement souhaité porte sur ce comportement ou son
libellé produit, le parcours de [self integration](QWEN_SELF_INTEGRATION.md)
peut proposer une modification du code sous contrôle humain.
