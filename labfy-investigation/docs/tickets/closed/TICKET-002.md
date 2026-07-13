# Ticket #002

## Titre

Ajouter le sélecteur de dossier d’enquête.

## Objectif

Permettre à l’utilisateur de sélectionner un dossier d’enquête au lancement de l’application.

## Responsabilités

- afficher un dialogue GTK de sélection de dossier ;
- retourner le dossier sélectionné ;
- gérer l’annulation sans erreur ;
- transmettre le chemin au module Application.

## Hors périmètre

- création de `00_BaseDeDonnees` ;
- ouverture de SQLite ;
- initialisation du schéma ;
- validation complète d’une enquête.

## Critères d’acceptation

- [ ] Le dialogue s’ouvre au lancement.
- [ ] Un dossier peut être sélectionné.
- [ ] L’annulation est gérée proprement.
- [ ] Le chemin sélectionné est affiché dans la fenêtre.
- [ ] Aucun warning.
- [ ] Les fonctions publiques sont documentées.
- [ ] Aucun état global.

## Commit attendu

```text
feat(gui): add investigation folder selector
