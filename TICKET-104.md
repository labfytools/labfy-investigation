# Ticket 104 — Lier et exploiter les extractions dans l’enquête

## Objectif

Rendre les fichiers produits par les outils d’analyse (OSINT, ExifTool,
analyse EML, OCR, récupération PDF, etc.) traçables et directement
exploitables depuis l’interface de l’enquête.

## Besoins

- Enregistrer chaque extraction comme une preuve traitée identifiable.
- Conserver la référence de l’entité ou de la preuve à l’origine de
  l’extraction.
- Afficher les extractions liées dans la fiche de l’objet source.
- Permettre la prévisualisation des fichiers texte avec défilement.
- Afficher les résultats texte dans les fiches concernées sans ouvrir le
  gestionnaire de fichiers.
- Depuis l’onglet **Dossier**, permettre de glisser-déposer une extraction
  sur le graphe afin de proposer sa création ou son rattachement à une
  entité.
- Préserver l’intégrité et le chemin relatif des fichiers produits dans
  `02_Preuves_Traitees/Extractions`.

## Contraintes d’implémentation

- Utiliser les associations persistées preuve–entité existantes, sans
  dupliquer les fichiers ni les résultats.
- Refuser les chemins sortant du dossier de l’enquête.
- Ne jamais exécuter automatiquement le contenu d’une extraction déposée
  dans le graphe.
- Conserver les commentaires Doxygen des nouvelles fonctions publiques.

## Critères d’acceptation

1. Une extraction terminée apparaît comme preuve traitée et indique sa
   source.
2. La fiche de la source liste les extractions associées.
3. Un fichier `.txt` peut être lu dans une zone défilante depuis sa fiche et
   depuis l’onglet Dossier.
4. Un glisser-déposer sur le graphe ouvre une action explicite de création ou
   d’association, sans modification silencieuse.
5. Les associations et chemins sont conservés après fermeture et réouverture
   de l’enquête.
6. Les tests unitaires couvrent l’association, les chemins invalides et le
   transfert Dossier → graphe.

## Vérification

- `make -j2`
- `make test`
- Vérification manuelle avec une extraction OSINT et un fichier texte dans
  `02_Preuves_Traitees/Extractions`.

