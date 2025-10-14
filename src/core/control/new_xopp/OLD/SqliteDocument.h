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

private:

private:

    std::string errorMessage;
};