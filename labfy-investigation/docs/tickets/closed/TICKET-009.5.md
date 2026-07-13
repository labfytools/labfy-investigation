# Ticket #009.5

## Titre

Ajouter une cible `make test`.

---

## Objectif

Automatiser la compilation et l'exécution de l'ensemble des tests du projet.

La commande suivante doit suffire :

```bash
make test
```

---

## Responsabilités

Le système de build doit :

- compiler chaque fichier de test ;
- lier uniquement les modules nécessaires ;
- exécuter tous les tests ;
- arrêter l'exécution si un test échoue ;
- supprimer les binaires de test avec `make clean`.

---

## Hors périmètre

Ce ticket ne doit pas :

- ajouter de nouveaux tests métier ;
- modifier le comportement des modules ;
- introduire un framework de test externe ;
- produire des rapports de couverture ;
- lancer Valgrind automatiquement.

---

## Fichiers concernés

```text
Makefile
```

Éventuellement :

```text
tests/Makefile
```

uniquement si le Makefile principal devient difficile à lire.

---

## Tests concernés

```text
tests/test_investigation_node.c
tests/test_investigation_tree_model.c
```

---

## Comportement attendu

La commande :

```bash
make test
```

doit :

1. compiler les binaires de test ;
2. exécuter chaque test ;
3. afficher clairement le résultat ;
4. retourner un code d'erreur si un test échoue.

---

## Critères d'acceptation

- [ ] `make test` compile tous les tests.
- [ ] `make test` exécute tous les tests.
- [ ] Un test en échec arrête la commande.
- [ ] `make clean` supprime les binaires de test.
- [ ] Aucun binaire de test n'est versionné.
- [ ] Le projet principal continue de compiler.
- [ ] Aucun warning.

---

## Commit attendu

```text
build: add automated test target
```
