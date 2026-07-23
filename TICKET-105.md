# Ticket 105 — Glisser-déposer des extractions vers le graphe

## Objectif

Permettre de prendre une extraction depuis `Dossier → Extractions` et de la
déposer dans le graphe afin de créer ou compléter les liens de l’enquête.

## Fonctionnement attendu

1. Une extraction sélectionnée dans la Sidebar peut être déplacée.
2. Le graphe accepte le dépôt uniquement sur une zone valide.
3. Une fenêtre de confirmation affiche le fichier, sa source et son contenu
   textuel disponible.
4. L’utilisateur choisit explicitement :
   - créer une nouvelle entité à partir de l’extraction ;
   - rattacher l’extraction à une entité existante ;
   - annuler.
5. L’association est enregistrée en base et visible dans la fiche concernée.
6. Une relation graphique peut être créée sans déplacer ni dupliquer le
   fichier original.

## Contraintes

- Ne jamais créer de relation silencieusement.
- Refuser les fichiers hors du dossier de l’enquête.
- Conserver les associations déjà présentes.
- Utiliser les API GTK de glisser-déposer et les commentaires Doxygen prévus
  par le projet.

## Critères d’acceptation

- Un fichier `.txt` d’OSINT est déplaçable depuis la Sidebar.
- Le dépôt sur le graphe ouvre le dialogue de choix.
- L’entité ou l’association créée survit à la réouverture de l’enquête.
- Les dépôts annulés ne modifient ni la base ni le graphe.
- Tests unitaires des chemins invalides, du choix d’action et de la
  persistance.

## Vérification

```sh
make -j2
make test
```

