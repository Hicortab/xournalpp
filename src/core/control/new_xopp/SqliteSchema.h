#pragma once

/*
 * Xournal++
 *
 * SQL Schema to create correct tables
 *
 * @author Xournal++ developers
 *
 * @license GNU GPLv2 or later
 */

namespace xoj {
namespace sqlite {

/**
 * Schema completo del database
 * Definito come costante per creazione iniziale
 */
constexpr const char* SCHEMA_SQL = R"SQL(

-- Abilita il supporto per le chiavi esterne (fondamentale per l'integrità dei dati)
PRAGMA foreign_keys = ON;

-- =================================================================
-- Tabella: Document
-- Contiene le informazioni di base del documento.
-- =================================================================
CREATE TABLE Document (
    document_id INTEGER PRIMARY KEY,
    title TEXT,
    creator TEXT,
    fileVersion REAL
);

-- =================================================================
-- Tabella: XojPage
-- Rappresenta una singola pagina all'interno di un documento.
-- =================================================================
CREATE TABLE XojPage (
    page_id INTEGER PRIMARY KEY,
    layer_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    backgroundImage BLOB,
    width REAL,
    height REAL,
    page_type TEXT CHECK(page_type IN ('Plain', 'Ruled', 'Lined', 'Staves', 'Graph', 'Dotted', 'IsoDotted', 'IsoGraph', 'Pdf', 'Image')),
    FOREIGN KEY (layer_id) REFERENCES Layer(layer_id) ON DELETE CASCADE
);

-- =================================================================
-- Tabella: Layer
-- Ogni pagina può avere più livelli (layers) sovrapposti.
-- =================================================================
CREATE TABLE Layer (
    layer_id INTEGER PRIMARY KEY,
    element_id INTEGER NOT NULL,
    visible INTEGER CHECK(visible IN (0, 1)),     -- 0 per Falso, 1 per Vero
    isModified INTEGER CHECK(isModified IN (0, 1)), -- 0 per Falso, 1 per Vero
    FOREIGN KEY (element_id) REFERENCES Element(element_id) ON DELETE CASCADE
);

-- =================================================================
-- Tabella: ElementType
-- Definisce il tipo di un elemento (es. tratto, immagine, testo).
-- =================================================================
CREATE TABLE ElementType (
    elementType_id INTEGER PRIMARY KEY,
    type TEXT NOT NULL UNIQUE CHECK(type IN ('ELEMENT_STROKE', 'ELEMENT_IMAGE', 'ELEMENT_TEXTPAGE', 'ELEMENT_TEXT')),
    visible INTEGER CHECK(visible IN (0, 1))
);

-- =================================================================
-- Tabella: Element
-- Rappresenta un singolo oggetto su un livello (es. una linea, un'immagine).
-- =================================================================
CREATE TABLE Element (
    element_id INTEGER PRIMARY KEY,
    layer_id INTEGER NOT NULL,
    elementType_id INTEGER NOT NULL,
    sizecalculated INTEGER,
    width REAL,
    weight REAL,
    x REAL,
    y REAL,
    color TEXT, -- Formato "RED, BLUE, GREEN"
    FOREIGN KEY (layer_id) REFERENCES Layer(layer_id) ON DELETE CASCADE,
    FOREIGN KEY (elementType_id) REFERENCES ElementType(elementType_id)
);

-- =================================================================
-- Tabella: XojFont
-- Memorizza le informazioni sui font utilizzati.
-- =================================================================
CREATE TABLE XojFont (
    font_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    size REAL
);

-- =================================================================
-- Tabella: Text
-- Contiene i dati specifici per gli elementi di tipo testo.
-- La chiave primaria è anche una chiave esterna verso Element,
-- creando una relazione uno-a-uno.
-- =================================================================
CREATE TABLE Text (
    element_id INTEGER PRIMARY KEY, -- Chiave Primaria e Esterna
    font_id INTEGER NOT NULL,
    text_content TEXT,
    FOREIGN KEY (element_id) REFERENCES Element(element_id) ON DELETE CASCADE,
    FOREIGN KEY (font_id) REFERENCES XcjFont(font_id)
);

)SQL";

} // namespace sqlite
} // namespace xoj