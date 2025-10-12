// src/core/control/sqlite/SqliteStroke.cpp
#include "SqliteStroke.h"

#include "model/Stroke.h"
#include "util/Color.h"

// Funzioni helper per convertire tra stringa e StrokeTool
static StrokeTool stringToToolType(const std::string& toolStr) {
    if (toolStr == "eraser") return StrokeTool::ERASER;
    if (toolStr == "highlighter") return StrokeTool::HIGHLIGHTER;
    return StrokeTool::PEN;
}

static const char* toolTypeToString(StrokeTool tool) {
    // Correzione: Usa l'operatore di cast implicito a StrokeTool::Value
    switch (tool) {
        case StrokeTool::ERASER: return "eraser";
        case StrokeTool::HIGHLIGHTER: return "highlighter";
        case StrokeTool::PEN:
        default: return "pen";
    }
}

SqliteStroke::SqliteStroke(sqlite3* db, Stroke* stroke) : db(db), stroke(stroke) {}
auto SqliteStroke::getErrorMessage() const -> std::string { return errorMessage; }

auto SqliteStroke::load(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT tool_type, color, width, fill, coordinates, pressure_data FROM strokes WHERE node_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { /*...*/ return false; }
    sqlite3_bind_int64(stmt, 1, nodeId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* toolTypeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int colorValue = sqlite3_column_int(stmt, 1);
        double width = sqlite3_column_double(stmt, 2);
        int fillValue = sqlite3_column_int(stmt, 3);
        
        stroke->setToolType(stringToToolType(std::string(toolTypeStr)));
        stroke->setColor(Color(colorValue));
        stroke->setWidth(width);
        stroke->setFill(fillValue);
        
        const auto* coords_data = static_cast<const float*>(sqlite3_column_blob(stmt, 4));
        int coords_bytes = sqlite3_column_bytes(stmt, 4);
        int num_points = coords_bytes / (sizeof(float) * 2);
        
        std::vector<Point> points;
        points.reserve(num_points);
        const auto* pressure_data = static_cast<const float*>(sqlite3_column_blob(stmt, 5));
        bool has_pressure = (sqlite3_column_type(stmt, 5) != SQLITE_NULL);

        for (int i = 0; i < num_points; ++i) {
            double pressure = has_pressure ? pressure_data[i] : Point::NO_PRESSURE;
            points.emplace_back(coords_data[i * 2], coords_data[i * 2 + 1], pressure);
        }
        stroke->setPointVector(std::move(points));
    } else {
        errorMessage = "Stroke data not found for node ID: " + std::to_string(nodeId);
    }
    
    sqlite3_finalize(stmt);
    return errorMessage.empty();
}

auto SqliteStroke::save(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO strokes (node_id, tool_type, color, width, fill, coordinates, pressure_data) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare stroke save query: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, nodeId);
    sqlite3_bind_text(stmt, 2, toolTypeToString(stroke->getToolType()), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, static_cast<uint32_t>(stroke->getColor()));
    sqlite3_bind_double(stmt, 4, stroke->getWidth());
    sqlite3_bind_int(stmt, 5, stroke->getFill());

    const auto& points = stroke->getPointVector();
    std::vector<float> coords_data;
    coords_data.reserve(points.size() * 2);
    for (const auto& p : points) {
        coords_data.push_back(static_cast<float>(p.x)); // Correzione: Cast esplicito per warning
        coords_data.push_back(static_cast<float>(p.y)); // Correzione: Cast esplicito per warning
    }
    sqlite3_bind_blob(stmt, 6, coords_data.data(), coords_data.size() * sizeof(float), SQLITE_TRANSIENT);

    if (stroke->hasPressure()) {
        std::vector<float> pressures;
        pressures.reserve(points.size());
        for (const auto& p : points) {
            pressures.push_back(static_cast<float>(p.z)); // Correzione: Cast esplicito per warning
        }
        sqlite3_bind_blob(stmt, 7, pressures.data(), pressures.size() * sizeof(float), SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 7);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        errorMessage = "Failed to save stroke data: " + std::string(sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}
