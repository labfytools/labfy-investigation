# ADR-0002 — Graphe comme interface principale

- **Statut :** accepté pour la cible
- **Date :** 2026-09-25
- **Portée :** contrat conceptuel ; aucun nouveau stockage dans J0

## Contexte

Une enquête relie des personnes, identifiants, infrastructures, preuves,
observations, événements, transactions et hypothèses. Des écrans isolés rendent
les rapprochements et leur provenance difficiles à comprendre.

## Décision

**THE GRAPH IS THE INVESTIGATION.** Le graphe devient la représentation et le
point de navigation principaux. Identity, Infrastructure, Finance, Evidence,
Timeline et Geo sont des projections du même réseau logique. SQLite et les
objets métier restent la source de vérité ; le renderer ne possède pas sa
propre vérité.

Une agrégation visuelle masque ou résume sans fusionner. Une proximité, un
chemin ou une arête de similarité ne prouve jamais une identité. Les liens
métier restent distincts des liens de support, contradiction et provenance.

## Conséquences

- nœuds et arêtes ont des identités durables indépendantes de leur position ;
- preuves, sources, observations et hypothèses peuvent être des nœuds de
  première classe sans devenir artificiellement des lignes de `entites` ;
- masquage, retrait d'une vue et suppression métier restent des opérations
  différentes ;
- chargement progressif, bornes de chemins et conservation du contexte sont
  nécessaires pour les grands graphes.

## Alternatives examinées

- **tableaux comme interface principale :** conservés comme vue accessible,
  mais insuffisants pour représenter le réseau ;
- **base graphe séparée :** rejetée à ce stade pour éviter deux vérités et une
  synchronisation fragile ;
- **personne unique au centre :** rejetée, une enquête peut contenir plusieurs
  personnes, identités candidates, preuves et pistes.

## Limites et révision

Le moteur de rendu et les objectifs chiffrés restent ouverts. Aucun volume ou
framerate n'est garanti avant mesures sur jeux synthétiques et matériel de
référence pendant J2/J6.
