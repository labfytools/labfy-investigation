# Design system cible

> **Statut :** direction `TARGET`, avec un poste Web local `CURRENT` sur fixtures
> `SPECIMEN` documenté dans [WEB_WORKBENCH.md](WEB_WORKBENCH.md).

## 1. Identité

L'interface cible utilise Catppuccin Mocha avec Lavender comme accent
principal. Les couleurs deviennent des tokens sémantiques, jamais des valeurs
éparpillées dans les composants.

| Token | Valeur initiale | Usage |
|---|---:|---|
| `surface.base` | `#1e1e2e` | fond principal |
| `surface.panel` | `#181825` | panneaux et focus workspace |
| `surface.raised` | `#313244` | cartes, menus et sélection |
| `accent.primary` | `#b4befe` | action principale et focus |
| `text.primary` | `#cdd6f4` | contenu principal |
| `text.muted` | `#a6adc8` | métadonnées |
| `state.success` | `#a6e3a1` | succès, avec icône et texte |
| `state.warning` | `#f9e2af` | avertissement, avec libellé |
| `state.danger` | `#f38ba8` | refus ou erreur, avec explication |
| `relation.info` | `#89b4fa` | information/provenance |

Les valeurs seront contrôlées avec les composants réels. La couleur seule ne
porte jamais type, confiance, disponibilité ou erreur.

## 2. Espace de travail

La cible est une interface de travail, pas une collection de tableaux :

- graphe central et retour immédiat au contexte global ;
- barre compacte avec recherche universelle ;
- panneau contextuel Focus Workspace ;
- palette de commandes ;
- indicateur de jobs, progression honnête et arrêt global visible ;
- vue liste/table accessible utilisant les mêmes objets.

Les nouveautés sont mises en évidence sans déplacer continuellement les nœuds.
Les positions épinglées et la sélection sont conservées.

Le renderer `CURRENT` classe les preuves en rectangles, les extractions en
losanges, les observations en capsules et les entités en cercles à partir de
`object_kind`. Une disposition déterministe par étages et colonnes rend les
directions lisibles sur le réseau alimenté ; les libellés suivent le zoom et
restent accessibles par sélection, voisinage, liste et recherche.

## 3. Actions et pivots

Le poste local emploie un menu contextuel ancré, complété par un onglet
`Actions`. Le critère reste le coût d'interaction minimal, sous réserve de ces
invariants :

- alternative linéaire visible et navigation complète au clavier ;
- aucune action essentielle réservée au survol ou au clic droit ;
- intention, disponibilité et motif d'indisponibilité lisibles ;
- contact réseau, coût éventuel et confirmation annoncés avant exécution ;
- backend toujours autoritaire pour scope, policy et budget.

Le tiroir réduit ne conserve que sa barre d’accès et libère sa rangée dans la
grille. Le viewport SVG est recalculé après ce redimensionnement.

## 4. Lecture novice et expert

Le novice voit des intentions métier et un parcours guidé : créer une enquête
synthétique, ajouter un élément, comprendre son type, voir les pivots, lancer
une action autorisée, examiner ses sources, revoir une hypothèse et préparer un
rapport.

L'expert déroule sans changer de vérité métier : outil et version, adapter,
paramètres expurgés, tentatives, brut, normalisation, transformation, règle de
corrélation et provenance.

## 5. Composants et états

Chaque composant prévoit chargement, vide, indisponible, résultat partiel,
erreur, annulation et contenu obsolète. Aucun pourcentage n'est inventé quand
l'outil ne fournit pas de mesure.

Les nœuds, arêtes, tableaux, badges de confiance et états de job partagent des
libellés contrôlés. L'interface rend visibles les objets masqués ou agrégés.

## 6. Accessibilité

- ordre de focus cohérent et focus visible ;
- commandes clavier documentées ;
- contrastes vérifiés sur les composants réels ;
- réduction des animations respectée ;
- informations disponibles autrement que par couleur, forme ou mouvement ;
- zoom, tailles de texte et petites largeurs testés ;
- vue liste/table équivalente pour l'exploitation des objets.

## 7. Critères futurs observables

Les prototypes synthétiques mesureront délai de première vue, latence de
sélection, fluidité du focus, mémoire, volume visible et coût du layout. Ils
testeront clavier, contraste, réduction des animations, états d'erreur et
écrans de tailles pertinentes. J0 ne revendique aucune performance mesurée.
