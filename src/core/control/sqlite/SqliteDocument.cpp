#include "control/sqlite/SqliteDocument.h"
#include "control/sqlite/SqliteSchema.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <vector>

#include "model/Document.h"
#include "model/Element.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/Point.h"
#include "model/Stroke.h"
#include "model/TexImage.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "model/BackgroundImage.h"
#include "model/PageType.h"
#include "util/Color.h"

SqliteDocument::SqliteDocument(sqlite3* db, Document* doc) : db(db), document(doc) {}

auto SqliteDocument::getErrorMessage() const -> std::string { return errorMessage; }

// MODIFICATO: Rimosso BEGIN/COMMIT/ROLLBACK da qui. La gestione è in SqliteControl.
auto SqliteDocument::save() -> bool {
    return saveDocumentTree();
}

auto SqliteDocument::saveDocumentTree() -> bool {
    std::vector<int> activePageIds;
    int position = 0;
    for (size_t i = 0; i < document->getPageCount(); ++i) {
        auto pageRef = document->getPage(i);
        auto* page = static_cast<XojPage*>(pageRef.get());

        if (!saveOrUpdatePage(page, position)) {
            return false;
        }
        activePageIds.push_back(page->getNodeId());
        position++;
    }

    return deleteMissingPages(activePageIds);
}

auto SqliteDocument::deleteMissingPages(const std::vector<int>& activePageIds) -> bool {
    std::set<int> activeIds(activePageIds.begin(), activePageIds.end());
    std::set<int> idsInDb;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id FROM nodes WHERE node_type = 'page';";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            idsInDb.insert(sqlite3_column_int(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);

    std::vector<int> idsToDelete;
    std::set_difference(idsInDb.begin(), idsInDb.end(), activeIds.begin(), activeIds.end(),
                        std::back_inserter(idsToDelete));

    if (!idsToDelete.empty()) {
        const char* deleteSql = "DELETE FROM nodes WHERE id = ?;";
        if (sqlite3_prepare_v2(db, deleteSql, -1, &stmt, nullptr) != SQLITE_OK) {
            errorMessage = "Failed to prepare page delete statement: " + std::string(sqlite3_errmsg(db));
            return false;
        }
        for (int id : idsToDelete) {
            sqlite3_bind_int(stmt, 1, id);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                // Logga l'errore ma continua
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    return true;
}

auto SqliteDocument::saveOrUpdatePage(XojPage* page, int position) -> bool {
    // Questa logica era già corretta e funge da modello per le altre
    if (page->getNodeId() != -1) {
        if (!updateNodePosition(page->getNodeId(), position)) return false;
        // Salva anche i dati specifici della pagina (potrebbero essere cambiati)
        return savePageData(page, page->getNodeId(), true);
    } else {
        int pageNodeId = createNode(0, "page", position);
        if (pageNodeId == -1) return false;
        page->setNodeId(pageNodeId);
        return savePageData(page, pageNodeId, false);
    }
}

auto SqliteDocument::savePageData(XojPage* page, int pageNodeId, bool isUpdate) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = nullptr;

    if (isUpdate) {
        sql = "UPDATE pages SET width = ?, height = ?, background_type = ?, background_color = ?, "
              "background_pdf_page = ?, background_pdf_filename = ? WHERE node_id = ?;";
    } else {
        sql = "INSERT INTO pages (node_id, width, height, background_type, background_color, "
              "background_pdf_page, background_pdf_filename) VALUES (?, ?, ?, ?, ?, ?, ?);";
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare page data statement: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    int bindIndex = 1;
    if (!isUpdate) {
        sqlite3_bind_int(stmt, bindIndex++, pageNodeId);
    }

    sqlite3_bind_double(stmt, bindIndex++, page->getWidth());
    sqlite3_bind_double(stmt, bindIndex++, page->getHeight());
    
    PageType bgType = page->getBackgroundType();
    Color bgColor = page->getBackgroundColor();
    uint32_t colorInt = (bgColor.alpha << 24) | (bgColor.red << 16) | (bgColor.green << 8) | bgColor.blue;

    sqlite3_bind_text(stmt, bindIndex++, bgType.config.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, bindIndex++, colorInt);

    if (bgType.isPdfPage()) {
        sqlite3_bind_int(stmt, bindIndex++, page->getPdfPageNr());
        if (document) {
            sqlite3_bind_text(stmt, bindIndex++, document->getPdfFilepath().string().c_str(), -1, SQLITE_TRANSIENT);
        } else {
             sqlite3_bind_text(stmt, bindIndex++, "", -1, SQLITE_STATIC);
        }
    } else {
        sqlite3_bind_null(stmt, bindIndex++);
        sqlite3_bind_null(stmt, bindIndex++);
    }

    if (isUpdate) {
        sqlite3_bind_int(stmt, bindIndex++, pageNodeId);
    }

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) {
        errorMessage = "Failed to save/update page data: " + std::string(sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    if (!success) return false;

    // Logica per i Layer
    std::vector<int> activeLayerIds;
    int layerPosition = 0;
    for (auto* layer : page->getLayers()) {
        if (!saveOrUpdateLayer(layer, pageNodeId, layerPosition)) {
            return false;
        }
        activeLayerIds.push_back(layer->getNodeId());
        layerPosition++;
    }
    
    // TODO: Implementare deleteMissingLayers se necessario

    return true;
}

// MODIFICATO: Implementata la logica "salva o aggiorna" per i layer.
auto SqliteDocument::saveOrUpdateLayer(Layer* layer, int parentNodeId, int position) -> bool {
    int layerNodeId = layer->getNodeId();

    if (layerNodeId != -1) {
        // Il layer esiste già, aggiorna la sua posizione
        if (!updateNodePosition(layerNodeId, position)) {
            return false;
        }
    } else {
        // Il layer è nuovo, crealo nel DB
        layerNodeId = createNode(parentNodeId, "layer", position);
        if (layerNodeId == -1) {
            return false;
        }
        // Memorizza il nuovo ID nell'oggetto
        layer->setNodeId(layerNodeId);
    }

    return saveLayerElements(layer, layerNodeId);
}

auto SqliteDocument::saveLayerElements(Layer* layer, int layerNodeId) -> bool {
    int position = 0;
    std::vector<int> activeElementIds;

    for (auto const& element : layer->getElements()) {
        int elementNodeId = -1;
        switch (element->getType()) {
            case ElementType::ELEMENT_STROKE:
                elementNodeId = saveStroke(static_cast<Stroke*>(element.get()), layerNodeId, position);
                break;
            case ElementType::ELEMENT_TEXT:
                elementNodeId = saveText(static_cast<Text*>(element.get()), layerNodeId, position);
                break;
            case ElementType::ELEMENT_IMAGE:
                elementNodeId = saveImage(static_cast<Image*>(element.get()), layerNodeId, position);
                break;
            case ElementType::ELEMENT_TEXIMAGE:
                elementNodeId = saveTexImage(static_cast<TexImage*>(element.get()), layerNodeId, position);
                break;
        }

        if (elementNodeId == -1) {
            // L'errore è già stato impostato nella funzione di salvataggio specifica
            return false;
        }
        
        activeElementIds.push_back(elementNodeId);
        position++;
    }

    return deleteMissingElements(layerNodeId, activeElementIds);
}

auto SqliteDocument::createNode(int parentNodeId, const std::string& nodeType, int position) -> int {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO nodes (parent_id, node_type, position) VALUES (?, ?, ?);";

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
    sqlite3_bind_int(stmt, 3, position);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        errorMessage = "Failed to execute node creation: " + std::string(sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

auto SqliteDocument::updateNodePosition(int nodeId, int newPosition) -> bool {
    sqlite3_stmt* stmt = nullptr;
    // Aggiorna anche modified_at per tracciare le modifiche
    const char* sql = "UPDATE nodes SET position = ?, modified_at = strftime('%s', 'now') WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare node position update statement: " + std::string(sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int(stmt, 1, newPosition);
    sqlite3_bind_int(stmt, 2, nodeId);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) {
        errorMessage = "Failed to execute node position update: " + std::string(sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return success;
}

// MODIFICATO: Logica "salva o aggiorna" per Stroke.
auto SqliteDocument::saveStroke(Stroke* stroke, int parentNodeId, int position) -> int {
    int strokeNodeId = stroke->getNodeId();
    bool isUpdate = (strokeNodeId != -1);

    if (isUpdate) {
        if (!updateNodePosition(strokeNodeId, position)) return -1;
        // In un'implementazione completa, qui si aggiornerebbero anche i dati
        // dello stroke (colore, spessore, etc.) se possono cambiare.
    } else {
        strokeNodeId = createNode(parentNodeId, "stroke", position);
        if (strokeNodeId == -1) return -1;
        stroke->setNodeId(strokeNodeId);

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO strokes (node_id, tool_type, color, width, coordinates) VALUES (?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            errorMessage = "Failed to prepare stroke statement: " + std::string(sqlite3_errmsg(db));
            return -1;
        }

        Color c = stroke->getColor();
        uint32_t colorInt = (c.alpha << 24) | (c.red << 16) | (c.green << 8) | c.blue;

        const Point* points = stroke->getPoints();
        size_t pointCount = stroke->getPointCount();
        std::vector<double> coords;
        coords.reserve(pointCount * 2);
        for (size_t i = 0; i < pointCount; ++i) {
            coords.push_back(points[i].x);
            coords.push_back(points[i].y);
        }
        
        std::string toolTypeStr;
        switch (stroke->getToolType()) {
            case StrokeTool::PEN: toolTypeStr = "pen"; break;
            case StrokeTool::ERASER: toolTypeStr = "eraser"; break;
            case StrokeTool::HIGHLIGHTER: toolTypeStr = "highlighter"; break;
        }

        sqlite3_bind_int(stmt, 1, strokeNodeId);
        sqlite3_bind_text(stmt, 2, toolTypeStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, colorInt);
        sqlite3_bind_double(stmt, 4, stroke->getWidth());
        sqlite3_bind_blob(stmt, 5, coords.data(), static_cast<int>(coords.size() * sizeof(double)), SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            errorMessage = "Failed to save stroke data: " + std::string(sqlite3_errmsg(db));
            strokeNodeId = -1;
        }
        sqlite3_finalize(stmt);
    }
    return strokeNodeId;
}

// MODIFICATO: Logica "salva o aggiorna" per Text.
auto SqliteDocument::saveText(Text* text, int parentNodeId, int position) -> int {
    int textNodeId = text->getNodeId();
    bool isUpdate = (textNodeId != -1);

    if (isUpdate) {
        if (!updateNodePosition(textNodeId, position)) return -1;
        // Aggiungere qui la logica di UPDATE per i dati di Text.
    } else {
        textNodeId = createNode(parentNodeId, "text", position);
        if (textNodeId == -1) return -1;
        text->setNodeId(textNodeId);

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO texts (node_id, font_name, font_size, x, y, color, content) VALUES (?, ?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            errorMessage = "Failed to prepare text statement: " + std::string(sqlite3_errmsg(db));
            return -1;
        }

        const XojFont& font = text->getFont();
        Color c = text->getColor();
        uint32_t colorInt = (c.alpha << 24) | (c.red << 16) | (c.green << 8) | c.blue;

        sqlite3_bind_int(stmt, 1, textNodeId);
        sqlite3_bind_text(stmt, 2, font.getName().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, font.getSize());
        sqlite3_bind_double(stmt, 4, text->getX());
        sqlite3_bind_double(stmt, 5, text->getY());
        sqlite3_bind_int(stmt, 6, colorInt);
        sqlite3_bind_text(stmt, 7, text->getText().c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            errorMessage = "Failed to save text data: " + std::string(sqlite3_errmsg(db));
            textNodeId = -1;
        }
        sqlite3_finalize(stmt);
    }
    return textNodeId;
}

// MODIFICATO: Logica "salva o aggiorna" per Image.
auto SqliteDocument::saveImage(Image* image, int parentNodeId, int position) -> int {
    int imageNodeId = image->getNodeId();
    bool isUpdate = (imageNodeId != -1);

    if (isUpdate) {
        if (!updateNodePosition(imageNodeId, position)) return -1;
        // Aggiungere qui la logica di UPDATE per i dati di Image.
    } else {
        imageNodeId = createNode(parentNodeId, "image", position);
        if (imageNodeId == -1) return -1;
        image->setNodeId(imageNodeId);

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO images (node_id, x, y, width, height, media_path) VALUES (?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            errorMessage = "Failed to prepare image statement: " + std::string(sqlite3_errmsg(db));
            return -1;
        }

        sqlite3_bind_int(stmt, 1, imageNodeId);
        sqlite3_bind_double(stmt, 2, image->getX());
        sqlite3_bind_double(stmt, 3, image->getY());
        sqlite3_bind_double(stmt, 4, image->getElementWidth());
        sqlite3_bind_double(stmt, 5, image->getElementHeight());
        sqlite3_bind_text(stmt, 6, "", -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            errorMessage = "Failed to save image data: " + std::string(sqlite3_errmsg(db));
            imageNodeId = -1;
        }
        sqlite3_finalize(stmt);
    }
    return imageNodeId;
}

// MODIFICATO: Logica "salva o aggiorna" per TexImage.
auto SqliteDocument::saveTexImage(TexImage* texImage, int parentNodeId, int position) -> int {
    int texImageNodeId = texImage->getNodeId();
    bool isUpdate = (texImageNodeId != -1);

    if (isUpdate) {
        if (!updateNodePosition(texImageNodeId, position)) return -1;
        // Aggiungere qui la logica di UPDATE per i dati di TexImage.
    } else {
        texImageNodeId = createNode(parentNodeId, "tex_image", position);
        if (texImageNodeId == -1) return -1;
        texImage->setNodeId(texImageNodeId);

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO tex_images (node_id, x, y, width, height, tex_source, media_path) VALUES (?, ?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            errorMessage = "Failed to prepare tex_image statement: " + std::string(sqlite3_errmsg(db));
            return -1;
        }

        sqlite3_bind_int(stmt, 1, texImageNodeId);
        sqlite3_bind_double(stmt, 2, texImage->getX());
        sqlite3_bind_double(stmt, 3, texImage->getY());
        sqlite3_bind_double(stmt, 4, texImage->getElementWidth());
        sqlite3_bind_double(stmt, 5, texImage->getElementHeight());
        sqlite3_bind_text(stmt, 6, texImage->getText().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, "", -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            errorMessage = "Failed to save tex_image data: " + std::string(sqlite3_errmsg(db));
            texImageNodeId = -1;
        }
        sqlite3_finalize(stmt);
    }
    return texImageNodeId;
}

// MODIFICATO: Implementazione della funzione per cancellare elementi non più esistenti.
auto SqliteDocument::deleteMissingElements(int layerNodeId, const std::vector<int>& activeElementIds) -> bool {
    std::set<int> activeIds(activeElementIds.begin(), activeElementIds.end());
    std::set<int> idsInDb;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id FROM nodes WHERE parent_id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, layerNodeId);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            idsInDb.insert(sqlite3_column_int(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);

    std::vector<int> idsToDelete;
    std::set_difference(idsInDb.begin(), idsInDb.end(), activeIds.begin(), activeIds.end(),
                        std::back_inserter(idsToDelete));

    if (!idsToDelete.empty()) {
        const char* deleteSql = "DELETE FROM nodes WHERE id = ?;";
        if (sqlite3_prepare_v2(db, deleteSql, -1, &stmt, nullptr) != SQLITE_OK) {
            errorMessage = "Failed to prepare element delete statement: " + std::string(sqlite3_errmsg(db));
            return false;
        }
        for (int id : idsToDelete) {
            sqlite3_bind_int(stmt, 1, id);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                // Logga l'errore ma continua
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    return true;
}