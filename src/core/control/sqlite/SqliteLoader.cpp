#include "control/sqlite/SqliteLoader.h"

#include "model/Document.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/Image.h"
#include "model/TexImage.h"
#include "model/XojPage.h"
#include "model/Font.h"

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
        return false;
    }

    if (!loadPages()) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to commit transaction";
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    closeDatabase();
    return true;
}

auto SqliteLoader::loadPages() -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, width, height FROM pages ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading pages";
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        double width = sqlite3_column_double(stmt, 1);
        double height = sqlite3_column_double(stmt, 2);

        auto page = std::make_unique<XojPage>(width, height);
        if (!loadLayers(page.get(), id)) {
            sqlite3_finalize(stmt);
            return false;
        }
        document->addPage(std::move(page));
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadLayers(XojPage* page, int pageId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id FROM layers WHERE page_id = ? ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading layers";
        return false;
    }

    sqlite3_bind_int(stmt, 1, pageId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        auto layer = new Layer();
        
        if (!loadStrokes(layer, id) || !loadTexts(layer, id) || !loadImages(layer, id) || !loadTexImages(layer, id)) {
            sqlite3_finalize(stmt);
            delete layer;
            return false;
        }
        
        page->addLayer(layer);
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadStrokes(Layer* layer, int layerId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT tool, color, width, data FROM strokes WHERE layer_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading strokes";
        return false;
    }

    sqlite3_bind_int(stmt, 1, layerId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* tool = sqlite3_column_text(stmt, 0);
        int color = sqlite3_column_int(stmt, 1);
        double width = sqlite3_column_double(stmt, 2);
        const void* data = sqlite3_column_blob(stmt, 3);
        int dataSize = sqlite3_column_bytes(stmt, 3);

        auto stroke = std::make_unique<Stroke>();
        stroke->setColor(color);
        stroke->setWidth(width);
        
        const auto* points = static_cast<const Point*>(data);
        int pointCount = dataSize / sizeof(Point);
        for (int i = 0; i < pointCount; ++i) {
            stroke->addPoint(points[i]);
        }

        layer->addElement(std::move(stroke));
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadTexts(Layer* layer, int layerId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT content, font, font_size, color, x, y FROM texts WHERE layer_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading texts";
        return false;
    }

    sqlite3_bind_int(stmt, 1, layerId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* content = sqlite3_column_text(stmt, 0);
        const unsigned char* fontName = sqlite3_column_text(stmt, 1);
        double fontSize = sqlite3_column_double(stmt, 2);
        int color = sqlite3_column_int(stmt, 3);
        double x = sqlite3_column_double(stmt, 4);
        double y = sqlite3_column_double(stmt, 5);

        auto text = std::make_unique<Text>();
        text->setText((const char*)content);
        text->setColor(color);
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

auto SqliteLoader::loadImages(Layer* layer, int layerId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT data, x, y, width, height FROM images WHERE layer_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading images";
        return false;
    }

    sqlite3_bind_int(stmt, 1, layerId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* data = sqlite3_column_blob(stmt, 0);
        int dataSize = sqlite3_column_bytes(stmt, 0);
        double x = sqlite3_column_double(stmt, 1);
        double y = sqlite3_column_double(stmt, 2);
        double width = sqlite3_column_double(stmt, 3);
        double height = sqlite3_column_double(stmt, 4);

        auto image = std::make_unique<Image>();
        image->setImage(std::string(static_cast<const char*>(data), dataSize));
        image->setX(x);
        image->setY(y);
        image->setWidth(width);
        image->setHeight(height);

        layer->addElement(std::move(image));
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadTexImages(Layer* layer, int layerId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT text, data, x, y, width, height FROM tex_images WHERE layer_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading TeX images";
        return false;
    }

    sqlite3_bind_int(stmt, 1, layerId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        const void* data = sqlite3_column_blob(stmt, 1);
        int dataSize = sqlite3_column_bytes(stmt, 1);
        double x = sqlite3_column_double(stmt, 2);
        double y = sqlite3_column_double(stmt, 3);
        double width = sqlite3_column_double(stmt, 4);
        double height = sqlite3_column_double(stmt, 5);
        
        auto texImage = std::make_unique<TexImage>();
        texImage->setText((const char*)text);
        texImage->loadData(std::string(static_cast<const char*>(data), dataSize));
        texImage->setX(x);
        texImage->setY(y);
        texImage->setWidth(width);
        texImage->setHeight(height);

        layer->addElement(std::move(texImage));
    }

    sqlite3_finalize(stmt);
    return true;
}