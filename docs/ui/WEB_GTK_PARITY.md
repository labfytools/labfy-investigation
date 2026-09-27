# Matrice de parité Web avant retrait GTK

> État : `CURRENT` local — `LABFY_WEB_FINAL_INTERFACE_AND_REMOVE_GTK_V1`.
> Les contrôles utilisent exclusivement les workspaces `SPECIMEN`.

| Fonction GTK historique | Équivalent Web CURRENT | Test Web | État |
| --- | --- | --- | --- |
| Graphe, sélection, pan, zoom, focus et retour | SVG interactif central, recherche et actions contextuelles | `test_workbench_browser.mjs` | remplacé |
| Positions et zoom persistés | Projection C et positions épinglées conservées au rafraîchissement | `test_workbench_populated_browser.mjs` | remplacé |
| Arbre / détails d'entité, preuve et relation | Drawer Détails et inspecteur à onglets | `test_local_workspace_import_browser.mjs` | remplacé |
| Provenance et revue | Onglets Provenance et Observations, promotion explicite | `test_j9_browser.mjs` | remplacé |
| Activité, tâches et annulation | Drawer Tâches, JobStore V4 et contrôles de file | `test_j8_runtime_browser.mjs` | remplacé |
| Actions OSINT contextuelles | Capability → service → job, plan/grant/campagne explicites | `test_research_browser.mjs` | remplacé |
| Import, aperçu et analyse de preuve | Réception privée, preview dérivé, analyses C | `test_local_workspace_import_browser.mjs` | remplacé |
| Rapports | Prévisualisation et dossier hors ligne vérifiable | `test_j9_browser.mjs` | remplacé |
| Dialogues de création GTK et formulaires spécialisés | Parcours non exposés par le bridge Web actuel | aucun | retiré, pas de logique métier supprimée |
| Widgets OCR de rendu GTK | Données, correction et provenance conservées dans services/DAO ; renderer retiré | tests C de services conservés | retiré, renderer seulement |

Les parcours retirés ne constituaient pas une API métier : ils dépendaient du
renderer GTK. Les DAO, services, migrations V20, fondation financière V21 et
JobStore V4 restent hors de cette suppression.
