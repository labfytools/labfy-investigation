# Dépendances externes

`libjpeg` décode JPEG, `libheif` HEIC/HEIF et Poppler GLib rend la première
page PDF du ticket #109. PNG utilise Cairo et la vidéo `GtkMediaFile` avec le
backend GTK/GStreamer. Aucun outil documentaire n’est lancé et aucun dérivé
n’est persisté.

| Fonction | Arch Linux | Ubuntu/Debian |
| --- | --- | --- |
| HEIC/HEIF | `libheif` | `libheif-dev` |
| PDF natif | `poppler-glib` | `libpoppler-glib-dev` |
| Vidéo GTK | `gst-plugins-base`, `gst-plugins-good` | `gstreamer1.0-plugins-base`, `gstreamer1.0-plugins-good` |

Les codecs vidéo supplémentaires restent optionnels.
Le Makefile vérifie explicitement GTK4, SQLite, libheif et Poppler GLib avant
compilation et renvoie vers ce document si une dépendance manque.

Ce fichier recense les outils utilisés par LabFy Investigation et leur
installation. Il sera complété à chaque intégration d’un nouvel outil.

La migration SQLite V13 et la gestion des propriétaires de
`preuve_entites` n'ajoutent aucune dépendance système.

## Principe d’installation

Les dépendances système sont installées avec le gestionnaire de paquets de la
distribution. Les outils OSINT Python sont installés dans un environnement
isolé afin de ne pas modifier le Python système.

Le dossier recommandé pour les exécutables OSINT est :

```text
~/.local/share/labfy-osint/bin
```

L’application ajoute automatiquement ce dossier au `PATH` lorsqu’il existe.
Pour les terminaux et les scripts, on peut également ajouter dans
`~/.profile` (ou `~/.zshrc`) :

```sh
export PATH="$HOME/.local/share/labfy-osint/bin:$PATH"
```

## Outils système

| Fonction | Exécutable | Arch Linux (pacman) | Ubuntu/Debian (apt) |
| --- | --- | --- | --- |
| OCR français et anglais | `tesseract` | `sudo pacman -S tesseract tesseract-data-fra tesseract-data-eng` | `sudo apt install tesseract-ocr tesseract-ocr-fra tesseract-ocr-eng` |
| Métadonnées | `exiftool` | `sudo pacman -S perl-image-exiftool` | `sudo apt install libimage-exiftool-perl` |
| Inspection, texte et rendu PDF | `pdfinfo`, `pdftotext`, `pdftoppm` | `sudo pacman -S poppler` | `sudo apt install poppler-utils` |
| Recherche DNS | `dig` | `sudo pacman -S bind` | `sudo apt install dnsutils` |
| Résolution réseau | `host` | `sudo pacman -S bind` | `sudo apt install bind9-host` |
| WHOIS | `whois` | `sudo pacman -S whois` | `sudo apt install whois` |
| Requêtes HTTPS | `curl` | `sudo pacman -S curl` | `sudo apt install curl` |
| Certificats/TLS | `openssl` | `sudo pacman -S openssl` | `sudo apt install openssl` |
| Récupération PDF | `qpdf` | `sudo pacman -S qpdf` | `sudo apt install qpdf` |
| Audit de mot de passe PDF | `john`, `pdf2john` | `sudo pacman -S john` | `sudo apt install john` |

Après installation, vérifier les outils avec :

```sh
command -v tesseract exiftool pdfinfo pdftotext pdftoppm dig host whois curl openssl qpdf john pdf2john
```

## Outils OSINT Python

Préparer l’environnement local :

```sh
python3 -m venv ~/.local/share/labfy-osint
~/.local/share/labfy-osint/bin/python -m pip install --upgrade pip
~/.local/share/labfy-osint/bin/pip install sherlock-project maigret holehe
```

Les commandes attendues par l’application sont :

```text
~/.local/share/labfy-osint/bin/sherlock
~/.local/share/labfy-osint/bin/maigret
~/.local/share/labfy-osint/bin/holehe
```

Sur Ubuntu, installer au préalable les outils de compilation et Python :

```sh
sudo apt install python3 python3-venv python3-pip
```

Sur Arch Linux :

```sh
sudo pacman -S python python-pip
```

## Vérification dans l’application

Tesseract est optionnel. L’étape OCR d’identité reste visible mais son lancement
est désactivé lorsque le registre d’outils ne fournit pas un exécutable vérifié.
Les langues réellement installées sont interrogées avec `--list-langs`; aucun
téléchargement n’est effectué par l’application.
Les choix sont triés et dédupliqués. `fra+eng` n’est proposé que lorsque les
deux packs sont réellement installés.

Au démarrage, le catalogue des outils indique le nombre d’exécutables
disponibles et leurs versions. Une commande non détectée doit d’abord être
vérifiée avec `command -v`, puis avec son option de version (`--version` ou
`-V` selon l’outil).

## Notes

- Ne pas installer ces outils dans le dépôt Git.
- ExifTool, Tesseract et Poppler sont optionnels pour le pivot EML. Leur
  absence laisse disponibles l'analyse des en-têtes et l'extraction MIME.
- Ne jamais stocker de jetons, mots de passe ou données privées dans ce
  fichier.
- Les versions peuvent varier selon la distribution ; noter ici toute
  commande particulière nécessaire à Ubuntu des forces de l’ordre.
