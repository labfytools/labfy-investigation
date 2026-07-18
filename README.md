# Objet

Ce dossier regroupe tous les éléments de l'enquête nommée **Ecris ici le nom de l'enquête**. Toutes les informations présentées dans ce dossier ont été collectées exclusivement à partir de sources ouvertes ou de documents transmis volontairement par les victimes et témoins. Aucune méthode intrusive ou d'accès non autorisé n'a été utilisée. Toutes les informations, seront regroupées voire, dans certains cas reformulées et compilées de façons organisées en suivant l'arborescence suivantes: 

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

## 01_Preuves_Traitees 
Documents obtenues non modifiées non traité authentifiés par un fichiers hashage.

## 03_Chronologie
Documents et évènements classé chronologiquement.

## 04_Entites
Inventaire des entités identifiées au cours de l'enquête (personnes, pseudonymes, comptes, adresses électroniques, IBAN, documents, etc.).

## 05_Recherches_Osint
Informations reccueillis en sources ouvertes disponibles publiquement. Les recherches sont documentées avec les outils utilisés, la date, les résultats obtenus et, le cas échéant, les sources consultées.

## 06_Hypotheses
Dossier contenant les différentes hypothèses forumlées avant compilation pour le rapport final.

## 07_Rapports
Dossier contenant les différents rapport en corrélation direct avec le rapprochement des differentes preuves et documents obtenues.

## 08_Export_gendarmerie
Dossier compilé et prêt pour exploitation.

## 09_Hash
Dossier contenant les fichiers hash servant à l'authentification des preuves.

# Convention de nommage

Les identifiants utilisés dans ce dossier sont normalisés.

| Préfixe | Signification |
|----------|---------------|
| P | Preuve |
| E | Entité |
| R | Recherche |
| H | Hypothèse |
| S | Source |
| PER | Personne |
| TAG | Tag |

Exemples :

P0001
E0007
S0003
H0002

# Intégrité des preuves

- Les fichiers originaux ne sont jamais modifiés.
- Toute analyse est réalisée sur une copie.
- Chaque preuve possède une empreinte SHA-256.
- Les modifications sont documentées.

# Enquete-gui

Application développée en C17 utilisant GTK4 et SQLite.

Elle permet de :

- gérer les preuves ;
- gérer les entités ;
- documenter les recherches ;
- générer les rapports ;
- faciliter la navigation dans la base de données.

## Licence

Ce projet est distribué sous licence MIT.

Voir le fichier `LICENSE` pour plus d'informations.
