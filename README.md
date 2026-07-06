# Objet

Ce dossier regroupe tous les éléments de l'enquête nommée **Ecris ici le nom de l'enquête**. Toutes les informations présentées dans ce dossier ont été collectées exclusivement à partir de sources ouvertes ou de documents transmis volontairement par les victimes et témoins. Aucune méthode intrusive ou d'accès non autorisé n'a été utilisée. Toutes les infotramtions, seront regroupées voir dans certains cas reformulées et compilées de façons organisées en suivants l'arborésence suivantes: 

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
❯ tree -L 4                                                                                              16:40
.
├── 00_BaseDeDonnées
│   └── Enquete.sqlite
├── 01_Preuves_Originales
│   ├── Captures_Ecran
│   ├── Conversations
│   ├── Documents
│   ├── Emails
│   ├── Photos
│   └── Videos
├── 02_Preuves_traitees
│   ├── Annotations
│   ├── Extractions
│   ├── OCR
│   └── Redactions
├── 03_Chronologie
│   ├── Chronologie.csv
│   └── Chronologie.md
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
│   ├── Annexes
│   └── Rapport.md
├── 08_Export_gendarmerie
│   ├── index.pdf
│   ├── Pièces_justificatives
│   └── Rapport_final.pdf
├── 09_Hash
│   ├── SHA256.txt
│   └── Vérification.md
├── Archives
├── Enquete-gui
│   ├── compile_flags.txt
│   ├── database
│   ├── docs
│   ├── enquete-gui
│   ├── include
│   │   ├── app.h
│   │   └── database.h
│   ├── Makefile
│   ├── resources
│   │   ├── css
│   │   ├── icons
│   │   └── ui
│   ├── src
│   │   ├── app.c
│   │   ├── app.o
│   │   ├── database.c
│   │   ├── database.o
│   │   ├── main.c
│   │   └── main.o
│   ├── tests
│   ├── tools
│   └── ui
└── README.md
```

## 01_Preuves_Originales 
Dossier contenant les preuves originales sans aucune modifiaction. 
Un fichier hash sera lié à chacune d'entre elles afin de valider l'originalité du document.

## 01_Preuves_Originales 
Documents obtenues non modifiées non treaité authentifié pas fichiers hash.

## 03_Chronologie
Documents et évènements classé chronologiquement.

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
