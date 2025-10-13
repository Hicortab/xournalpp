#pragma once

#include <memory>
#include <string>

#include "sqlite3.h"
#include "model/DocumentHandler.h"
#include "model/Document.h"
#include "filesystem.h"

namespace fs = std::filesystem;

class Document;
class XojPage;
class Layer;

class SqliteLoader {
public:
    explicit SqliteLoader();
    ~SqliteLoader();

    SqliteLoader(const SqliteLoader&) = delete;
    SqliteLoader& operator=(const SqliteLoader&) = delete;

    std::unique_ptr<Document> load(const fs::path& path);
    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    auto openDatabase(const std::string& dbPath) -> bool;
    void closeDatabase();

    auto loadPages() -> bool;
    auto loadLayers(XojPage* page, int pageId) -> bool;
    auto loadStrokes(Layer* layer, int layerId) -> bool;
    auto loadTexts(Layer* layer, int layerId) -> bool;
    auto loadImages(Layer* layer, int layerId) -> bool;
    auto loadTexImages(Layer* layer, int layerId) -> bool;

public:
    sqlite3* db = nullptr;
    
    DocumentHandler sqliteLoaderDocumentHandler;
    std::unique_ptr<Document> document;

    std::string errorMessage;
};