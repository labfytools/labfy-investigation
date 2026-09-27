# Toolkit, adapters et capabilities

> **Statut :** contrat `TARGET` et inventaire `CURRENT` vérifié dans le dépôt
> **Important :** la présence dans ce document ne signifie ni installation ni
> disponibilité sur la machine

## 1. Contrat cible

Le frontend consomme des capabilities, pas une liste de commandes. Une
capability déclare au minimum :

```text
tool_id / tool_version          adapter_id / adapter_version
capability_id                   output_contract_version
accepted_object_types           produced_object_types
action_class                    effects
requirements                    network_contact
credentials_required            cost / risk
availability / health           unavailable_reason
```

Familles d'adapters : `NATIVE`, `SUBPROCESS`, `HTTP_API`, `MANUAL`. Un adapter
ne dépend ni de GTK, ni du frontend Web, ni directement de SQLite. Il conserve
le brut et produit des observations normalisées ; les services contrôlent jobs,
scope, persistance et publication.

Le futur gestionnaire distingue `ready`, `missing`, `incompatible`,
`credentials_missing` et `unhealthy`, avec version, origine d'installation et
profil d'exécution. Installation et mise à jour restent explicites et
versionnées ; aucun clic sur un nœud ne les déclenche.

## 2. État réellement présent — CURRENT

Le catalogue C enregistre les outils ci-dessous comme **optionnels**. Leur
détection ne prouve pas une intégration complète. L'action DNS dispose d'un
parcours structuré ; les recherches Sherlock, Maigret et Holehe sont des actions
cataloguées dont la disponibilité dépend de l'outil. Les parcours documentaires
emploient déjà ExifTool, Tesseract et qpdf de façon spécialisée.

| Outil / source primaire | Rôle actuel | Licence et dépendance | Entrée / sortie | Réseau, secrets et limites |
|---|---|---|---|---|
| [`dig`, `host` — BIND](https://www.isc.org/bind/) | DNS ; `dig` a un parcours révisable | licence du paquet à vérifier avant redistribution ; exécutables BIND optionnels | domaine / texte puis propositions DNS | `TARGET` pour DNS réel ; aucun secret ; contrat Toolkit générique absent |
| [`whois`](https://github.com/rfc1036/whois) | catalogue optionnel | licence du paquet à vérifier ; exécutable optionnel | cible / texte | registre tiers ; aucun secret prévu ; sortie hétérogène |
| [curl](https://curl.se/) | catalogue optionnel HTTP | licence curl à vérifier avant redistribution ; exécutable optionnel | URL / octets | tiers ou cible ; secrets possibles ; pas une capability générique sûre |
| [OpenSSL](https://www.openssl.org/) | catalogue optionnel TLS | licence/version du paquet à qualifier ; exécutable optionnel | endpoint ou fichier / texte | cible si connexion ; sortie dépendante de version |
| [Tesseract](https://github.com/tesseract-ocr/tesseract) | OCR spécialisé | Apache-2.0 vérifiée ; Leptonica et modèles de langue | image / texte, TSV | `NONE` ; qualité dépendante du document et des langues |
| [ExifTool](https://exiftool.org/) | métadonnées spécialisées | licence double à vérifier avant redistribution ; Perl et modules packagés | fichier / JSON | `NONE` ; contenu et métadonnées non fiables |
| [qpdf](https://qpdf.readthedocs.io/) | traitement PDF spécialisé | Apache-2.0 ou Artistic-2.0 vérifiée ; exécutable et bibliothèques packagées | PDF / PDF ou diagnostic | `NONE` ; PDF hostile, chiffrement et taille à borner |
| [John, pdf2john](https://www.openwall.com/john/) | récupération PDF facultative | licence/édition à vérifier ; exécutables optionnels | PDF / candidats et diagnostic | `NONE` ; coût CPU/temps élevé, aucun succès garanti |
| [Sherlock](https://github.com/sherlock-project/sherlock), [Maigret](https://github.com/soxoj/maigret) | actions username cataloguées | licences et dépendances Python à qualifier avant admission | username / résultats propres à l'outil | nombreux tiers ; conditions par site ; normalisation uniforme absente |
| [Holehe](https://github.com/megadose/holehe) | action e-mail cataloguée, à requalifier | licence et dépendances Python à requalifier | e-mail / réponses propres à l'outil | nombreux tiers ; effets observables possibles ; statut audit « à requalifier » |

Ce statut provient de `src/core/tool_catalog.c`,
`src/models/osint_action_catalog.c` et des parcours spécialisés. Il ne décrit
pas la présence des exécutables sur un poste donné.

## 3. Candidats recommandés par l'audit — TARGET

Les licences marquées « vérifiée » ont été confrontées à la source officielle
pendant J0 ; les autres restent une condition d'admission, pas une supposition.
Les versions ne sont volontairement pas figées ici.

| Capacité / outil | Statut Labfy et référence primaire | Licence / dépendances | Entrée / sortie cible | Contact réseau, secrets et limites |
|---|---|---|---|---|
| [ExifTool](https://exiftool.org/) | `CURRENT` spécialisé ; candidat adapter local | Perl et modules ; Perl Artistic/GPL à vérifier avant redistribution | fichier → JSON + brut | `NONE` ; formats non fiables, tailles bornées |
| [Tesseract](https://github.com/tesseract-ocr/tesseract) | `CURRENT` spécialisé ; candidat adapter OCR | Leptonica et modèles ; Apache-2.0 vérifiée | image → texte/TSV/hOCR selon contrat | `NONE` ; langues/modèles, qualité variable |
| [qpdf](https://qpdf.readthedocs.io/) | `CURRENT` spécialisé ; candidat adapter PDF | bibliothèques packagées ; Apache-2.0 ou Artistic-2.0 vérifiée | PDF → diagnostic ou dérivé | `NONE` ; chiffrement, bombes et taille |
| [RDAP](https://www.icann.org/rdap) | protocole candidat, aucun fournisseur choisi | client HTTP/provider à choisir ; licence à vérifier | domaine/IP/ASN → JSON | `THIRD_PARTY`, rate limits et conditions propres au registre |
| [Certificate Transparency](https://certificate.transparency.dev/) | écosystème candidat, aucune API unique choisie | client/provider à choisir ; licence à vérifier | domaine/certificat → entrées structurées | `THIRD_PARTY`, couverture et conditions propres au journal/service |
| [Wayback CDX](https://github.com/internetarchive/wayback/tree/master/wayback-cdx-server) | API candidate | client HTTP ; licence et conditions à vérifier | URL/domaine → index CDX/JSON | `THIRD_PARTY`, disponibilité, quotas, contenu archivé non fiable |
| [Subfinder](https://github.com/projectdiscovery/subfinder) | candidat passif, non intégré | binaire Go et providers ; MIT vérifiée | domaine → JSONL/liste avec sources | `THIRD_PARTY`, clés possibles, quotas et conditions par provider |
| [theHarvester](https://github.com/laramies/theHarvester) | candidat multi-source, non intégré | Python/modules/providers ; licence à vérifier | domaine → résultats structurés/bruts | `THIRD_PARTY`, clés et quotas selon modules |
| [Maigret](https://github.com/soxoj/maigret) | `CURRENT` catalogué, adapter commun non livré | Python/modules ; licence à vérifier | username → JSON/rapport + brut | `THIRD_PARTY`, faux positifs et conditions des sites |

RDAP est un protocole et Certificate Transparency un écosystème de journaux :
chaque provider futur aura son propre contrat, ses limites et ses conditions.

## 4. Candidats optionnels ou suspendus

| Outil/service | Position F0 | Condition avant intégration |
|---|---|---|
| [Sherlock](https://github.com/sherlock-project/sherlock) | optionnel, catalogué `CURRENT` | licence, formats, limites et dépendance des sources |
| [Amass](https://github.com/owasp-amass/amass) | optionnel | mode passif/actif, licence et ressources |
| [dnsx](https://github.com/projectdiscovery/dnsx) | optionnel | contact DNS, concurrence et provenance |
| [httpx](https://github.com/projectdiscovery/httpx) | optionnel | classe `PUBLIC_ACTIVE`, scope et redirections |
| [Katana](https://github.com/projectdiscovery/katana) | optionnel | crawling actif, budgets et contenus hostiles |
| [SpiderFoot](https://github.com/smicallef/spiderfoot) | optionnel | architecture, licences des modules et duplication fonctionnelle |
| [Nominatim](https://nominatim.org/) | provider géographique optionnel | politique d'usage, données transmises et cache |
| [GraphSense](https://graphsense.org/) | optionnel spécialisé | API/provider, licence, coût et interprétation financière |
| [Holehe](https://github.com/megadose/holehe) | à requalifier malgré le catalogue actuel | effets observables, maintenance, licence et légitimité des requêtes |
| Metagoofil, Photon, BlockSci | non retenus actuellement | nouvelle analyse explicite nécessaire |
| Nmap, Nuclei, Greenbone/OpenVAS | future piste autorisée séparée | seulement laboratoire/cibles autorisées, jamais v0.1.0 |

## 5. Runner et contrat machine

Le lot J4 est désormais `CURRENT` pour l'analyse EML native et ExifTool. Son
contrat détaillé, ses profils et limites sont dans
[`LOCAL_TOOLKIT_RUNNER.md`](../architecture/LOCAL_TOOLKIT_RUNNER.md). Le registre
est interrogé à l'exécution et lors de la projection ; les autres outils de ce
catalogue ne sont pas implicitement migrés.

Un adapter subprocess utilise des arguments séparés, un environnement minimal,
un répertoire privé, des limites CPU/mémoire/fichiers/processus, timeout,
annulation et arrêt des descendants. `stdout` et `stderr` sont capturés en
streaming avec plafonds distincts.

Le contrat distingue : aperçu tronqué, artefact réellement conservé et statut
`PARTIAL`. Une empreinte ne peut pas être présentée comme celle d'un contenu
complet qui n'a pas été conservé. La publication passe par staging, validation,
renommage puis transaction ou compensation documentée.

Pour Python/CLI/API, le contrat machine est versionné : schéma de sortie,
erreurs structurées, timeout, plafond, compatibilité et fixtures synthétiques.
Les sorties d'outil restent non fiables.

## 6. Règle d'admission

Avant intégration : source primaire, licence et redistribution, maintenance,
version, formats, dépendances, contact réseau, secrets, coût, limites, isolation,
normalisation, provenance et tests synthétiques sont documentés. Une absence
d'outil désactive proprement la capability. Aucun auto-update, téléchargement
exécutable ou installation silencieuse n'est autorisé.
