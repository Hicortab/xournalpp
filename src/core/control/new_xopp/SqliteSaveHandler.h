#pragma once

#include <memory>
#include <cstdint>

class SqliteControl;
class Document; // Aggiunto per chiarezza

class SqliteSaveHandler {
public:
    SqliteSaveHandler();
    ~SqliteSaveHandler() override;

    void prepareSave(const Document* doc, const fs::path& target) override;
    auto save(const std::string& path, const std::string& oldPath) -> bool;
    void saveTo(const fs::path& filepath, ProgressListener* listener = nullptr) override;
    const std::string& getErrorMessage() override;

private:

    SqliteDatabaseUtility sqliteControl;

    const Document* document = nullptr;
    std::string errorMessage;
    
};