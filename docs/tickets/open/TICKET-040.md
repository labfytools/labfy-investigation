# TICKET-040 — Inventaire et qualification de l’arsenal OSINT

## Statut

À faire

## Priorité

Très haute

## Nature

Recherche, audit juridique, architecture et préparation des futures intégrations.

La présence d’un outil dans cet inventaire ne signifie pas qu’il est approuvé.

---

## Objectif

Constituer une base large, structurée et maintenable d’outils OSINT pouvant être intégrés à Labfy Investigation afin d’aider à enquêter légalement sur :

- pseudonymes et comptes publics ;
- adresses e-mail ;
- numéros de téléphone ;
- domaines, IP, certificats et infrastructures publiques ;
- documents, faux papiers, photos et vidéos ;
- historiques de pages supprimées ;
- dépôts publics ;
- IBAN, entreprises et cryptomonnaies ;
- relations entre personnes, comptes, domaines et moyens de paiement ;
- réseaux d’escrocs réutilisant les mêmes identifiants.

Le résultat doit être traçable, reproductible et présentable à des enquêteurs.

---

## Cadre légal non négociable

Labfy Investigation reste limité à :

- sources publiquement accessibles ;
- données remises légalement par une victime ou un enquêteur ;
- API et comptes utilisés conformément à leurs autorisations ;
- collecte sans contournement d’authentification ;
- recherche passive ou explicitement autorisée ;
- conservation de la méthode, de la source et de la sortie brute.

Sont exclus :

- intrusion ;
- exploitation de vulnérabilité ;
- brute force d’authentification ;
- credential stuffing ;
- phishing ;
- vol ou réutilisation de jetons ;
- usurpation ;
- social engineering ;
- accès à une ressource privée ;
- modification d’une ressource distante ;
- utilisation de données obtenues illicitement.

---

## Livrables

```text
docs/osint/TOOL_INVENTORY.md
docs/osint/LEGAL_AND_OPERATIONAL_POLICY.md
data/osint_tools.json
data/osint_tools.csv
```

Le JSON deviendra la future source de génération du catalogue étendu.

---

## Schéma d’une fiche outil

```text
identifier
name
repository
homepage
publisher
category
subcategories
description
license
license_verified
open_source_status
latest_release
last_activity
primary_language
installation_methods
ubuntu_packages
runtime_dependencies
input_types
output_formats
supports_json
supports_csv
supports_stdout
supports_file_output
requires_account
requires_api_key
requires_paid_service
network_activity
collection_mode
authentication_mode
rate_limit_risk
terms_of_service_risk
personal_data_risk
false_positive_risk
evidentiary_value
reproducibility
tool_version_command
integration_mode
integration_priority
legal_status
maintenance_status
decision
decision_reason
notes
reviewed_at
reviewed_by
```

---

## Valeurs normalisées

### open_source_status

```text
OPEN_SOURCE
SOURCE_AVAILABLE
OPEN_CLIENT_CLOSED_SERVICE
CLOSED_SOURCE
UNKNOWN
```

### collection_mode

```text
LOCAL_ONLY
PASSIVE_REMOTE
REMOTE_PUBLIC_REQUESTS
AUTHENTICATED_PUBLIC_API
ACTIVE_LOW_IMPACT
ACTIVE_INTRUSIVE
MIXED
```

### legal_status

```text
APPROVED_PASSIVE
APPROVED_WITH_CONFIGURATION
MANUAL_REVIEW_REQUIRED
RESTRICTED_TO_AUTHORIZED_TARGETS
REJECTED
UNKNOWN
```

### integration_mode

```text
DIRECT_CLI
DIRECT_LIBRARY
LOCAL_SERVICE
REMOTE_API
BROWSER_ASSISTED
EXPORT_IMPORT
REFERENCE_ONLY
REJECTED
```

### integration_priority

```text
P0
P1
P2
P3
BACKLOG
REJECTED
```

### decision

```text
INTEGRATE
PROTOTYPE
MONITOR
MANUAL_ONLY
REJECT
UNREVIEWED
```

---

## Notation sur 100

- valeur opérationnelle : 30 ;
- intégrabilité : 20 ;
- légalité et maîtrise du risque : 20 ;
- traçabilité : 15 ;
- maintenance : 10 ;
- coût : 5.

Un outil sous 50/100 ne doit pas être intégré sans justification écrite.

---

# Inventaire initial à auditer

## A. Pseudonymes, comptes et identités publiques

| Outil | Dépôt/projet | Usage | Priorité |
|---|---|---|---|
| Sherlock | `sherlock-project/sherlock` | Recherche de pseudonymes | P0 |
| Maigret | `soxoj/maigret` | Recherche approfondie de pseudonymes | P0 |
| Blackbird | `p1ngul1n0/blackbird` | Comptes et enrichissement | P1 |
| WhatsMyName | `WebBreacher/WhatsMyName` | Base de règles de présence | P0 |
| Enola | `theyahya/enola` | Recherche rapide de pseudonymes | P2 |
| Social Analyzer | `qeeqbox/social-analyzer` | Analyse multi-plateformes | P2 |
| socialscan | `iojw/socialscan` | Pseudonymes et e-mails | P1 |
| GHunt | `mxrch/GHunt` | Écosystème Google | P1 |
| GitFive | `mxrch/GitFive` | Corrélation GitHub | P1 |
| Holehe | `megadose/holehe` | Présence d’une adresse sur des services | P0 |
| MOSINT | `alpkeskin/mosint` | Enrichissement d’e-mails | P2 |
| h8mail | `khast3x/h8mail` | Jeux de données autorisés uniquement | P1 restreint |
| Ignorant | `megadose/ignorant` | Présence d’un téléphone sur des services | Revue |
| Instaloader | `instaloader/instaloader` | Contenus Instagram publics | Revue |
| snscrape | `JustAnotherArchivist/snscrape` | Collecte sociale publique | Revue |
| gallery-dl | `mikf/gallery-dl` | Conservation de médias publics | P1 |
| yt-dlp | `yt-dlp/yt-dlp` | Métadonnées et médias publics | P0 |

## B. E-mails

| Outil | Usage | Priorité |
|---|---|---|
| Holehe | Présence sur des services | P0 |
| MOSINT | Enrichissement multi-source | P2 |
| h8mail | Recherche dans des sources licites | Restreint |
| GHunt | Enrichissement Google | P1 |
| theHarvester | E-mails liés à un domaine | P0 |
| SpiderFoot | Modules d’enrichissement | P1 |
| Recon-ng | Framework modulaire | P2 |
| python-email-validator | Validation et normalisation | P1 |
| dnspython | Vérification DNS et MX | P1 |
| Clients OpenPGP | Identifiants publiés dans des clés | P2 |

## C. Téléphones

| Outil | Dépôt/projet | Usage | Priorité |
|---|---|---|---|
| PhoneInfoga | `sundowndev/phoneinfoga` | Pays, opérateur, formats, sources publiques | P0 |
| libphonenumber | `google/libphonenumber` | Parsing et validation | P0 |
| python-phonenumbers | `daviddrysdale/python-phonenumbers` | Adaptateur Python | P2 |
| Ignorant | `megadose/ignorant` | Présence sur certains services | Revue |
| Normalisation E.164 interne | Labfy | Canonicalisation | P0 |

Aucun outil ne doit prétendre identifier le titulaire réel d’un numéro sans source officielle ou judiciaire.

## D. Domaines, DNS, certificats et infrastructures

| Outil | Dépôt/projet | Usage | Priorité |
|---|---|---|---|
| Amass | `owasp-amass/amass` | Cartographie d’actifs | P1 |
| Subfinder | `projectdiscovery/subfinder` | Sous-domaines passifs | P0 |
| theHarvester | `laramies/theHarvester` | Domaines, e-mails, IP, noms | P0 |
| SpiderFoot | `smicallef/spiderfoot` | Automatisation multi-source | P1 |
| Recon-ng | `lanmaster53/recon-ng` | Framework modulaire | P2 |
| sn0int | `kpcyrd/sn0int` | Framework semi-automatique | P2 |
| BBOT | `blacklanternsecurity/bbot` | Orchestration récursive | Revue |
| dnsx | `projectdiscovery/dnsx` | Requêtes DNS structurées | P1 |
| Findomain | `Findomain/Findomain` | Sous-domaines | P2 |
| assetfinder | `tomnomnom/assetfinder` | Actifs passifs | P2 |
| dnstwist | `elceef/dnstwist` | Typosquatting et variantes | P0 |
| URLCrazy | `urbanadventurer/urlcrazy` | Variantes de domaines | P1 |
| CertStream | `CaliDog/certstream-python` | Certificats publics | P1 |
| massdns | `blechschmidt/massdns` | Résolution de masse | Restreint |
| puredns | `d3mondev/puredns` | Résolution de masse | Restreint |
| shuffledns | `projectdiscovery/shuffledns` | Résolution de masse | Restreint |
| Knockpy | `guelfoweb/knock` | Sous-domaines | Revue |
| Sublist3r | `aboul3la/Sublist3r` | Sous-domaines | Surveillance |
| httpx | `projectdiscovery/httpx` | Qualification HTTP | Restreint |
| httprobe | `tomnomnom/httprobe` | Services HTTP | Restreint |
| Katana | `projectdiscovery/katana` | Exploration Web | Restreint |
| RDAP clients | À sélectionner | Enregistrements normalisés | P0 |
| curl | Projet curl | Requêtes HTTP reproductibles | P0 |
| OpenSSL | Projet OpenSSL | Certificats TLS | P0 |
| dig / host / whois | Paquets système | DNS et enregistrements | P0 |

Les outils à forte volumétrie sont désactivés par défaut.

## E. Historique du Web et conservation

| Outil | Dépôt/projet | Usage | Priorité |
|---|---|---|---|
| waybackurls | `tomnomnom/waybackurls` | URLs Wayback | P0 |
| gau | `lc/gau` | URLs d’archives multiples | P0 |
| waymore | `xnl-h4ck3r/waymore` | Historique approfondi | P1 |
| ArchiveBox | `ArchiveBox/ArchiveBox` | Archivage local | P0 |
| Browsertrix Crawler | `webrecorder/browsertrix-crawler` | WARC/WACZ | P0 |
| Browsertrix | `webrecorder/browsertrix` | Service local | P2 |
| ArchiveWeb.page | `webrecorder/archiveweb.page` | Capture navigateur | P0 |
| pywb | `webrecorder/pywb` | Relecture WARC | P1 |
| warcio | `webrecorder/warcio` | Lecture/écriture WARC | P0 |
| ReplayWeb.page | `webrecorder/replayweb.page` | Relecture d’archives | P1 |
| SingleFile | `gildas-lormeau/SingleFile` | Copie autonome d’une page | P0 |
| monolith | `Y2Z/monolith` | Sauvegarde autonome | P1 |
| grab-site | `ArchiveTeam/grab-site` | Archivage WARC | Revue |
| Heritrix | `internetarchive/heritrix3` | Crawl d’archivage | Restreint |
| waybackpy | `akamhy/waybackpy` | Client Wayback | P2 |
| Perma | `harvard-lil/perma` | Préservation de références | Manuel/API |

## F. Documents, photos et vidéos

| Outil | Dépôt/projet | Usage | Priorité |
|---|---|---|---|
| ExifTool | `exiftool/exiftool` | Métadonnées multi-format | P0 |
| MediaInfo | `MediaArea/MediaInfo` | Audio et vidéo | P0 |
| FFmpeg / ffprobe | `FFmpeg/FFmpeg` | Analyse technique | P0 |
| Exiv2 | `Exiv2/exiv2` | Métadonnées d’images | P1 |
| Hachoir | `hachoir/hachoir` | Formats binaires | P1 |
| oletools | `decalage2/oletools` | Documents Office | P0 |
| DidierStevensSuite | `DidierStevens/DidierStevensSuite` | PDF et OLE | P1 |
| Poppler tools | Projet Poppler | PDF, texte et images | P0 |
| qpdf | `qpdf/qpdf` | Structure PDF | P0 |
| Tesseract OCR | `tesseract-ocr/tesseract` | OCR local | P0 |
| ImageMagick | `ImageMagick/ImageMagick` | Inspection de copies | P1 |
| OpenCV | `opencv/opencv` | Comparaison d’images | P2 |
| ImageHash | `JohannesBuchner/imagehash` | Empreintes perceptuelles | P1 |
| pHash | Projet pHash | Empreintes perceptuelles | P2 |
| file/libmagic | Paquet système | Identification de format | P0 |
| strings/binutils | Paquet système | Chaînes de caractères | P1 |
| MAT2 | `jvoisin/mat2` | Métadonnées sur copies | P2 |
| yt-dlp | `yt-dlp/yt-dlp` | Conservation de médias publics | P0 |
| gallery-dl | `mikf/gallery-dl` | Galeries publiques | P1 |
| InVID-WeVerify | Extension/service | Vérification manuelle | Manuel |
| face_recognition | `ageitgey/face_recognition` | Comparaison locale | Restreint |
| DeepFace | `serengil/deepface` | Comparaison locale | Restreint |
| InsightFace | `deepinsight/insightface` | Analyse faciale | Restreint |

Aucune recherche biométrique massive ne doit être intégrée.

## G. Dépôts publics et code

| Outil | Usage | Priorité |
|---|---|---|
| GitFive | Corrélation GitHub | P1 |
| GitHub CLI | API et données publiques | P0 |
| TruffleHog | Dépôts publics autorisés | Revue |
| Gitleaks | Secrets publiés | Revue |
| ggshield | Dépôts autorisés | Revue |
| ripgrep | Recherche locale | P0 |
| git-sizer | Structure de dépôt | P2 |
| Git natif | Historique, auteurs et dates | P0 |

Un secret découvert ne doit jamais être utilisé pour accéder à un compte.

## H. Réputation et APIs

| Outil/client | Usage | Priorité |
|---|---|---|
| vt-cli | VirusTotal | API |
| OTX Python SDK | AlienVault OTX | API |
| urlscan-go | URLScan | API |
| Shodan Python | Shodan | API |
| Censys Python | Censys | API |
| pyGreynoise | GreyNoise | API |
| PyMISP | MISP | P2 |
| OpenCTI client | OpenCTI | P2 |
| YARA | Classement local | P2 |
| capa | Analyse locale | Backlog |

Toute donnée envoyée à un tiers doit être affichée avant l’appel.

## I. Corrélation et graphes

| Outil | Usage | Priorité |
|---|---|---|
| NetworkX | Algorithmes de graphe | P1 |
| igraph | Graphes performants | P2 |
| Graphviz | Rendu | P0 |
| Gephi | Analyse visuelle externe | Export |
| Cytoscape | Analyse visuelle externe | Export |
| OpenCTI | Référence de modèle | Référence |
| MISP | Référence d’indicateurs | Référence |
| Aleph | Recherche documentaire et entités | P1 |
| OpenSanctions | Entités et sanctions publiques | P1 |
| FollowTheMoney | Modèle de relations | P1 |
| Neo4j Community | Base graphe externe | P3 |
| OSINTBuddy | Graphe d’enquête | Surveillance |

Labfy conserve son propre modèle de données.

## J. Géolocalisation

| Outil | Usage | Priorité |
|---|---|---|
| geopy | Géocodage configurable | P2 |
| Nominatim | Géocodage OpenStreetMap | P1 |
| OSMnx | Réseaux géographiques | P3 |
| Overpass API | Données OpenStreetMap | P2 |
| QGIS | Analyse externe | Export |
| SunCalc | Position du soleil | P2 |
| Skyfield | Calculs astronomiques | P3 |
| ExifTool | Coordonnées de médias | P0 |

## K. IBAN, entreprises et cryptomonnaies

| Outil | Usage | Priorité |
|---|---|---|
| python-stdnum | Validation IBAN et identifiants | P0 |
| iban4j | Validation IBAN | P3 |
| OpenSanctions | Personnes et organisations | P1 |
| FollowTheMoney | Relations structurées | P1 |
| bitcoin-etl | Données publiques Bitcoin | P2 |
| ethereum-etl | Données publiques Ethereum | P2 |
| GraphSense | Analyse de blockchains | P2 |
| BlockSci | Analyse locale | Surveillance |
| mempool | Explorateur Bitcoin open source | P2 |
| Bitcoin Core | Nœud local | P3 |
| Geth | Nœud Ethereum local | P3 |

Un IBAN ne permet pas d’identifier publiquement son titulaire. Labfy se limite à la validation, au pays, à la banque lorsqu’elle est publique et aux corrélations entre dossiers.

---

# Outils exclus du mode OSINT standard

```text
nmap
masscan
naabu
nuclei
gobuster
ffuf
dirsearch
sqlmap
hydra
metasploit
Sn1per
Nikto
WPScan
scanners actifs Burp
outils d’exploitation
outils de credential stuffing
outils de phishing
```

Décision initiale :

```text
integration_mode = REJECTED
decision = REJECT
```

---

# Vague prioritaire pour l’affaire d’escroquerie

## P0

```text
Sherlock
Maigret
WhatsMyName
Holehe
PhoneInfoga
libphonenumber
GitHub CLI
ExifTool
MediaInfo
ffprobe
Tesseract OCR
Poppler tools
qpdf
oletools
file/libmagic
yt-dlp
waybackurls
gau
ArchiveBox
Browsertrix Crawler
ArchiveWeb.page
warcio
SingleFile
theHarvester
Subfinder
dnstwist
python-stdnum
Graphviz
```

## P1

```text
Blackbird
GHunt
GitFive
h8mail avec sources licites uniquement
gallery-dl
SpiderFoot
Amass
dnsx
URLCrazy
OpenSanctions
FollowTheMoney
Aleph
NetworkX
ImageHash
VirusTotal CLI
urlscan
```

---

# Audit obligatoire par outil

1. données envoyées sur Internet ;
2. domaines et API contactés ;
3. compte, cookie ou jeton requis ;
4. compatibilité avec les CGU ;
5. volume de requêtes ;
6. accès strictement public ;
7. capacité de modifier une ressource distante ;
8. risque de faux positif ;
9. précision des sources ;
10. sortie brute conservable ;
11. reproductibilité ;
12. commande de version ;
13. licence et redistribution ;
14. installation Ubuntu ;
15. dépendances ;
16. coût ou API obligatoire ;
17. activité du projet ;
18. vulnérabilités connues ;
19. télémétrie ;
20. scripts d’installation distants.

Aucun `curl | sh`, `wget | bash`, `sudo pip install` ou `sudo npm install -g` ne doit être exécuté sans audit.

---

# Modèle de sécurité d’intégration

```text
arguments séparés
aucun shell
utilisateur non privilégié
dossier temporaire
environnement minimal
timeout
limites stdout/stderr
annulation
journal
empreinte du binaire
version
domaines contactés
sortie brute UTC
SHA-256
```

---

# Valeur probatoire

Labfy doit distinguer :

```text
résultat de l’outil
source publique
copie locale
interprétation
corrélation
hypothèse
```

Chaque exécution doit conserver :

```text
outil
version
commande
arguments
date UTC
source
URL
sortie brute
capture
archive
SHA-256
statut HTTP
notes
niveau de confiance
```

Un résultat Sherlock, Maigret, Holehe ou PhoneInfoga ne prouve jamais une identité à lui seul.

---

# Phases

## Phase 1

Créer le JSON, le CSV et le schéma.

## Phase 2

Auditer toute la liste P0.

## Phase 3

Auditer la liste P1.

## Phase 4

Attribuer note, priorité, mode d’intégration, statut juridique et décision.

## Phase 5

Produire le rapport des outils retenus, manuels, surveillés et rejetés.

## Phase 6

Validation humaine avant tout passage à `INTEGRATE`.

---

# Contrôles automatisés futurs

- identifiants uniques ;
- noms non vides ;
- dépôt renseigné ;
- licence vérifiée pour tout outil approuvé ;
- décision renseignée ;
- aucune priorité P0 pour un outil rejeté ;
- aucun outil actif marqué passif ;
- JSON valide ;
- CSV généré depuis le JSON ;
- aucune clé API ;
- aucun secret ;
- aucune donnée issue d’une enquête réelle.

---

# Critères d’acceptation

- au moins 80 outils ou composants recensés ;
- toutes les catégories couvertes ;
- tous les P0 audités ;
- licences P0 vérifiées ;
- installation Ubuntu P0 documentée ;
- qualification juridique P0 ;
- commande de version P0 ;
- séparation claire passif/actif/intrusif ;
- aucun outil rejeté proposé à l’utilisateur ;
- inventaire Markdown, JSON et CSV ;
- décisions justifiées ;
- feuille de route d’intégration ;
- document présentable à des enquêteurs.

---

# Suite prévue

```text
#041 — Initialisation asynchrone des dépendances
#042 — Adaptateurs pseudonymes
#043 — Adaptateurs e-mail
#044 — Adaptateurs téléphone
#045 — Documents et métadonnées
#046 — Archivage probatoire du Web
#047 — Domaines et infrastructures passives
#048 — Graphe de corrélation
```

