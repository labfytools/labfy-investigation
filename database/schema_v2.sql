/******************************************************************************
 * Labfy Investigation
 *
 * Migration du schéma SQLite V1 vers V2
 ******************************************************************************/

/*
 * Nom du fichier tel qu’il existait avant son import.
 *
 * La colonne name existante conserve le nom interne utilisé dans
 * l’arborescence de l’enquête.
 */
ALTER TABLE preuves
ADD COLUMN original_name TEXT;

/*
 * Date déclarée de collecte de la preuve.
 *
 * Elle est distincte de :
 *
 * - file_created_at : date technique connue du fichier ;
 * - imported_at     : date d’import dans l’enquête.
 */
ALTER TABLE preuves
ADD COLUMN collected_at TEXT;

/*
 * Description textuelle initiale de la provenance.
 *
 * Une future évolution pourra relier une preuve à la table sources
 * avec un identifiant métier.
 */
ALTER TABLE preuves
ADD COLUMN source TEXT;

/*
 * Correspondance avec EvidenceIntegrityStatus :
 *
 * 0 = UNKNOWN
 * 1 = VALID
 * 2 = MISSING
 * 3 = MODIFIED
 * 4 = ERROR
 */
ALTER TABLE preuves
ADD COLUMN integrity_status INTEGER NOT NULL DEFAULT 0
CHECK (
    integrity_status BETWEEN 0 AND 4
);

/*
 * Les éventuelles preuves V1 utilisent leur ancien nom visible comme
 * nom original afin de rester lisibles après migration.
 */
UPDATE preuves
SET original_name = name
WHERE original_name IS NULL
   OR length(trim(original_name)) = 0;

CREATE INDEX idx_preuves_imported_at
ON preuves(imported_at);

/*
 * SQLite ne permet pas d’ajouter directement une contrainte NOT NULL
 * à une colonne ajoutée lorsque des lignes peuvent déjà exister.
 *
 * Ces triggers renforcent donc les insertions et modifications V2.
 */
CREATE TRIGGER preuves_v2_validate_insert
BEFORE INSERT ON preuves
FOR EACH ROW
WHEN
       NEW.original_name IS NULL
    OR length(trim(NEW.original_name)) = 0
    OR NEW.size_bytes IS NULL
    OR NEW.size_bytes < 0
    OR NEW.sha256 IS NULL
    OR length(NEW.sha256) != 64
    OR NEW.sha256 != lower(NEW.sha256)
    OR NEW.integrity_status NOT BETWEEN 0 AND 4
BEGIN
    SELECT RAISE(
        ABORT,
        'La preuve ne respecte pas les contraintes du schéma V2.'
    );
END;

CREATE TRIGGER preuves_v2_validate_update
BEFORE UPDATE ON preuves
FOR EACH ROW
WHEN
       NEW.original_name IS NULL
    OR length(trim(NEW.original_name)) = 0
    OR NEW.size_bytes IS NULL
    OR NEW.size_bytes < 0
    OR NEW.sha256 IS NULL
    OR length(NEW.sha256) != 64
    OR NEW.sha256 != lower(NEW.sha256)
    OR NEW.integrity_status NOT BETWEEN 0 AND 4
BEGIN
    SELECT RAISE(
        ABORT,
        'La preuve ne respecte pas les contraintes du schéma V2.'
    );
END;
