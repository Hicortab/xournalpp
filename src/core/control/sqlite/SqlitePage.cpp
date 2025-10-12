#include "control/sqlite/SqlitePage.h"
#include "model/XojPage.h"
#include "control/pagetype/PageTypeHandler.h"

SqlitePage::SqlitePage(sqlite3* db, XojPage* page) : db(db), page(page) {}
auto SqlitePage::getErrorMessage() const -> std::string { return errorMessage; }

auto SqlitePage::save(int64_t nodeId) -> bool {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO pages (node_id, width, height, background_type, background_color) VALUES (?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        errorMessage = "Failed to prepare page save query: " + std::string(sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, nodeId);
    sqlite3_bind_double(stmt, 2, page->getWidth());
    sqlite3_bind_double(stmt, 3, page->getHeight());
    
    PageType bgType = page->getBackgroundType();
    std::string bgTypeStr = PageTypeHandler::getStringForPageTypeFormat(bgType.format);
    sqlite3_bind_text(stmt, 4, bgTypeStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, static_cast<uint32_t>(page->getBackgroundColor()));

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        errorMessage = "Failed to save page data: " + std::string(sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}