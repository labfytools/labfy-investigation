# Documentation de Labfy Investigation

> **Version cible :** v0.1.0 Foundation, en préparation et non publiée
> **Interface actuelle :** GTK4
> **Interface cible :** Web local-first ; prototype expérimental J2/J3 présent

Cette page sépare l'usage du logiciel actuel, la direction acceptée et les
travaux futurs. Une description `TARGET` ne signifie jamais que la fonction est
déjà disponible.

## Utiliser et contribuer aujourd'hui — CURRENT

- [README principal](../README.md) : présentation et fonctions disponibles ;
- [dépendances](DEPENDENCE.md) : compilation et outils optionnels ;
- [développement](DEVELOPMENT.md) et [conventions](CONVENTIONS.md) ;
- [architecture de la base](database/DATABASE_ARCHITECTURE.md) et
  [audit courant du schéma](database/SCHEMA_AUDIT_CURRENT.md) ;
- [test manuel du pivot EML](testing/EML_PIVOT_MANUAL_TEST.md).

Les guides GTK restent applicables tant que l'interface actuelle est maintenue.

## Direction v0.1.0 — TARGET

- [architecture actuelle et cible](ARCHITECTURE.md) : document canonique ;
- [contrat du graphe d'investigation](architecture/INVESTIGATION_GRAPH.md) ;
- [Toolkit et statuts d'intégration](osint/TOOLKIT.md) ;
- [design system](ui/DESIGN_SYSTEM.md) ;
- [sécurité, scope et secrets](security/SECURITY_MODEL.md) ;
- [roadmap J0–J9](ROADMAP.md) et [backlog préparé](BACKLOG.md).
- [baseline J1 V20/V21](database/J1_BASELINE.md) ;
- [prototype Web Graph J2](../prototypes/web-graph/README.md) et
  [ADR de pile expérimentale](architecture/decisions/0004-web-graph-prototype.md).
- [projection cœur → Web J3 en lecture seule](architecture/CORE_GRAPH_READONLY.md).
- [analyse EML persistée → graphe Web J3](architecture/EML_ANALYSIS_TO_GRAPH.md).
- [Toolkit local, runner et ExifTool J4](architecture/LOCAL_TOOLKIT_RUNNER.md).
- [preuves, chronologie et rapport local J9](architecture/EVIDENCE_TIMELINE_REPORT.md).
- [source SVG éditable de la bannière](../resources/images/labfy-investigation-banner.svg).

## Décisions acceptées

- [ADR-0001 — interface Web local-first](architecture/decisions/0001-web-local-first.md) ;
- [ADR-0002 — graphe comme interface principale](architecture/decisions/0002-graph-first.md) ;
- [ADR-0003 — réutilisation et polyglottisme contrôlé](architecture/decisions/0003-controlled-polyglot.md).

Ces ADR figent une direction. JavaScript/SVG/Python/SSE décrivent le prototype
expérimental J2 ; le framework, le moteur graphique, la bibliothèque HTTP et le
packaging de production restent à décider avant J6.

## Historique

Les audits versionnés décrivent un état daté. Ils éclairent les décisions mais
ne remplacent ni le code, ni les migrations, ni les tests de la branche ouverte.
