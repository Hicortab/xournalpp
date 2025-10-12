// src/core/control/sqlite/SqliteStroke.h
#pragma once

#include <string>
#include "sqlite3.h"
#include <cstdint>

class Stroke;

class SqliteStroke {
public:
    SqliteStroke(sqlite3* db, Stroke* stroke);
    ~SqliteStroke() = default;

    SqliteStroke(const SqliteStroke&) = delete;
    SqliteStroke& operator=(const SqliteStroke&) = delete;
    SqliteStroke(SqliteStroke&&) = delete;
    SqliteStroke& operator=(SqliteStroke&&) = delete;

    auto load(int64_t nodeId) -> bool;
    auto save(int64_t nodeId) -> bool;

    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    sqlite3* db;
    Stroke* stroke;
    std::string errorMessage;
};