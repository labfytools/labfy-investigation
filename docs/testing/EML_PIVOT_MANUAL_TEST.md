# Test manuel du parcours EML

Ce test emploie uniquement la fixture synthétique
`tests/fixtures/eml/manual_smoke_test.eml`. Il ne nécessite aucune donnée
réelle.

## Parcours synthétique

1. Compiler avec `make -j8`.
2. Créer une enquête synthétique neuve dans un répertoire de test.
3. Importer `tests/fixtures/eml/manual_smoke_test.eml` comme preuve.
4. Sélectionner cette preuve dans la barre latérale.
5. Vérifier que l’action compacte « Analyser l’e-mail » devient active.
6. Lancer l’action et observer « Analyse complète de l’e-mail » dans le
   panneau d’activité.
7. Pendant un premier essai, annuler la tâche depuis ce panneau et vérifier
   qu’aucun dialogue de succès ni aucune nouvelle entité n’apparaît.
8. Relancer l’analyse et vérifier dans le dialogue les en-têtes, les deux
   pièces jointes, les textes, les métadonnées et les avertissements relatifs
   aux outils optionnels éventuellement absents.
9. Vérifier que l’IBAN commençant par `FR00` est indiqué comme donnée de
   démonstration invalide et n’est pas proposé à l’intégration.
10. Vérifier que les valeurs sont sélectionnables et copiables.
11. Fermer avec « Rejeter et fermer » et confirmer l’absence de nouvelle
    entité, relation ou rattachement.
12. Relancer, sélectionner quelques adresses ou domaines synthétiques, puis
    choisir « Intégrer les éléments sélectionnés ».
13. Vérifier le message de bilan, le rafraîchissement du graphe, de la barre
    latérale et des détails de preuve.
14. Fermer puis rouvrir l’enquête synthétique et vérifier que les objets
    confirmés sont toujours présents.
15. Recalculer l’intégrité de la preuve et vérifier que son SHA-256 est
    inchangé.

Noter séparément le comportement lorsque ExifTool, Tesseract ou les outils
Poppler ne sont pas installés : les en-têtes et l’extraction MIME doivent
rester consultables.

## Test sur une copie d’enquête réelle

Ne jamais commencer sur l’unique exemplaire d’une enquête. Copier le dossier
complet, vérifier que la copie s’ouvre, conserver une sauvegarde distincte,
puis lancer Labfy Investigation uniquement sur cette copie.

Ne jamais transmettre à Codex une preuve, une base SQLite, une pièce jointe
ou une donnée sensible. En cas d’anomalie, relever uniquement les étapes, les
messages techniques expurgés et le comportement observé.
