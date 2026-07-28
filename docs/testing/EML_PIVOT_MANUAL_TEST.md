# Test manuel du parcours EML

Ce parcours complète les tests automatiques. Il utilise exclusivement la
fixture synthétique `tests/fixtures/eml/manual_smoke_test.eml`, dans une
enquête synthétique neuve. Toute donnée ou base réelle est interdite.

## Préparation et tests automatiques

```sh
make -j8
make -j8 test
```

Un échec de compilation ou de test est bloquant. Créer ensuite une enquête
neuve dans un répertoire temporaire dédié, importer la fixture et vérifier son
intégrité depuis la fiche. L'empreinte doit correspondre à celle enregistrée.

## Parcours manuel

| Étape | Action | Résultat attendu | Échec bloquant |
|---|---|---|---|
| 1 | Sélectionner la fixture importée. | « Analyser l'e-mail » est disponible. | Action absente ou active sur un fichier non EML. |
| 2 | Lancer l'analyse puis l'annuler depuis le panneau d'activité. | État annulé, aucun dialogue d'intégration, aucune observation, entité ou relation créée. | Écriture persistante après annulation. |
| 3 | Relancer l'analyse. | Le dialogue affiche en-têtes, deux pièces jointes, textes, métadonnées et avertissements disponibles. | Plantage ou modification de la preuve. |
| 4 | Contrôler `From`, `Sender`, `Reply-To`, `Return-Path`, `To`, `Cc`, `Bcc`, `Message-ID` et chaque `Received`. | Chaque proposition d'en-tête indique son rôle et son origine avec occurrence. | Rôle ou origine perdus. |
| 5 | Contrôler les indicateurs. | `192.0.2.10` et `198.51.100.20` sont des IP ; `1.0` n'est jamais un domaine ; `Message-ID` n'est pas une adresse e-mail. | Mauvaise qualification. |
| 6 | Contrôler les propositions bancaires. | L'IBAN synthétique commençant par `FR00` est invalide et non intégrable ; aucune attribution pénale n'est déduite du titulaire déclaré. | Promotion automatique ou donnée invalide intégrable. |
| 7 | Fermer avec « Rejeter et fermer ». | Aucun objet persistant nouveau. | Observation, entité ou lien créé. |
| 8 | Relancer, cocher seulement « Conserver dans la fiche » sur quelques propositions. | « Promouvoir en entité » reste décoché par défaut et n'est activable que pour une proposition conservée. | Promotion implicite. |
| 9 | Intégrer la sélection. | Le bilan annonce zéro promotion ; la fiche reste sélectionnée et affiche toutes les observations choisies, leur valeur canonique, type, rôle, source/occurrence, provenance, validation et « Graphe : Non ajoutée ». Aucune ligne `entites` ou `preuve_entites` n'est créée. | Nœud ou rattachement créé par la conservation. |
| 10 | Changer de preuve puis revenir. | Toutes les observations réapparaissent immédiatement. | Observation perdue au rafraîchissement. |
| 11 | Fermer puis rouvrir l'enquête synthétique. | Observations, rôles, origines et état non promu persistent. | Perte de données. |
| 12 | Relancer et promouvoir explicitement une seule observation conservée. | Une seule entité est créée ou réutilisée, un seul nœud apparaît et la fiche indique la promotion. | Plusieurs nœuds ou promotion non demandée. |
| 13 | Répéter la même intégration. | Aucune observation ni entité en double. | Doublon. |
| 14 | Dans « Observations extraites », choisir « Retirer du graphe » et confirmer. | L'observation reste présente avec rôle et provenance ; elle repasse à « Graphe : Non ajoutée ». | Observation supprimée. |
| 15 | Vérifier le graphe. | Le nœud disparaît seulement s'il n'a aucune autre référence. | Suppression d'une entité encore utilisée. |
| 16 | Promouvoir deux rôles vers la même valeur canonique, puis en retirer un. | L'entité et l'autre observation promue restent présentes. | Entité partagée supprimée. |
| 17 | Relier une entité promue à une relation, puis retirer la promotion. | La relation et l'entité restent présentes. | Relation ou entité utilisée supprimée. |
| 18 | Réouvrir l'enquête. | États de conservation, promotion et retrait persistants. | État uniquement visuel. |
| 19 | Créer manuellement le même rattachement preuve-entité qu'une promotion, puis retirer cette promotion. | L'observation est détachée, mais le rattachement manuel, le nœud et l'entité restent présents. | Le rattachement manuel ou l'entité disparaît. |
| 19 | Recalculer l'intégrité. | Le SHA-256 final de la fixture est inchangé. | Empreinte modifiée. |

## Dépendances optionnelles

Répéter l'analyse avec ExifTool, Tesseract ou les outils Poppler absents, selon
les possibilités du poste. L'outil concerné doit être signalé indisponible ou
le résultat partiel ; les en-têtes et l'extraction MIME restent consultables.
Un plantage ou l'impossibilité d'accéder aux résultats EML de base est
bloquant.

## Limites connues

- La promotion est proposée dans le dialogue d'analyse, pas depuis la fiche.
- La fiche affiche encore les codes persistés et ne montre pas séparément la
  valeur brute différente ni l'UUID de l'entité.
- Les sorties des outils sont bornées et annulables, mais sans délai maximal
  autonome.
- Les fichiers du staging sont nettoyés ; le parcours ne conserve pas encore
  les pièces jointes comme dérivés confirmés.
- Tester le retrait avec un rattachement manuel préexistant : V13 conserve sa
  source `manual` indépendamment de la source `eml_observation`.
