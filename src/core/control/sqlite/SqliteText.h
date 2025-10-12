// src/core/control/sqlite/SqliteText.h
#pragma once

#include <string>
#include "sqlite3.h"
#include <cstdint>

class Text;

class SqliteText {
public:
    SqliteText(sqlite3* db, Text* text);
    ~SqliteText() = default;

    SqliteText(const SqliteText&) = delete;
    SqliteText& operator=(const SqliteText&) = delete;
    SqliteText(SqliteText&&) = delete;
    SqliteText& operator=(SqliteText&&) = delete;

    auto load(int64_t nodeId) -> bool;
    auto save(int64_t nodeId) -> bool;

    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    sqlite3* db;
    Text* text;
    std::string errorMessage;
};