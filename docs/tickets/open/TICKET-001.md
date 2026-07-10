# Ticket #001

## Titre

Créer le module Application.

## Objectif

Créer le point d’entrée applicatif chargé de gérer le cycle de vie de GTK.

## Responsabilités

- créer l’objet GtkApplication ;
- lancer la boucle principale GTK ;
- libérer proprement les ressources ;
- afficher une fenêtre minimale au signal `activate`.

## Hors périmètre

- SQLite ;
- ouverture d’une enquête ;
- sélecteur de dossier ;
- logique métier.

## Critères d’acceptation

- [ ] Le projet compile en C17.
- [ ] Aucun warning.
- [ ] Aucun état global.
- [ ] Les fonctions publiques sont documentées avec Doxygen.
- [ ] Une fenêtre GTK minimale s’affiche.
- [ ] La fermeture de l’application est propre.

## Commit attendu

```text
feat(core): create application lifecycle
