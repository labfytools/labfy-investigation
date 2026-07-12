# Ticket #003

## Titre

Créer le module `Investigation`.

## Objectif

Créer l'objet métier représentant une enquête ouverte par Labfy Investigation.

Le module doit rester indépendant de GTK et de l'interface graphique.

## Responsabilités

Le module doit :

- mémoriser le chemin racine de l'enquête ;
- construire le chemin de la base de données modèle ;
- exposer ces chemins en lecture seule ;
- gérer proprement sa mémoire ;
- vérifier qu'un chemin d'enquête est exploitable.

## Hors périmètre

Ce ticket ne doit pas :

- ouvrir SQLite ;
- créer la base de données ;
- modifier l'arborescence de l'enquête ;
- afficher de message GTK ;
- gérer les preuves ou les entités.

## Dépendances

- libc ;
- GLib uniquement si nécessaire pour la gestion des chaînes et chemins.

Aucune dépendance GTK ou SQLite.

## API attendue

Le module doit permettre un usage proche de :

```c
Investigation *investigation = NULL;

investigation = investigation_new("/chemin/vers/enquete");

if (investigation == NULL)
{
    /* Gestion de l'erreur */
}

const char *root_path = investigation_get_root_path(investigation);
const char *database_path = investigation_get_database_path(investigation);

investigation_free(investigation);
