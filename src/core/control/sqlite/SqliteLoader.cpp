#include "control/sqlite/SqliteLoader.h"

#include "model/Document.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/Image.h"
#include "model/TexImage.h"
#include "model/XojPage.h"
#include "model/Font.h"
#include "util/Color.h"

SqliteLoader::SqliteLoader(Document* doc) : document(doc) {}

SqliteLoader::~SqliteLoader() { closeDatabase(); }

auto SqliteLoader::getErrorMessage() const -> std::string { return errorMessage; }

void SqliteLoader::closeDatabase() {
    if (db) {
        sqlite3_close_v2(db);
        db = nullptr;
    }
}

auto SqliteLoader::openDatabase(const std::string& dbPath) -> bool {
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        errorMessage = "Cannot open database: " + std::string(sqlite3_errmsg(db));
        closeDatabase();
        return false;
    }
    return true;
}

auto SqliteLoader::load(const std::string& path) -> bool {
    if (!openDatabase(path)) {
        return false;
    }

    if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to begin transaction";
        closeDatabase();
        return false;
    }

    if (!loadPages()) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        closeDatabase();
        return false;
    }

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to commit transaction";
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        closeDatabase();
        return false;
    }

    closeDatabase();
    return true;
}

auto SqliteLoader::loadPages() -> bool {
    sqlite3_stmt* stmt = nullptr;
    // Seleziona i nodi di tipo 'page' e uniscili con i loro attributi nella tabella 'pages'
    const char* sql = "SELECT n.id, p.width, p.height FROM nodes n JOIN pages p ON n.id = p.node_id WHERE n.node_type = 'page' ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading pages";
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int nodeId = sqlite3_column_int(stmt, 0);
        double width = sqlite3_column_double(stmt, 1);
        double height = sqlite3_column_double(stmt, 2);

        auto page = std::make_unique<XojPage>(width, height);
        if (!loadLayers(page.get(), nodeId)) { // Passa il nodeId della pagina
            sqlite3_finalize(stmt);
            return false;
        }
        document->addPage(std::move(page));
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadLayers(XojPage* page, int pageNodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    // Seleziona i nodi 'layer' figli della pagina corrente
    const char* sql = "SELECT n.id FROM nodes n WHERE n.node_type = 'layer' AND n.parent_id = ? ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading layers";
        return false;
    }

    sqlite3_bind_int(stmt, 1, pageNodeId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int layerNodeId = sqlite3_column_int(stmt, 0);
        auto layer = new Layer();
        
        if (!loadStrokes(layer, layerNodeId) || !loadTexts(layer, layerNodeId) || !loadImages(layer, layerNodeId) || !loadTexImages(layer, layerNodeId)) {
            sqlite3_finalize(stmt);
            delete layer;
            return false;
        }
        
        page->addLayer(layer);
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadStrokes(Layer* layer, int layerNodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT s.tool_type, s.color, s.width, s.coordinates, s.pressure_data FROM nodes n JOIN strokes s ON n.id = s.node_id WHERE n.node_type = 'stroke' AND n.parent_id = ? ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading strokes";
        return false;
    }

    sqlite3_bind_int(stmt, 1, layerNodeId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* toolStr = sqlite3_column_text(stmt, 0);
        int colorInt = sqlite3_column_int(stmt, 1);
        double width = sqlite3_column_double(stmt, 2);
        const void* coordsData = sqlite3_column_blob(stmt, 3);
        int coordsSize = sqlite3_column_bytes(stmt, 3);
        const void* pressureData = sqlite3_column_blob(stmt, 4);
        int pressureSize = sqlite3_column_bytes(stmt, 4);

        auto stroke = std::make_unique<Stroke>();
        stroke->setColor(Color(static_cast<uint32_t>(colorInt)));
        stroke->setWidth(width);
        
        if (strcmp(reinterpret_cast<const char*>(toolStr), "eraser") == 0) {
            stroke->setToolType(StrokeTool::ERASER);
        } else if (strcmp(reinterpret_cast<const char*>(toolStr), "highlighter") == 0) {
            stroke->setToolType(StrokeTool::HIGHLIGHTER);
        } else {
            stroke->setToolType(StrokeTool::PEN);
        }

        const auto* points = static_cast<const Point*>(coordsData);
        int pointCount = coordsSize / sizeof(Point);
        for (int i = 0; i < pointCount; ++i) {
            stroke->addPoint(points[i]);
        }
        
        if (pressureData && pressureSize > 0) {
            const auto* pressures = static_cast<const double*>(pressureData);
            int pressureCount = pressureSize / sizeof(double);
            std::vector<double> pressureVec(pressures, pressures + pressureCount);
            stroke->setPressure(pressureVec);
        }

        layer->addElement(std::move(stroke));
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadTexts(Layer* layer, int layerNodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT t.content, t.font_name, t.font_size, t.color, t.x, t.y FROM nodes n JOIN texts t ON n.id = t.node_id WHERE n.node_type = 'text' AND n.parent_id = ? ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading texts";
        return false;
    }

    sqlite3_bind_int(stmt, 1, layerNodeId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* content = sqlite3_column_text(stmt, 0);
        const unsigned char* fontName = sqlite3_column_text(stmt, 1);
        double fontSize = sqlite3_column_double(stmt, 2);
        int colorInt = sqlite3_column_int(stmt, 3);
        double x = sqlite3_column_double(stmt, 4);
        double y = sqlite3_column_double(stmt, 5);

        auto text = std::make_unique<Text>();
        text->setText((const char*)content);
        text->setColor(Color(static_cast<uint32_t>(colorInt)));
        text->setX(x);
        text->setY(y);

        XojFont& font = text->getFont();
        font.setName((const char*)fontName);
        font.setSize(fontSize);
        
        layer->addElement(std::move(text));
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadImages(Layer* layer, int layerNodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    // NOTA: Questa query assume che i dati dell'immagine siano ancora in un BLOB.
    // Per usare media_path, dovresti fare un'altra query alla tabella media_files.
    const char* sql = "SELECT i.x, i.y, i.width, i.height, m.path FROM nodes n JOIN images i ON n.id = i.node_id JOIN media_files m ON i.media_path = m.path WHERE n.node_type = 'image' AND n.parent_id = ? ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading images";
        return false;
    }

    sqlite3_bind_int(stmt, 1, layerNodeId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        double x = sqlite3_column_double(stmt, 0);
        double y = sqlite3_column_double(stmt, 1);
        double width = sqlite3_column_double(stmt, 2);
        double height = sqlite3_column_double(stmt, 3);
        // const void* data = sqlite3_column_blob(stmt, 4); // Esempio se si usasse un BLOB
        // int dataSize = sqlite3_column_bytes(stmt, 4);

        auto image = std::make_unique<Image>();
        // image->setImage(std::string(static_cast<const char*>(data), dataSize));
        image->setX(x);
        image->setY(y);
        image->setWidth(width);
        image->setHeight(height);
        
        // Qui dovresti caricare l'immagine dal percorso specificato in media_path
        // Per ora, lasciamo l'immagine vuota

        layer->addElement(std::move(image));
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadTexImages(Layer* layer, int layerNodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT t.tex_source, t.x, t.y, t.width, t.height FROM nodes n JOIN tex_images t ON n.id = t.node_id WHERE n.node_type = 'teximage' AND n.parent_id = ? ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading TeX images";
        return false;
    }

    sqlite3_bind_int(stmt, 1, layerNodeId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        double x = sqlite3_column_double(stmt, 1);
        double y = sqlite3_column_double(stmt, 2);
        double width = sqlite3_column_double(stmt, 3);
        double height = sqlite3_column_double(stmt, 4);
        
        auto texImage = std::make_unique<TexImage>();
        texImage->setText((const char*)text);
        texImage->setX(x);
        texImage->setY(y);
        texImage->setWidth(width);
        texImage->setHeight(height);
        
        // Anche qui, i dati dell'immagine renderizzata dovrebbero essere caricati
        // dal percorso in media_path.

        layer->addElement(std::move(texImage));
    }

    sqlite3_finalize(stmt);
    return true;
}