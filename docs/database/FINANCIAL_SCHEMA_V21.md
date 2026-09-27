# Schéma financier V21 — tranche 3

`database/schema_v21.sql` est un schéma autonome réservé à la création directe
de bases neuves et aux tests synthétiques. `schema_current.sql` et la version
runtime restent volontairement en V20 jusqu’à la migration de tranche 4.

Les tables `bank_observations`, `bank_proposals`,
`bank_observation_revisions` et `bank_decisions` séparent le brut immuable,
les regroupements proposés, les corrections humaines et l’historique des
décisions. Leurs historiques et leurs provenances sont append-only.

`bank_accounts` étend une ligne existante de `entites` de type
`bank_account`. `declared_bank_holders` conserve seulement un nom déclaré et
des liens explicites facultatifs. `financial_transactions` conserve un montant
décimal textuel exact et un statut observé, sans certification externe.

Les DAO ne créent ni entité canonique, ni personne, ni relation, ni compte ou
transaction implicite. Les données legacy ne sont ni lues ni migrées dans
cette tranche.
