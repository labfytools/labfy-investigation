# Recherche OSINT assistée V1 — contrat de laboratoire

> **Statut :** `CURRENT` dans le worktree, validation exclusivement synthétique
> **Portée :** persistance V4, décisions de scope et parcours Web local de
> recherche assistée ; pas une qualification de fournisseur public

## 1. Limites de ce qui est livré

La recherche assistée V1 ajoute une persistance opérationnelle au `JobStore`
local. La migration est additive de V3 vers V4 : elle conserve les objets et
contrats de jobs locaux V3. Elle ne remplace pas la base métier V20 et ne
modifie pas la tranche financière V21.

Le `JobStore` reste servi par son worker mono-propriétaire existant. Les
opérations de recherche n'introduisent ni second ordonnanceur ni exécution
concurrente implicite. L'admission des objets de recherche ne démarre pas, à
elle seule, une campagne.

Cette tranche est qualifiée uniquement par des fixtures `SPECIMEN`, des
réponses injectées et un fournisseur de laboratoire loopback explicitement
configuré par le harness. Aucun fournisseur public n'est configuré ou qualifié
par ce contrat. Une absence de configuration ou d'autorisation de stockage
rend l'adapter concerné indisponible ; elle ne déclenche ni installation, ni
appel externe, ni repli vers un endpoint implicite.

## 2. Contrats persistants distincts

Les contrats versionnés de recherche assistée sont distincts des contrats
`labfy.local_plan.v1` et de session Web. Un cookie, un CSRF ou l'état de
l'interface ne font pas partie de leur contenu durable.

| Objet | Contrat | Rôle durable |
| --- | --- | --- |
| `ResearchPlan` | `labfy.research_plan.v1` | intention bornée, seeds, actions exactes et empreintes d'entrée |
| `ResearchAction` | composant du plan | capability, fournisseur, endpoint, sujet, divulgation, classe de contact et plafonds |
| `ScopeGrant` | `labfy.scope_grant.v1` | décision liée à une enquête et à un plan, actions sélectionnées, exclusions, expiration et budgets |
| `ResearchCampaign` | `labfy.research_campaign.v1` | identité idempotente d'une campagne admise ; elle ne vaut pas démarrage automatique |
| `ResearchResult` | `labfy.research_result.v1` | résultat brut référencé par chemin relatif et empreinte de contenu |
| `ResearchReceipt` | `labfy.research_receipt.v1` | décision de policy associée au résultat, à l'action, au grant et à la campagne |

L'admission est atomique : une même clé d'idempotence et le même contenu
retournent l'objet durable existant ; une même clé et un contenu différent sont
en conflit. Un plan ou un grant invalide ne laisse pas d'écriture partielle.
Un résultat ne peut pas être publié sans reçu visant une action explicitement
sélectionnée dans le grant de sa campagne.

## 3. Scope, policy et destinations

Avant tout contact, une décision est évaluée sur l'enquête, l'empreinte du
plan, l'action exacte, le grant et les budgets demandés. Les motifs observables
incluent notamment l'exclusion, la révocation, l'expiration, une enquête ou un
plan différent, une action non sélectionnée ou altérée, et un budget dépassé.
La révocation est durable et ne supprime pas l'historique du grant.

La validation HTTP compare exactement le sujet, l'endpoint, le port et la
sélection. Le profil réseau est une allowlist d'endpoints et de ports, jamais
une règle par suffixe. Les adresses non publiquement routables sont refusées
par le contrat du transport de production. Une redirection, un CNAME, une URL
nouvelle ou une seed dérivée ne propage jamais le scope : chacun constitue une
nouvelle destination à décider explicitement.

Le transport commun reste sans backend réseau lié et retourne donc
`UNAVAILABLE` sans contact. Son contrat interdit proxy, cookies, `netrc`,
authentification ambiante, redirection automatique et résolution fournie par
l'appelant. La variante libcurl est testée uniquement avec le routage de
fixture vers loopback ; son callback réseau refuse les adresses non publiques.
Les préfixes privés, locaux, documentaires et de benchmark ne sont pas des
adresses publiques. Les plafonds du corps compressé reçu sur le fil, du corps
décompressé et des en-têtes sont appliqués séparément, y compris sans
`Content-Length` et avec un transfert chunked.
Cela ne qualifie aucune configuration de production ni aucun fournisseur
externe.

## 4. Adapters normalisateurs — CURRENT en laboratoire

Les adapters suivants normalisent des données de fixture et préservent leurs
limites de périmètre :

- DNS : types bornés A, AAAA, CNAME, MX, NS, TXT, SOA et PTR ; ni `ANY`, ni
  AXFR, ni trace n'appartiennent au type public. Un cycle CNAME est signalé et
  ne crée pas de piste suivante implicite.
- RDAP : sélection du service depuis un bootstrap versionné et borné ; le
  bootstrap routé ne vaut jamais scope pour le service retourné.
- CDX : correspondance d'URL ou d'hôte exact, captures normalisées et curseur
  explicite ; il n'y a pas d'élargissement de la cible.
- Brave et SearXNG : disponibilité explicite (`UNAVAILABLE_CONFIG` ou
  `UNAVAILABLE_STORAGE_AUTHORIZATION`) tant que l'endpoint, la référence de
  secret et l'autorisation nécessaires ne sont pas réunis. Aucun secret n'est
  persisté dans ces contrats.
- Page : production de texte inerte borné ; scripts, liens, sous-ressources et
  pièces jointes ne sont ni exécutés ni suivis.

Ces résultats peuvent indiquer une `next_piste`, qui reste une information à
préparer et approuver ; elle ne confère aucune autorisation dérivée.

## 5. Publication et provenance C

Le cœur C sépare la décision, l'artefact brut, le résultat et le reçu. Le
résultat conserve un chemin relatif d'artefact et son SHA-256 ; la publication
atomique exige le reçu de policy correspondant. Cette provenance permet de
revenir au grant, à l'action et à la campagne, sans assimiler un contenu reçu
à une identité confirmée ou à une autorisation de poursuivre.

Les résultats de laboratoire du parcours Web sont stockés comme artefacts
privés de l'espace de travail de test. Le bridge expose un snapshot de
recherche, pas un accès arbitraire au répertoire d'artefacts ni une base de
provenance indépendante du cœur C.

La publication fichier/SQLite utilise un journal privé créé avant le renommage
final. Il lie l'identité déterministe du résultat à l'empreinte attendue. Si le
processus s'interrompt après le renommage, le rejeu de la même campagne vérifie
ce journal et l'artefact, puis enregistre atomiquement résultat et reçu sans
recontacter le fournisseur. Un état possédé incohérent est compensé avant de
retourner une erreur. Le journal est supprimé après publication durable ; cette
reprise n'ajoute ni table ni numéro de schéma.

## 6. Poste Web local — préparation et décision humaine

Le panneau de recherche du poste Web local prépare un plan depuis une
sélection et une révision de snapshot précises. L'interface présente les
actions et leurs divulgations, puis exige une décision explicite :

1. préparer un plan ;
2. sélectionner ou refuser les actions pour former le `ScopeGrant` ;
3. admettre une campagne pour les seules actions accordées ;
4. préparer et approuver séparément une seconde vague éventuelle.

L'action refusée reste refusée ; elle ne devient pas admissible au lancement
d'une autre action. La révocation du grant est une mutation explicite et
durable. Une révision de sélection périmée, un identifiant étranger ou une
forme de requête invalide sont refusés.

Les sessions, cookies et CSRF restent des contrôles de la surface Web et sont
distincts des contrats ci-dessus. Les validations couvrent l'isolation de deux
espaces de travail synthétiques A/B : un plan, grant, campagne ou résultat de
l'espace A n'est pas visible ni réutilisable dans l'espace B.

## 7. Procédure de validation autorisée

Les seuls parcours documentés ici utilisent les harnesses synthétiques du
dépôt et leurs répertoires temporaires. Les contrôles Web dédiés sont :

```sh
python3 prototypes/web-graph/tests/test_research_web.py
node prototypes/web-graph/tests/test_research_browser.mjs
```

Ils démarrent leur fournisseur `SPECIMEN` de loopback à durée de vie bornée et
ne nécessitent ni endpoint public, ni jeton, ni enquête réelle. Les tests C
`test_research_store`, `test_research_policy`, `test_research_transport` et
`test_research_adapters` couvrent respectivement la migration/persistance, les
décisions de policy, les bornes du transport/libcurl et les normalisations de
fixtures. Leur exécution relève du harness de validation de la tranche ; elle
ne constitue pas une autorisation de contacter des services externes.

## 8. Ce qui reste TARGET ou non qualifié

Restent hors de ce livrable : qualification et configuration d'un fournisseur
public, gestion opérationnelle de secrets, exécution de recherche hors
laboratoire, propagation automatique de scope, ordonnancement autonome de
recherche, ainsi que le serveur, le protocole d'événements, le renderer et le
packaging Web de production. Le Web local ne change pas le statut de GTK comme
parcours fonctionnel existant.
