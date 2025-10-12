// src/core/control/xojfile/SqliteSaveHandler.h
#pragma once

#include "control/xojfile/AbstractSaveHandler.h"
#include <memory>
#include <cstdint>

class SqliteControl;
class Document; // Aggiunto per chiarezza

class SqliteSaveHandler : public AbstractSaveHandler {
public:
    SqliteSaveHandler();
    ~SqliteSaveHandler() override;

    void prepareSave(const Document* doc, const fs::path& target) override;
    void saveTo(const fs::path& filepath, ProgressListener* listener = nullptr) override;
    const std::string& getErrorMessage() override;

private:
    std::unique_ptr<SqliteControl> sqliteControl;
    const Document* document = nullptr;
    std::string errorMessage;
};