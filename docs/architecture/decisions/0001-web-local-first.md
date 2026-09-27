# ADR-0001 — Interface Web local-first

- **Statut :** accepté pour la cible
- **Date :** 2026-09-25
- **Portée :** direction v0.1.0 ; aucune implémentation Web dans J0

## Contexte

GTK4 fournit l'interface `CURRENT`. La cible doit rendre le graphe, les vues
spécialisées, les actions contextuelles et la provenance accessibles dans un
espace de travail cohérent, sans transformer Web en dépendance cloud.

## Décision

L'interface principale `TARGET` sera Web et locale. Le cœur reste l'autorité
pour les données, les mutations, le scope, les capabilities et les budgets.
SQLite, preuves, artefacts et rapports restent locaux par défaut. GTK coexiste
jusqu'à ce qu'un remplacement validé couvre les parcours nécessaires.

## Conséquences

- la future API écoute en loopback par défaut et applique authentification,
  contrôle `Host`/`Origin`, protection CSRF et défense contre le DNS rebinding ;
- le frontend ne lit ni SQLite ni les preuves directement ;
- les contrats API et événements sont versionnés et permettent reconnexion,
  rattrapage après un trou et contrôle de révision ;
- le packaging et la maintenance d'une chaîne frontend deviennent des coûts à
  mesurer avant choix.

## Alternatives examinées

- **GTK seul :** préserve l'existant mais ne porte pas à lui seul la direction
  d'espace de travail Web retenue ;
- **application cloud :** rejetée comme dépendance obligatoire, contraire au
  contrat local-first ;
- **réécriture immédiate :** rejetée, car elle mettrait en risque les parcours
  fonctionnels et les services utiles.

## Décisions encore ouvertes

Framework, moteur de graphe, serveur HTTP, SSE/WebSocket/polling et packaging
restent `UNDECIDED` jusqu'à comparaison et prototype synthétique J2.

## Révision

Cette décision peut être réexaminée si un prototype démontre une impossibilité
de satisfaire simultanément sécurité locale, accessibilité, performances et
distribution, avec mesures reproductibles à l'appui.
