#include "SqliteDocument.h"
#include <sqlite3.h>  // for sqlite3, sqlite3_exec, sqlite3_errmsg, s...

auto SqliteDatabaseUtility::getErrorMessage() const -> std::string { return errorMessage; }

auto SqliteDatabaseUtility::createSchema() -> bool {
    return executeQuery(xoj::sqlite::SCHEMA_SQL, "Failed to create schema");
}

auto SqliteDatabaseUtility::executeQuery(sqlite3* db, const char* sql, const std::string& errorMsg) -> bool {
    char* zErrMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &zErrMsg) != SQLITE_OK) {
        errorMessage = errorMsg + ": " + zErrMsg;
        sqlite3_free(zErrMsg);
        return false;
    }
    return true;
}


void SqliteDatabaseUtility::closeDatabase(sqlite3* db) {
    if (db) {
        sqlite3_close_v2(db);
        db = nullptr;
    }
}

auto SqliteLoader::openDatabase(sqlite3* db, const std::string& dbPath) -> bool {
    if (sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        errorMessage = "Cannot open database: " + std::string(sqlite3_errmsg(db));
        closeDatabase();
        return false;
    }
    return true;
}