#include "control/sqlite/SqliteLoader.h"

// Aggiunti per la gestione dei tipi di pagina e dei PDF
#include "control/pagetype/PageTypeHandler.h"
#include "model/BackgroundImage.h"
#include "model/Document.h"
#include "model/Font.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/PageType.h"
#include "model/Stroke.h"
#include "model/TexImage.h"
#include "model/Text.h"
#include "model/XojPage.h"
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
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
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

    if (sqlite3_exec(db, "BEGIN DEFERRED TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
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
    const char* sql = "SELECT n.id, p.width, p.height, p.background_type, p.background_color, p.background_pdf_page, p.background_pdf_filename FROM nodes n JOIN pages p ON n.id = p.node_id WHERE n.node_type = 'PAGE' AND n.parent_id IS NULL ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading pages: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    bool pdfParsed = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int pageNodeId = sqlite3_column_int(stmt, 0);
        double width = sqlite3_column_double(stmt, 1);
        double height = sqlite3_column_double(stmt, 2);

        auto page = std::make_unique<XojPage>(width, height);

        // Carica le informazioni sullo sfondo
        const char* bgTypeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (bgTypeStr) {
            PageType bgType;
            bgType.format = PageTypeHandler::getPageTypeFormatForString(bgTypeStr);
            page->setBackgroundType(bgType);

            /*
            
                TODO xopj

            if (bgType.format == PageTypeFormat::Pdf && !pdfParsed) {
                const char* pdfFilename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
                if (pdfFilename) {
                    // Imposta le informazioni del PDF sul documento.
                    // Nota: qui si assume che il PDF non sia allegato ma un file esterno.
                    // La logica per i PDF allegati andrebbe gestita qui.
                    document->setPdfPath(fs::path(pdfFilename));
                    pdfParsed = true; // Per evitare di ricaricare lo stesso PDF per ogni pagina
                }
            }*/
        }

        int bgColorInt = sqlite3_column_int(stmt, 4);
        page->setBackgroundColor(Color(static_cast<uint32_t>(bgColorInt)));

        int pdfPageNum = sqlite3_column_int(stmt, 5);
        if (pdfPageNum > 0) {
            page->setBackgroundPdfPageNr(pdfPageNum - 1);
        }

        if (!loadLayers(page.get(), pageNodeId)) {
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
    const char* sql = "SELECT n.id FROM nodes n WHERE n.node_type = 'LAYER' AND n.parent_id = ? ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading layers: " + std::string(sqlite3_errmsg(db));
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
    const char* sql = "SELECT s.tool_type, s.color, s.width, s.coordinates, s.pressure_data FROM nodes n JOIN strokes s ON n.id = s.node_id WHERE n.node_type = 'STROKE' AND n.parent_id = ? ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading strokes: " + std::string(sqlite3_errmsg(db));
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
        
        if (toolStr) {
            if (strcmp(reinterpret_cast<const char*>(toolStr), "eraser") == 0) {
                stroke->setToolType(StrokeTool::ERASER);
            } else if (strcmp(reinterpret_cast<const char*>(toolStr), "highlighter") == 0) {
                stroke->setToolType(StrokeTool::HIGHLIGHTER);
            } else {
                stroke->setToolType(StrokeTool::PEN);
            }
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
    const char* sql = "SELECT t.content, t.font_name, t.font_size, t.color, t.x, t.y FROM nodes n JOIN texts t ON n.id = t.node_id WHERE n.node_type = 'TEXT' AND n.parent_id = ? ORDER BY n.position;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare statement for loading texts: " + std::string(sqlite3_errmsg(db));
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
        text->setText(reinterpret_cast<const char*>(content));
        text->setColor(Color(static_cast<uint32_t>(colorInt)));
        text->setX(x);
        text->setY(y);

        XojFont& font = text->getFont();
        font.setName(reinterpret_cast<const char*>(fontName));
        font.setSize(fontSize);
        
        layer->addElement(std::move(text));
    }

    sqlite3_finalize(stmt);
    return true;
}

auto SqliteLoader::loadImages(Layer* layer, int layerNodeId) -> bool {
    // Questa funzione andrebbe implementata per caricare i dati da media_files
    return true;
}

auto SqliteLoader::loadTexImages(Layer* layer, int layerNodeId) -> bool {
    // Questa funzione andrebbe implementata per caricare i dati da media_files
    return true;
}