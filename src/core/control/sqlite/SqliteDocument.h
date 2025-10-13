#pragma once

#include "sqlite3.h"
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class Document;
class XojPage;
class Layer;
class Stroke;
class Text;
class Image;
class TexImage;

class SqliteDocument {
public:
    SqliteDocument(sqlite3* db, Document* doc);
    auto save() -> bool;
    [[nodiscard]] auto getErrorMessage() const -> std::string;

    // Metodo pubblico per la creazione di nodi, ora accetta anche la posizione
    auto createNode(int parentNodeId, const std::string& nodeType, int position) -> int;

    // Metodo pubblico per l'aggiornamento dei nodi
    auto updateNodePosition(int nodeId, int newPosition) -> bool;

private:
    // Salva l'intero documento
    auto saveDocumentTree() -> bool;

    // Gestione delle pagine (salvataggio, aggiornamento, eliminazione)
    auto saveOrUpdatePage(XojPage* page, int position) -> bool;
    auto savePageData(XojPage* page, int pageNodeId, bool isUpdate) -> bool;
    auto deleteMissingPages(const std::vector<int>& activePageIds) -> bool;

    // Gestione dei layer e dei loro elementi
    auto saveOrUpdateLayer(Layer* layer, int parentNodeId, int position) -> bool;
    auto saveLayerElements(Layer* layer, int layerNodeId) -> bool;
    auto deleteMissingElements(int layerNodeId, const std::vector<int>& activeElementIds) -> bool;

    // Metodi specifici per salvare gli elementi (Stroke, Text, ecc.)
    auto saveStroke(Stroke* stroke, int parentNodeId, int position) -> int;
    auto saveText(Text* text, int parentNodeId, int position) -> int;
    auto saveImage(Image* image, int parentNodeId, int position) -> int;
    auto saveTexImage(TexImage* texImage, int parentNodeId, int position) -> int;

private:
    sqlite3* db;
    Document* document;
    std::string errorMessage;
};