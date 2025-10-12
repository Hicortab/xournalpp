// src/core/control/sqlite/SqliteLayer.cpp
#include "SqliteLayer.h"

#include "model/Layer.h"

SqliteLayer::SqliteLayer(sqlite3* db, Layer* layer) : db(db), layer(layer) {}

auto SqliteLayer::getErrorMessage() const -> std::string {
    return errorMessage;
}

auto SqliteLayer::load(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT name, visible FROM layers WHERE node_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare layer load query: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, nodeId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        bool visible = sqlite3_column_int(stmt, 1) == 1;

        if (name) {
            layer->setName(name);
        }
        layer->setVisible(visible);
        
    } else {
        errorMessage = "Layer data not found for node ID: " + std::to_string(nodeId);
    }

    sqlite3_finalize(stmt);
    return errorMessage.empty();
}

auto SqliteLayer::save(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO layers (node_id, name, visible) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare layer save query: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, nodeId);
    sqlite3_bind_text(stmt, 2, layer->getName().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, layer->isVisible() ? 1 : 0);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        errorMessage = "Failed to save layer data: " + std::string(sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}