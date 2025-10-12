#include "control/sqlite/SqliteControl.h"

#include <filesystem>
#include <memory>

#include "control/sqlite/SqliteDocument.h"
#include "control/sqlite/SqliteSchema.h"
#include "model/Document.h"

namespace fs = std::filesystem;

SqliteControl::SqliteControl(Document* doc) : document(doc) {}

SqliteControl::~SqliteControl() { closeDatabase(); }

auto SqliteControl::getErrorMessage() const -> std::string { return errorMessage; }

void SqliteControl::closeDatabase() {
    if (db) {
        sqlite3_close_v2(db);
        db = nullptr;
    }
}

auto SqliteControl::openDatabase(const std::string& dbPath) -> bool {
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        errorMessage = "Cannot open database: " + std::string(sqlite3_errmsg(db));
        closeDatabase();
        return false;
    }
    return true;
}

auto SqliteControl::executeAndCheck(const char* sql, const std::string& errorMsg) -> bool {
    char* zErrMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &zErrMsg) != SQLITE_OK) {
        errorMessage = errorMsg + ": " + zErrMsg;
        sqlite3_free(zErrMsg);
        return false;
    }
    return true;
}

auto SqliteControl::createSchema() -> bool {
    return executeAndCheck(xoj::sqlite::SCHEMA_SQL, "Failed to create schema");
}

auto SqliteControl::save(const std::string& path, const std::string& oldPath) -> bool {
    std::string dbPath = path;
    bool isNewFile = !fs::exists(dbPath);

    if (!oldPath.empty() && path != oldPath && fs::exists(oldPath)) {
        try {
            fs::copy_file(oldPath, dbPath, fs::copy_options::overwrite_existing);
        } catch (const fs::filesystem_error& e) {
            errorMessage = "Failed to copy database: " + std::string(e.what());
            return false;
        }
    }

    if (!openDatabase(dbPath)) {
        return false;
    }

    if (isNewFile) {
        if (!createSchema()) {
            return false;
        }
    }

    docHandler = std::make_unique<SqliteDocument>(db, document);

    if (!executeAndCheck("BEGIN TRANSACTION;", "Failed to begin transaction")) {
        return false;
    }

    if (!docHandler->save()) {
        errorMessage = docHandler->getErrorMessage();
        executeAndCheck("ROLLBACK;", "");
        return false;
    }

    if (!executeAndCheck("COMMIT;", "Failed to commit transaction")) {
        executeAndCheck("ROLLBACK;", "");
        return false;
    }

    closeDatabase();
    return true;
}