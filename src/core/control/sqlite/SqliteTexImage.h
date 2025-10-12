// src/core/control/sqlite/SqliteTexImage.h
#pragma once

#include <string>
#include "sqlite3.h"
#include <cstdint>

class TexImage;

class SqliteTexImage {
public:
    SqliteTexImage(sqlite3* db, TexImage* texImage);
    ~SqliteTexImage() = default;

    SqliteTexImage(const SqliteTexImage&) = delete;
    SqliteTexImage& operator=(const SqliteTexImage&) = delete;
    SqliteTexImage(SqliteTexImage&&) = delete;
    SqliteTexImage& operator=(SqliteTexImage&&) = delete;

    auto load(int64_t nodeId) -> bool;
    auto save(int64_t nodeId) -> bool;

    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    sqlite3* db;
    TexImage* texImage;
    std::string errorMessage;
};