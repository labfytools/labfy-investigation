# Modification contrôlée de Labfy par Qwen V1

**État : CURRENT local après validation sur dépôt de test `SPECIMEN`.**

Le niveau 1 est le manifeste déclaratif ; le niveau 2 ajoute une capability
dynamique après Gate B. Si ces deux niveaux ne suffisent pas, Qwen peut
justifier une modification bornée de Labfy et fournir zones, fichiers, risque
et recettes de tests. La porte C1 humaine valide seulement la préparation.

`CodeChangeService` crée alors un worktree Git détaché à partir d'un SHA
immuable. `DeveloperAgent` donne à Qwen des outils bornés de lecture,
recherche, édition conditionnée par hash, tests allowlistés et lecture du diff.
Il n'existe pas d'outil shell, Git commit/push, installation de dépendances,
écriture sur main ou approbation humaine. Les chemins autorisés sont
explicitement fixés dans la proposition ; liens symboliques, `.git`, sous
modules, binaires, secrets et sorties volumineuses sont refusés.

Les recettes obligatoires sont déduites des surfaces touchées. Un changement
Web requiert notamment les tests Node et Firefox pertinents, puis la suite
Firefox canonique. Labfy produit un diff borné, un digest et un aperçu local
sur `SPECIMEN`. La porte C2 humaine vient après cette revue. L'application
vérifie que le dépôt cible est propre et que son HEAD correspond exactement à
la base, applique le patch sans commit, puis répète les tests. Un état périmé
ou un test raté arrête le parcours ; rollback et reprise après interruption
sont explicites.

Le smoke `manual_qwen_self_integration_smoke.py` opère exclusivement sur un
clone temporaire, jamais sur le dépôt de développement réel. Il exige un vrai
appel Qwen à `code.change.propose`, la lecture/édition/tests/diff du Developer
Agent, un aperçu Firefox et l'application contrôlée dans ce clone. Les portes
C1 et C2 restent des décisions humaines simulées par le harness de test,
séparées des sorties du modèle.
