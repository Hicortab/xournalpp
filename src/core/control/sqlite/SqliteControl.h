#pragma once

#include <string>
#include <memory> // Correzione: Aggiunto header per std::unique_ptr
#include <cstdint>
#include "sqlite3.h"

class Document;
class SqliteDocument;

class SqliteControl {
public:
    explicit SqliteControl(Document* doc);
    ~SqliteControl();

    SqliteControl(const SqliteControl&) = delete;
    SqliteControl& operator=(const SqliteControl&) = delete;

    auto save(const std::string& path, const std::string& oldPath) -> bool;
    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    auto openDatabase(const std::string& dbPath) -> bool;
    void closeDatabase();
    auto createSchema() -> bool;
    auto executeAndCheck(const char* sql, const std::string& errorMsg) -> bool;

private:
    sqlite3* db = nullptr;
    Document* document;
    std::unique_ptr<SqliteDocument> docHandler;
    std::string errorMessage;
};