#pragma once

#include <string>
#include "filesystem.h"

class Document;
class ProgressListener;

class AbstractSaveHandler {
public:
    virtual ~AbstractSaveHandler() = default;
    virtual void prepareSave(const Document* doc, const fs::path& target) = 0;
    virtual void saveTo(const fs::path& filepath, ProgressListener* listener = nullptr) = 0;
    virtual const std::string& getErrorMessage() = 0;
};