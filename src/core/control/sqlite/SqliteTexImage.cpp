#include "control/sqlite/SqliteTexImage.h"
#include "model/TexImage.h"
#include "util/Rectangle.h"

SqliteTexImage::SqliteTexImage(sqlite3* db, TexImage* texImage) : db(db), texImage(texImage) {}
auto SqliteTexImage::getErrorMessage() const -> std::string { return errorMessage; }

auto SqliteTexImage::load(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT x, y, width, height, tex_source FROM tex_images WHERE node_id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, nodeId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        double x = sqlite3_column_double(stmt, 0);
        double y = sqlite3_column_double(stmt, 1);
        double width = sqlite3_column_double(stmt, 2);
        double height = sqlite3_column_double(stmt, 3);
        const char* tex_source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        texImage->setX(x);
        texImage->setY(y);
        texImage->setWidth(width);
        texImage->setHeight(height);
        texImage->setText(std::string(tex_source));
    }
    sqlite3_finalize(stmt);
    return true;
}

auto SqliteTexImage::save(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO tex_images (node_id, x, y, width, height, tex_source, media_path) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, nodeId);
    sqlite3_bind_double(stmt, 2, texImage->getX());
    sqlite3_bind_double(stmt, 3, texImage->getY());
    sqlite3_bind_double(stmt, 4, texImage->getElementWidth());
    sqlite3_bind_double(stmt, 5, texImage->getElementHeight());
    sqlite3_bind_text(stmt, 6, texImage->getText().c_str(), -1, SQLITE_TRANSIENT);
    
    // Salva i dati binari (PDF/PNG)
    const std::string& data = texImage->getBinaryData();
    sqlite3_bind_blob(stmt, 7, data.data(), data.length(), SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return false; }
    sqlite3_finalize(stmt);
    return true;
}