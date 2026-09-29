# Générations rootless de la toolbox

**État : CURRENT local, éprouvé avec `SPECIMEN`.**

`ToolProvisioningService` conserve demandes, décisions, générations et
intégrations sous un répertoire XDG privé. `PodmanToolboxBuilder` produit une
image immuable par génération à partir d'une base Debian épinglée et de
versions de paquets résolues. Le manifeste de génération conserve image,
paquets, versions, santé, documents et SBOM. Une génération n'est publiée comme
active qu'après quarantaine réussie. Le rollback explicite choisit une
génération antérieure validée ; il ne modifie pas l'historique des décisions.

`ToolboxExec` lance uniquement un binaire déclaré avec argv structuré,
utilisateur rootless, réseau désactivé, lecture seule, limites de durée, de
mémoire et de sortie. Il ne monte ni HOME, ni dépôt source, ni bibliothèque
d'enquêtes. Les entrées/sorties passent par le contrat de l'adapter et restent
bornées. L'exécution échoue si Podman rootless ou l'image admise manque ; aucune
exécution hôte de secours n'est prévue.

L'image de validation `SPECIMEN` et les répertoires XDG temporaires sont
supprimés par les scripts de smoke. Les générations utiles d'une installation
locale restent dans le stockage XDG utilisateur, jamais dans Git.
