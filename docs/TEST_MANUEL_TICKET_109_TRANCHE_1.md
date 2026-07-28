# Test manuel — ticket #109, tranche 1

Utiliser uniquement une enquête de démonstration et des fichiers synthétiques
marqués `SPECIMEN`.

1. Cliquer sur Annuler à chacune des quatre étapes doit fermer uniquement
   l’assistant et laisser la fenêtre principale utilisable. Refaire le contrôle
   avec la croix, puis pendant le chargement d’un aperçu.
2. Ouvrir une enquête synthétique, puis demander l’ajout d’une personne.
3. Vérifier les quatre étapes `Personne`, `Rôles`, `Preuve`, `Confirmation`.
4. Revenir en arrière : les champs, rôles multiples et la preuve choisie
   doivent être conservés. Aucun rôle sensible ne doit être précoché.
5. Rechercher par nom, description et type, combiner avec le filtre métier,
   puis revenir à `Tous les types`.
6. Choisir des PNG et JPEG synthétiques : vérifier chargement, aperçu et
   métadonnées complètes, puis les états invalide et absent.
7. Sélectionner rapidement plusieurs preuves : seul le dernier aperçu doit
   apparaître, puis fermer la fenêtre pendant un chargement.
8. Choisir zéro puis une preuve existante. Aucun import, OCR, PDF, vidéo ou
   analyse EML avancée ne doit être proposé.
9. Confirmer et vérifier que personne, rôles, rattachement et source `manual`
   apparaissent ensemble après actualisation.
10. Changer d’enquête pendant un aperçu puis avant confirmation : le résultat
   tardif et la création doivent être refusés sans écriture ni rafraîchissement.

L’aperçu de tranche 1 accepte uniquement PNG/JPEG intègres. Il refuse les
fichiers absents, modifiés, corrompus ou supérieurs aux limites de 25 Mio,
12 000 pixels par côté et 40 millions de pixels. Le rendu mémoire est borné à
1 024 × 1 024 et aucun dérivé n’est persisté.
