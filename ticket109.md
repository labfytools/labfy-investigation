## Contexte

Lors de la création d’une personne dans une enquête, l’utilisateur doit
actuellement renseigner les données manuellement et ne peut pas choisir
clairement la nature de la personne ni associer facilement une preuve.

Le parcours devient particulièrement long lorsque les noms de fichiers
ne permettent pas de savoir ce qu’ils contiennent.

Le cas des documents d’identité nécessite également un traitement
forensique strict :

- conservation de l’original ;
- analyse sur une copie de travail ;
- OCR contrôlé ;
- validation humaine ;
- distinction entre texte brut, valeur normalisée et correction manuelle ;
- aucune affirmation automatique concernant l’identité réelle ou
  l’authenticité du document.

## Objectif

Créer un assistant fluide permettant, depuis la fenêtre de création d’une
personne :

1. de sélectionner son rôle ou sa nature dans l’enquête ;
2. de choisir une preuve déjà importée ;
3. d’importer immédiatement une nouvelle preuve ;
4. de visualiser la preuve avant de la sélectionner ;
5. de qualifier le type de preuve ;
6. de lancer une analyse adaptée au type choisi ;
7. d’utiliser l’OCR pour préremplir les champs d’un document d’identité ;
8. de corriger manuellement les données proposées ;
9. de conserver la provenance complète de chaque valeur ;
10. de garantir l’intégrité du fichier original.

## Terminologie

Ne pas confondre :

- le rôle de la personne dans l’enquête ;
- son niveau d’identification ;
- l’authenticité du document présenté ;
- la confiance accordée aux différentes observations.

Une personne peut par exemple avoir :

- rôle : Identité présentée ;
- identification : Non vérifiée ;
- document : Authenticité indéterminée ;
- hypothèse : Identité potentiellement usurpée.

L’application ne doit jamais transformer automatiquement cette personne
en auteur identifié.

## Fenêtre de création d’une personne

Ajouter un champ contrôlé :

    Rôle dans l’enquête

Valeurs initiales proposées :

- Auteur présumé ;
- Identité présentée ;
- Identité potentiellement usurpée ;
- Victime ;
- Témoin ;
- Titulaire bancaire déclaré ;
- Intermédiaire ;
- Personne citée ;
- Autre.

Ces valeurs doivent provenir du vocabulaire contrôlé du projet.

Le champ existant « Identification » reste distinct et conserve des états
tels que :

- Inconnu ;
- Non vérifié ;
- Partiellement identifié ;
- Identifié ;
- Contesté.

## Association d’une preuve

Depuis la fenêtre de création d’une personne, proposer deux actions :

- Sélectionner une preuve existante ;
- Importer une nouvelle preuve.

L’utilisateur ne doit pas être obligé de fermer la fenêtre, importer la
preuve ailleurs, puis recommencer la création de la personne.

### Preuve existante

La sélection doit afficher au minimum :

- miniature ou aperçu ;
- nom du fichier ;
- type de preuve ;
- taille ;
- date d’import ;
- empreinte SHA-256 abrégée ;
- description éventuelle ;
- état d’intégrité.

Prévoir une recherche et un filtrage par type.

### Nouvelle preuve

L’import déclenché depuis la fenêtre doit utiliser le mécanisme forensique
central du projet :

- copie contrôlée dans l’enquête ;
- calcul du SHA-256 ;
- conservation du nom original ;
- enregistrement de la provenance ;
- aucune modification du fichier source ;
- détection du type MIME réel ;
- détection des collisions ;
- traitement asynchrone ;
- annulation sûre.

À la fin de l’import, la nouvelle preuve doit être automatiquement
sélectionnée dans la fenêtre de création de la personne.

## Aperçu des preuves

La fenêtre de sélection et d’import doit fournir un aperçu suffisamment
grand pour identifier le contenu sans occuper toute la fenêtre.

Prévoir une zone responsive avec :

- conservation du ratio ;
- ajustement à la zone ;
- zoom avant et arrière ;
- retour à l’ajustement ;
- défilement lorsque l’image est agrandie ;
- message explicite lorsque l’aperçu est indisponible.

Prise en charge minimale :

- image : aperçu direct ;
- PDF : première page avec navigation entre les pages ;
- vidéo : miniature, durée et informations principales ;
- EML : résumé des en-têtes et liste des pièces jointes ;
- autre fichier : icône, type MIME, taille et métadonnées disponibles.

L’aperçu est une représentation dérivée.

Il ne doit jamais modifier le fichier original.

Toute conversion, génération de miniature, rotation, amélioration,
redimensionnement ou rendu PDF doit être réalisée dans une zone de
travail temporaire contrôlée.

## Qualification du type de preuve

Permettre de sélectionner ou confirmer un type contrôlé, par exemple :

- Document d’identité ;
- Carte nationale d’identité ;
- Passeport ;
- Permis de conduire ;
- Document bancaire ;
- Capture d’écran ;
- Conversation ;
- Courrier électronique ;
- PDF ;
- Photo ;
- Vidéo ;
- Billet ou justificatif de commande ;
- Autre.

La détection automatique peut proposer un type, mais l’utilisateur doit
toujours pouvoir le corriger avant validation.

Le type choisi détermine les analyses proposées.

## Document d’identité

Lorsqu’une preuve est qualifiée comme document d’identité, proposer :

    Analyser le document

Le traitement doit être asynchrone et annulable.

Il doit :

1. vérifier l’intégrité de la preuve ;
2. créer une copie de travail ;
3. conserver l’original strictement intact ;
4. appliquer les conversions uniquement à la copie ;
5. exécuter l’OCR ;
6. conserver le texte OCR brut ;
7. proposer des champs structurés ;
8. attendre une validation humaine avant toute intégration.

## Champs OCR proposés

Selon les informations réellement visibles, proposer notamment :

- nom ;
- prénoms ;
- sexe déclaré ;
- date de naissance ;
- lieu de naissance ;
- nationalité déclarée ;
- taille ;
- numéro du document ;
- date de délivrance ;
- date d’expiration ;
- autorité de délivrance ;
- zone lisible par machine ;
- autres valeurs observées.

Ne jamais inventer une valeur absente ou illisible.

Chaque proposition doit posséder un état :

- détectée ;
- partielle ;
- incertaine ;
- invalide ;
- corrigée ;
- rejetée ;
- confirmée.

## Validation et correction humaine

Avant la création de la personne, afficher côte à côte autant que possible :

- l’aperçu du document ;
- les valeurs extraites ;
- les champs modifiables ;
- le niveau de confiance OCR ;
- la provenance précise.

Pour chaque champ, conserver séparément :

- valeur OCR brute ;
- valeur normalisée ;
- valeur corrigée manuellement ;
- valeur finalement confirmée ;
- méthode d’extraction ;
- langue OCR ;
- version de l’outil ;
- date UTC ;
- preuve source ;
- page ou zone source lorsque disponible.

Une correction manuelle ne doit jamais écraser le résultat OCR brut.

L’utilisateur doit pouvoir :

- modifier une proposition ;
- rejeter une proposition ;
- laisser un champ vide ;
- revenir à la valeur OCR ;
- confirmer uniquement certains champs.

## Authenticité et identité usurpée

Ajouter un statut contrôlé pour le document :

- Authenticité indéterminée ;
- Présumé authentique ;
- Suspect ;
- Présumé falsifié ;
- Falsifié confirmé ;
- Document usurpé présumé ;
- Document usurpé confirmé.

Les statuts affirmatifs doivent nécessiter une validation explicite et
une justification.

L’OCR ne doit jamais conclure :

- que le document est authentique ;
- que la personne figurant sur le document est l’auteur ;
- que l’identité est réellement usurpée ;
- que le titulaire a participé aux faits.

Dans le cas courant, le document doit pouvoir être enregistré comme :

    Identité présentée — authenticité indéterminée

avec une hypothèse séparée :

    Identité potentiellement usurpée

## Création de la personne

Après validation, créer la personne avec uniquement les champs confirmés.

La personne doit être liée à la preuve par une relation factuelle, par
exemple :

- Identité observée dans ;
- Document présenté au nom de ;
- Données extraites depuis ;
- Identité déclarée dans.

Ne pas créer automatiquement une relation :

- Est l’auteur ;
- Identité réelle de ;
- A usurpé l’identité de.

Les résultats OCR rejetés ne doivent pas devenir des attributs de la
personne.

## Intégrité et provenance

Le fichier original doit rester immuable.

Avant chaque analyse :

- recalculer ou vérifier le SHA-256 ;
- bloquer l’analyse en cas de divergence ;
- ne jamais réécrire l’empreinte enregistrée pour masquer une différence.

Tous les fichiers dérivés doivent être identifiables comme tels :

- miniature ;
- rendu de page PDF ;
- image préparée pour OCR ;
- recadrage ;
- correction d’orientation ;
- texte OCR ;
- JSON de métadonnées.

Chaque dérivé doit conserver :

- preuve parente ;
- outil ;
- version ;
- arguments ;
- date UTC ;
- SHA-256 ;
- statut ;
- avertissements.

## Interface

Le parcours doit rester possible depuis une seule fenêtre ou un assistant
cohérent :

    Création de la personne
        ↓
    choix du rôle
        ↓
    sélection ou import de la preuve
        ↓
    aperçu
        ↓
    qualification du type
        ↓
    analyse facultative
        ↓
    révision des propositions
        ↓
    création de la personne et des relations confirmées

Utiliser des boutons compacts avec icônes et infobulles.

Ne pas ajouter de gros boutons occupant inutilement l’interface.

La fermeture ou l’annulation ne doit créer ni personne partielle, ni
attribut partiel, ni relation partielle.

## Transactions

La création finale doit être transactionnelle pour :

- la personne ;
- ses attributs confirmés ;
- le rattachement à la preuve ;
- les observations OCR confirmées ;
- les relations ;
- les références aux fichiers dérivés conservés.

En cas d’échec :

- rollback complet ;
- original inchangé ;
- aucune personne incomplète ;
- aucune relation orpheline ;
- aucun fichier temporaire présenté comme preuve permanente.

## Tests obligatoires

Ajouter des fixtures exclusivement synthétiques.

Couvrir au minimum :

1. choix de chaque rôle contrôlé ;
2. sélection d’une preuve existante ;
3. import d’une nouvelle preuve depuis la fenêtre ;
4. sélection automatique après import ;
5. aperçu image ;
6. aperçu PDF ;
7. aperçu indisponible ;
8. navigation entre pages ;
9. qualification manuelle du type ;
10. document d’identité déclenchant l’OCR ;
11. autre type ne déclenchant pas automatiquement l’OCR ;
12. original inchangé ;
13. SHA-256 inchangé ;
14. travail sur copie ;
15. texte OCR brut conservé ;
16. valeur normalisée séparée ;
17. correction manuelle séparée ;
18. rejet d’un champ ;
19. champ illisible laissé vide ;
20. création avec seulement les valeurs confirmées ;
21. provenance par champ ;
22. annulation ;
23. rollback ;
24. fermeture de fenêtre pendant l’analyse ;
25. changement d’enquête pendant l’analyse ;
26. aucune écriture dans la mauvaise base ;
27. aucune donnée réelle dans les fixtures ;
28. aucune conclusion automatique sur l’authenticité ;
29. aucune fusion automatique avec l’auteur présumé ;
30. réouverture de l’enquête avec données persistantes.

Séparer autant que possible :

- services d’import ;
- génération d’aperçu ;
- OCR ;
- modèle de révision ;
- DAO ;
- tests GTK ciblés ;
- validation visuelle manuelle.

## Critères d’acceptation

Le ticket est terminé seulement si :

- le rôle de la personne peut être sélectionné ;
- une preuve existante peut être choisie ;
- une nouvelle preuve peut être importée sans quitter le parcours ;
- un aperçu exploitable est disponible ;
- le type de preuve peut être confirmé ou corrigé ;
- un document d’identité peut être analysé par OCR ;
- les propositions OCR sont modifiables avant intégration ;
- le texte OCR brut n’est jamais écrasé ;
- l’original reste inchangé ;
- la provenance est persistée ;
- seules les valeurs confirmées alimentent la personne ;
- aucune identité n’est attribuée automatiquement à l’auteur présumé ;
- tous les tests passent.

## Sécurité de développement

Codex et tout agent de développement doivent travailler uniquement avec :

- le dépôt source ;
- des fixtures synthétiques ;
- des bases SQLite temporaires synthétiques.

Ils ne doivent jamais accéder à :

- Enquete.sqlite réelle ;
- documents d’identité réels ;
- captures réelles ;
- e-mails réels ;
- données bancaires réelles ;
- vidéos réelles ;
- autres preuves de l’enquête.
