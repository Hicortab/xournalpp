#include "control/sqlite/SqliteDocument.h"

#include "model/Document.h"
#include "model/XojPage.h"
#include "model/Layer.h"
#include "model/Element.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/Point.h"
#include "control/pagetype/PageTypeHandler.h"
#include "util/Color.h"

SqliteDocument::SqliteDocument(sqlite3* db, Document* doc) : db(db), document(doc) {}

auto SqliteDocument::getErrorMessage() const -> std::string { return errorMessage; }

auto SqliteDocument::createNode(int parentNodeId, const std::string& nodeType) -> int {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO nodes (parent_id, node_type) VALUES (?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare node creation statement: " + std::string(sqlite3_errmsg(db));
        return -1;
    }

    if (parentNodeId > 0) {
        sqlite3_bind_int(stmt, 1, parentNodeId);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    sqlite3_bind_text(stmt, 2, nodeType.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        errorMessage = "Failed to execute node creation: " + std::string(sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

auto SqliteDocument::save() -> bool {
    for (size_t i = 0; i < document->getPageCount(); ++i) {
        if (!savePage(document->getPage(i).get(), 0)) {
            return false;
        }
    }
    return true;
}

auto SqliteDocument::savePage(XojPage* page, int parentNodeId) -> bool {
    int pageNodeId = createNode(parentNodeId, "page");
    if (pageNodeId == -1) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO pages (node_id, width, height, background_type, background_color, background_pdf_page, background_pdf_filename) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare page statement: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    const PageType& bg = page->getBackgroundType();
    std::string bgTypeStr = PageTypeHandler::getStringForPageTypeFormat(bg.format);
    Color bgcolor = page->getBackgroundColor();
    uint32_t bgColorInt = (static_cast<uint32_t>(bgcolor.alpha) << 24) | (bgcolor.red << 16) | (bgcolor.green << 8) | bgcolor.blue;

    sqlite3_bind_int(stmt, 1, pageNodeId);
    sqlite3_bind_double(stmt, 2, page->getWidth());
    sqlite3_bind_double(stmt, 3, page->getHeight());
    sqlite3_bind_text(stmt, 4, bgTypeStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, bgColorInt);

    if (bg.isPdfPage()) {
        /*
            sqlite3_bind_int(stmt, 6, page->getPdfPageNr() + 1);
            const auto& pdfPath = document->getPdfFilepath();
            if (!pdfPath.empty()) {
                sqlite3_bind_text(stmt, 7, pdfPath.u8string().c_str(), -1, SQLITE_TRANSIENT);
            } else {
                sqlite3_bind_null(stmt, 7);
            }
        */
    } else {
        sqlite3_bind_null(stmt, 6);
        sqlite3_bind_null(stmt, 7);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        errorMessage = "Failed to save page data: " + std::string(sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    for (auto const* layer : page->getLayers()) {
        if (!saveLayer(const_cast<Layer*>(layer), pageNodeId)) {
            return false;
        }
    }
    return true;
}

auto SqliteDocument::saveLayer(Layer* layer, int parentNodeId) -> bool {
    int layerNodeId = createNode(parentNodeId, "layer");
    if (layerNodeId == -1) return false;

    // ... eventuale salvataggio dei dati del layer ...

    for (const auto& element : layer->getElements()) {
        bool success = false;
        switch (element->getType()) {
            case ELEMENT_STROKE:
                success = saveStroke(static_cast<Stroke*>(element.get()), layerNodeId);
                break;
            case ELEMENT_TEXT:
                success = saveText(static_cast<Text*>(element.get()), layerNodeId);
                break;
            default:
                success = true;
                break;
        }
        if (!success) {
            return false;
        }
    }
    return true;
}

auto SqliteDocument::saveStroke(Stroke* stroke, int parentNodeId) -> bool {
    int strokeNodeId = createNode(parentNodeId, "stroke");
    if (strokeNodeId == -1) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO strokes (node_id, tool_type, color, width, coordinates, pressure_data) VALUES (?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare stroke statement: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    const char* toolType = "pen";
    if (stroke->getToolType() == StrokeTool::HIGHLIGHTER) toolType = "highlighter";
    else if (stroke->getToolType() == StrokeTool::ERASER) toolType = "eraser";

    Color c = stroke->getColor();
    uint32_t colorInt = (static_cast<uint32_t>(c.alpha) << 24) | (c.red << 16) | (c.green << 8) | c.blue;

    sqlite3_bind_int(stmt, 1, strokeNodeId);
    sqlite3_bind_text(stmt, 2, toolType, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, colorInt);
    sqlite3_bind_double(stmt, 4, stroke->getWidth());

    const auto& points = stroke->getPointVector();
    std::vector<Point> coordsOnly;
    std::vector<double> pressuresOnly;

    if (!points.empty()){
        coordsOnly.reserve(points.size());
        if (stroke->hasPressure()) {
            pressuresOnly.reserve(points.size());
        }
        for(const auto& p : points) {
            coordsOnly.emplace_back(p.x, p.y);
            if (stroke->hasPressure()) {
                pressuresOnly.push_back(p.z);
            }
        }
    }

    sqlite3_bind_blob(stmt, 5, coordsOnly.data(), coordsOnly.size() * sizeof(Point), SQLITE_STATIC);
    if (stroke->hasPressure()) {
        sqlite3_bind_blob(stmt, 6, pressuresOnly.data(), pressuresOnly.size() * sizeof(double), SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) {
        errorMessage = "Failed to save stroke data: " + std::string(sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return success;
}

auto SqliteDocument::saveText(Text* text, int parentNodeId) -> bool {
    int textNodeId = createNode(parentNodeId, "text");
    if (textNodeId == -1) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO texts (node_id, font_name, font_size, x, y, color, content) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare text statement: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    XojFont& font = text->getFont();
    Color c = text->getColor();
    uint32_t colorInt = (static_cast<uint32_t>(c.alpha) << 24) | (c.red << 16) | (c.green << 8) | c.blue;

    sqlite3_bind_int(stmt, 1, textNodeId);
    sqlite3_bind_text(stmt, 2, font.getName().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, font.getSize());
    sqlite3_bind_double(stmt, 4, text->getX());
    sqlite3_bind_double(stmt, 5, text->getY());
    sqlite3_bind_int(stmt, 6, colorInt);
    sqlite3_bind_text(stmt, 7, text->getText().c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) {
        errorMessage = "Failed to save text data: " + std::string(sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return success;
}