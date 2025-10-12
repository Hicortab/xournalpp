// src/core/control/sqlite/SqliteImage.h
#pragma once

#include <string>
#include "sqlite3.h"
#include <cstdint>

class Image;

class SqliteImage {
public:
    SqliteImage(sqlite3* db, Image* image);
    ~SqliteImage() = default;

    SqliteImage(const SqliteImage&) = delete;
    SqliteImage& operator=(const SqliteImage&) = delete;
    SqliteImage(SqliteImage&&) = delete;
    SqliteImage& operator=(SqliteImage&&) = delete;
    
    auto load(int64_t nodeId) -> bool;
    auto save(int64_t nodeId) -> bool;

    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    sqlite3* db;
    Image* image;
    std::string errorMessage;
};