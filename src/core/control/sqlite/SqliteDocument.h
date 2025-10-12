#pragma once

#include "sqlite3.h"
#include <memory>
#include <string>

// Forward declarations
class Document;
class XojPage; // Corretto: usa XojPage
class Layer;
class Stroke;
class Text;

class SqliteDocument {
public:
    SqliteDocument(sqlite3* db, Document* doc);
    auto save() -> bool;
    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    auto savePage(XojPage* page, int parentNodeId) -> bool; // Corretto: usa XojPage
    auto saveLayer(Layer* layer, int parentNodeId) -> bool;
    auto saveStroke(Stroke* stroke, int parentNodeId) -> bool;
    auto saveText(Text* text, int parentNodeId) -> bool;

    auto createNode(int parentNodeId, const std::string& nodeType) -> int;

private:
    sqlite3* db;
    Document* document;
    std::string errorMessage;
};