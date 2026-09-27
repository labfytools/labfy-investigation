# Espace local et import navigateur — CURRENT

## Contrat

`make web-workspace-local WORKSPACE=/chemin/explicite` fixe l’unique destination
de l’instance. Le navigateur ne transmet jamais de chemin serveur. Après la
session locale, un espace absent présente **Créer l’enquête** ; seul ce POST
appelle `tools/local-jobs create-workspace`. Il crée une base V20 à la racine,
un JobStore V3 vide et `.labfy/runtime/workspace.json`. Aucun corpus, preuve,
observation, plan ou job n’est préinséré. Les espaces SPECIMEN J5–J9 conservent
leur manifeste et leurs commandes historiques.

Une disposition est non ambiguë lorsqu’elle contient soit `Enquete.sqlite` et
le manifeste générique à la racine, soit le manifeste SPECIMEN historique.
L’absence de base dans un espace manifesté, une version non supportée ou un
manifeste incohérent est une erreur d’ouverture ; il n’y a pas de recréation ni
de conversion d’une ancienne enquête GTK.

## Réception, préparation et publication

Le contrôle `input type=file` envoie les octets par `PUT application/octet-stream`
vers un `upload_id` opaque. Session, Host, Origin et CSRF sont contrôlés. Les
JSON restent plafonnés à 4 Kio. Les limites d’import sont 4 Mio par fichier,
8 fichiers et 16 Mio par sélection, 64 Mio de staging, deux réceptions et
30 secondes. Chaque sélection possède une identité opaque : le serveur réserve
atomiquement son nombre de fichiers, ses octets de lot, les octets de staging
et une place de réception avant de lire le corps. Le client sérialise une
sélection multiple ; le serveur relit la longueur effective par blocs avec une
échéance interruptible et refuse transfert tronqué, signature/type
incohérents, nom avec séparateur/contrôle et formats autres que EML/PNG/JPEG.

Le staging privé `.labfy/uploads` n’est pas servi. Les octets sont inchangés et
leur SHA-256 est calculé puis revérifié par le cœur. `EvidenceImporter` accepte
facultativement l’UUID réservé et le nom original de l’intention ; son contrat
historique reste inchangé quand ces champs valent `NULL`. La date de collecte
absente reste `NULL`. Un contenu identique dans deux intentions produit deux
preuves ; le rejeu d’une même clé retrouve le même UUID. Une clé liée à une
autre intention est refusée.

Les états persistés sont `RECEIVING`, `TRANSFERRING`, `PREPARED`, `CONFIRMING`,
`IMPORTED`, `CANCELLED` et `FAILED`. Réception, confirmation et annulation d’un
même upload sont sérialisées ; un état terminal ne peut pas être rouvert par un
nouveau PUT. Les reçus JSON privés `.labfy/imports` décrivent l’opération et
référencent la preuve V20 sans devenir une seconde vérité métier. L’intention
canonique structurée et le reçu `CONFIRMING` sont durables avant le premier
effet. Un verrou de publication sérialise ensuite les confirmations. L’ordre
est : réception temporaire, contrôle/format, renommage en `payload`, UUID
réservé, intention, copie immuable, transaction SQLite, reçu `IMPORTED`, puis
nouveaux snapshots. Un arrêt avant confirmation laisse une
préparation annulable ; après commit, la preuve est retrouvée par UUID et le
reçu peut être réconcilié. Chaque rejeu compare l’intention, le record V20 et
l’original effectivement publié (identités, métadonnées, taille et SHA-256).
Un reçu tronqué, un record incohérent ou un original manquant/altéré donne une
erreur de récupération, jamais une nouvelle preuve ni une réparation silencieuse.

## Raccordement et limites

Après chaque publication, le cœur relit les DAO et tente de republier graphe,
corrélations et planner. Cette projection est réessayable : son indisponibilité
ne transforme pas une preuve déjà commitée et vérifiée en import en attente.
Aucune analyse ne démarre à l’import : l’utilisateur approuve les
recommandations et budgets J8. Les rapports J9 utilisent ensuite les objets
persistés et leurs dérivés. Le mode affiché provient du manifeste : `SPECIMEN`
ou `local expérimental`, sans prétendre classifier les données.

Ce lot expérimental ne prend pas en charge ZIP, répertoire récursif, SQLite
téléversée, anciennes dispositions GTK, accès distant, TLS ni multi-utilisateur.
Il ne constitue pas une certification de production ni une autorisation de
traiter des enquêtes réelles.
