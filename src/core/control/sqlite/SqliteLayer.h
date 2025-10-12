// src/core/control/sqlite/SqliteLayer.h
#pragma once

#include <string>
#include "sqlite3.h"
#include <cstdint>

class Layer;

class SqliteLayer {
public:
    SqliteLayer(sqlite3* db, Layer* layer);
    ~SqliteLayer() = default;

    // Disabilita copia e spostamento
    SqliteLayer(const SqliteLayer&) = delete;
    SqliteLayer& operator=(const SqliteLayer&) = delete;
    SqliteLayer(SqliteLayer&&) = delete;
    SqliteLayer& operator=(SqliteLayer&&) = delete;

    /**
     * @brief Carica i dati del layer dal database.
     * @param nodeId L'ID del nodo corrispondente a questo layer.
     * @return true se il caricamento ha successo, altrimenti false.
     */
    auto load(int64_t nodeId) -> bool;

    /**
     * @brief Salva i dati del layer nel database.
     * @param nodeId L'ID del nodo corrispondiente a questo layer.
     * @return true se il salvataggio ha successo, altrimenti false.
     */
    auto save(int64_t nodeId) -> bool;

    [[nodiscard]] auto getErrorMessage() const -> std::string;

private:
    sqlite3* db;
    Layer* layer;
    std::string errorMessage;
};