# Objet

Ce dossier regroupe tous les éléments de l'enquête nommée **Enquête Scam Concert**. Toutes les informations présentées dans ce dossier ont été collectées exclusivement à partir de sources ouvertes ou de documents transmis volontairement par les victimes et témoins. Aucune méthode intrusive ou d'accès non autorisé n'a été utilisée. Toutes les infotramtions, seront regroupées voir dans certains cas reformulées et compilées de façons organisées en suivants l'arborésence suivantes: 

# Objectifs

Ce dossier a pour objectif de :

- préserver les preuves recueillies ;
- documenter les recherches réalisées ;
- établir des liens entre les différentes informations collectées ;
- produire un rapport clair, vérifiable et exploitable par les services d'enquête.

# Principes

- Les preuves originales ne sont jamais modifiées.
- Toute modification est réalisée sur une copie.
- Chaque information est associée à une ou plusieurs preuves.
- Les hypothèses sont clairement distinguées des faits.
- Chaque recherche est documentée afin d'assurer la traçabilité des travaux.

# Arborésence
```bash
.
├── 00_README
├── 01_Preuves_Originales
│   ├── Captures_Ecran
│   ├── Conversations
│   ├── Documents
│   ├── Emails
│   ├── Photos
│   └── Videos
├── 02_Preuves_tratees
│   ├── Annotations
│   ├── Extractions
│   ├── OCR
│   └── Redactions
├── 03_Chronologie
│   ├── Chronologie.csv
│   ├── Chronologie.md
│   └── Enquete.sqlite
├── 04_Entites
│   ├── Adresses_Email
│   ├── Autres
│   ├── Comptes_Bancaires
│   ├── Comptes_Instagram
│   ├── Documents_Identite
│   ├── IBAN
│   ├── Personnes
│   └── Pseudonymes
├── 05_Recherches_Osint
│   ├── Divers
│   ├── Domaines
│   ├── Emails
│   ├── Google
│   ├── IBAN
│   ├── Recherche_Image
│   └── Reseaux_Sociaux
├── 06_Hypothèses
│   ├── A_Verifier.md
│   └── Hypotheses.md
├── 07_Rapports
│   └── Annexes
├── 08_Export_gendarmerie
│   ├── index.pdf
│   ├── Pièces_justificatives
│   └── Rapport_final.pdf
├── 09_Hash
│   ├── SHA256.txt
│   └── Vérification.md
├── Archives
├── enquete.sqlite
└── Rapport.md
```

## 00_README
Dossier contenant les explications concernant l'organisation structurelle du projet, les outils utilisées...

## 01_Preuves_Originales 
Dossier contenant les preuves originales sans aucune modifiaction. 
Un fichier hash sera lié à chacune d'entre elles afin de valider l'originalité du document.

## 01_Preuves_Originales 
Documents obtenues non modifiées non treaité authentifié pas fichiers hash.

## 03_Chronoligie
Documents et évènements classé chronologiqyuement. Une base sqlite y regroupera tout pour lier les différentes données.

## 04_Entites
Inventaire des entités identifiées au cours de l'enquête (personnes, pseudonymes, comptes, adresses électroniques, IBAN, documents, etc.).

## 05_Recherches_Osint
Infotrmation reccueillis en sources ouvertes disponibles publiquement. Les recherches sont documentées avec les outils utilisés, la date, les résultats obtenus et, le cas échéant, les sources consultées.

## 06_Hypotheses
Dossier contenant les différentes hypothèses forumlées avnt compilation pour le rapport final.

## 07_Rapports
Dossier contenant les differants rapport en corrélation direct avec le rapprochement des differentes preuves et documents obtenues.

## 08_Export_gendarmerie
Dossier compilé et prêt pour exploitation.

## 09_Hash
Dossier contenant les fichiers hash servant à l'authentification des preuves.
