// src/core/control/sqlite/SqliteText.cpp
#include "SqliteText.h"

#include "model/Text.h"
#include "model/Point.h" // Correzione: Aggiunto header per Point
#include "util/Color.h" // Correzione: Aggiunto header per Color

SqliteText::SqliteText(sqlite3* db, Text* text) : db(db), text(text) {}
auto SqliteText::getErrorMessage() const -> std::string { return errorMessage; }


auto SqliteText::load(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT font_name, font_size, x, y, color, content FROM texts WHERE node_id = ?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { /*...*/ }
    sqlite3_bind_int64(stmt, 1, nodeId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* font_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        double font_size = sqlite3_column_double(stmt, 1);
        double x = sqlite3_column_double(stmt, 2);
        double y = sqlite3_column_double(stmt, 3);
        int color = sqlite3_column_int(stmt, 4);
        const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

        // Correzione: setFont si aspetta un oggetto XojFont
        text->setFont(XojFont(std::string(font_name), font_size));
        
        // Correzione: 'setPosition' non esiste, usa setX e setY
        text->setX(x);
        text->setY(y);
        
        text->setColor(Color(color)); // Correzione
        text->setText(std::string(content));
    } else {
        errorMessage = "Text data not found for node ID: " + std::to_string(nodeId);
    }

    sqlite3_finalize(stmt);
    return errorMessage.empty();
}

auto SqliteText::save(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO texts (node_id, font_name, font_size, x, y, color, content) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, nodeId);
    sqlite3_bind_text(stmt, 2, text->getFontName().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, text->getFontSize());
    sqlite3_bind_double(stmt, 4, text->getX());
    sqlite3_bind_double(stmt, 5, text->getY());
    sqlite3_bind_int(stmt, 6, static_cast<uint32_t>(text->getColor())); // Correzione
    sqlite3_bind_text(stmt, 7, text->getText().c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return false; }
    sqlite3_finalize(stmt);
    return true;
}