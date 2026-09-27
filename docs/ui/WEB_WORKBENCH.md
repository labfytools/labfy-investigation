# Poste Web local — guide d’utilisation

> **Statut CURRENT local :** poste utilisable au-dessus des services C et du
> JobStore V3 existants, depuis une bibliothèque d'enquêtes explicitement
> choisie. Les validations emploient des fixtures `SPECIMEN`. Il ne constitue
> ni une autorisation d’ouvrir une enquête réelle, ni un service distant.

## Organisation

À l'ouverture, la bibliothèque affiche les enquêtes locales enregistrées et
permet d'en créer une avec un titre explicite. Une enquête ouverte reste la
seule enquête active de cette instance ; le poste n'en restaure pas une autre
automatiquement. Le titre et la génération de la bibliothèque identifient le
contexte affiché, afin qu'une réponse tardive d'une enquête précédente ne le
repeuple pas.

Le graphe reste la surface principale. La barre haute porte l’identité de
l’enquête, le statut local et la recherche. La rangée suivante change de
projection (`Réseau`, `Preuves`, `Chronologie`, `Infrastructure locale`) sans
créer une seconde vérité métier.

L’inspecteur droit sépare `Détails`, `Actions` et `Provenance`. Les détails
exposent d’abord le libellé et l’état, puis les identifiants et valeurs
persistées. Les actions sont celles du registre de capabilities du cœur : leur
motif, leur contact réseau et leur disponibilité ne sont pas recodés dans le
frontend.

Le tiroir inférieur sépare les tâches, les prochaines actions, les plans et les
rapports. Il peut être réduit pour rendre de l’espace au graphe. Les jobs et les
plans sont relus depuis le JobStore ; un rafraîchissement ne les relance pas. Le
brouillon du rapport (sélection, sections, titre et commentaire) est conservé
dans la session du navigateur lors d’une actualisation. Sa clé comprend le
contrat de session, l’origine locale et l’identifiant d’enquête : il n’est pas
repris automatiquement dans une autre enquête.

## Réseau alimenté et classification

La famille visuelle dépend de `object_kind`, jamais du seul sous-type : entité
en cercle, preuve en rectangle, extraction en losange et observation en
capsule. Les snapshots historiques sans `object_kind` passent par un fallback
explicite sur `EVIDENCE`, `SOURCE`, `EXTRACTION` et `OBSERVATION`.

Le placement est un état de présentation déterministe par étages preuve →
extraction → observation → entité, découpés en colonnes bornées. Il ne crée,
ne fusionne et ne retire aucun objet ou lien. Au niveau global, les libellés
sont réduits pour laisser lire les directions ; sélection, voisins, recherche,
liste et clavier donnent accès au détail. Les positions déjà affichées et
épinglées sont conservées à l’arrivée de résultats. `Réinitialiser` recalcule
et ajuste explicitement la disposition complète.

## Parcours clavier et contextuel

- flèches dans le graphe : parcourir les objets ;
- `Entrée` : sélectionner l’objet courant ;
- `Menu` ou `Maj+F10` : ouvrir les actions près de l’objet ;
- `Échap` : fermer ce menu et rendre le focus au graphe ;
- onglet `Actions` : alternative visible au clic droit ;
- `Retour` : restaurer le contexte précédent après un focus de voisinage.

Le menu est borné au viewport. Les formes distinguent entités et preuves ; les
styles de traits distinguent provenance, appui et contradiction. La couleur
n’est jamais le seul porteur de sens.

`Réseau` quitte une projection spécialisée, efface filtres et focus et revient
à la vue globale. `Retour` restaure sélection, projection, filtres, focus, zoom
et déplacement. Une origine ouverte depuis `Provenance` crée également un
retour vers l’objet de départ.

## Rapport hors ligne

Depuis `Rapports`, sélectionner explicitement les objets, les sections et le
commentaire humain, puis prévisualiser la coupe révisionnée. La génération
produit cinq téléchargements authentifiés : `report.html`, `report.json`,
`report.pdf`, `manifest.json` et `NOTICE.txt`. Le HTML reçu s’ouvre hors ligne,
sans script ni ressource distante. Le manifeste permet de vérifier taille et
SHA-256 des quatre fichiers de contenu avec :

```bash
python3 prototypes/web-graph/report_bundle.py verify /chemin/vers/<report_id>
```

Toute modification du titre, du commentaire, des sections ou de la sélection
invalide immédiatement l’aperçu. `Générer` reste désactivé jusqu’à une nouvelle
coupe C et une réponse devenue périmée pendant la saisie est ignorée.

Une date avec fuseau à la minute reste positionnable et conserve sa précision
(`2026-09-26T12:05Z`, `2026-09-26T14:05+02:00`). Le rapport matérialise le début
de cette minute pour l’instant normalisé, sans prétendre connaître les secondes.
Une date sans fuseau ou invalide reste non positionnable.

## Lancement et limites

Lancer le poste local sur une bibliothèque explicitement choisie :

```bash
make web PORT=8081 LIBRARY=/chemin/vers/la-bibliotheque-locale
```

L’écoute reste loopback, la session est authentifiée et les mutations exigent
le jeton CSRF. Le poste n’expose pas le répertoire des preuves et n’installe
aucun outil. Les projections vides, erreurs, annulations et limites sont des
états explicites ; elles ne prouvent jamais une absence d’information.

Pour les validations, utiliser seulement une bibliothèque temporaire et des
enquêtes `SPECIMEN`. Les commandes d'exploitation `start`, `status`, `stop` et
`code`, les répertoires XDG privés et les plafonds de réception sont documentés
dans [le contrat du poste Web](../architecture/WEB_WORKSPACE_CONTROL.md).
