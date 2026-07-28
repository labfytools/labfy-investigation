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
12. Relancer et vérifier que chaque proposition affiche deux actions
    distinctes : « Conserver dans la fiche » et « Promouvoir en entité ».
    La promotion doit être décochée et désactivée tant que la conservation
    n’est pas cochée.
13. Vérifier que chaque proposition affiche son rôle et son
    origine (`From`, `To`, `Message-ID` ou `Received #n`). Vérifier
    explicitement que `192.0.2.10`, `198.51.100.20` sont des IP et que
    `1.0` n’est jamais proposé comme domaine.
14. Cocher uniquement « Conserver dans la fiche » pour quelques adresses,
    domaines et relais synthétiques, puis
    choisir « Intégrer les éléments sélectionnés ».
15. Vérifier que le bilan annonce zéro promotion, qu’aucun nœud ni lien
    `preuve_entites` n’est créé et que la
    fiche de la preuve reste sélectionnée. La section « Entités observées
    dans cette preuve » doit afficher valeur canonique, type, rôle,
    en-tête/occurrence, provenance et « graphe : non ajoutée ».
16. Fermer puis rouvrir l’enquête synthétique et vérifier que les
    observations, rôles et origines sont toujours présents sans nœud.
17. Relancer l’analyse, cocher la conservation et « Promouvoir en entité »
    pour une seule observation, confirmer et vérifier qu’un seul nœud
    apparaît et que la fiche indique la promotion.
18. Répéter la promotion et vérifier qu’aucune observation ni entité n’est
    dupliquée.
19. Dans « Observations extraites », utiliser « Retirer du graphe » sur
    l’observation promue et confirmer. Vérifier que l’observation, son rôle
    et sa provenance restent affichés avec « Graphe : Non ajoutée ».
20. Vérifier que le nœud disparaît lorsqu’il n’a aucune autre référence.
21. Promouvoir deux observations de rôles différents vers la même valeur
    canonique, puis n’en retirer qu’une. Vérifier que l’entité partagée et
    l’autre observation restent présentes.
22. Fermer puis rouvrir l’enquête synthétique et vérifier que tous les états
    de promotion et de retrait persistent.
23. Recalculer l’intégrité de la preuve et vérifier que son SHA-256 est
    inchangé.

Noter séparément le comportement lorsque ExifTool, Tesseract ou les outils
Poppler ne sont pas installés : les en-têtes et l’extraction MIME doivent
rester consultables.

Ce parcours doit rester exclusivement synthétique : aucune enquête, preuve,
base SQLite, pièce jointe ou donnée réelle ne doit être utilisée.
