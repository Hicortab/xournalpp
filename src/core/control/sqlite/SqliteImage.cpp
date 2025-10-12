// src/core/control/sqlite/SqliteImage.cpp
#include "SqliteImage.h"
#include "model/Image.h"

// Correzione: Includi Rectangle per la sua definizione, anche se non la usiamo direttamente
#include "util/Rectangle.h"

SqliteImage::SqliteImage(sqlite3* db, Image* image) : db(db), image(image) {}

auto SqliteImage::getErrorMessage() const -> std::string {
    return errorMessage;
}

auto SqliteImage::load(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT x, y, width, height, image_data, mime_type FROM images WHERE node_id = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare image load query: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int64(stmt, 1, nodeId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        double x = sqlite3_column_double(stmt, 0);
        double y = sqlite3_column_double(stmt, 1);
        double width = sqlite3_column_double(stmt, 2);
        double height = sqlite3_column_double(stmt, 3);
        const char* data = static_cast<const char*>(sqlite3_column_blob(stmt, 4));
        int data_size = sqlite3_column_bytes(stmt, 4);

        // Correzione: Usa i metodi della classe base Element e setImage per i dati
        image->setX(x);
        image->setY(y);
        image->setWidth(width);
        image->setHeight(height);
        image->setImage(std::string_view(data, data_size));

    } else {
        errorMessage = "Image data not found for node ID: " + std::to_string(nodeId);
    }
    
    sqlite3_finalize(stmt);
    return errorMessage.empty();
}

auto SqliteImage::save(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    // Correzione: Schema aggiornato per i dati binari
    const char* sql = "INSERT INTO images (node_id, x, y, width, height, mime_type, image_data) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare image save query: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, nodeId);
    sqlite3_bind_double(stmt, 2, image->getX());
    sqlite3_bind_double(stmt, 3, image->getY());
    sqlite3_bind_double(stmt, 4, image->getElementWidth());
    sqlite3_bind_double(stmt, 5, image->getElementHeight());
    sqlite3_bind_text(stmt, 6, "image/png", -1, SQLITE_TRANSIENT); // Ipotizziamo PNG per ora

    // Correzione: Usa i metodi corretti per ottenere i dati binari
    const unsigned char* data = image->getRawData();
    size_t data_len = image->getRawDataLength();
    sqlite3_bind_blob(stmt, 7, data, data_len, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        errorMessage = "Failed to save image data: " + std::string(sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    
    sqlite3_finalize(stmt);
    return true;
}