# Contrat conceptuel du graphe d'investigation

> **Statut :** `TARGET`, à implémenter par tranches après J0
> **Invariant :** SQLite et les objets métier restent la source de vérité

## 1. Rôle

Le graphe représente le réseau complet d'une enquête et sert de point d'entrée
vers la provenance, les preuves, les hypothèses, les jobs et les pivots. Il ne
constitue ni une deuxième base ni une simple visualisation décorative.

```text
Investigation
  ├─ objets métier et identifiants
  ├─ preuves, artefacts et sources
  ├─ observations, claims et hypothèses
  ├─ événements et transactions
  └─ exécutions, transformations et décisions
```

Le dossier d'enquête fournit le contexte d'ensemble. Aucune personne unique
n'est imposée au centre.

## 2. Identité d'un nœud

Une projection de nœud expose au minimum :

```text
graph_node_id       stable dans l'investigation
investigation_id    frontière d'isolation
object_kind         famille métier
object_id           identité du modèle source
type                type contrôlé
label               présentation, jamais identité
state               état métier pertinent
confidence_summary  niveau et raisons disponibles
provenance_summary  accès aux sources
capabilities        instantané des actions applicables
revision            contrôle des mises à jour concurrentes
```

Personnes, organisations, e-mails, comptes, domaines, IP, documents, preuves,
sources, observations, événements et hypothèses peuvent être projetés. Cela ne
les convertit pas tous en `entites`.

## 3. Identité d'une arête

Une arête possède une identité stable, deux extrémités, un type, une direction
éventuelle, une période et sa précision éventuelles, la nature de l'assertion,
son état de revue et les observations justificatives. Plusieurs relations entre
les mêmes objets restent possibles.

Trois familles ne doivent pas être confondues :

- liens métier : propriété, communication, transaction ou autre relation ;
- liens de connaissance : support, contradiction ou similarité ;
- liens de provenance : source, dérivation, transformation ou exécution.

Une proximité graphique ou un chemin ne constitue pas une preuve d'identité.

## 4. Présentation sans mutation métier

Positions, coordonnées, nœuds de regroupement, filtres, sélection, zoom et état
replié sont des états de présentation. Les actions suivantes restent distinctes :

- masquer temporairement ;
- replier dans un groupe réversible ;
- retirer d'une vue enregistrée ;
- retirer une projection ;
- supprimer une donnée métier selon son propre contrat.

Déplier restitue les éléments. L'interface indique combien d'objets restent
masqués et permet de les atteindre.

## 5. Projections partagées

- **Identity :** personnes, organisations, alias, usernames, comptes, e-mails,
  téléphones et profils ;
- **Infrastructure :** domaines, hosts, IP, URL, certificats et services ;
- **Finance :** comptes, IBAN, BIC, wallets, transactions et flux ;
- **Evidence :** source → preuve → transformation → observation → claim ;
- **Timeline :** événements avec fuseau, précision et provenance ;
- **Geo :** lieux et coordonnées avec leur source et leur incertitude.

Ces vues utilisent les mêmes identités et relations. Elles ne créent ni silo ni
fusion implicite.

## 6. Exploration bornée

L'espace de travail prévoit vue globale, focus, sélection multiple, voisins
communs, chemins entre objets, filtres et retour au contexte précédent. Toute
recherche de chemins est bornée par profondeur, nombre de résultats, temps et
coût ; « tous les chemins pertinents » n'est pas une opération illimitée.

Le chargement est progressif et paginé. Les objets non affichés restent comptés.
Les nouvelles observations sont signalées sans imposer une réorganisation
permanente : positions épinglées, ajout différé et recentrage sont prévus.

## 7. Focus Workspace et commande

Sélectionner un objet recentre ou isole son voisinage et ouvre un panneau avec :

- relations, observations, preuves, événements et hypothèses ;
- jobs en cours ou passés ;
- pivots et capabilities applicables ;
- disponibilité, motif d'indisponibilité, contact réseau, coût et confirmation ;
- navigation « Why do we know this? » jusqu'au brut.

Le frontend demande les capabilities au backend. Une capability affichée est
un instantané : le backend revérifie disponibilité, scope, policy et budget lors
du déclenchement. Un bouton masqué ne constitue jamais un contrôle de sécurité.

Un menu radial, une barre contextuelle ou un hybride pourra minimiser le coût
d'interaction. Toute action possède aussi une commande visible et accessible au
clavier ; aucune fonction essentielle ne dépend du survol ou du clic droit.

## 8. Contrat d'événements à étudier en J2

Les événements futurs doivent permettre ordre déterministe, reconnexion,
détection d'un trou, snapshot de rattrapage, annulation et absence de double
publication visuelle. Le protocole, les endpoints et le transport restent
`UNDECIDED` et ne sont pas implémentés par J0.

## 9. Validation future

Les jeux synthétiques croîtront par paliers et mesureront délai de première vue,
latence de sélection, fluidité du focus, volume visible, mémoire et coût du
layout. Aucun objectif tel que « 10 000 nœuds fluides » n'est affirmé avant
mesure sur un matériel de référence.

Critères invariants : identité stable, absence de perte logique pendant
l'agrégation, contexte restaurable, chemins bornés, navigation clavier et
provenance accessible.
