// src/core/control/xojfile/SqliteSaveHandler.cpp
#include "SqliteSaveHandler.h"

#include "control/sqlite/SqliteControl.h"
#include "model/Document.h"

SqliteSaveHandler::SqliteSaveHandler() = default;
SqliteSaveHandler::~SqliteSaveHandler() = default;

void SqliteSaveHandler::prepareSave(const Document* doc, const fs::path& target) {
    this->document = doc;
}

void SqliteSaveHandler::saveTo(const fs::path& filepath, ProgressListener* listener) {
    // Correzione: Non esiste XojFile, si usa direttamente il Document*
    sqliteControl = std::make_unique<SqliteControl>(const_cast<Document*>(this->document));

    // Correzione: u8string() ritorna una std::u8string. Usiamo string() per std::string.
    // E la vecchia path è utile per la logica di copia.
    std::string oldPath = this->document->getFilepath().string();
    std::string newPath = filepath.string();
    
    if (oldPath == newPath) {
        oldPath.clear(); // Se stiamo salvando sullo stesso file, non c'è una "vecchia" path
    }

    if (!sqliteControl->save(newPath, oldPath)) {
        this->errorMessage = sqliteControl->getErrorMessage();
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