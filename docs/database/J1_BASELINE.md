# Baseline J1 — runtime V20 et fondation financière V21

> État vérifié le 2026-09-25 sur le worktree local non commité.

| Contrat | État réellement validé | Limite |
|---|---|---|
| Création runtime | `Database` crée et migre jusqu'à V20 | V21 n'est pas appelée par le runtime |
| Migrations publiées | V1 à V20, testées sur fixtures synthétiques | aucune migration V20 → V21 dans J1/J2 |
| V21 directe | `schema_install_v21_direct()` accepte une base strictement vide | refuse une base non vide ; usage tests/bases neuves seulement |
| Modèles financiers | observations, propositions, révisions, décisions, comptes et transactions | aucune promotion ou entité implicite |
| DAO financiers | persistance transactionnelle et réouverture | dépend du schéma autonome V21 de test |
| Intégrité | contraintes, FK, rollback et refus d'entrées invalides couverts | aucun backfill legacy |

Snapshot reproductible : HEAD `ccf9bdd87a2d113593f12577dc8bd651cafa52f6`
plus les changements locaux dont les empreintes sont relevées dans le compte
rendu de J1/J2. HEAD seul ne décrit pas cette baseline.

Commandes ciblées :

```bash
make -j8 tests/test_financial_foundation tests/test_bank_structured_extractor \
  tests/test_schema_v21 tests/test_financial_dao tests/test_database
./tests/test_financial_foundation
./tests/test_bank_structured_extractor
./tests/test_schema_v21
./tests/test_financial_dao
./tests/test_database
```

Les numéros 1 à 20 sont occupés par le runtime ; 21 désigne la fondation
autonome déjà présente. Le prochain numéro runtime ne sera choisi qu'avec la
future migration financière et ses tests de conservation. Aucun V22 n'est
réservé par ce document. J1 reste donc partiel au sens roadmap : la migration
runtime financière demeure volontairement différée, sans bloquer le prototype
J2 qui ne lit aucune base.
