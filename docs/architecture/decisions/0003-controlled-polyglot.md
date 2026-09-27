# ADR-0003 — Réutilisation et polyglottisme contrôlé

- **Statut :** accepté pour la cible
- **Date :** 2026-09-25
- **Portée :** critères de choix technologique

## Contexte

Le cœur C17 contient des modèles, DAO, services, migrations et parcours GTK
utiles. Les outils spécialisés et un futur frontend peuvent employer d'autres
technologies plus adaptées. Une réécriture de principe ferait perdre des
contrats déjà testés ; une intégration sans limites compromettrait sécurité et
reproductibilité.

## Décision

Conserver le cœur C17 utile et extraire progressivement ses responsabilités.
Autoriser Python, JavaScript/TypeScript, SQL, CLI, bibliothèques et API HTTP
lorsque fiabilité, isolation, licence, maintenance et coût d'intégration sont
documentés. Les composants communiquent par des contrats versionnés ; aucun
adapter ne dépend de l'UI ni ne publie directement dans SQLite.

## Conséquences

- Make reste la chaîne `CURRENT` ; J0 n'ajoute ni Node/npm, ni Meson, ni Docker ;
- les outils absents rendent une capability indisponible, sans installation
  silencieuse ;
- les sorties non fiables sont bornées, conservées comme brut puis normalisées ;
- les versions, paramètres expurgés, erreurs et transformations participent à
  la provenance.

## Alternatives examinées

- **tout réécrire en C :** rejeté lorsque des outils matures apportent une
  solution mieux maintenue ;
- **tout réécrire autour du frontend :** rejeté, car l'UI ne doit pas devenir
  propriétaire des contrats métier ;
- **ABI de plugins dynamiques immédiate :** reportée : un contrat de processus
  ou HTTP versionné est plus simple à isoler et à faire évoluer au départ.

## Révision

Chaque intégration est réévaluée si sa licence, sa maintenance, ses conditions
d'usage, sa surface réseau ou son coût de confinement ne sont plus acceptables.
