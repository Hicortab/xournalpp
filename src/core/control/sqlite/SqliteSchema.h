// src/core/control/sqlite/SqliteSchema.h
#pragma once

#include <string>

namespace xoj {
namespace sqlite {

/**
 * Schema completo del database
 * Definito come costante per creazione iniziale
 */
constexpr const char* SCHEMA_SQL = R"SQL(
-- Metadati del documento
CREATE TABLE IF NOT EXISTS document_metadata (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    version INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    modified_at INTEGER NOT NULL,
    title TEXT,
    author TEXT,
    subject TEXT,
    keywords TEXT
);

-- Tabella principale dei nodi
CREATE TABLE IF NOT EXISTS nodes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    parent_id INTEGER REFERENCES nodes(id) ON DELETE CASCADE,
    node_type TEXT NOT NULL,
    position INTEGER NOT NULL,
    created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    modified_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    is_dirty INTEGER DEFAULT 0,
    UNIQUE(parent_id, position)
);

CREATE INDEX IF NOT EXISTS idx_nodes_parent ON nodes(parent_id);
CREATE INDEX IF NOT EXISTS idx_nodes_type ON nodes(node_type);
CREATE INDEX IF NOT EXISTS idx_nodes_dirty ON nodes(is_dirty) WHERE is_dirty = 1;

-- Attributi generici dei nodi
CREATE TABLE IF NOT EXISTS node_attributes (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    attr_name TEXT NOT NULL,
    attr_value TEXT NOT NULL,
    PRIMARY KEY (node_id, attr_name)
) WITHOUT ROWID;

CREATE INDEX IF NOT EXISTS idx_attributes_node ON node_attributes(node_id);

-- Pagine
CREATE TABLE IF NOT EXISTS pages (
    node_id INTEGER PRIMARY KEY REFERENCES nodes(id) ON DELETE CASCADE,
    width REAL NOT NULL,
    height REAL NOT NULL,
    background_type TEXT,
    background_color INTEGER,
    background_pdf_page INTEGER,
    background_pdf_filename TEXT
) WITHOUT ROWID;

-- Layer
CREATE TABLE IF NOT EXISTS layers (
    node_id INTEGER PRIMARY KEY REFERENCES nodes(id) ON DELETE CASCADE,
    name TEXT,
    visible INTEGER DEFAULT 1
) WITHOUT ROWID;

-- Strokes (tratti penna)
CREATE TABLE IF NOT EXISTS strokes (
    node_id INTEGER PRIMARY KEY REFERENCES nodes(id) ON DELETE CASCADE,
    tool_type TEXT NOT NULL,
    color INTEGER NOT NULL,
    width REAL NOT NULL,
    fill INTEGER DEFAULT -1,
    line_style TEXT,
    coordinates BLOB NOT NULL,
    pressure_data BLOB
) WITHOUT ROWID;

CREATE INDEX IF NOT EXISTS idx_strokes_tool ON strokes(tool_type);

-- Testo
CREATE TABLE IF NOT EXISTS texts (
    node_id INTEGER PRIMARY KEY REFERENCES nodes(id) ON DELETE CASCADE,
    font_name TEXT NOT NULL,
    font_size REAL NOT NULL,
    x REAL NOT NULL,
    y REAL NOT NULL,
    color INTEGER NOT NULL,
    content TEXT NOT NULL
) WITHOUT ROWID;

-- Immagini
CREATE TABLE IF NOT EXISTS images (
    node_id INTEGER PRIMARY KEY REFERENCES nodes(id) ON DELETE CASCADE,
    x REAL NOT NULL,
    y REAL NOT NULL,
    width REAL NOT NULL,
    height REAL NOT NULL,
    media_path TEXT NOT NULL
) WITHOUT ROWID;

-- TeX/LaTeX
CREATE TABLE IF NOT EXISTS tex_images (
    node_id INTEGER PRIMARY KEY REFERENCES nodes(id) ON DELETE CASCADE,
    x REAL NOT NULL,
    y REAL NOT NULL,
    width REAL NOT NULL,
    height REAL NOT NULL,
    tex_source TEXT NOT NULL,
    media_path TEXT
) WITHOUT ROWID;

-- Audio
CREATE TABLE IF NOT EXISTS audio (
    node_id INTEGER PRIMARY KEY REFERENCES nodes(id) ON DELETE CASCADE,
    timestamp INTEGER NOT NULL,
    media_path TEXT NOT NULL
) WITHOUT ROWID;

-- Gestione file media
CREATE TABLE IF NOT EXISTS media_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT UNIQUE NOT NULL,
    original_name TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    size INTEGER NOT NULL,
    hash TEXT NOT NULL,
    reference_count INTEGER DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_media_hash ON media_files(hash);
CREATE INDEX IF NOT EXISTS idx_media_path ON media_files(path);

-- Storia delle modifiche per undo/redo
CREATE TABLE IF NOT EXISTS change_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    node_id INTEGER NOT NULL,
    operation TEXT NOT NULL,
    old_data BLOB,
    new_data BLOB
);

CREATE INDEX IF NOT EXISTS idx_history_node ON change_history(node_id);
CREATE INDEX IF NOT EXISTS idx_history_time ON change_history(timestamp);

-- NUOVO TRIGGER per gestire 'position' automaticamente
CREATE TRIGGER IF NOT EXISTS set_node_position
AFTER INSERT ON nodes
FOR EACH ROW
WHEN NEW.position IS NULL OR NEW.position = 0 -- Si attiva solo se la posizione non è specificata
BEGIN
    UPDATE nodes
    SET position = (
        SELECT IFNULL(MAX(position), -1) + 1
        FROM nodes
        WHERE parent_id = NEW.parent_id
    )
    WHERE id = NEW.id;
END;

-- Trigger per aggiornare modified_at automaticamente
CREATE TRIGGER IF NOT EXISTS update_node_modified
AFTER UPDATE ON nodes
FOR EACH ROW
WHEN NEW.modified_at = OLD.modified_at
BEGIN
    UPDATE nodes SET modified_at = strftime('%s', 'now')
    WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS update_document_modified
AFTER UPDATE ON nodes
FOR EACH ROW
BEGIN
    UPDATE document_metadata SET modified_at = strftime('%s', 'now')
    WHERE id = 1;
END;

-- Trigger per gestire reference count dei media
CREATE TRIGGER IF NOT EXISTS increment_media_ref
AFTER INSERT ON images
FOR EACH ROW
BEGIN
    UPDATE media_files SET reference_count = reference_count + 1
    WHERE path = NEW.media_path;
END;

CREATE TRIGGER IF NOT EXISTS decrement_media_ref
AFTER DELETE ON images
FOR EACH ROW
BEGIN
    UPDATE media_files SET reference_count = reference_count - 1
    WHERE path = OLD.media_path;
    
    DELETE FROM media_files
    WHERE path = OLD.media_path AND reference_count <= 0;
END;

-- View per gerarchie
CREATE VIEW IF NOT EXISTS node_hierarchy AS
WITH RECURSIVE tree(id, parent_id, node_type, level, path) AS (
    SELECT id, parent_id, node_type, 0, CAST(id AS TEXT)
    FROM nodes WHERE parent_id IS NULL
    UNION ALL
    SELECT n.id, n.parent_id, n.node_type, tree.level + 1,
           tree.path || '/' || CAST(n.id AS TEXT)
    FROM nodes n
    JOIN tree ON n.parent_id = tree.id
)
SELECT * FROM tree;

-- View per nodi modificati
CREATE VIEW IF NOT EXISTS dirty_nodes AS
SELECT n.*, nh.path, nh.level
FROM nodes n
JOIN node_hierarchy nh ON n.id = nh.id
WHERE n.is_dirty = 1
ORDER BY nh.level, n.position;
)SQL";

/**
 * Query per verificare integrità database
 */
constexpr const char* INTEGRITY_CHECK_SQL = R"SQL(
PRAGMA integrity_check;
PRAGMA foreign_key_check;
)SQL";

/**
 * Query per ottimizzare database
 */
constexpr const char* OPTIMIZE_SQL = R"SQL(
ANALYZE;
VACUUM;
)SQL";

/**
 * Indici aggiuntivi per performance
 */
constexpr const char* PERFORMANCE_INDICES_SQL = R"SQL(
CREATE INDEX IF NOT EXISTS idx_nodes_modified ON nodes(modified_at DESC);
CREATE INDEX IF NOT EXISTS idx_pages_size ON pages(width, height);
)SQL";

} // namespace sqlite
} // namespace xoj