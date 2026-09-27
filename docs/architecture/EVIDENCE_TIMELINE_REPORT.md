# Preuves, chronologie et rapports locaux — J9

Statut : **CURRENT — COMPLETE_LOCAL**. Ce lot complète le poste Web local sans
modifier le schéma publié : `Enquete.sqlite` reste V20, le JobStore reste V3 et
la tranche financière V21 demeure indépendante.

## Projections

Le graphe cœur reste la vérité commune. Les vues **Preuves**, **Chronologie** et
**Infrastructure locale** filtrent cette projection ; elles ne créent ni base ni
identité parallèle. La chronologie transporte les valeurs persistées de collecte,
d’import, d’observation, d’intégration et d’exécution. Une valeur sans précision
ou fuseau suffisant est montrée comme telle et n’est jamais complétée par
inférence. Les rapprochements d’infrastructure restent exploratoires.

## Sélection et prévisualisation

Depuis le détail d’un objet, « Ajouter au rapport » constitue une sélection
explicite. Le service C `local_report_service_build()` vérifie l’appartenance à
l’enquête, ferme la provenance dans une transaction SQLite en lecture et applique
des limites d’objets, de dépendances, d’événements et d’octets. Le profil V1 est
`MINIMAL` : aucun original, chemin absolu, secret, base SQLite, EML complet ou
sortie complète d’outil n’est inclus. Le commentaire est explicitement humain.

La révision SHA-256 couvre règles, enquête, sélection et rôles, profil, sections,
limites et valeurs rendues. La génération recalcule cette même coupe ; elle refuse
donc un aperçu devenu périmé au lieu de mélanger deux états.

## Dossier hors ligne

Une génération publie atomiquement sous `exports/reports/<report_id>/` :

- `report.html`, autonome, sans script ni ressource distante ;
- `report.json`, document canonique ;
- `report.pdf`, rendu local borné ;
- `manifest.json`, tailles, SHA-256, sélection, profil et exclusions ;
- `NOTICE.txt`, mode de vérification et portée des empreintes.

Le staging privé se trouve sous `.labfy/reports/staging`. Une interruption avant
publication ne rend aucun dossier partiel ; une réponse perdue après publication
est récupérée par l’intention idempotente. Une intention réutilisée avec un autre
document est refusée. Le vérificateur rejette absence, fichier supplémentaire,
lien symbolique, chemin dangereux, dépassement, contrat inconnu, taille ou hash
discordant :

```bash
python3 prototypes/web-graph/report_bundle.py verify /chemin/du/dossier
```

Le serveur ne sert que les cinq noms autorisés, après vérification complète du
dossier. Authentification locale, Host/Origin et CSRF restent obligatoires pour
les mutations. Un `GET` ne déclenche jamais une analyse ou une génération.

## Exécution SPECIMEN

```bash
make web-workspace-j9 WORKSPACE=/tmp/labfy-j9-specimen
```

Cette commande crée une fixture synthétique uniquement si le workspace ne
contient pas déjà `Enquete.sqlite`. Elle ne doit jamais viser une enquête réelle.
Le PDF utilise Cairo/Pango et la police locale DejaVu Sans, déjà fournis avec le
runtime graphique. Le répertoire Unicode validé couvre notamment le français,
`œ`, `€`, les ponctuations typographiques et `Ł`. Pango mesure et replie les
lignes ; un contenu qui ne peut pas être paginé dans la limite de 40 pages est
refusé sans substitution. `pdftotext` et `pdftoppm` servent à valider le texte
et toutes les pages rendues.

## Renforcement d’intégrité

La fermeture justificative suit les arêtes de provenance de la cible vers sa
source : elle n’exporte plus les observations sœurs. Désactiver Preuves réduit
les objets aux références minimales annoncées ; désactiver Chronologie ou
Infrastructure produit respectivement une collection temporelle ou réseau vide.
Le profil MINIMAL refuse tout dépassement d’événements plutôt que de tronquer.

Le vérificateur considère le dossier comme non fiable : racine ou composant
symbolique, fichier non régulier, entrée supplémentaire, structure JSON mal
typée, clé JSON ou chemin de manifeste dupliqué, hash mal formé et incohérence
entre manifeste et document sont des échecs contrôlés. Il garantit l’intégrité
interne, pas l’authenticité face à une réécriture cohérente non signée.

L’intention versionnée est écrite atomiquement avant le rendu. Un verrou par
intention fait converger les demandes identiques ; un document différent est
refusé, y compris quand une tâche mémoire est déjà `GENERATING` ou `READY`.
Après publication complète mais avant reçu, un nouveau processus vérifie le
dossier et réconcilie le même identifiant sans second rendu.
