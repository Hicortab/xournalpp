#include "control/sqlite/SqliteDocument.h"
#include <stdexcept>

#include "model/Document.h"
#include "model/XojPage.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/Image.h"
#include "model/TexImage.h"
#include "model/Element.h"

#include "control/sqlite/SqlitePage.h"
#include "control/sqlite/SqliteLayer.h"
#include "control/sqlite/SqliteStroke.h"
#include "control/sqlite/SqliteText.h"
#include "control/sqlite/SqliteImage.h"
#include "control/sqlite/SqliteTexImage.h"

const char* elementTypeToString(ElementType type) {
    switch (type) {
        case ELEMENT_STROKE: return "STROKE";
        case ELEMENT_TEXT: return "TEXT";
        case ELEMENT_IMAGE: return "IMAGE";
        case ELEMENT_TEXIMAGE: return "TEXIMAGE";
    }
    throw std::runtime_error("Unsupported element type");
}

SqliteDocument::SqliteDocument(sqlite3* db, Document* doc) : db(db), document(doc) {}
auto SqliteDocument::getErrorMessage() const -> std::string { return errorMessage; }

auto SqliteDocument::save() -> bool {
    if (!saveMetadata()) return false;
    if (!saveNodeHierarchy()) return false;
    return true;
}

auto SqliteDocument::saveMetadata() -> bool { return true; }

auto SqliteDocument::saveNodeHierarchy() -> bool {
    for (size_t i = 0; i < document->getPageCount(); ++i) {
        ConstPageRef pageRef = document->getPage(i);
        XojPage* page = const_cast<XojPage*>(pageRef.get());

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO nodes (parent_id, node_type, position) VALUES (?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        
        sqlite3_bind_null(stmt, 1);
        sqlite3_bind_text(stmt, 2, "PAGE", -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, i);
        if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return false; }
        sqlite3_finalize(stmt);
        
        int64_t pageNodeId = sqlite3_last_insert_rowid(db);

        SqlitePage pageSaver(db, page);
        if (!pageSaver.save(pageNodeId)) return false;

        for (size_t j = 0; j < page->getLayerCount(); ++j) {
            Layer* layer = page->getLayers()[j];
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
            sqlite3_bind_int64(stmt, 1, pageNodeId);
            sqlite3_bind_text(stmt, 2, "LAYER", -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, j);
            if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return false; }
            sqlite3_finalize(stmt);
            int64_t layerNodeId = sqlite3_last_insert_rowid(db);

            SqliteLayer layerSaver(db, layer);
            if (!layerSaver.save(layerNodeId)) return false;

            int elementPosition = 0;
            for (const auto& elementPtr : layer->getElements()) {
                if (!saveNodeRecursive(elementPtr.get(), layerNodeId, elementPosition++)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool SqliteDocument::saveNodeRecursive(Element* element, std::optional<int64_t> parentId, int position) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO nodes (parent_id, node_type, position) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, *parentId);
    sqlite3_bind_text(stmt, 2, elementTypeToString(element->getType()), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, position);

    if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return false; }
    sqlite3_finalize(stmt);
    int64_t nodeId = sqlite3_last_insert_rowid(db);

    bool success = false;
    switch (element->getType()) {
        case ELEMENT_STROKE:
            success = SqliteStroke(db, static_cast<Stroke*>(element)).save(nodeId);
            break;
        case ELEMENT_TEXT:
            success = SqliteText(db, static_cast<Text*>(element)).save(nodeId);
            break;
        case ELEMENT_IMAGE:
            success = SqliteImage(db, static_cast<Image*>(element)).save(nodeId);
            break;
        case ELEMENT_TEXIMAGE:
            success = SqliteTexImage(db, static_cast<TexImage*>(element)).save(nodeId);
            break;
    }
    return success;
}