#pragma once

#include <string>
#include <cstdint>
#include "sqlite3.h"

class XojPage; // Correzione: Usa XojPage

class SqlitePage {
public:
    SqlitePage(sqlite3* db, XojPage* page); // Correzione: Usa XojPage
    ~SqlitePage() = default;

    SqlitePage(const SqlitePage&) = delete;
    SqlitePage& operator=(const SqlitePage&) = delete;

    auto save(int64_t nodeId) -> bool;

    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    sqlite3* db;
    XojPage* page; // Correzione: Usa XojPage
    std::string errorMessage;
};