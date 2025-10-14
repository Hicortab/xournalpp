// src/core/control/xojfile/SqliteSaveHandler.cpp
#include "SqliteSaveHandler.h"

#include "control/sqlite/SqliteDocument.h"
#include "model/Document.h"

SqliteSaveHandler::SqliteSaveHandler() = default;
SqliteSaveHandler::~SqliteSaveHandler() = default;

void SqliteSaveHandler::prepareSave(const Document* doc, const fs::path& target) {
    this->document = doc;
    this->document->filepath = target;
}

auto SqliteControl::save(const std::string& path, const std::string& oldPath) -> bool {
    std::string dbPath = path;
    bool isNewFile = !fs::exists(dbPath);

    SqliteDatabaseUtility utility;

    if (!oldPath.empty() && path != oldPath && fs::exists(oldPath)) {
        try {
            fs::copy_file(oldPath, dbPath, fs::copy_options::overwrite_existing);
        } catch (const fs::filesystem_error& e) {
            errorMessage = "Failed to copy database: " + std::string(e.what());
            return false;
        }
    }

    if (!utility.openDatabase(dbPath)) {
        return false;
    }

    if (isNewFile) {
        if (!utility.createSchema()) {
            return false;
        }
    }

    docHandler = std::make_unique<SqliteDocument>(db, document);

    if (!utility.executeAndCheck("BEGIN TRANSACTION;", "Failed to begin transaction")) {
        return false;
    }

    if (!docHandler->save()) {
        errorMessage = docHandler->getErrorMessage();
        utility.executeAndCheck("ROLLBACK;", "");
        return false;
    }

    if (!utility.executeAndCheck("COMMIT;", "Failed to commit transaction")) {
        utility.executeAndCheck("ROLLBACK;", "");
        return false;
    }

    utility.closeDatabase();
    return true;
}

void SqliteSaveHandler::saveTo(const fs::path& filepath, ProgressListener* listener) {
    // Correzione: u8string() ritorna una std::u8string. Usiamo string() per std::string.
    // E la vecchia path è utile per la logica di copia.
    std::string oldPath = this->document->getFilepath().string();
    std::string newPath = filepath.string();
    
    if (oldPath == newPath) {
        oldPath.clear(); // Se stiamo salvando sullo stesso file, non c'è una "vecchia" path
    }

    if (!save(newPath, oldPath)) {
        this->errorMessage = getErrorMessage();
    }
}


const std::string& SqliteSaveHandler::getErrorMessage() {
    // Se sqliteControl non è stato creato, ritorna un messaggio di errore locale
    if (!sqliteControl) {
        return this->errorMessage;
    }
    // NOTA: Questo ritornerà l'errore dell'ULTIMA operazione.
    // Per ora va bene, ma in futuro potrebbe essere migliorato.
    this->errorMessage = sqliteControl->getErrorMessage();
    return this->errorMessage;
}