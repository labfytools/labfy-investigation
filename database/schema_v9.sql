CREATE TABLE IF NOT EXISTS relation_types
(
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    code           TEXT UNIQUE,
    label          TEXT NOT NULL,
    normalized_key TEXT NOT NULL UNIQUE,
    description    TEXT,
    is_system      INTEGER NOT NULL DEFAULT 0 CHECK (is_system IN (0, 1)),
    CHECK (code IS NULL OR length(trim(code)) > 0),
    CHECK (length(trim(label)) > 0),
    CHECK (length(normalized_key) > 0)
);

INSERT OR IGNORE INTO relation_types(code,label,normalized_key,description,is_system) VALUES
('resolves_to','Résout vers','résout vers','Résolution DNS vers une adresse IP.',1),
('aliases_to','Alias de','alias de','Alias DNS canonique.',1),
('uses_name_server','Utilise le serveur de noms','utilise le serveur de noms','Serveur DNS faisant autorité.',1),
('links_to','Lié à','lié à','Lien générique entre deux entités.',1),
('sends','Envoie','envoie','Transmission d’un élément.',1),
('uses','Utilise','utilise','Utilisation d’une ressource ou identité.',1),
('controls','Contrôle','contrôle','Contrôle d’une entité.',1),
('owns','Possède','possède','Possession d’une entité.',1),
('knows','Connaît','connaît','Connaissance entre personnes.',1),
('redirects_to','Redirige vers','redirige vers','Redirection vers une cible.',1);
