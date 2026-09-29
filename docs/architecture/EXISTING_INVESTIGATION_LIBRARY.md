# Enquêtes existantes dans la bibliothèque locale — CURRENT local

Ce parcours a été validé uniquement avec deux enquêtes `SPECIMEN` temporaires.
Il ne donne ni accès à un sélecteur de chemin arbitraire, ni autorisation de
traiter une enquête réelle pendant le développement.

## Parcours humain

Le démarrage `--lazy-library` ne lit pas la bibliothèque. Depuis le poste Web,
« Charger les enquêtes » lit le registre, puis « Rechercher les dossiers
existants » déclenche une discovery distincte. Celle-ci examine uniquement les
enfants directs de `LABFY_LIBRARY`, sans descendre dans les sous-dossiers.
L'utilisateur enregistre un candidat, l'ouvre, puis peut choisir « Fermer
l’enquête » avant d'en ouvrir une autre. « Accueil » ne ferme pas l'enquête.
Une seule enquête est active par instance.

Les routes `POST /api/v1/library/discover-existing`,
`POST /api/v1/library/register-existing` et `POST /api/v1/library/close`
appliquent les contrôles de session, Host, Origin, CSRF, type et taille de corps
du serveur Web. Le navigateur ne transmet qu'un `candidate_id` opaque, une
génération et une clé d'idempotence ; il ne peut fournir un chemin serveur.
La réponse de discovery expose un nom d'affichage, un titre, un identifiant
d'enquête éventuel, un état et une raison bornée, jamais un chemin absolu.

## Registre et validation

`labfy.web_library.registry.v2` conserve des entrées `MANAGED` sous
`workspaces/<uuid>` et des entrées `EXISTING` référencées par un nom d'enfant
direct. La référence reste privée et relative à la bibliothèque. Le lecteur
accepte aussi `registry.v1` ; la conversion en v2 n'a lieu qu'à la première
mutation humaine explicite. L'écriture utilise un temporaire privé, `fsync`,
remplacement atomique et `fsync` du répertoire. Des identifiants, références ou
clés d'idempotence dupliqués sont refusés. Une entrée enregistrée devenue
absente ou invalide apparaît `MISSING` ou `INVALID` sans bloquer les autres.

La discovery est en lecture seule. Elle ignore les fichiers, dossiers cachés,
éléments internes et sous-dossiers imbriqués ; elle borne sa réponse à 256
candidats. Elle refuse les liens symboliques du dossier, des parents de
métadonnées, de la base, du JobStore et du manifeste. Le bridge C
`validate-workspace-json` vérifie en lecture seule le schéma runtime courant,
l'identité, le titre et le JobStore de la même enquête. Il ne publie aucun
snapshot et ne migre aucune base.

Les états des candidats sont `READY_TO_REGISTER`, `ALREADY_REGISTERED`,
`NEEDS_METADATA_MIGRATION`, `UNSUPPORTED_LEGACY_LAYOUT` et `INVALID`.
Un dossier prêt est inscrit par une mise à jour du registre uniquement : ni
copie, ni déplacement, ni renommage, ni modification de fichiers métier.
Lorsqu'une base et un JobStore sont déjà compatibles mais que seul
`workspace.json` manque, une action humaine séparée peut créer ce manifeste
après prévisualisation. La base reste inchangée. Un manifeste présent mais
invalide n'est jamais écrasé. Aucune migration SQLite n'est effectuée.

L'inscription revalide le candidat et sa signature au moment de la mutation.
Si le dossier a changé, l'utilisateur doit relancer la discovery. Une même
intention et une même clé se rejouent sans second enregistrement ; une clé liée
à une autre intention est refusée. L'ouverture revalide le registre, le
mapping de stockage, le manifeste et les données via le bridge C avant de
publier l'identité active. Un échec d'export ne laisse pas de demi-ouverture.

## Fermeture et contexte Qwen

La fermeture annule les turns Agent non terminaux du workspace et attend de
façon bornée les appels déjà en cours avant d'effacer l'identité active. Elle
terminalise la mission active. Les propositions et demandes historiques gardent
leur provenance, mais ne deviennent pas actives dans l'enquête suivante.
L'interface abandonne ses requêtes en cours et réinitialise graphe, sélection,
panneaux Agent, mission, propositions, toolbox et code change.

Sans enquête active, le démarrage d'un turn est refusé et l'interface affiche
« Ouvrez une enquête pour utiliser Qwen. ». Après A → fermeture → B,
`InvestigationContext` est reconstruit depuis B. Les références objet et les
continuations de A ne sont pas acceptées sous B. Qwen utilise les capabilities
existantes ; aucun accès direct au filesystem ou à SQLite ne lui est ajouté.

Un sélecteur de chemin extérieur à `LABFY_LIBRARY`, la discovery récursive,
une migration automatique de base et plusieurs contextes actifs simultanés
restent hors de ce contrat.
