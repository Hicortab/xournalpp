#pragma once

#include <memory>
#include <string>

#include "sqlite3.h"

class Document;
class XojPage;
class Layer;

class SqliteLoader {
public:
    explicit SqliteLoader(Document* doc);
    ~SqliteLoader();

    SqliteLoader(const SqliteLoader&) = delete;
    SqliteLoader& operator=(const SqliteLoader&) = delete;

    auto load(const std::string& path) -> bool;
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

private:
    sqlite3* db = nullptr;
    Document* document;
    std::string errorMessage;
};