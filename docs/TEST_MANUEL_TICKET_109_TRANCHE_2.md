# Test manuel — ticket #109, tranche 2

Comparer le même EML synthétique dans la fiche directe et dans l’assistant :
From, To, Cc, Date, Subject, Message-ID, corps et inventaire détaillé des
pièces jointes doivent être identiques. Tester ensuite PNG/JPEG/HEIC/HEIF,
PDF, MP4/MOV et texte, les changements rapides, la fermeture pendant
chargement ou vidéo, la réouverture et l’ouverture d’une fiche personne.
Vérifier l’absence de critique GTK, d’écriture SQLite et d’OCR.

Utiliser exclusivement une enquête temporaire et des fichiers synthétiques
dont le nom ou le contenu porte `SPECIMEN`.

1. Créer une enquête temporaire et y importer préalablement deux preuves
   synthétiques PNG/JPEG.
2. Ouvrir « Ajouter une personne », renseigner une désignation fictive et
   sélectionner plusieurs rôles.
3. Dans « Preuves », rechercher puis ajouter les deux preuves existantes.
   Vérifier qu’un second ajout est refusé sans modifier la preuve.
4. Choisir « Importer des fichiers » et sélectionner simultanément un PNG, un
   JPEG et un PDF synthétiques. Vérifier que rien n’apparaît encore dans
   `01_Preuves_Originales` ni dans SQLite.
5. Activer chaque élément retenu. Vérifier les aperçus PNG/JPEG et le message
   explicite d’indisponibilité du PDF.
6. Modifier individuellement les types métier, naviguer en arrière puis en
   avant et vérifier la conservation de l’état.
7. Retirer une preuve existante et un fichier nouveau. Vérifier que la preuve
   existante reste dans l’enquête et que la copie de staging retirée disparaît.
8. Sélectionner deux fois le même fichier, puis deux fichiers de même contenu.
   Vérifier le message de doublon et l’unicité de la sélection.
9. Ajouter un fichier identique à une preuve existante. Vérifier la
   réutilisation explicite de cette preuve.
10. Atteindre Confirmation et contrôler la liste complète, les origines,
    types, MIME, tailles, empreintes abrégées et compteurs.
11. Annuler puis vérifier l’absence de nouvelle personne, preuve, rôle,
    rattachement et copie définitive, ainsi que le nettoyage du staging.
12. Recommencer, puis fermer la fenêtre pendant le staging. Vérifier la
    fermeture propre, le nettoyage et le maintien de l’application.
13. Recommencer et changer d’enquête pendant une préparation ou une création.
    Vérifier l’annulation, l’absence d’écriture dans la nouvelle enquête et
    l’absence de rafraîchissement tardif.
14. Confirmer normalement. Vérifier que toutes les preuves sont rattachées à
    la personne avec une source `manual`, puis rouvrir l’enquête.
15. Comparer les empreintes des fichiers sources avant et après le parcours.
    Elles doivent être identiques.
16. Vérifier qu’aucun OCR ni outil documentaire avancé n’a été lancé.
17. Ouvrir la liste des preuves disponibles, choisir successivement deux
    preuves, revenir à « Aucune preuve associée », activer une recherche et un
    filtre, puis sélectionner de nouveau une preuve. Vérifier l’absence de
    crash et de critique GTK.
18. Ajouter une image JPEG historique synthétique dont le MIME enregistré est
    NULL et le type métier `document`. Vérifier que le contenu JPEG intègre est
    détecté, que l’aperçu apparaît et que « MIME détecté : image/jpeg » est
    affiché sans modification SQLite.
19. Renommer un fichier texte synthétique avec l’extension `.jpg`, puis
    vérifier que l’aperçu est refusé malgré l’extension. Refaire avec un MIME
    historique `image/jpeg` incohérent et vérifier le même refus.
20. Modifier le type individuel de l’image de `document` vers `email`.
    Vérifier immédiatement la même valeur dans la ligne retenue, le sélecteur,
    les métadonnées et le résumé après un aller-retour entre les étapes.

21. Tester des fixtures PNG, JPEG, HEIC, HEIF, MP4, MOV, EML, TXT, CSV, LOG,
    Markdown et PDF. Vérifier l’image, le lecteur sans lecture automatique,
    le texte passif, les en-têtes EML et la première page PDF.
22. Tester format invalide, extension trompeuse et fichier trop volumineux.
23. Changer rapidement de preuve et vérifier le rejet tardif et l’arrêt vidéo.
24. Fermer puis changer d’enquête pendant un aperçu ; vérifier l’annulation.
25. Vérifier les empreintes inchangées et l’absence d’OCR, d’accès distant et
    d’écriture SQLite pendant l’aperçu.
26. Avec un EML multipart synthétique, vérifier que le corps `text/plain` est
    préféré au HTML et que nom, MIME, taille, disposition et Content-ID des
    pièces jointes sont visibles sans fichier extrait persistant.

Limites : aucun OCR ni analyse avancée. La vidéo dépend des codecs GStreamer.
