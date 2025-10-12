// src/core/control/sqlite/SqliteDocument.h
#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include "sqlite3.h"

class Document;
class Element;

class SqliteDocument {
public:
    SqliteDocument(sqlite3* db, Document* doc);
    ~SqliteDocument() = default;

    auto load() -> bool;
    auto save() -> bool;

    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    auto loadMetadata() -> bool;
    auto saveMetadata() -> bool;
    
    auto loadNodeHierarchy() -> bool;
    auto saveNodeHierarchy() -> bool;

    // Correzione: Aggiunte dichiarazioni mancanti
    auto saveNodeRecursive(Element* element, std::optional<int64_t> parentId, int position) -> bool;
    auto loadNodeRecursive(int64_t nodeId) -> Element*;

private:
    sqlite3* db;
    Document* document;
    std::string errorMessage;
};